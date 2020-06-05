#include "livemodifiable.h"

static volatile sig_atomic_t server_modifying(0);

static volatile sig_atomic_t server_terminate, server_success, server_failure, server_paused;
static void child_sig_handler(int signo)
{
    if (signo == SIGCHLD)
    {
        server_terminate = 1;

        int status;
        if (wait(&status) < 0) {
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

static volatile sig_atomic_t interrupt(0);
static volatile sig_atomic_t connection_terminate(0);
static void par_sig_handler(int signo)
{
    interrupt = 1;
    if (signo == SIGCHLD)
    {
        connection_terminate = 1;
    }
    if (signo == SIGINT)
    {
        server_modifying = !server_modifying;
    }
}

int CreateSocket()
{
    int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        RED << getpid() << ":: "; perror("Error in Creating Listening Socket"); RESET1
        return -1;
    }

    const int enable = 1;
     if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        RED << getpid() << ":: "; perror("Error in setting socket option to enable Reuse of Address"); RESET1
    }
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
        RED << getpid() << ":: "; perror("Error in setting socket option to enable Reuse of Port"); RESET1
    }

    return socket_fd;
}

int BindSocket(int socket, sockaddr_in& server_addr)
{
    server_addr = {AF_INET, htons(PORT), inet_addr("127.0.0.1"), sizeof(sockaddr_in)};
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socket, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
        RED << getpid() << ":: "; perror("Error in binding Listening Socket"); RESET1
        return -1;
    }
    return 0;
}

int TerminateChannel(map <pid_t, sockaddr_in>& directory)
{
    pid_t channel_pid = wait(NULL);
    if (channel_pid < 0) return -1;
    GREEN << getpid() << ":: CLIENT @ "
          << inet_ntoa(directory[channel_pid].sin_addr)
          << "::" << ntohs(directory[channel_pid].sin_port)
          << " DISCONNECTED"; RESET2
    directory.erase(channel_pid);
    return 0;
}

int BlockSignal(int signo)
{
    sigset_t sigmask; sigemptyset(&sigmask); sigaddset(&sigmask, signo);
    if (sigprocmask(SIG_BLOCK, &sigmask, NULL) < 0){
        RED << getpid() << ":: "; perror("Error in Blocking Signal");
        return -1;
    }
    return 0;
}

int UnblockSignal(int signo)
{
    sigset_t sigmask; sigemptyset(&sigmask); sigaddset(&sigmask, signo);
    if (sigprocmask(SIG_UNBLOCK, &sigmask, NULL) < 0){
        RED << getpid() << ":: "; perror("Error in Unblocking Signal");
        return -1;
    }
    return 0;
}

int UnblockAllSignal()
{
    sigset_t emptymask; sigemptyset(&emptymask);
    if (sigprocmask(SIG_SETMASK, &emptymask, NULL) < 0){
        RED << getpid() << ":: "; perror("Error in Clearing SIGMASK");
        return -1;
    }
    return 0;
}

int HandleModification(pid_t data_channel, int pipe_fd[2], int* offset)
{
    WHITE << data_channel << ":: CODE MODIFICATION STARTED" << endl ;

    /* Signal Data Channel about modification */
    kill(data_channel, SIGMODIFY);

    /* Recv 'offset' from data channel as marker for next execvp()*/
    char offset_str[BUFFSIZE+1];
    close(pipe_fd[WRITE]);
    read(pipe_fd[READ], offset_str, BUFFSIZE+1);
    close(pipe_fd[READ]);
    sscanf(offset_str, "%d", offset);
}

int HandleTermination(pid_t data_channel, int offset, sig_atomic_t success,
                      sig_atomic_t failure, sig_atomic_t paused)
{
    if (success){
        GREEN << data_channel << ":: DATA CHANNEL TERMINATION SUCCESSFUL"; RESET2;
    }
    if (failure){
        RED << data_channel << ":: DATA CHANNEL FAILED"; RESET2;
    }
    if (paused){
        BLUE << getpid() << ":: RECEIVED " << offset << " from DATA CHANNEL " << data_channel; RESET2;
    }
}

signed main(int argc, char* argv[])
{
    if (argc < 2) {
        RED << "Please Enter Path of Server Executable"; RESET2;
        exit(EXIT_FAILURE);
    }
    string executable(argv[1]);

    /* Block SIGINT and SIGCHLD from occuring outside pselect() */
    if ((BlockSignal(SIGINT) < 0) or (BlockSignal(SIGCHLD) < 0)) {
        exit(EXIT_FAILURE);
    }
    sigset_t emptymask; sigemptyset(&emptymask);

    /* Install SIGINT  Handler for Wrapper */
    /* Install SIGCHLD Handler for Wrapper */
    signal(SIGINT, par_sig_handler);
    signal(SIGCHLD, par_sig_handler);

    int listening_socket, connection_socket;
    sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(sockaddr_in);
    map <pid_t, sockaddr_in> connection_directory;

    if ((listening_socket = CreateSocket()) < 0) {
        exit(EXIT_FAILURE);
    }

    if (BindSocket(listening_socket, serv_addr) < 0) {
        exit(EXIT_FAILURE);
    }

    if (listen(listening_socket, 10) < 0) {
        RED << getpid() << ":: "; perror("Error in Listening on Listening Socket"); RESET1
        exit(EXIT_FAILURE);
    }

    bool is_failure(0), is_timeout(0);
    while(1)
    {
        WHITE << getpid() << ":: Waiting for NEW CONNECTIONS..."; RESET2;
        while(1)
        {
            fd_set read_fds; FD_ZERO(&read_fds); FD_SET(listening_socket, &read_fds);
            timespec timeout = (timespec){.tv_sec = TIMEOUT, .tv_nsec = 0};

            WHITE << getpid() << ":: Waiting for EVENT..."; RESET2;
            int ready_fd = pselect(listening_socket+1, &read_fds, NULL, NULL, &timeout, &emptymask);
            if (ready_fd < 0) {
                if (errno == EINTR and interrupt)
                {
                    interrupt = 0;
                    if (connection_terminate) {
                        connection_terminate = 0;
                        TerminateChannel(connection_directory);
                    }
                }
                else
                {
                    RED << getpid() << ":: "; perror("Error in select()"); RESET1;
                    is_failure = 1;
                    break;
                }
            }
            else if (ready_fd == 0)
            {
                GREEN << getpid() << ":: SERVER TIMEOUT"; RESET2;
                is_timeout = 1;
                break;
            }
            else if (FD_ISSET(listening_socket, &read_fds))
            {
                if ((connection_socket = accept(listening_socket,
                                               reinterpret_cast<sockaddr*>(&cli_addr),
                                               &cli_len)) < 0)
                {
                    RED << getpid() << ":: "; perror("Error in Accepting Incoming Connection"); RESET1
                } else {
                    break;
                }
            }
        }
        if (is_failure or is_timeout) {
            close(listening_socket);
            break;
        }

        /***************************FORKING CONNECTION CHANNEL******************************/
        pid_t conn_channel = fork();
        if (conn_channel == 0)
        {
            /* Empty SIGNAL MASK of Connection Channel */
            if (UnblockAllSignal() < 0) {
                exit(EXIT_FAILURE);
            }

            /* Install SIGINT  Handler for Connection Channel */
            /* Install SIGCHLD Handler for Connection Channel */
            signal(SIGINT, child_sig_handler);
            signal(SIGCHLD, child_sig_handler);

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

            if (server_modifying)
            {
                WHITE << getpid() << ":: Server MODIFYING, Connection QUEUED.."; RESET2;
                while (server_modifying) pause();
                WHITE << getpid() << ":: Server MODIFIED, Connection STARTS."; RESET2;
            }

            /* Start Data Transfer by spawning Data Channel */
            int offset = 0;
            do
            {
                server_terminate = server_success = server_failure = server_paused = 0;

                int pipe_fd[2]; pipe(pipe_fd);
                /*****************************FORKING DATA CHANNEL******************************/
                pid_t data_channel = fork();
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
                    while(1)
                    {
                        if (server_terminate)                   /* SIGCHLD - Data Channel has Terminated */
                        {
                            if (server_success) {
                                GREEN << data_channel << ":: DATA TRANSFER SUCCESSFUL"; RESET2;
                            }
                            if (server_failure) {
                                RED << data_channel << ":: DATA TRANSFER FAILED"; RESET2;
                            }
                            break;
                        }
                        if (server_modifying)                   /* SIGINT - Modification has Started */
                        {
                            HandleModification(data_channel, pipe_fd, &offset);

                            /* Waiting for Data Channel to terminate */
                            while(!server_terminate) pause();

                            HandleTermination(data_channel, offset, server_success, server_failure, server_paused);
                            break;
                        }

                        WHITE << getpid() << ":: PAUSED for SIGCHLD or SIGINT"; RESET2;
                        while(!server_terminate and !server_modifying) pause();
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
            while(!server_success and !server_failure);

            /* The connection channel exits after success or failure of transfer */
            if(server_failure) exit(EXIT_FAILURE);
            if(server_success) exit(EXIT_SUCCESS);
        }
        else
        {
            GREEN << conn_channel << ":: CLIENT @ "
                  << inet_ntoa(cli_addr.sin_addr) << "::" << ntohs(cli_addr.sin_port)
                  << " CONNECTED"; RESET2;
            connection_directory[conn_channel] = cli_addr;
        }
    }

    /* Waiting for all Connection Channels to Terminate */
    while(TerminateChannel(connection_directory) == 0);

    if (is_failure) {   RED << getpid() << ":: SERVER TERMINATED"; RESET2; exit(EXIT_FAILURE);}
    if (is_timeout) { GREEN << getpid() << ":: SERVER TERMINATED"; RESET2; exit(EXIT_SUCCESS);}
}