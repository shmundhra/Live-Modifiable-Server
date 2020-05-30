#include "livemodifiable.h"

Data::Data() = default;
Data::Data(PacketType packet_type, int packet_len, int packet_offset, char* packet_data)
{
    type = htonl(PacketType(packet_type));
    len = htonl(packet_len);
    offset = htonl(packet_offset);

    bzero(data, DATASIZE * sizeof(char));
    memcpy(data, packet_data, packet_len);
}

Error::Error() = default;
Error::Error(PacketType packet_type, int packet_len, char* packet_msg)
{
    type = htonl(PacketType(packet_type));
    len = htonl(packet_len);

    bzero(msg, ERRSIZE * sizeof(char));
    memcpy(msg, packet_msg, packet_len);
}

Info::Info() = default;
Info::Info(PacketType packet_type, int packet_len, char* packet_msg)
{
    type = htonl(PacketType(packet_type));
    len = htonl(packet_len);

    bzero(msg, INFOSIZE * sizeof(char));
    memcpy(msg, packet_msg, packet_len);
}

ostream& operator <<(ostream& os, PacketType& packet_type)
{
    switch(packet_type)
    {
        case PacketType::DATA:  return os << "DATA";
        case PacketType::ERROR: return os << "ERROR";
        case PacketType::INFO:  return os << "INFO";
        default: return os << "Invalid";
    }
}

int assignType(PacketType& packet_type, int type)
{
    switch(type)
    {
        case 1: packet_type = PacketType(PacketType::ERROR); break;
        case 2: packet_type = PacketType(PacketType::DATA);  break;
        case 4: packet_type = PacketType(PacketType::INFO);  break;
        default: RED << "Can't recognize packet type - " << type; RESET2;
                 return -1;
    }
    return 0;
}


int recvType(const int& socket, PacketType& packet_type)
{
    int type;
    if (recv(socket, reinterpret_cast<void*>(&type), sizeof(int), MSG_WAITALL) < 0) {
        return -1;
    }
    if (assignType(packet_type, ntohl(type)) < 0) {
        return -1;
    }
    CYAN << getpid() << ":: Received Packet of Type: "; BLUE << packet_type; RESET2;

    return sizeof(type);
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
    if (recv(socket, reinterpret_cast<void*>(data), DATASIZE, MSG_WAITALL) < 0) {
        return -1;
    }
    CYAN << getpid() << ":: Received Data Packet at Offset: ";
    BLUE << ntohl(*offset) << "\n" << data; RESET2;

    return ntohl(len);
}

int sendInfo(const int& socket, char* info_msg)
{
    Info* info_packet = new Info(PacketType::INFO, strlen(info_msg), info_msg);

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
    Data* data_packet = new Data(PacketType::DATA, len, offset, data);

    int send_ = 0;
    for(int total_send = 0; total_send < sizeof(Data); total_send += send_)
    {
        if ((send_ = send(socket, reinterpret_cast<void*>((char*)data_packet + total_send),
                          sizeof(Data)-total_send, 0)) < 0) {
            return -1;
        }
    }
    CYAN << getpid() << ":: Sent Data Packet of Length: ";
    BLUE << len << " at Offset: " << offset; RESET2;
    return len;
}
