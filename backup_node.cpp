#include "livemodifiable.h"

signed main(int argc, char* argv[])
{
    signal(SIGINT, SIG_IGN);

    string IP_ADDRESS(IP_ADDR);
    if (argc >= 2) {
        IP_ADDRESS = argv[1];
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

    int recv_ = 0;
    map<pid_t, string> file;
    while(1)
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

        switch (packet_type)
        {
            case INFO         :{
                                    char* info_msg = (char*)calloc(INFOSIZE+1, sizeof(char));
                                    if (recvInfo(socket_fd, info_msg) < 0)
                                    {
                                        RED << LOG; perror("Error in Receiving Info Packet"); RESET1
                                        exit(EXIT_FAILURE);
                                    }

                                    free(info_msg);
                                    break;
                               }
            case BACKUPINFO   :{
                                    pid_t pid;
                                    char* info_msg = (char*)calloc(INFOSIZE+1, sizeof(char));
                                    if (recvBackupInfo(socket_fd, info_msg, &pid) < 0)
                                    {
                                        RED << LOG; perror("Error in Receiving Backup Info"); RESET1
                                        exit(EXIT_FAILURE);
                                    }
                                    char action[] = {0, 0, 0, 0};
                                    char filename[FILESIZE+1];
                                    bzero(filename, (FILESIZE+1)*sizeof(char));
                                    sscanf(info_msg, "%s %s", action, filename);
                                    file[pid] = "BACKUP_" + string(filename);

                                    int fd = open((char*)(file[pid].c_str()), O_CREAT | O_APPEND);
                                    if (fd < 0) {
                                        RED << LOG << file[pid]; perror(" Error in Creating"); RESET1
                                        continue;
                                    }
                                    close(fd);

                                    free(info_msg);
                                    break;
                                }
            case BACKUPDATA   :{
                                    pid_t pid; int offset;
                                    char* data = (char*)calloc(DATASIZE+1, sizeof(char));
                                    if ((recv_ = recvBackupData(socket_fd, &offset, data, &pid)) < 0) {
                                        RED << LOG; perror("Error in Receiving Backup Data"); RESET1;
                                        continue;
                                    }

                                    int fd = open((char*)(file[pid].c_str()), O_WRONLY | O_APPEND);
                                    if (fd < 0) {
                                        RED << LOG << file[pid]; perror(" Error in Opening"); RESET1
                                        continue;
                                    }
                                    if (write(fd, data, recv_) < 0) {
                                        RED << LOG; perror("Error in Writing to Backup File"); RESET1
                                        continue;
                                    }
                                    close(fd);

                                    free(data);
                                    break;
                                }
        }
    }
    exit(EXIT_SUCCESS);
}
