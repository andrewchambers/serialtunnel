#pragma once

#include "protocol.h"



class PacketBuilder {
    
    public:
       std::vector<ProtocolPacket> addData(uint8_t * p,int sz);
       std::vector<ProtocolPacket> addData(const std::vector<uint8_t> & data);
       std::vector<ProtocolPacket> addData(const std::string & data);
    
    private:
        std::vector<uint8_t> buffered;
};


std::vector<uint8_t>
encodePacket(const ProtocolPacket & p);

std::vector<uint8_t>
encodePackets(const std::vector<ProtocolPacket> & pkts);

std::string
encodePacket_s(const ProtocolPacket & p);
