#include "livemodifiable.h"

static volatile sig_atomic_t interrupt(0);
static void sig_handler(int signo)
{
    if (signo == SIGMODIFY)
    {
        interrupt = 1;
    }
}

bool success(0), failure(0);

void GET(int socket, char* file, int& offset, int pipe_fd[2])
{
    /* Install Data Producer - Open File */
    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        RED << getpid() << ":: "; perror("Error in Opening File"); RESET1
        if (sendError(socket, (char*)string("Error in Opening File").c_str()) < 0) {
            RED << getpid() << ":: "; perror("Error in Sending Error Message"); RESET1
        }
        failure = 1;
        return;
    }

    char buffer[DATASIZE+1];

    int file_size = lseek(fd, 0, SEEK_END);
    if (!(offset < file_size))
    {
        if (sendData(socket, 0, offset, buffer) < 0) {
            RED << getpid() << ":: "; perror("Error in Sending Data"); RESET1
            failure = 1;
        } else {
            GREEN << getpid() << ":: FILE TRANSFER already SUCCESSFUL"; RESET2;
            success = 1;
        }
    }
    else
    {
        lseek(fd, offset, SEEK_SET);                    // Shift to position 'offset' in the file

        int read_, recv_;
        while(!failure and !success and !interrupt)
        {
            /* Production of Data - Read from File */
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

            /* Send Produce to Client */
            if (sendData(socket, read_, offset, buffer) < 0)
            {
                RED << getpid() << ":: "; perror("Error in Sending Data"); RESET1
                failure = 1;
                break;
            }
            else if (read_ == 0)
            {
                GREEN << getpid() << ":: File Transfer Complete"; RESET2;
                success = 1;
                break;
            }
            offset += read_;

            /* Receive Ack from Client */
            PacketType packet_type;
            if ((recv_ = recvType(socket, packet_type)) < 0) {
                RED << getpid() << ":: "; perror("Error in Receiving Packet Type"); RESET1
                exit(EXIT_FAILURE);
            }
            switch (packet_type)
            {
                case PacketType::ERROR: {
                                            char* error_msg = (char*)calloc(ERRSIZE+1, sizeof(char));
                                            if (recvError(socket, error_msg) < 0)
                                            {
                                                RED << getpid() << ":: "; perror("Error in Receiving Error Packet"); RESET1
                                            }
                                            free(error_msg);
                                            failure = 1;
                                            break;
                                        }

                case PacketType::ACK:   {
                                            int next_offset;
                                            if (recvAck(socket, &next_offset) < 0)
                                            {
                                                RED << getpid() << ":: "; perror("Error in Receiving Ack Packet"); RESET1
                                                failure = 1;
                                            }
                                            else
                                            {
                                                // lseek(fd, next_offset, SEEK_SET)
                                                lseek(fd, next_offset - offset, SEEK_CUR);
                                                offset = next_offset;
                                            }
                                            break;
                                        }

                default: RED << getpid() << ":: Unexpected PacketType"; RESET2;
            }
            sleep(1);
        }
    }

    close(fd);
    return;
}

void PUT(int socket, char* file, int& offset, int pipe_fd[2])
{
    /* Install Data Consumer - ?? */

    int recv_;
    while(!failure and !success and !interrupt)
    {
        /* Send ACK Request of Data at offset to Client */
        if (sendAck(socket, offset) < 0){
            RED << getpid() << ":: "; perror("Error in Sending Ack Packet");
            exit(EXIT_FAILURE);
        }

        /* Receive Data from Client */
        PacketType packet_type;
        if ((recv_ = recvType(socket, packet_type)) < 0) {
            RED << getpid() << ":: "; perror("Error in Receiving Packet Type"); RESET1
            exit(EXIT_FAILURE);
        }
        switch (packet_type)
        {
            case PacketType::ERROR: {
                                        char* error_msg = (char*)calloc(ERRSIZE+1, sizeof(char));
                                        if (recvError(socket, error_msg) < 0)
                                        {
                                            RED << getpid() << ":: "; perror("Error in Receiving Error Packet"); RESET1
                                        }
                                        free(error_msg);
                                        failure = 1;
                                        break;
                                    }

            case PacketType::DATA:  {
                                        int recv_offset;
                                        char* data = (char*)calloc(DATASIZE+1, sizeof(char));
                                        if ((recv_ = recvData(socket, &recv_offset, data)) < 0)
                                        {
                                            RED << getpid() << ":: "; perror("Error in Receiving Data Packet"); RESET1
                                            failure = 1;
                                            break;
                                        }
                                        if (recv_ == 0) {
                                            GREEN << "FILE TRANSFER is COMPLETE"; RESET2;
                                            success = 1;
                                            break;
                                        }

                                        /* Consume the Produce Received */
                                        offset = recv_offset + recv_;
                                        free(data);
                                        break;
                                    }

            default: RED << getpid() << ":: Unexpected PacketType"; RESET2;
        }
        sleep(1);
    }
}

signed main(int argc, char* argv[])
{
    if (argc < 6) {
        RED << "Insufficient CLA Provided"; RESET2;
    }

    /* Install Signal Handlers */
    signal(SIGMODIFY, sig_handler);
    signal(SIGINT, SIG_IGN);

    /* Extract Socket File Descriptor from 1st CLA */
    int socket;
    sscanf(argv[1], "%d", &socket);

    /* Extract Action and Filename from Command in 2nd CLA */
    char action[] = {0, 0, 0, 0};
    char filename[FILESIZE+1];
    bzero(filename, (FILESIZE+1)*sizeof(char));
    sscanf(argv[2], "%s %s", action, filename);
    BLUE << getpid() << ":: ACTION RECEIVED by Server: " << action; RESET2;
    BLUE << getpid() << ":: FILENAME RECEIVED by Server: " << filename; RESET2;

    /* Extract Offset from where to start File, from 3rd CLA */
    int offset;
    sscanf(argv[3], "%d", &offset);
    BLUE << getpid() << ":: OFFSET RECEIVED by Server: " << offset; RESET2;

    /* Extract Pipe File Descriptors to send offset to Parent on Pausing */
    int pipefd[2];
    sscanf(argv[4], "%d", &pipefd[READ]);
    sscanf(argv[5], "%d", &pipefd[WRITE]);

    if (!strcmp(action, "GET")) GET(socket, filename, offset, pipefd);
    if (!strcmp(action, "PUT")) PUT(socket, filename, offset, pipefd);

    /* Send 'offset' to wrapper as marker to start transfer in next execvp */
    if(interrupt == 1 and success != 1 and failure != 1)
    {
        if (sendInfo(socket, (char*)string("Modification Taking Place...").c_str()) < 0) {
            RED << getpid() << ":: "; perror("Error in Sending Modification Start Message"); RESET1
            failure = 1;
        }

        char offset_Str[BUFFSIZE+1];
        snprintf(offset_Str, BUFFSIZE+1, "%d", offset);

        close(pipefd[READ]);
        write(pipefd[WRITE], offset_Str, BUFFSIZE);
        close(pipefd[WRITE]);
    }

    close(socket), close(pipefd[READ]), close(pipefd[WRITE]);

    if(failure == 1) exit(EXIT_FAILURE);
    if(success == 1) exit(EXIT_SUCCESS);
    if(interrupt == 1) exit(EXIT_PAUSED);
}
