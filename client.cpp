#include "livemodifiable.h"

static volatile sig_atomic_t terminate_;
static void sig_handler(int signo)
{
    if(signo == SIGINT)
    {
        terminate_ = 1;
    }
}

signed main(int argc, char* argv[])
{
    terminate_ = 0;
    signal(SIGINT, sig_handler);

    if (argc < 2) {
        RED << "Insufficient CLA"; RESET2;
        exit(EXIT_FAILURE);
    }
    string FileName(argv[1]);

    string IP_ADDRESS("127.0.0.1");
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
    GREEN << "CONNECTED to SERVER..."; RESET2;

    if (sendInfo(socket_fd, (char*)FileName.c_str()) < 0) {
        RED << getpid() << ":: "; perror("Error in Sending FileName Information to Server"); RESET1
        exit(EXIT_FAILURE);
    }

    int recv_ = 0;
    while(!terminate_)
    {
        WHITE << "WAITING for Packet..."; RESET2;
        PacketType packet_type;
        if ((recv_ = recvType(socket_fd, packet_type)) < 0) {
            RED << getpid() << ":: "; perror("Error in Receiving Packet Type"); RESET1
            exit(EXIT_FAILURE);
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

            case PacketType::DATA : {
                                        /* Receive Produce from Server */
                                        int offset;
                                        char* data = (char*)calloc(DATASIZE+1, sizeof(char));
                                        if ((recv_ = recvData(socket_fd, &offset, data)) < 0)
                                        {
                                            RED << getpid() << ":: "; perror("Error in Receiving Data Packet"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        if (recv_ == 0) {
                                            GREEN << "FILE TRANSFER is COMPLETE"; RESET2;
                                            break;
                                        }
                                        free(data);

                                        /* Consume the Produce Received */
                                        offset += recv_;
                                        // if (offset < 1500) offset -= offset % 33;

                                        /* Send ACK of Consumption */
                                        if (sendAck(socket_fd, offset) < 0){
                                            RED << getpid() << ":: "; perror("Error in Sending Ack Packet");
                                            exit(EXIT_FAILURE);
                                        }
                                        break;
                                    }

            default: RED << "Unexpected PacketType"; RESET2;
        }
        if(!recv_) break;
    }
    exit(EXIT_SUCCESS);
}
