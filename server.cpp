#include "livemodifiable.h"

int interrupt;
void sig_handler(int signo)
{
    if (signo == SIGMODIFY)
    {
        interrupt = 1;
    }
    if (signo == SIGINT)
    {
    }
}

signed main(int argc, char* argv[])
{
    if (argc < 6) {
        cerr << "Insufficient CLA Provided" << endl;
    }

    int socket;
    sscanf(argv[1], "%d", &socket);

    char* filename = new char[FILESIZE];
    snprintf(filename, FILESIZE, "%s", argv[2]);
    CYAN cerr << "FILE RECEIVED by Server: " << filename << endl; RESET1

    int offset;
    sscanf(argv[3], "%d", &offset);
    CYAN cerr << "OFFSET RECEIVED by Server: " << offset << endl; RESET1

    int* pipefd = new int[2];
    sscanf(argv[4], "%d", &pipefd[READ]);
    sscanf(argv[5], "%d", &pipefd[WRITE]);

    signal(SIGMODIFY, sig_handler);
    signal(SIGINT, sig_handler);

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        RED perror("Error in Opening File"); RESET1
        close(socket);
        exit(EXIT_FAILURE);
    }
    lseek(fd, 0, SEEK_SET);

    int read_, total_read;
    for(total_read = 0; total_read < offset; total_read += read_)
    {
        char* c = new char;
        if ((read_ = read(fd, c, sizeof(char))) < 0) {
            RED perror("Error in Reading File"); RESET1

            close(fd);
            close(socket);
            exit(EXIT_FAILURE);
        }
        if (read_ == 0) {
            GREEN cerr << "File Transfer was Already Completed" << endl; RESET1

            close(fd);
            close(socket);
            exit(EXIT_SUCCESS);
        }
    }

    int send_, total_send;
    interrupt = 0;
    while(!interrupt)
    {
        char* buffer = new char[DATASIZE];
        if ((read_ = read(fd, buffer, DATASIZE)) < 0) {
            RED perror("Error in Reading File"); RESET1
            break;
        }
        if (read_ == 0) {
            GREEN cerr << "File Transfer Complete" << endl; RESET1
            sendData(socket, buffer, read_);

            close(fd);
            close(socket);
            exit(EXIT_SUCCESS);
        }

        if ((send_ = sendData(socket, buffer, read_)) < 0) {
            RED perror("Error in Sending Data"); RESET1
            break;
        }
        sleep(3);
        offset += read_;
    }

    char* offset_Str = new char[LLSIZE];
    snprintf(offset_Str, LLSIZE, "%d", offset);

    close(pipefd[READ]);
    write(pipefd[WRITE], offset_Str, LLSIZE);
    close(pipefd[WRITE]);

    close(fd);
    exit(EXIT_FAILURE);
}
