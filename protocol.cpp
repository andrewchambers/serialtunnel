#include "protocol.h"

#include <iostream>

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
    this->pingInterval = 100;
    this->timeoutInterval = this->pingInterval * 10;
    this->lastSendAttempt = 0;
    this->sendAttemptInterval = 10;
}

ProtoState Protocol::getState() const {
    return state;
}

std::vector<ProtocolPacket> Protocol::timerEvent(uint64_t now) {
    
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
Protocol::packetEvent(ProtocolPacket & packet,uint64_t now,bool wantData) {
    
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


std::vector<ProtocolPacket> Protocol::sendData(std::vector<uint8_t>  data, uint64_t now) {
    std::vector<ProtocolPacket> ret;
    
    if (! this->readyForData()) {
        //XXX bug
        return ret;
    }
        
    this->outgoingDataPacket = std::tr1::shared_ptr<ProtocolPacket>(new ProtocolPacket(TYPE_DATA,this->seqnum,data));
    this->lastSendAttempt = now;
    ret.push_back(*(this->outgoingDataPacket));
    
    return ret;
}

std::vector<ProtocolPacket> Protocol::sendData(const char * s, uint64_t now) {
    std::vector<uint8_t> data(s,s+strlen(s));
    return sendData(data,now);
}


void Protocol::listen() {
    this->outgoingDataPacket.reset();
    this->state = STATE_LISTENING;
}


std::vector<ProtocolPacket> Protocol::connect(uint64_t now) {   
    std::vector<ProtocolPacket> ret;
    this->outgoingDataPacket.reset();
    this->state = STATE_CONNECTING;
    this->lastKeepAlive = now;
    this->lastPingSendTime = now;
    ret.push_back(ProtocolPacket(TYPE_CON));
    return ret;
}
