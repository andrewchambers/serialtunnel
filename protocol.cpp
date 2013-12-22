#include "protocol.h"

#include <iostream>
#include <cstdio>
#include <algorithm>
#include "base64.h"

// Protocol Packet

ProtocolPacket::ProtocolPacket(PacketType t) : type(t) , seqnum(0) {

}

ProtocolPacket::ProtocolPacket(PacketType t, uint32_t seq) : type(t) , seqnum(seq) {

}

ProtocolPacket::ProtocolPacket(PacketType t,uint32_t seq, char data_in[] , uint32_t n ) 
    : type(t) , seqnum(seq) , data(data_in,data_in + n) {

}

ProtocolPacket::ProtocolPacket(PacketType t,uint32_t seq, std::string d ) 
    : type(t) , seqnum(seq) , data(d.begin(),d.end()) {

}


ProtocolPacket::ProtocolPacket(PacketType t,uint32_t seq, std::vector<uint8_t> d ) 
    : type(t) , seqnum(seq) , data(d) {

}

// Protocol

Protocol::Protocol() {
    this->state = STATE_UNINIT;
    this->seqnum = 0;
    this->expectedDataSeqnum = 0;
    this->lastKeepAlive = 0;
    this->lastPingSendTime = 0;
    this->pingInterval = 1000;
    this->timeoutInterval = this->pingInterval * 10;
    this->lastSendAttempt = 0;
    this->sendAttemptInterval = 50;
}

ProtoState Protocol::getState() const {
    return state;
}

std::vector<ProtocolPacket> Protocol::_timerEvent(uint64_t now) {
    
    std::vector<ProtocolPacket> ret;
    
    if (this->state == STATE_UNINIT) {
        return ret;
    }
    
    if (this->state != STATE_LISTENING) {
        if (now - this->lastKeepAlive > this->timeoutInterval) {
            this->state = STATE_UNINIT;
        }
    }
    
    
    if (this->state == STATE_CONNECTED) {
        if ( (now - this->lastPingSendTime) > this->pingInterval ) {
            this->lastPingSendTime = now;
            ret.push_back(ProtocolPacket(TYPE_PING));
        }
        
        
        if (this->outgoingDataPacket) {
            if (now - this->lastSendAttempt > this->sendAttemptInterval) {
                this->lastSendAttempt = now;
                ret.push_back(*(this->outgoingDataPacket));
            }
        }
    }
     
    return ret;
}


std::pair<std::vector<ProtocolPacket>,std::vector<uint8_t> > 
Protocol::_packetEvent(ProtocolPacket & packet,uint64_t now,bool wantData) {
    
    std::pair<std::vector<ProtocolPacket>,std::vector<uint8_t> > ret;
    
    if (this->outgoingDataPacket) {
        if (packet.type == TYPE_ACK) {
            if (packet.seqnum == this->outgoingDataPacket->seqnum) {
                this->outgoingDataPacket.reset();
                this->seqnum += 1;
                this->lastKeepAlive = now;
            }
        }
    }
    
    
    if(wantData  && this->state == STATE_CONNECTED && packet.type == TYPE_DATA && packet.seqnum <= this->expectedDataSeqnum) {
        ret.first.push_back(ProtocolPacket(TYPE_ACK,packet.seqnum));
        if (packet.seqnum == this->expectedDataSeqnum) {
            this->expectedDataSeqnum += 1;
            for(std::vector<uint8_t>::iterator it = packet.data.begin(); it != packet.data.end() ; it++) {
                ret.second.push_back(*it);
            }
                
        }
    }
    
    if (packet.type == TYPE_PING && (this->state == STATE_CONNECTED)) {
        this->lastKeepAlive = now; 
    }
    
    if (this->state == STATE_LISTENING) {
        if(packet.type == TYPE_CON) {
            this->lastPingSendTime = now;
            this->lastKeepAlive = now;
            this->state = STATE_CONNECTED;
            ret.first.push_back(ProtocolPacket(TYPE_CONACK));
        }
    }
    
    if (this->state == STATE_CONNECTING) {
        if(packet.type == TYPE_CONACK) {
            this->lastPingSendTime = now;
            this->lastKeepAlive = now;
            this->state = STATE_CONNECTED;
        }
    }
    
    return ret;
}


bool Protocol::readyForData() const {
    if (this->state != STATE_CONNECTED)
        return false;
    
    if (this->outgoingDataPacket)
        return false;
    
    return true;
}


std::vector<ProtocolPacket> Protocol::_sendData(std::vector<uint8_t>  data, uint64_t now) {
    std::vector<ProtocolPacket> ret;
    
    if (! this->readyForData()) {
        std::cerr << "FATAL BUG: _sendData:" << __FILE__ << ":" << __LINE__ << std::endl;
        exit(1);
    }
    
    this->outgoingDataPacket = std::tr1::shared_ptr<ProtocolPacket>(new ProtocolPacket(TYPE_DATA,this->seqnum,data));
    this->lastSendAttempt = now;
    ret.push_back(*(this->outgoingDataPacket));
    
    return ret;
}

std::vector<ProtocolPacket> Protocol::_sendData(const char * s, uint64_t now) {
    std::vector<uint8_t> data(s,s+strlen(s));
    return _sendData(data,now);
}


void Protocol::listen() {
    this->outgoingDataPacket.reset();
    this->state = STATE_LISTENING;
}


std::vector<ProtocolPacket> Protocol::_connect(uint64_t now) {   
    std::vector<ProtocolPacket> ret;
    this->outgoingDataPacket.reset();
    this->state = STATE_CONNECTING;
    this->lastKeepAlive = now;
    this->lastPingSendTime = now;
    ret.push_back(ProtocolPacket(TYPE_CON));
    return ret;
}

std::vector<uint8_t> Protocol::timerEvent(uint64_t time) {
    return encodePackets(_timerEvent(time));
}

std::pair<std::vector<uint8_t>,std::vector<uint8_t> > 
Protocol::dataEvent(const std::vector<uint8_t> & datain,uint64_t time, bool wantData) {
    
    std::vector<ProtocolPacket> ret;
    
    std::vector<uint8_t> dataout;
    
    std::vector<ProtocolPacket> arrived = pb.addData(datain);
    
    for(std::vector<ProtocolPacket>::iterator it = arrived.begin(); it != arrived.end() ; it++) {
        std::pair<std::vector<ProtocolPacket>,std::vector<uint8_t> > presp = _packetEvent(*it,time,wantData);
        
        ret.insert(ret.end(),presp.first.begin(),presp.first.end());
        dataout.insert(dataout.end(),presp.second.begin(),presp.second.end());
        
    }
    
    return std::pair<std::vector<uint8_t>,std::vector<uint8_t> >(encodePackets(ret),dataout);
    
}

std::vector<uint8_t> Protocol::sendData(std::vector<uint8_t>  data, uint64_t time) {
    return encodePackets(_sendData(data,time));
}

std::vector<uint8_t> Protocol::sendData(const char * c, uint64_t time){
    return encodePackets(_sendData(c,time));
}

std::vector<uint8_t> Protocol::connect(uint64_t time) {
    return encodePackets(_connect(time));   
}




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


