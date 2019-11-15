#include "livemodifiable.h"

Data::Data() = default;
Data::Data(PacketType packet_type, int packet_len, char* packet_data)
{
    type = htonl(PacketType(packet_type));
    len = htonl(int(packet_len));

    memset(data, 0, DATASIZE);
    snprintf(data, DATASIZE, "%s", packet_data);
}

Error::Error() = default;
Error::Error(PacketType packet_type, int packet_len, char* packet_msg)
{
    type = htonl(PacketType(packet_type));
    len = htonl(int(packet_len));

    memset(msg, 0, ERRSIZE);
    snprintf(msg, ERRSIZE, "%s", packet_msg);
}

Info::Info() = default;
Info::Info(PacketType packet_type, int packet_len, char* packet_msg) 
{
    type = htonl(PacketType(packet_type));
    len = htonl(int(packet_len));

    memset(msg, 0, INFOSIZE);
    snprintf(msg, INFOSIZE, "%s", packet_msg);
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
        default: return -1;
    }
    return 0;
}


int recvType(const int& socket, PacketType& packet_type)
{
    int type;
    if (recv(socket, reinterpret_cast<void*>(&type), sizeof(int), MSG_WAITALL) < 0) {
        return -1;
    }
    if (type == 0) {
        if (recv(socket, reinterpret_cast<void*>(&type), sizeof(short), MSG_WAITALL) < 0) {
            return -1;
        }
        type <<= 16;
    }
    CYAN cerr << "Received Packet of Type: " << ntohl(type) << endl; RESET1

    if (assignType(packet_type, ntohl(type)) < 0) {
        return -1;
    }
    CYAN cerr << "Received Packet of Type: " << packet_type << endl; RESET1

    return sizeof(type);
}

int recvInfo(const int& socket, char* info_msg)
{
    int len;
    if (recv(socket, reinterpret_cast<void*>(&len), sizeof(int), MSG_WAITALL) < 0) {
        return -1;
    }
    CYAN cerr << "Received Info Packet of Length: " << ntohl(len) << endl; RESET1

    if (recv(socket, reinterpret_cast<void*>(info_msg), INFOSIZE, MSG_WAITALL) < 0) {
        return -1;
    }
    CYAN cerr << "Received Info Packet of Message: " << info_msg << endl; RESET1

    return ntohl(len);
}

int recvData(const int& socket, char* data)
{
    int len;
    if (recv(socket, reinterpret_cast<void*>(&len), sizeof(int), MSG_WAITALL) < 0) {
        return -1;
    }
    CYAN cerr << "Received Data Packet of Length: " << ntohl(len) << endl; RESET1

    if (recv(socket, reinterpret_cast<void*>(data), DATASIZE, MSG_WAITALL) < 0) {
        return -1;
    }
    CYAN cerr << "Received Data Packet:\n" << data << endl; RESET1

    return ntohl(len);
}

int sendInfo(const int& socket, char* info_msg)
{
    Info* info_packet = new Info(PacketType::INFO, strlen(info_msg), info_msg);

    int send_ = 0;
    for(int total_send = 0; total_send < sizeof(Info); total_send += send_)
    {
        if ((send_ = send(socket, reinterpret_cast<void*>(info_packet + total_send),
                          sizeof(Info)-total_send, 0)) < 0) {
            return -1;
        }
    }
    CYAN cerr << "Sent Info Packet: " << info_msg << endl; RESET1
    return strlen(info_msg);
}

int sendData(const int& socket, char* data, int len)
{
    Data* data_packet = new Data(PacketType::DATA, len, data);

    int send_ = 0;
    for(int total_send = 0; total_send < sizeof(Data); total_send += send_)
    {
        if ((send_ = send(socket, reinterpret_cast<void*>(data_packet + total_send),
                          sizeof(Data)-total_send, 0)) < 0) {
            return -1;
        }
    }
    CYAN cerr << "Sent Data Packet of Length: " << len << endl; RESET1
    return len;
}
