#include "livemodifiable.h"

signed main(int argc, char* argv[])
{   
    if (argc < 2) {
        RED << "Insufficient CLA"; RESET2;
        exit(EXIT_FAILURE);
    }
    string FileName(argv[1]);

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        RED; perror("Error in Creating Connection Socket"); RESET1
        exit(EXIT_FAILURE);
    }

    sockaddr_in serv_addr = {AF_INET, htons(PORT), inet_addr("127.0.0.1"), sizeof(sockaddr_in)};

    if (connect(socket_fd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        RED; perror("Error in Connecting to TCP Server"); RESET1
        exit(EXIT_FAILURE);
    }
    GREEN << "CONNECTED to SERVER..."; RESET2;

    if (sendInfo(socket_fd, (char*)FileName.c_str()) < 0) {
        RED; perror("Error in Sending FileName Information to Server"); RESET1
        exit(EXIT_FAILURE);
    }

    int recv_ = 0;
    while(1)
    {   
        WHITE << "WAITING for Packet..."; RESET2;
        PacketType packet_type;
        if ((recv_ = recvType(socket_fd, packet_type)) < 0) {
            RED; perror("Error in Receiving Packet Type"); RESET1
            exit(EXIT_FAILURE);
        }
       
        switch(packet_type)
        {
            case PacketType::INFO : {
                                        char* info_msg = (char*)calloc(INFOSIZE+1, sizeof(char));
                                        if (recvInfo(socket_fd, info_msg) < 0) 
                                        {
                                            RED; perror("Error in Receiving Info Packet"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        free(info_msg);
                                        break;
                                    }

            case PacketType::DATA : {
                                        char* data = (char*)calloc(DATASIZE+1, sizeof(char));
                                        if ((recv_ = recvData(socket_fd, data)) < 0)
                                        {
                                            RED; perror("Error in Receiving Data Packet"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        if (recv_ == 0) {
                                            GREEN << "FILE TRANSFER is COMPLETE"; RESET2;
                                            break;
                                        }
                                        free(data);
                                        break;
                                    }

            default: RED << "Unexpected PacketType"; RESET2;
        }
        if(!recv_) break;
    }
    exit(EXIT_SUCCESS);
}
