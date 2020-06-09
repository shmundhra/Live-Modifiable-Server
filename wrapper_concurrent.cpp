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

/* Creates a TCP Socket and sets the socket level options to Reuse Address and Port */
/* Returns -1 on Error and socket_fd value on Success */
int CreateSocket()
{
    int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        RED << LOG; perror("Error in Creating Listening Socket"); RESET1
        return -1;
    }

    const int enable = 1;
     if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        RED << LOG; perror("Error in setting socket option to enable Reuse of Address"); RESET1
    }
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
        RED << LOG; perror("Error in setting socket option to enable Reuse of Port"); RESET1
    }

    return socket_fd;
}

/* Binds the TCP Socket (Param1) to IP_ADDR::PORT(Param2) */
/* Returns -1 on Error and 0 on Success */
int BindSocket(int socket, sockaddr_in& server_addr)
{
    server_addr = {AF_INET, htons(PORT), inet_addr(IP_ADDR), sizeof(sockaddr_in)};
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socket, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
        RED << LOG; perror("Error in binding Listening Socket"); RESET1
        return -1;
    }
    return 0;
}

/* Reaps a Terminated Child's status and removes from directory (Param1) */
/* Returns 0 if a Child Terminated, -1 if all Children Terminated */
int TerminateChannel(map <pid_t, sockaddr_in>& directory)
{
    pid_t channel_pid = wait(NULL);
    if (channel_pid < 0) return -1;
    GREEN << LOG << "CLIENT @ "
          << inet_ntoa(directory[channel_pid].sin_addr)
          << "::" << ntohs(directory[channel_pid].sin_port)
          << " DISCONNECTED"; RESET2
    directory.erase(channel_pid);
    return 0;
}

/* Blocks signo (Param1) Signal */
/* Returns -1 on Error, 0 on Success */
int BlockSignal(int signo)
{
    sigset_t sigmask; sigemptyset(&sigmask); sigaddset(&sigmask, signo);
    if (sigprocmask(SIG_BLOCK, &sigmask, NULL) < 0){
        RED << LOG; perror("Error in Blocking Signal");
        return -1;
    }
    return 0;
}

/* Unblocks signo (Param1) Signal */
/* Returns -1 on Error, 0 on Success */
int UnblockSignal(int signo)
{
    sigset_t sigmask; sigemptyset(&sigmask); sigaddset(&sigmask, signo);
    if (sigprocmask(SIG_UNBLOCK, &sigmask, NULL) < 0){
        RED << LOG; perror("Error in Unblocking Signal");
        return -1;
    }
    return 0;
}

/* Unblocks All Signals */
/* Returns -1 on Error, 0 on Success */
int UnblockAllSignal()
{
    sigset_t emptymask; sigemptyset(&emptymask);
    if (sigprocmask(SIG_SETMASK, &emptymask, NULL) < 0){
        RED << LOG; perror("Error in Clearing SIGMASK");
        return -1;
    }
    return 0;
}

/* Sends Signal to data_channel (Param1) */
/* Receives Offset (Param3) through pipe_fd[] (Param2) */
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

/* Handles the Appropriate Prompts after Termination of data_channel (Param1) */
int HandleTermination(pid_t data_channel, int offset)
{
    if (server_success){
        GREEN << data_channel << ":: DATA CHANNEL TERMINATION SUCCESSFUL"; RESET2;
    }
    if (server_failure){
        RED << data_channel << ":: DATA CHANNEL FAILED"; RESET2;
    }
    if (server_paused){
        BLUE << LOG << "RECEIVED " << offset << " from DATA CHANNEL " << data_channel; RESET2;
    }
}

/* Creates a Vector of Char Arrays with Each Array holding Information for a Backup Node */
/* Returns Compressed Backup Node Information */
vector <char*> BackupBuffer(vector<pair<int, sockaddr_in>> const& backup_nodes)
{
    vector<char*>backup_buffers;
    for(int i = 0; i < backup_nodes.size(); i++)
    {
        int fd = backup_nodes[i].first;
        sockaddr_in node_addr = backup_nodes[i].second;
        char* buffer = (char*)calloc(FDSIZE + FDSIZE + FDSIZE + BUFFSIZE + 1, sizeof(char));
        snprintf(buffer,FDSIZE + FDSIZE + FDSIZE + BUFFSIZE + 1, "%d-%hu-%hu-%u", 
                        fd, node_addr.sin_family, node_addr.sin_port, node_addr.sin_addr.s_addr);
        backup_buffers.push_back(buffer);
    }
    return backup_buffers;
}

signed main(int argc, char* argv[])
{
    if (argc < 2) {
        RED << LOG << "Please Enter Path of Server Executable"; RESET2;
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
        RED << LOG; perror("Error in Listening on Listening Socket"); RESET1
        exit(EXIT_FAILURE);
    }

    vector <pair<int, sockaddr_in>> backup_nodes;
    WHITE << LOG << "Waiting for BACKUP NODES..."; RESET2;
    while(1)
    {
        fd_set read_fds; FD_ZERO(&read_fds);
        FD_SET(listening_socket, &read_fds), FD_SET(STDIN_FILENO, &read_fds);
        timespec timeout = (timespec){.tv_sec = TIMEOUT/2, .tv_nsec = 0};

        WHITE << LOG << "Waiting for EVENT..."; RESET2;
        int ready_fd = pselect(listening_socket+1, &read_fds, NULL, NULL, &timeout, &emptymask);
        if (ready_fd < 0)
        {
            if (interrupt /*&& errno == EINTR*/) {
                interrupt = 0;
                continue;
            } else {
                RED << LOG; perror("Error in Accepting Backup Nodes"); RESET1;
            }
        }
        if (ready_fd == 0)
        {
            GREEN << LOG << "ACCEPTED ALL BACKUP NODES... "; RESET2;
            break;
        }
        if (ready_fd > 0)
        {
            if (FD_ISSET(listening_socket, &read_fds))
            {
                if ((connection_socket = accept(listening_socket,
                                                reinterpret_cast<sockaddr*>(&cli_addr),
                                                &cli_len)) < 0)
                {
                    RED << LOG; perror("Error in Accepting Backup Nodes"); RESET1;
                }
                else
                {
                    GREEN << LOG << "BACKUP NODE @ "
                          << inet_ntoa(cli_addr.sin_addr) << "::" << ntohs(cli_addr.sin_port); RESET2;
                    if (EMULATING) sleep(5);
                    sendInfo(connection_socket, (char*)"Registered Node");
                    backup_nodes.push_back({connection_socket, cli_addr});
                }
            }
            if (FD_ISSET(STDIN_FILENO, &read_fds))
            {
                char c; cin >> c;
                GREEN << LOG << "ACCEPTED " << backup_nodes.size() << " BACKUP NODES... "; RESET2;
                break;
            }
        }
    }

    bool is_failure(0), is_timeout(0);
    while(1)
    {
        WHITE << LOG << "Waiting for NEW CONNECTIONS..."; RESET2;
        while(1)
        {
            fd_set read_fds; FD_ZERO(&read_fds);
            FD_SET(listening_socket, &read_fds), FD_SET(STDIN_FILENO, &read_fds);
            timespec timeout = (timespec){.tv_sec = TIMEOUT, .tv_nsec = 0};

            WHITE << LOG << "Waiting for EVENT..."; RESET2;
            int ready_fd = pselect(listening_socket+1, &read_fds, NULL, NULL, &timeout, &emptymask);
            if (ready_fd < 0) {
                if (interrupt /*&& errno == EINTR*/)
                {
                    interrupt = 0;
                    if (connection_terminate) {
                        connection_terminate = 0;
                        TerminateChannel(connection_directory);
                    }
                }
                else
                {
                    RED << LOG; perror("Error in select()"); RESET1;
                    is_failure = 1;
                    break;
                }
            }
            if (ready_fd == 0)
            {
                GREEN << LOG << "SERVER TIMEOUT"; RESET2;
                is_timeout = 1;
                break;
            }
            if (ready_fd > 0)
            {
                if (FD_ISSET(listening_socket, &read_fds))
                {
                    if ((connection_socket = accept(listening_socket,
                                                    reinterpret_cast<sockaddr*>(&cli_addr),
                                                    &cli_len)) < 0)
                    {
                        RED << LOG; perror("Error in Accepting Incoming Connection"); RESET1
                    } else {
                        if (EMULATING) sleep(5);
                        break;
                    }
                }
                if (FD_ISSET(STDIN_FILENO, &read_fds))
                {   
                    char c; cin >> c;
                    is_timeout = 1;
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
            int recv_; PacketType packet_type;
            if ((recv_ = recvType(connection_socket, packet_type)) < 0) {
                RED << LOG; perror("Error in Receiving Packet Type"); RESET1
                exit(EXIT_FAILURE);
            }
            if (recv_ == 0) {
                GREEN << LOG << "CLIENT TERMINATED CONNECTION"; RESET2;
                close(connection_socket);
                exit(EXIT_SUCCESS);
            }

            if (packet_type != PacketType::INFO) {
                RED << LOG << "INFO Packet Expected, " << packet_type << " Packet Received." ; RESET2;
                close(connection_socket);
                exit(EXIT_FAILURE);
            }

            /* Receive FileName */
            char command[INFOSIZE+1];
            bzero(command, (INFOSIZE+1) * sizeof(char));
            if (recvInfo(connection_socket, command) < 0) {
                RED << LOG; perror("Error in Receiving Info Packet"); RESET1
                exit(EXIT_FAILURE);
            }

            if (server_modifying)
            {
                WHITE << LOG << "Server MODIFYING, Connection QUEUED.."; RESET2;
                while (server_modifying) pause();
                WHITE << LOG << "Server MODIFIED, Connection STARTS."; RESET2;
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
                    char pipefd_buffer[FDSIZE + FDSIZE + 1];
                    snprintf(pipefd_buffer, FDSIZE + FDSIZE, "%d#%d", pipe_fd[READ], pipe_fd[WRITE]);

                    vector<char*> argv = {
                                            (char*)executable.c_str(),
                                            (char*)(to_string(connection_socket).c_str()),
                                            command,
                                            (char*)(to_string(offset).c_str()),
                                            pipefd_buffer,
                                            (char*)(to_string(backup_nodes.size()).c_str())
                                        };
                    vector<char*> backup_info = BackupBuffer(backup_nodes);
                    for (auto buffer: backup_info) {
                        argv.push_back(buffer);
                    }
                    argv.push_back((char*)NULL);

                    execvp(argv[0], &argv[0]);

                    RED << LOG << "execvp() has Returned"; RESET2;
                    if (sendError(connection_socket, (char*)string("Server Failed").c_str()) < 0){
                        RED << LOG; perror("Error in Sending Error Message"); RESET2;
                    }
                    exit(EXIT_FAILURE);
                }
                else
                {
                    WHITE << LOG << "STARTING DATA CHANNEL " << data_channel ; RESET2
                    while(1)
                    {
                        if (server_terminate)                   /* SIGCHLD - Data Channel has Terminated */
                        {
                            if (server_success) {
                                GREEN << LOG << "DATA TRANSFER SUCCESSFUL"; RESET2;
                            }
                            if (server_failure) {
                                RED << LOG << "DATA TRANSFER FAILED"; RESET2;
                            }
                            break;
                        }
                        if (server_modifying)                   /* SIGINT - Modification has Started */
                        {
                            HandleModification(data_channel, pipe_fd, &offset);

                            /* Waiting for Data Channel to terminate */
                            while(!server_terminate) pause();

                            HandleTermination(data_channel, offset);
                            break;
                        }

                        WHITE << LOG << "PAUSED for SIGCHLD or SIGINT"; RESET2;
                        while(!server_terminate and !server_modifying) pause();
                    }
                    if (server_paused)
                    {
                        /* Wait for modification to get over */
                        while (server_modifying) pause();
                        WHITE << LOG << "CODE MODIFICATION ENDED"; RESET2;
                    }
                }
                close(pipe_fd[READ]), close(pipe_fd[WRITE]);
            }
            while(!server_success and !server_failure);

            /* The connection channel exits after success or failure of transfer */
            close(connection_socket);
            if(server_failure) exit(EXIT_FAILURE);
            if(server_success) exit(EXIT_SUCCESS);
        }
        else
        {
            GREEN << conn_channel << ":: CLIENT @ "
                  << inet_ntoa(cli_addr.sin_addr) << "::" << ntohs(cli_addr.sin_port)
                  << " CONNECTED"; RESET2;
            connection_directory[conn_channel] = cli_addr;
            close(connection_socket);
        }
    }

    /* Waiting for all Connection Channels to Terminate */
    while(TerminateChannel(connection_directory) == 0);

    if (is_failure) {   RED << LOG << "SERVER TERMINATED"; RESET2; exit(EXIT_FAILURE);}
    if (is_timeout) { GREEN << LOG << "SERVER TERMINATED"; RESET2; exit(EXIT_SUCCESS);}
}