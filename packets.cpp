#include "packets.h"
#include "base64.h"
#include <algorithm>
#include <arpa/inet.h>
#include <iostream>
#include <cstdio>

const int MOD_ADLER = 65521;

template <class T>
uint32_t checksumFunc(T it, T end) {
    uint32_t a = 1, b = 0;
    while(it != end) {
        a = (a + *it) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
        it++;
    }
     
    return (b << 16) | a;
}


//XXX limit the size of buffered to stop memory running out with rubbish

std::vector<ProtocolPacket> 
PacketBuilder::addData(uint8_t * p,int sz) {
    const std::vector<uint8_t> vdata(p,p+sz);    
    return addData(vdata);
}

std::vector<ProtocolPacket>
PacketBuilder::addData(const std::string & data) {
    std::vector<uint8_t> vdata(data.begin(),data.end());    
    return addData(vdata);
}

std::vector<ProtocolPacket>
PacketBuilder::addData(const std::vector<uint8_t> & data) {
    
    std::vector<ProtocolPacket> ret;
    
    buffered.insert(buffered.end(),data.begin(),data.end());
    
    while(true) {
        std::vector<uint8_t>::iterator it;
        
        it = std::find(buffered.begin(),buffered.end(),'\n');
        
        if (it == buffered.end()) {
            break;
        }
        
        std::vector<uint8_t> packetData(buffered.begin(),it);
        buffered.erase(buffered.begin(),it + 1);
        
        std::vector<uint8_t> decoded = b64decode(packetData);
        
        uint32_t checksum = 0;
        
        if(decoded.size() < 12) {
            continue;
        }
        
        checksum |= decoded[0];
        checksum |= decoded[1] << 8;
        checksum |= decoded[2] << 16;
        checksum |= decoded[3] << 24;
        
        if( checksum != checksumFunc(decoded.begin() + 4,decoded.end()) ) {
            continue;
        }
        
        uint32_t t = 0;
        uint32_t seqnum = 0;
        
        t |= decoded[4];
        t |= decoded[5] << 8;
        t |= decoded[6] << 16;
        t |= decoded[7] << 24;
        
        seqnum |= decoded[8];
        seqnum |= decoded[9] << 8;
        seqnum |= decoded[10] << 16;
        seqnum |= decoded[11] << 24;
        
        PacketType type = static_cast<PacketType>(t);;
        
        std::vector<uint8_t> data(decoded.begin() + 12,decoded.end());
        
        ret.push_back(ProtocolPacket(type,seqnum,data));
        
    }
    return ret;
}


std::string
encodePacket_s(const ProtocolPacket & p) {
    std::vector<uint8_t> ret = encodePacket(p);
    return std::string(ret.begin(),ret.end());
}


std::vector<uint8_t>
encodePackets(const std::vector<ProtocolPacket> & pkts) {
    std::vector<uint8_t> ret;
    
    for(std::vector<ProtocolPacket>::const_iterator it = pkts.begin(); it != pkts.end() ; it++) {
        std::vector<uint8_t> curout = encodePacket(*it);
        ret.insert(ret.end(),curout.begin(),curout.end());
    }
    return ret;
}

std::vector<uint8_t>
encodePacket(const ProtocolPacket & p) {
    
    std::vector<uint8_t> ret;
    
    uint32_t type = static_cast<uint32_t>(p.type);
    uint32_t seqnum = p.seqnum;
    
    for(int i = 0; i < 4 ; i++) {
        uint8_t x = type & 0xff;
        ret.push_back(x);
        type = type >> 8;
    }
    
    for(int i = 0; i < 4 ; i++) {
        uint8_t x = seqnum & 0xff;
        ret.push_back(x);
        seqnum = seqnum >> 8;
    }
    
    ret.insert(ret.end(),p.data.begin(),p.data.end());
    
    uint32_t checksum = checksumFunc(ret.begin(),ret.end());
    
    
    
    for(int i = 0; i < 4 ; i++) {
        uint8_t x = (checksum) &  0xff;
        ret.insert(ret.begin(),x);
        checksum = checksum >> 8;
    }
    
    std::reverse(ret.begin(),ret.begin() + 4);
    
    ret = b64encode_v(ret);
    
    ret.push_back('\n');
    
    return ret;
        
}




