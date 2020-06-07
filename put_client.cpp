#include "livemodifiable.h"

static volatile sig_atomic_t terminate_(0);
static void sig_handler(int signo)
{
    if(signo == SIGINT)
    {
        terminate_ = 1;
    }
}

signed main(int argc, char* argv[])
{
    signal(SIGINT, sig_handler);

    if (argc < 2) {
        RED << "Insufficient CLA"; RESET2;
        exit(EXIT_FAILURE);
    }
    string FileName(argv[1]);

    int fd = open((char*)FileName.c_str(), O_RDONLY);
    if (fd < 0) {
        RED << getpid() << ":: "; perror("Error in Opening File"); RESET1
        exit(EXIT_FAILURE);
    }

    string IP_ADDRESS("10.0.0.1");
    if (argc >= 3) {
        IP_ADDRESS = argv[2];
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        RED << getpid() << ":: "; perror("Error in Creating Connection Socket"); RESET1
        exit(EXIT_FAILURE);
    }

    sockaddr_in serv_addr = {AF_INET, htons(PORT), inet_addr(IP_ADDRESS.c_str()), sizeof(sockaddr_in)};
    if (connect(socket_fd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        RED << getpid() << ":: "; perror("Error in Connecting to TCP Server"); RESET1
        exit(EXIT_FAILURE);
    }
    sleep(5);
    GREEN << getpid() << ":: CONNECTED to SERVER..."; RESET2;

    string Command("PUT " + FileName);
    if (sendInfo(socket_fd, (char*)Command.c_str()) < 0) {
        RED << getpid() << ":: "; perror("Error in Sending Command to Server"); RESET1
        exit(EXIT_FAILURE);
    }

    int offset = 0;
    int read_, recv_;
    while(!terminate_)
    {
        WHITE << "WAITING for Packet..."; RESET2;
        PacketType packet_type;
        if ((recv_ = recvType(socket_fd, packet_type)) < 0) {
            RED << getpid() << ":: "; perror("Error in Receiving Packet Type"); RESET1
            exit(EXIT_FAILURE);
        }
        if (recv_ == 0) {
            GREEN << getpid() << ":: SERVER TERMINATED CONNECTION"; RESET2;
            break;
        }

        switch(packet_type)
        {
            case PacketType::ERROR : {
                                        char* error_msg = (char*)calloc(ERRSIZE+1, sizeof(char));
                                        if (recvError(socket_fd, error_msg) < 0)
                                        {
                                            RED << getpid() << ":: "; perror("Error in Receiving Error Packet"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        free(error_msg);
                                        exit(EXIT_FAILURE);
                                     }

            case PacketType::INFO : {
                                        char* info_msg = (char*)calloc(INFOSIZE+1, sizeof(char));
                                        if (recvInfo(socket_fd, info_msg) < 0)
                                        {
                                            RED << getpid() << ":: "; perror("Error in Receiving Info Packet"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        free(info_msg);
                                        break;
                                    }

            case PacketType::ACK : {
                                        /* Receive ACK from Server */
                                        int new_offset;
                                        if (recvAck(socket_fd, &new_offset) < 0)
                                        {
                                            RED << getpid() << ":: "; perror("Error in Receiving Ack Packet"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        else
                                        {
                                            lseek(fd, new_offset - offset, SEEK_CUR);
                                            offset = new_offset;
                                        }

                                        /* Production of Data - Read from File */
                                        char* data = (char*)calloc((DATASIZE+1), sizeof(char));
                                        if ((read_ = read(fd, data, DATASIZE)) < 0)
                                        {
                                            RED << getpid() << ":: "; perror("Error in Reading File"); RESET1
                                            if (sendError(socket_fd, (char*)string("Error in Reading File").c_str()) < 0) {
                                                RED << getpid() << ":: "; perror("Error in Sending Error Message"); RESET1
                                            }
                                            exit(EXIT_FAILURE);
                                        }

                                        /* Send Produce to Server */
                                        if (sendData(socket_fd, read_, offset, data) < 0)
                                        {
                                            RED << getpid() << ":: "; perror("Error in Sending Data"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        else if (read_ == 0)
                                        {
                                            GREEN << getpid() << ":: File Transfer Complete"; RESET2;
                                            exit(EXIT_SUCCESS);
                                        }
                                        free(data);
                                        offset += read_;
                                        break;
                                    }

            default: RED << "Unexpected PacketType"; RESET2;
        }
        if(!recv_) break;
    }
    exit(EXIT_SUCCESS);
}
