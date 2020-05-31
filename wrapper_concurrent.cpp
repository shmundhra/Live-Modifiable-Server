#include "livemodifiable.h"

pid_t conn_channel, data_channel;

int open_for_signal;
int interrupt;
int server_terminate, server_success, server_failure, server_paused;
int server_modifying;

void child_sig_handler(int signo)
{
    if(open_for_signal == 0) return;

    interrupt = 1;
    if (signo == SIGCHLD)
    {
        server_terminate = 1;

        int status;
        if (waitpid(data_channel, &status, 0) < 0) {
            server_failure = 1;
            return;
        }
        if (WIFEXITED(status) and WEXITSTATUS(status) == EXIT_SUCCESS) {
            server_success = 1;
        }
        else if (WIFEXITED(status) and WEXITSTATUS(status) == EXIT_PAUSED) {
            server_paused = 1;
        }
        else {
            server_failure = 1;
        }
    }
    if (signo == SIGINT)
    {
        server_modifying = !server_modifying;
    }
}

void par_sig_handler(int signo)
{
    if(open_for_signal == 0) return;

    if (signo == SIGCHLD)
    {
        pid_t connection = wait(NULL);
    }
    if (signo == SIGINT)
    {
        server_modifying = !server_modifying;
    }
}

signed main(int argc, char* argv[])
{
    open_for_signal = 0;

    if (argc < 2) {
        RED << "Please Enter Path of Server Executable"; RESET2;
        exit(EXIT_FAILURE);
    }
    string executable(argv[1]);

    signal(SIGINT, par_sig_handler);
    signal(SIGCHLD, par_sig_handler);

    int connection_socket;
    sockaddr_in cli_addr;
    socklen_t cli_len;

    int listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_socket < 0) {
        RED << getpid() << ":: "; perror("Error in Creating Listening Socket"); RESET1
        exit(EXIT_FAILURE);
    }
    /*
    const int enable = 1;
    if (setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        RED << getpid() << ":: "; perror("Error in setting socket option to enable Reuse of Address"); RESET1
    }
    if (setsockopt(listening_socket, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
        RED << getpid() << ":: "; perror("Error in setting socket option to enable Reuse of Port"); RESET1
    }*/
    
    sockaddr_in serv_addr = {AF_INET, htons(PORT), inet_addr("127.0.0.1"), sizeof(sockaddr_in)};
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listening_socket, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0) {
        RED << getpid() << ":: "; perror("Error in binding Listening Socket"); RESET1
        exit(EXIT_FAILURE);
    }

    if (listen(listening_socket, 10) < 0) {
        RED << getpid() << ":: "; perror("Error in Listening on Listening Socket"); RESET1
        exit(EXIT_FAILURE);
    }

    open_for_signal = 1;
    server_modifying = 0;
    while(1)
    {
        WHITE << getpid() << ":: Waiting for NEW CONNECTIONS..."; RESET2;
        do {
            if((connection_socket = accept(listening_socket,
                                          reinterpret_cast<sockaddr*>(&cli_addr),
                                          &cli_len)) < 0)
            {
                RED << getpid() << ":: "; perror("Error in Accepting Incoming Connection"); RESET1
            }
        }while(connection_socket < 0);

        if (server_modifying){
            WHITE << getpid() << ":: Server MODIFYING, Connection QUEUED.."; RESET2;
            while (server_modifying) pause();
            WHITE << getpid() << ":: Server MODIFIED, Connection STARTS."; RESET2;
        }

        /************************FORKING CONNECTION CHANNEL******************************/
        conn_channel = fork();
        if (conn_channel == 0)
        {
            open_for_signal = 0;

            GREEN << getpid() << ":: CLIENT @ "
                  << inet_ntoa(cli_addr.sin_addr) << "::" << ntohs(cli_addr.sin_port)
                  << " CONNECTED"; RESET2;

            signal(SIGMODIFY, child_sig_handler);
            signal(SIGCHLD, child_sig_handler);
            signal(SIGINT, child_sig_handler);

            /* Receive Info Packet Header */
            PacketType packet_type;
            if (recvType(connection_socket, packet_type) < 0) {
                RED << getpid() << ":: "; perror("Error in Receiving Packet Type"); RESET1
                exit(EXIT_FAILURE);
            }
            if (packet_type != PacketType::INFO) {
                RED << "INFO Packet Expected, " << packet_type << " Packet Received." ; RESET2;
                exit(EXIT_FAILURE);
            }

            /* Receive FileName */
            char filename[INFOSIZE+1];
            bzero(filename, (INFOSIZE+1) * sizeof(char));
            if (recvInfo(connection_socket, filename) < 0) {
                RED << getpid() << ":: "; perror("Error in Receiving Info Packet"); RESET1
                exit(EXIT_FAILURE);
            }

            long long offset = 0;
            server_success = 0;
            while(!server_success and !server_failure)
            {
                int pipe_fd[2]; pipe(pipe_fd);

                open_for_signal = 1;
                /************************FORKING DATA CHANNEL******************************/
                data_channel = fork();
                if (data_channel == 0)
                {
                    char* argv[] = {
                                        (char*)executable.c_str(),
                                        (char*)(to_string(connection_socket).c_str()),
                                        filename,
                                        (char*)(to_string(offset).c_str()),
                                        (char*)(to_string(pipe_fd[READ]).c_str()),
                                        (char*)(to_string(pipe_fd[WRITE]).c_str()),
                                        (char*)NULL
                                    };
                    execvp(argv[0], argv);
                    RED << "execvp() has Returned"; RESET2;
                    if (sendError(connection_socket, (char*)string("Server Failed").c_str()) < 0){
                        RED << getpid() << ":: "; perror("Error in Sending Error Message"); RESET2;
                    }
                    exit(EXIT_FAILURE);
                }
                else
                {   
                    WHITE << getpid() << ":: STARTING DATA CHANNEL " << data_channel ; RESET2

                    interrupt = server_terminate = server_success = server_failure = server_paused = server_modifying = 0;
                    while(!server_terminate)
                    {
                        WHITE << getpid() << ":: PAUSED for SIGCHLD or SIGINT"; RESET2;
                        while(!interrupt) pause();

                        if (server_terminate)
                        {
                            if (server_success){
                                GREEN << data_channel << ":: DATA TRANSFER SUCCESSFUL"; RESET2;
                            }
                            if (server_failure){
                                RED << data_channel << ":: DATA TRANSFER FAILED"; RESET2;
                            }
                        }
                        if (server_modifying)
                        {
                            WHITE << data_channel << ":: CODE MODIFICATION STARTED" << endl ;

                            /* Signal Data Channel about modification */
                            kill(data_channel, SIGMODIFY);

                            /* Recv 'offset' from data channel as marker for next execvp()*/
                            char offset_str[LLSIZE];
                            close(pipe_fd[WRITE]);
                            read(pipe_fd[READ], offset_str, LLSIZE);
                            close(pipe_fd[READ]);

                            /* Waiting for Data Channel to terminate */
                            while(!server_terminate) pause();

                            if (server_success){
                                GREEN << data_channel << ":: DATA CHANNEL TERMINATION SUCCESSFUL"; RESET2;
                            }
                            if (server_failure){
                                RED << data_channel << ":: DATA CHANNEL FAILED"; RESET2;
                            }
                            if (server_paused){
                                sscanf(offset_str, "%lld", &offset);
                                BLUE << getpid() << ":: RECEIVED " << offset << " from DATA CHANNEL " << data_channel; RESET2;
                            }
                        }
                    }
                    if (server_paused)
                    {   
                        /* Wait for modification to get over */
                        while (server_modifying) pause();
                        WHITE << getpid() << ":: CODE MODIFICATION ENDED"; RESET2;
                    }
                }
                close(pipe_fd[READ]), close(pipe_fd[WRITE]);
            }
            /* The connection channel exits after success or failure of transfer */
            if(server_failure) exit(EXIT_FAILURE);
            if(server_success) exit(EXIT_SUCCESS);
        }
        else
        {
        }
    }
}