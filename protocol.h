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

class PacketBuilder {
    
    public:
       std::vector<ProtocolPacket> addData(uint8_t * p,int sz);
       std::vector<ProtocolPacket> addData(const std::vector<uint8_t> & data);
       std::vector<ProtocolPacket> addData(const std::string & data);
    
    private:
        std::vector<uint8_t> buffered;
};


class Protocol {
    
    public: 
        Protocol();


        void listen();
        bool readyForData() const;
        ProtoState getState() const;
        
        std::vector<uint8_t> timerEvent(uint64_t time);
        std::pair<std::vector<uint8_t>,std::vector<uint8_t> > 
        dataEvent(const std::vector<uint8_t> & datain,uint64_t time, bool wantData = false);
    
        std::vector<uint8_t> sendData(std::vector<uint8_t>  data, uint64_t time);
        std::vector<uint8_t> sendData(const char * c, uint64_t time);
        std::vector<uint8_t> connect(uint64_t time);    
        
        
    private:
        
        std::vector<ProtocolPacket> _timerEvent(uint64_t time);
        std::pair<std::vector<ProtocolPacket>,std::vector<uint8_t> > _packetEvent(ProtocolPacket & pPacket,uint64_t time, bool wantData = false);
    
        std::vector<ProtocolPacket> _sendData(std::vector<uint8_t>  data, uint64_t time);
        std::vector<ProtocolPacket> _sendData(const char * c, uint64_t time);
        std::vector<ProtocolPacket> _connect(uint64_t time);    
        
    
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
        
        PacketBuilder pb;
        
            
};


std::vector<uint8_t>
encodePacket(const ProtocolPacket & p);

std::vector<uint8_t>
encodePackets(const std::vector<ProtocolPacket> & pkts);

std::string
encodePacket_s(const ProtocolPacket & p);

