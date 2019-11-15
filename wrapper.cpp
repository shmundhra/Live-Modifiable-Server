#include "livemodifiable.h"

pid_t conn_channel, data_channel;

int modify, success, finished;
void child_sig_handler(int signo)
{
    if (signo == SIGMODIFY)
    {
        modify++;
    }
    if (signo == SIGCHLD)
    {
        finished = 1;

        int* status = new int;
        if (waitpid(data_channel, status, 0) < 0) {
            return;
        }
        if (WIFEXITED(*status) and WEXITSTATUS(*status) == EXIT_SUCCESS) {
            success = 1;
        }
        delete status;
    }
    if (signo == SIGINT)
    {
    }
}

int interrupt, complete;
void par_sig_handler(int signo)
{
    if (signo == SIGCHLD)
    {
        complete = 1;
    }
    if (signo == SIGINT)
    {
        interrupt = 1;
    }
}

signed main(int argc, char* argv[])
{
    if (argc < 2) {
        RED cerr << "Please Enter Path of Server Executable" << endl; RESET1
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
        RED perror("Error in Creating Listening Socket"); RESET1
        exit(EXIT_FAILURE);
    }

    const int* enable = new int(1);
    if (setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, enable, sizeof(enable)) < 0) {
        RED perror("Error in setting socket option to enable Reuse of Address"); RESET1
    }
    sockaddr_in serv_addr = {AF_INET, htons(PORT), inet_addr("127.0.0.1"), sizeof(sockaddr_in)};
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listening_socket, reinterpret_cast<struct sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0) {
        RED perror("Error in binding Listening Socket"); RESET1
        exit(EXIT_FAILURE);
    }

    if (listen(listening_socket, 10) < 0) {
        RED perror("Error in Listening on Listening Socket"); RESET1
        exit(EXIT_FAILURE);
    }

    
        if ((connection_socket = accept(listening_socket,
                                        reinterpret_cast<sockaddr*>(&cli_addr),
                                        &cli_len)) < 0)
        {
            RED perror("Error in Accepting Incoming Connection"); RESET1
        }
        cerr << "Connection Established with a Client" << endl;
        complete = 0;

        /************************FORKING CONNECTION CHANNEL******************************/
        if ((conn_channel = fork()) < 0) {
            RED perror("Error in Forking Connection Channel"); RESET1
            exit(EXIT_FAILURE);
        }

        if (conn_channel == 0)
        {
            signal(SIGMODIFY, child_sig_handler);
            signal(SIGCHLD, child_sig_handler);
            signal(SIGINT, child_sig_handler);

            PacketType packet_type;
            if (recvType(connection_socket, packet_type) < 0) {
                RED perror("Error in Receiving Packet Type"); RESET1
                exit(EXIT_FAILURE);
            }
            if (packet_type != PacketType::INFO)
            {
                RED cerr << "INFO Packet Expected, " << packet_type << " Packet Received." << endl; RESET1
                exit(EXIT_FAILURE);
            }

            char* filename = new char[INFOSIZE];
            if (recvInfo(connection_socket, filename) < 0) {
                RED perror("Error in Receiving Info Packet"); RESET1
                exit(EXIT_FAILURE);
            }
            GREEN cerr << "File Name Received is :" << filename << endl; RESET1

            int* offset = new int(0);
            success = finished = 0;
            while(!success)
            {
                GREEN cerr << "Starting Connection Channel" << endl; RESET1

                int* pipe_fd = new int[2];
                if (pipe(pipe_fd) < 0) {
                    RED perror("Error in Creating Pipe between Control and Data Channels"); RESET1
                    exit(EXIT_FAILURE);
                }

                /************************FORKING DATA CHANNEL******************************/
                if ((data_channel = fork()) < 0) {
                    RED perror("Error in Forking Data Connection Channel"); RESET1
                    exit(EXIT_FAILURE);
                }
                if (data_channel == 0)
                {
                    char* argv[] = {
                                        (char*)executable.c_str(),
                                        (char*)(to_string(connection_socket).c_str()),
                                        filename,
                                        (char*)(to_string(*offset).c_str()),
                                        (char*)(to_string(pipe_fd[READ]).c_str()),
                                        (char*)(to_string(pipe_fd[WRITE]).c_str()),
                                        (char*)NULL
                                    };
                    execvp(argv[0], argv);
                    RED cerr << "execvp() has Returned" << endl; RESET1;
                }
                else
                {
                    modify = 0;
                    while(!(finished or modify == 2))
                    {
                        WHITE cerr << "Paused for SIGCHLD or SIGINT" << endl; RESET1
                        pause();
                        if (success)                                    //  Caught SIGCHLD with EXIT_SUCCESS
                        {
                            GREEN cerr << "PROGRAM TERMINATION SUCCESSFUL" << endl; RESET1
                        }
                        else if (finished)                              //  Caught SIGCHLD with EXIT_FAILURE
                        {
                            GREEN cerr << "PROGRAM FAILED" << endl; RESET1
                        }
                        else                                            //  Caught SIGINT
                        {
                            WHITE cerr << "Paused for SIGCHLD or SIGMODIFY" << endl; RESET1
                            pause();
                            if (success)                                //  Caught SIGCHLD with EXIT_SUCCESS (Rare)
                            {
                                GREEN cerr << "PROGRAM TERMINATION SUCCESSFUL" << endl; RESET1
                            }
                            else if (finished)                          //  Caught SIGCHLD with EXIT_FAILURE (Rare)
                            {
                                GREEN cerr << "PROGRAM FAILED" << endl; RESET1
                            }
                            else if (modify == 1)                       //  Caught SIGMODIFY First Time (Common)
                            {
                                WHITE cerr << "Code Modification has started" << endl ;

                                close(pipe_fd[WRITE]);
                                kill(data_channel, SIGMODIFY);

                                char *offset_Str = new char[LLSIZE];
                                read(pipe_fd[READ], offset_Str, LLSIZE);
                                close(pipe_fd[READ]);

                                // Wait for the child to finish main.
                                while(!finished) {
                                    pause();
                                }
                                finished = 0;

                                if (success)        //  Caught SIGCHLD with EXIT_SUCCESS (Rare)
                                {
                                    GREEN cerr << "PROGRAM TERMINATION SUCCESSFUL" << endl; RESET1
                                }
                                else                //  Caught SIGCHLD with EXIT_FAILURE (Common)
                                {
                                    sscanf(offset_Str, "%d", offset);
                                    WHITE cerr << "Received " << *offset << " from Data Channel" << endl; RESET1
                                }
                                delete[] offset_Str;
                            }
                            else if (modify == 2)   //  Caught SIGMODIFY Second Time (Common)
                            {
                                WHITE cerr << "Code Modification has ended" << endl ; RESET1
                            }
                        }
                    }
                }
                delete[] pipe_fd;
            }
            // COMES OUT only after Termination
        }
        else
        {
            while(!complete)
            {
                cerr << "Waiting Outside for SIGCHLD or SIGINT" << endl;
                pause();
                if (interrupt)
                {
                    WHITE cerr << "Modification Started" << endl; RESET1

                    sleep(1);
                    kill(conn_channel, SIGMODIFY);
                    if (sendInfo(connection_socket, "Modification Taking Place...") < 0) {
                        RED perror("Error in Sending Modification Start Message"); RESET1
                        break;
                    }
                    interrupt = 0;

                    WHITE cerr << "Waiting Inside for SIGCHLD or SIGINT" << endl; RESET1
                    pause();
                    if (interrupt)
                    {
                        WHITE cerr << "Modification Ended" << endl; RESET1
                        if (sendInfo(connection_socket, "Modification Over !!") < 0) {
                            RED perror("Error in Sending Modification End Message"); RESET1
                            break;
                        }
                        sleep(1);
                        kill(conn_channel, SIGMODIFY);
                        interrupt = 0;
                    }
                }
            }
            complete = 0;
            GREEN cerr << "TERMINATION of CHILD" << endl; RESET1
        }
    
}