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

    string IP_ADDRESS(IP_ADDR);
    if (argc >= 3) {
        IP_ADDRESS = argv[2];
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        RED << LOG; perror("Error in Creating Connection Socket"); RESET1
        exit(EXIT_FAILURE);
    }

    sockaddr_in serv_addr = {AF_INET, htons(PORT), inet_addr(IP_ADDRESS.c_str()), sizeof(sockaddr_in)};
    if (connect(socket_fd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        RED << LOG; perror("Error in Connecting to TCP Server"); RESET1
        exit(EXIT_FAILURE);
    }
    if (EMULATING) sleep(5);
    GREEN << LOG << "CONNECTED to SERVER..."; RESET2;

    string Command("GET " + FileName);
    if (sendInfo(socket_fd, (char*)Command.c_str()) < 0) {
        RED << LOG; perror("Error in Sending Command to Server"); RESET1
        exit(EXIT_FAILURE);
    }

    int recv_ = 0;
    while(!terminate_)
    {
        WHITE << "WAITING for Packet..."; RESET2;
        PacketType packet_type;
        if ((recv_ = recvType(socket_fd, packet_type)) < 0) {
            RED << LOG; perror("Error in Receiving Packet Type"); RESET1
            exit(EXIT_FAILURE);
        }
        if (recv_ == 0) {
            GREEN << LOG << "SERVER TERMINATED CONNECTION"; RESET2;
            break;
        }

        switch(packet_type)
        {
            case PacketType::ERROR : {
                                        char* error_msg = (char*)calloc(ERRSIZE+1, sizeof(char));
                                        if (recvError(socket_fd, error_msg) < 0)
                                        {
                                            RED << LOG; perror("Error in Receiving Error Packet"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        free(error_msg);
                                        exit(EXIT_FAILURE);
                                     }

            case PacketType::INFO : {
                                        char* info_msg = (char*)calloc(INFOSIZE+1, sizeof(char));
                                        if (recvInfo(socket_fd, info_msg) < 0)
                                        {
                                            RED << LOG; perror("Error in Receiving Info Packet"); RESET1
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
                                            RED << LOG; perror("Error in Receiving Data Packet"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        if (recv_ == 0) {
                                            GREEN << "FILE TRANSFER is COMPLETE"; RESET2;
                                            break;
                                        }
                                        free(data);

                                        /* Consume the Produce Received */
                                        int next_offset = offset + recv_;
                                        // if (next_offset < 1500) next_offset -= next_offset % 33;

                                        /* Send ACK of Consumption */
                                        if (sendAck(socket_fd, next_offset) < 0){
                                            RED << LOG; perror("Error in Sending Ack Packet");
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
