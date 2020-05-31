#ifndef _livemodifiable_h_
#define _livemodifiable_h_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <bits/stdc++.h>
using namespace std;

#define PORT 10015
#define SIGMODIFY SIGUSR1
#define EXIT_PAUSED 10
#define READ 0
#define WRITE 1
#define LLSIZE sizeof(long long)
#define ERRSIZE 52
#define INFOSIZE 52
#define FILESIZE 52
#define DATASIZE 200

#define BLACK   cerr << "\033[1;30m"
#define RED     cerr << "\033[1;31m"
#define GREEN   cerr << "\033[1;32m"
#define YELLOW  cerr << "\033[1;33m"
#define BLUE    cerr << "\033[1;34m"
#define MAGENTA cerr << "\033[1;35m"
#define CYAN    cerr << "\033[1;36m"
#define WHITE   cerr << "\033[1;37m"
#define RESET1  cerr << "\033[0m";   cerr.flush();
#define RESET2  cerr << "\033[0m";   cerr << endl;

enum PacketType {
    ERROR = 1,
    DATA = 2,
    INFO = 4
};

struct Data {
    int type;
    int len;
    int offset;
    char data[DATASIZE];

    Data();
    Data(PacketType packet_type, int packet_len, int packet_offset, char* packet_data);
};

struct Error {
    int type;
    int len;
    char msg[ERRSIZE];

    Error();
    Error(PacketType packet_type, int packet_len, char* packet_msg);
};

struct Info {
    int type;
    int len;
    char msg[INFOSIZE];

    Info();
    Info(PacketType packet_type, int packet_len, char* packet_msg);
};

ostream& operator <<(ostream& os, PacketType& packet_type);

int assignType(PacketType& packet_type, int type);
int recvType(const int& socket, PacketType& packet_type);

int recvError(const int& socket, char* error_msg);
int recvInfo(const int& socket, char* info_msg);
int recvData(const int& socket, int *offset, char* data);

int sendError(const int& socket, char* error_msg);
int sendInfo(const int& socket, char* info_msg);
int sendData(const int& socket, int len, int offset, char* data);

#endif
