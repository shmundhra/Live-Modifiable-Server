#include "livemodifiable.h"

signed main(int argc, char* argv[])
{
    signal(SIGINT, SIG_IGN);

    string IP_ADDRESS("127.0.0.1");
    if (argc >= 2) {
        IP_ADDRESS = argv[1];
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
    GREEN << getpid() << ":: CONNECTED to SERVER..."; RESET2;

    int recv_ = 0;
    while(1)
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

        switch (packet_type)
        {
            case PacketType::INFO   :{
                                        char* info_msg = (char*)calloc(INFOSIZE+1, sizeof(char));
                                        if (recvInfo(socket_fd, info_msg) < 0)
                                        {
                                            RED << getpid() << ":: "; perror("Error in Receiving Info Packet"); RESET1
                                            exit(EXIT_FAILURE);
                                        }
                                        free(info_msg);
                                        break;
                                    }
            case PacketType::DATA   :{
                                        pid_t pid; int offset;
                                        char* data = (char*)calloc(DATASIZE+1, sizeof(char));
                                        if ((recv_ = recvBackup(socket_fd, &offset, data, &pid)) < 0) {
                                            RED << getpid() << ":: "; perror("Error in Receiving Backup"); RESET1;
                                            continue;
                                        }

                                        int fd = open((char*)(to_string(pid).c_str()), O_WRONLY | O_CREAT | O_APPEND);
                                        if (fd < 0) {
                                            RED << getpid() << ":: "; perror("Error in Opening Backup File"); RESET1
                                            continue;
                                        }
                                        if (write(fd, data, recv_) < 0) {
                                            RED << getpid() << ":: "; perror("Error in Writing to Backup File"); RESET1
                                            continue;
                                        }

                                        free(data);
                                        break;
                                    }
        }
    }
    exit(EXIT_SUCCESS);
}
