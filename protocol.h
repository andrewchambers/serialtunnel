#pragma once
#include <stdint.h>
#include <tr1/memory>
#include <string>
#include <vector>

#include <cstring>

enum ProtoState {
    STATE_UNINIT,
    STATE_LISTENING,
    STATE_CONNECTING,
    STATE_CONNECTED,
};

enum PacketType {
    TYPE_PING,
    TYPE_CON,
    TYPE_CONACK,
    TYPE_ACK,
    TYPE_DATA
};

class ProtocolPacket {

    public:
        PacketType type;
        uint32_t seqnum;
        std::vector<uint8_t> data;
        ProtocolPacket(PacketType t,uint32_t seqnum, char data[], uint32_t n );
        ProtocolPacket(PacketType t,uint32_t seqnum, std::string d );
        ProtocolPacket(PacketType t,uint32_t seqnum, std::vector<uint8_t> data );
        ProtocolPacket(PacketType t,uint32_t seqnum);
        ProtocolPacket(PacketType t);

};

class Protocol {
    
    public: 
        Protocol();
        std::vector<ProtocolPacket> timerEvent(uint64_t time);
        std::pair<std::vector<ProtocolPacket>,std::vector<uint8_t> > packetEvent(ProtocolPacket & pPacket,uint64_t time, bool wantData = false);
        std::vector<ProtocolPacket> sendData(std::vector<uint8_t>  data, uint64_t time);
        std::vector<ProtocolPacket> sendData(const char * c, uint64_t time);
        std::vector<ProtocolPacket> connect(uint64_t time);
        void listen();
        bool readyForData() const;
        ProtoState getState() const;
        
        
        
    private:
        ProtoState state;
        uint32_t seqnum;
        uint32_t expectedDataSeqnum;
        uint64_t timeoutInterval;
        uint64_t lastKeepAlive;
        uint64_t lastPingSendTime;
        uint64_t lastSendAttempt;
        uint64_t sendAttemptInterval;
        uint64_t pingInterval;
        std::tr1::shared_ptr<ProtocolPacket> outgoingDataPacket;
            
};
