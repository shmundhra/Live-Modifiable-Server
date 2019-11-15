#include "livemodifiable.h"

signed main(int argc, char* argv[])
{
    if (argc < 2) {
        RED cerr << "Insufficient CLA" << endl; RESET1
        exit(EXIT_FAILURE);
    }
    string FileName(argv[1]);

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        RED perror("Error in Creating Connection Socket"); RESET1
        exit(EXIT_FAILURE);
    }

    sockaddr_in serv_addr = {AF_INET, htons(PORT), inet_addr("127.0.0.1"), sizeof(sockaddr_in)};

    if (connect(socket_fd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        RED perror("Error in Connecting to TCP Server"); RESET1
        exit(EXIT_FAILURE);
    }
    GREEN cerr << "Connected to Server" << endl; RESET1

    if (sendInfo(socket_fd, (char*)FileName.c_str()) < 0) {
        RED perror("Error in Sending FileName Information to Server"); RESET1
        exit(EXIT_FAILURE);
    }

    int recv_ = 0;
    while(1)
    {   
        WHITE cerr << "Waiting for PacketHeader" << endl; RESET1
        PacketType packet_type;
        if ((recv_ = recvType(socket_fd, packet_type)) < 0) {
            RED perror("Error in Receiving Packet Type"); RESET1
            exit(EXIT_FAILURE);
        }
       
        switch(packet_type)
        {
            case PacketType::INFO : {
                                        char* info_msg = new char[INFOSIZE];
                                        if (recvInfo(socket_fd, info_msg) < 0) 
                                        {
                                            RED perror("Error in Receiving Info Packet"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        break;
                                    }

            case PacketType::DATA : {
                                        char* data = new char[DATASIZE];
                                        if ((recv_ = recvData(socket_fd, data)) < 0)
                                        {
                                            RED perror("Error in Receiving Data Packet"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        if (recv_ == 0) {
                                            GREEN cerr << "File Transfer is Complete" << endl; RESET1
                                            exit(EXIT_SUCCESS);
                                        }
                                        break;
                                    }

            default: RED cerr << "Unexpected PacketType" << endl; RESET1
        }
    }

    return 0;
}
