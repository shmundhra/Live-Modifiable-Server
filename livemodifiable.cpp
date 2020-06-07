#include "livemodifiable.h"

Error::Error() = default;
Error::Error(int packet_len, char* packet_msg)
{
    type = htonl(PacketType::ERROR);
    len = htonl(packet_len);

    bzero(msg, ERRSIZE * sizeof(char));
    memcpy(msg, packet_msg, packet_len);
}

Info::Info() = default;
Info::Info(int packet_len, char* packet_msg)
{
    type = htonl(PacketType::INFO);
    len = htonl(packet_len);

    bzero(msg, INFOSIZE * sizeof(char));
    memcpy(msg, packet_msg, packet_len);
}

Data::Data() = default;
Data::Data(int packet_len, int packet_offset, char* packet_data)
{
    type = htonl(PacketType::DATA);
    len = htonl(packet_len);
    offset = htonl(packet_offset);

    bzero(data, DATASIZE * sizeof(char));
    memcpy(data, packet_data, packet_len);
}

Ack::Ack() = default;
Ack::Ack(int next_offset)
{
    type = htonl(PacketType::ACK);
    offset = htonl(next_offset);
}

BackupData::BackupData() = default;
BackupData::BackupData(Data backup_data, pid_t server_pid)
{   
    data = backup_data;
    data.type = htonl(PacketType::BACKUPDATA);
    pid = htonl(server_pid);
}

BackupInfo::BackupInfo() = default;
BackupInfo::BackupInfo(Info backup_info, pid_t server_pid)
{
    info = backup_info;
    info.type = htonl(PacketType::BACKUPINFO);
    pid = htonl(server_pid);
}

ostream& operator <<(ostream& os, PacketType& packet_type)
{
    switch(packet_type)
    {
        case PacketType::DATA       : return os << "DATA";
        case PacketType::ACK        : return os << "ACK";
        case PacketType::ERROR      : return os << "ERROR";
        case PacketType::INFO       : return os << "INFO";
        case PacketType::BACKUPDATA : return os << "BACKUP DATA";
        case PacketType::BACKUPINFO : return os << "BACKUP INFO";
        default: return os << "Invalid";
    }
}

static int assignType(PacketType& packet_type, int type)
{
    switch(type)
    {
        case 1: packet_type = PacketType(PacketType::ERROR); break;
        case 2: packet_type = PacketType(PacketType::DATA);  break;
        case 3: packet_type = PacketType(PacketType::ACK);  break;
        case 4: packet_type = PacketType(PacketType::INFO);  break;
        case 5: packet_type = PacketType(PacketType::BACKUPDATA);  break;
        case 6: packet_type = PacketType(PacketType::BACKUPINFO);  break;
        default: RED << "Can't recognize packet type - " << type; RESET2;
                 return -1;
    }
    return 0;
}

int recvType(const int& socket, PacketType& packet_type)
{
    int type;
    int recv_ = recv(socket, reinterpret_cast<void*>(&type), sizeof(int), MSG_WAITALL);
    if (recv_ < 0) {
        return -1;
    }
    if (recv_ == 0) {
        return 0;
    }
    if (assignType(packet_type, ntohl(type)) < 0) {
        return -1;
    }
    CYAN << getpid() << ":: Received Packet of Type: "; BLUE << packet_type; RESET2;

    return sizeof(type);
}

int recvError(const int& socket, char* error_msg)
{
    int len;
    if (recv(socket, reinterpret_cast<void*>(&len), sizeof(int), MSG_WAITALL) < 0) {
        return -1;
    }
    CYAN << getpid() << ":: Received Error Packet of Length: "; BLUE << ntohl(len); RESET2;

    if (recv(socket, reinterpret_cast<void*>(error_msg), ERRSIZE, MSG_WAITALL) < 0) {
        return -1;
    }
    CYAN << getpid() << ":: Received Error Packet of Message: "; RED << error_msg; RESET2;

    return ntohl(len);
}

int recvInfo(const int& socket, char* info_msg)
{
    int len;
    if (recv(socket, reinterpret_cast<void*>(&len), sizeof(int), MSG_WAITALL) < 0) {
        return -1;
    }
    CYAN << getpid() << ":: Received Info Packet of Length: "; BLUE << ntohl(len); RESET2;

    if (recv(socket, reinterpret_cast<void*>(info_msg), INFOSIZE, MSG_WAITALL) < 0) {
        return -1;
    }
    CYAN << getpid() << ":: Received Info Packet of Message: "; BLUE << info_msg; RESET2;

    return ntohl(len);
}

int recvData(const int& socket, int* offset, char* data)
{
    int len;
    if (recv(socket, reinterpret_cast<void*>(&len), sizeof(int), MSG_WAITALL) < 0) {
        return -1;
    }
    CYAN << getpid() << ":: Received Data Packet of Length: "; BLUE << ntohl(len); RESET2;

    if (recv(socket, reinterpret_cast<void*>(offset), sizeof(int), MSG_WAITALL) < 0) {
        return -1;
    }
    *offset = ntohl(*offset);
    if (recv(socket, reinterpret_cast<void*>(data), DATASIZE, MSG_WAITALL) < 0) {
        return -1;
    }
    CYAN << getpid() << ":: Received Data Packet at Offset: ";
    BLUE << *offset << "\n" << data; RESET2;

    return ntohl(len);
}

int recvAck(const int& socket, int* offset)
{
    if (recv(socket, reinterpret_cast<void*>(offset), sizeof(int), MSG_WAITALL) < 0) {
        return -1;
    }
    *offset = ntohl(*offset);
    CYAN << getpid() << ":: Received Ack Packet with offset: ";
    BLUE << *offset; RESET2;

    return sizeof(offset);
}

int recvBackupData(const int& socket, int* offset, char* data, pid_t* pid)
{
    int len;
    if ((len = recvData(socket, offset, data)) < 0) {
        return -1;
    }
    if (recv(socket, reinterpret_cast<void*>(pid), sizeof(pid_t), MSG_WAITALL) < 0) {
        return -1;
    }
    *pid = ntohl(*pid);

    CYAN << getpid() << ":: Received Backup Data From: "; BLUE << *pid; RESET2;
    return len;
}

int recvBackupInfo(const int& socket, char* info_msg, pid_t* pid)
{
    int len;
    if ((len = recvInfo(socket, info_msg)) < 0) {
        return -1;
    }
    if (recv(socket, reinterpret_cast<void*>(pid), sizeof(pid_t), MSG_WAITALL) < 0) {
        return -1;
    }
    *pid = ntohl(*pid);

    CYAN << getpid() << ":: Received Backup Info of Message: "; BLUE << info_msg;
    CYAN << " From: "; BLUE << *pid; RESET2;
    
    return len;
}

int sendError(const int& socket, char* error_msg)
{
    Error* error_packet = new Error(strlen(error_msg), error_msg);

    int send_ = 0;
    for(int total_send = 0; total_send < sizeof(Error); total_send += send_)
    {
        if ((send_ = send(socket, reinterpret_cast<void*>((char*)error_packet + total_send),
                          sizeof(Error)-total_send, 0)) < 0) {
            return -1;
        }
    }
    CYAN << getpid() << ":: Sent Error Packet: "; RED << error_msg; RESET2;
    return strlen(error_msg);
}

int sendInfo(const int& socket, char* info_msg)
{
    Info* info_packet = new Info(strlen(info_msg), info_msg);

    int send_ = 0;
    for(int total_send = 0; total_send < sizeof(Info); total_send += send_)
    {
        if ((send_ = send(socket, reinterpret_cast<void*>((char*)info_packet + total_send),
                          sizeof(Info)-total_send, 0)) < 0) {
            return -1;
        }
    }
    CYAN << getpid() << ":: Sent Info Packet: "; BLUE << info_msg; RESET2;
    return strlen(info_msg);
}

int sendData(const int& socket, int len, int offset, char* data)
{
    Data* data_packet = new Data(len, offset, data);

    int send_ = 0;
    for(int total_send = 0; total_send < sizeof(Data); total_send += send_)
    {
        if((send_ = send(socket, reinterpret_cast<void*>((char*)data_packet + total_send),
                         sizeof(Data)-total_send, 0)) < 0) {
            return -1;
        }
    }
    CYAN << getpid() << ":: Sent Data Packet of Length: ";
    BLUE << len; CYAN << " at Offset: "; BLUE << offset; RESET2;
    return len;
}

int sendAck(const int& socket, int offset)
{
    Ack* ack_packet = new Ack(offset);

    int send_ = 0;
    for(int total_send = 0; total_send < sizeof(Ack); total_send += send_)
    {
        if((send_ = send(socket, reinterpret_cast<void*>((char*)ack_packet + total_send),
                         sizeof(Ack)-total_send, 0)) < 0){
            return -1;
        }
    }
    CYAN << getpid() << ":: Sent Ack Packet with Offset: "; BLUE << offset; RESET2;
    return sizeof(offset);
}

int sendBackupData(const int& socket, Data data, pid_t pid)
{
    BackupData* backup_packet = new BackupData(data, pid);

    int send_ = 0;
    for(int total_send = 0; total_send < sizeof(BackupData); total_send += send_)
    {
        if((send_ = send(socket, reinterpret_cast<void*>((char*)backup_packet + total_send),
                         sizeof(BackupData)-total_send, 0)) < 0) {
            return -1;
        }
    }
    CYAN << getpid() << ":: Sent Backup Data Packet of Length: ";
    BLUE << ntohl(data.len); CYAN << " at Offset: "; BLUE << ntohl(data.offset); RESET2;
    return ntohl(data.len);
}

int sendBackupInfo(const int& socket, char* info_msg, pid_t pid)
{
    BackupInfo* backup_packet = new BackupInfo(Info(strlen(info_msg), info_msg), pid);

    int send_ = 0;
    for(int total_send = 0; total_send < sizeof(BackupInfo); total_send += send_)
    {
        if((send_ = send(socket, reinterpret_cast<void*>((char*)backup_packet + total_send),
                         sizeof(BackupInfo)-total_send, 0)) < 0) {
            return -1;
        }
    }
    CYAN << getpid() << ":: Sent Backup Info Packet: "; BLUE << info_msg; RESET2;
    return strlen(info_msg);
}
