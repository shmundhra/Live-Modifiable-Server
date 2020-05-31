#include "livemodifiable.h"

int success, failure, interrupt;
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
        RED << "Insufficient CLA Provided"; RESET2;
    }

    /* Extract Socket File Descriptor from 1st CLA */
    int socket;
    sscanf(argv[1], "%d", &socket);

    /* Extract FileName from 2nd CLA */
    char filename[FILESIZE+1];
    bzero(filename, (FILESIZE+1)*sizeof(char));
    memcpy(filename, argv[2], FILESIZE);
    BLUE << getpid() << ":: FILENAME RECEIVED by Server: " << filename; RESET2;

    /* Extract Offset from where to start File, from 3rd CLA */
    long long offset;
    sscanf(argv[3], "%lld", &offset);
    BLUE << getpid() << ":: OFFSET RECEIVED by Server: " << offset; RESET2;

    /* Extract Pipe File Descriptors to send offset to Parent on Pausing */
    int pipefd[2];
    sscanf(argv[4], "%d", &pipefd[READ]);
    sscanf(argv[5], "%d", &pipefd[WRITE]);

    /* Install Signal Handlers */
    signal(SIGMODIFY, sig_handler);
    signal(SIGINT, sig_handler);

    /* Open File */
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        RED << getpid() << ":: "; perror("Error in Opening File"); RESET1
        if (sendError(socket, (char*)string("Error in Opening File").c_str()) < 0) {
            RED << getpid() << ":: "; perror("Error in Sending Error Message"); RESET1
        }
        close(pipefd[READ]), close(pipefd[WRITE]);
        close(socket);
        exit(EXIT_FAILURE);
    }

    char buffer[DATASIZE+1];
    success = failure = interrupt = 0;

    int file_size = lseek(fd, 0, SEEK_END);
    if(!(offset < file_size))
    {
        if (sendData(socket, 0, offset, buffer) < 0){
            RED << getpid() << ":: "; perror("Error in Sending Data"); RESET1
            failure = 1;
        } else {
            GREEN << getpid() << ":: FILE TRANSFER already SUCCESSFUL"; RESET2;
            success = 1;
        }
    }
    else
    {
        lseek(fd, offset, SEEK_SET);    // Shift to position 'offset' in the file

        int read_, total_read;
        while(!interrupt)
        {
            bzero(buffer, (DATASIZE+1) * sizeof(char));
            if ((read_ = read(fd, buffer, DATASIZE)) < 0)
            {
                RED << getpid() << ":: "; perror("Error in Reading File"); RESET1
                if (sendError(socket, (char*)string("Error in Reading File").c_str()) < 0) {
                    RED << getpid() << ":: "; perror("Error in Sending Error Message"); RESET1
                }
                failure = 1;
                break;
            }
            if (read_ == 0)
            {   
                if (sendData(socket, 0, offset, buffer) < 0){
                    RED << getpid() << ":: "; perror("Error in Sending Data"); RESET1
                    failure = 1;
                } else {
                    GREEN << getpid() << ":: File Transfer Complete"; RESET2;
                    success = 1;
                }
                break;
            }
            if (sendData(socket, read_, offset, buffer) < 0)
            {
                RED << getpid() << ":: "; perror("Error in Sending Data"); RESET1
                failure = 1;
                break;
            }
            sleep(1);
            offset += read_;
        }
    }

    /* Send 'offset' to wrapper as marker to start transfer in next execvp */
    if(interrupt == 1)
    {
        if (sendInfo(socket, (char*)string("Modification Taking Place...").c_str()) < 0) {
            RED << getpid() << ":: "; perror("Error in Sending Modification Start Message"); RESET1
            failure = 1;
        }

        char offset_Str[LLSIZE];
        snprintf(offset_Str, LLSIZE, "%lld", offset);

        close(pipefd[READ]);
        write(pipefd[WRITE], offset_Str, LLSIZE);
        close(pipefd[WRITE]);
    }

    close(pipefd[READ]), close(pipefd[WRITE]);
    close(fd), close(socket);

    if(failure == 1) exit(EXIT_FAILURE);
    if(interrupt == 1) exit(EXIT_PAUSED);
    if(success == 1) exit(EXIT_SUCCESS);
}
