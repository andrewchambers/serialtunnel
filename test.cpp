#include "protocol.h"
#include "base64.h"

#include <iostream>
#include <set>
#include <utility>

static int totalTests = 0;
static int failedTests = 0;


#define TEST(T) do { \
                     totalTests++;\
                     if((T()) != 0) { \
                         std::cout << "FAILED: " #T << std::endl;\
                         failedTests++;\
                     }\
                 } while(0)

//Only call this from top level of any tests.

#define ASSERT(X) do { if(!(X)) { std::cout << "ASSERTION FAILED:" \
                    << __FILE__ << ":" << __LINE__ << ":" << #X << std::endl; return 1; } } while(0)



int testPacketConstructors() {
    
    ProtocolPacket p = ProtocolPacket(TYPE_PING);
    
    ASSERT(p.type == TYPE_PING);
    ASSERT(p.seqnum == 0);
    ASSERT(p.data.size() == 0);
    
    char hello[] = "hello";
    ProtocolPacket p2 = ProtocolPacket(TYPE_DATA,2,hello,5);    
    
    ASSERT(p2.type == TYPE_DATA);
    ASSERT(p2.seqnum == 2);
    ASSERT(p2.data.size() == 5);
    ASSERT(p2.data[0] == 'h');
    ASSERT(p2.data[3] == 'l');
    
    return 0;
}


int testProtocolConstructors() {
    Protocol p = Protocol();
    
    ASSERT(p.state == STATE_UNINIT);
    ASSERT(p.lastKeepAlive == 0);
    ASSERT(p.lastPingSendTime == 0);
    
    return 0;
}


int testTimeout() {
    Protocol p;
    
    #define T(A,B) \
        p = Protocol();\
        p.state = A;\
        p.timerEvent(p.lastKeepAlive + p.timeoutInterval);\
        ASSERT(p.state == A);\
        p.timerEvent(p.lastKeepAlive + p.timeoutInterval + 1);\
        ASSERT(p.state == B);
    
    T(STATE_LISTENING,STATE_LISTENING);
    T(STATE_UNINIT,STATE_UNINIT);
    T(STATE_CONNECTING,STATE_UNINIT);
    T(STATE_CONNECTED,STATE_UNINIT);
    
    #undef T
    return 0;
}


int testPinging() {
    Protocol p;
    
    p = Protocol();
    std::vector<ProtocolPacket> out;
    
    #define T(STATE) \
        p.state = STATE;\
        out = p.timerEvent(p.pingInterval);\
        ASSERT(out.size() == 0);\
        ASSERT(p.lastPingSendTime == 0);\
        out = p.timerEvent(p.pingInterval + 1);\
        if (STATE == STATE_CONNECTED) { \
            ASSERT(out.size() == 1);\
            ASSERT(out[0].type == TYPE_PING);\
            ASSERT(p.lastPingSendTime == (p.pingInterval + 1));\
        } else { \
            ASSERT(out.size() == 0);\
            ASSERT(p.lastPingSendTime == 0);\
        }
       
    T(STATE_UNINIT);
    T(STATE_LISTENING);
    T(STATE_CONNECTING);
    T(STATE_CONNECTED);
    
    #undef T
    
    return 0;
}


int testAck() {
    char hello[] = "hello";
    Protocol p;
    
    std::vector<ProtocolPacket> out;
    p = Protocol();
    p.state = STATE_CONNECTED;
    p.seqnum = 5;
    p.outgoingDataPacket = std::tr1::shared_ptr<ProtocolPacket>(new ProtocolPacket(TYPE_DATA,5,hello,5));
    
    ProtocolPacket pp = ProtocolPacket(TYPE_ACK,4);
    out = p.packetEvent(pp,8).first;
    ASSERT(p.lastKeepAlive == 0);
    ASSERT(p.seqnum == 5);
    ASSERT(out.size() == 0);
    ASSERT(p.outgoingDataPacket);
    pp = ProtocolPacket(TYPE_ACK,5);
    out = p.packetEvent(pp,8).first;
    ASSERT(p.lastKeepAlive == 8);
    ASSERT(p.seqnum == 6);
    ASSERT(out.size() == 0);
    ASSERT(!(p.outgoingDataPacket));
   
    p = Protocol();
    p.state = STATE_CONNECTED;
    p.seqnum = 5;
    p.outgoingDataPacket = std::tr1::shared_ptr<ProtocolPacket>(new ProtocolPacket(TYPE_DATA,5,hello,5));   
    
    pp = ProtocolPacket(TYPE_DATA,5);
    out = p.packetEvent(pp,8).first;
    ASSERT(p.lastKeepAlive != 8);
    ASSERT(p.seqnum == 5);
    ASSERT(out.size() == 0);
    ASSERT(p.outgoingDataPacket);
    return 0;
}

int testReadyToSend() {
    Protocol p;
    p.state = STATE_CONNECTED;
    ASSERT(p.readyForData());
    p.outgoingDataPacket = std::tr1::shared_ptr<ProtocolPacket>(new ProtocolPacket(TYPE_DATA,5,NULL,0));
    ASSERT(!p.readyForData());
    
    return 0;
}

int testSendData(){
    Protocol p;
    p.state = STATE_CONNECTED;
    p.seqnum = 1337;
    ASSERT(p.readyForData());
    std::vector<char> emptydata;
    std::vector<ProtocolPacket> out = p.sendData(emptydata,5);
    ASSERT(out.size() == 1);
    ASSERT(out[0].seqnum == 1337);
    ASSERT(p.outgoingDataPacket->seqnum == 1337);
    ASSERT(p.lastSendAttempt == 5);
    return 0;
}

int testDataResending() {
    Protocol p;
    p.state = STATE_CONNECTED;
    p.seqnum = 1337;
    p.pingInterval = 9000; // suppress any pings
    ASSERT(p.readyForData() == true);
    std::vector<char> emptydata;
    std::vector<ProtocolPacket> out = p.sendData(emptydata,0);
    ASSERT(out.size() == 1);
    ASSERT(out[0].seqnum == 1337);
    ASSERT(p.outgoingDataPacket);
    ASSERT(p.outgoingDataPacket->seqnum == 1337);
    out = p.timerEvent(p.lastSendAttempt + p.sendAttemptInterval);
    ASSERT(out.size() == 0);
    out = p.timerEvent(p.lastSendAttempt + p.sendAttemptInterval + 1);
    ASSERT(out.size() == 1);
    ASSERT(p.outgoingDataPacket->seqnum == 1337);
    return 0; 
}

int testListening() {
    Protocol p;
    p.outgoingDataPacket = std::tr1::shared_ptr<ProtocolPacket>(new ProtocolPacket(TYPE_DATA,5,NULL,0));
    p.listen();
    ASSERT(p.state == STATE_LISTENING);
    ASSERT(! p.outgoingDataPacket);
    std::vector<ProtocolPacket> out;
    ProtocolPacket pp = ProtocolPacket(TYPE_CON);
    out = p.packetEvent(pp,50).first;
    ASSERT( p.state == STATE_CONNECTED);
    ASSERT( out.size() == 1);
    ASSERT( out[0].type == TYPE_CONACK);
    return 0;
}

int testConnecting() {
    std::vector<ProtocolPacket> out;
    Protocol p;
    p.outgoingDataPacket = std::tr1::shared_ptr<ProtocolPacket>(new ProtocolPacket(TYPE_DATA,5,NULL,0));
    out = p.connect(0);
    ASSERT(p.state == STATE_CONNECTING);
    ASSERT(! p.outgoingDataPacket);
    
    ASSERT(out.size() == 1);
    ASSERT(out[0].type == TYPE_CON);
    
    ProtocolPacket pp(TYPE_CONACK);
    
    out = p.packetEvent(pp,50).first;
    ASSERT(p.state == STATE_CONNECTED);
    ASSERT(out.size() == 0);
    
    return 0;
}

int testConnect() {
    
    Protocol a;
    Protocol b;
    
    uint64_t t = 0;
    a.listen();
    std::vector<ProtocolPacket> fora = b.connect(t);
    std::vector<ProtocolPacket> forb;
    
    std::vector<ProtocolPacket> out;
    
    std::set<PacketType> packetTypes;
    
    for( int i = 0;  i < 1000 ; i++) {
        for (std::vector<ProtocolPacket>::iterator it = fora.begin() ; it != fora.end() ; it++ ) {
            packetTypes.insert(it->type);
            out = a.packetEvent(*it,t).first;
            forb.insert(forb.end(),out.begin(),out.end()); 
        }
        fora.clear();
        for (std::vector<ProtocolPacket>::iterator it = forb.begin() ; it != forb.end() ; it++ ) {
            packetTypes.insert(it->type);
            out = b.packetEvent(*it,t).first;
            fora.insert(fora.end(),out.begin(),out.end());
        }
        forb.clear();
        
        out = a.timerEvent(t);
        forb.insert(forb.end(),out.begin(),out.end());
        out = b.timerEvent(t);
        fora.insert(fora.end(),out.begin(),out.end());
        
        t += 1;
    }
    
    ASSERT(a.state == STATE_CONNECTED);
    ASSERT(b.state == STATE_CONNECTED);
    
    ASSERT(packetTypes.find(TYPE_CON) != packetTypes.end());
    ASSERT(packetTypes.find(TYPE_CONACK) != packetTypes.end());
    ASSERT(packetTypes.find(TYPE_PING) != packetTypes.end());
    
    for ( int i = 0; i < 10000 ; i++) {
        a.timerEvent(t);
        b.timerEvent(t);
        t += 1;
    }
    ASSERT(a.state == STATE_UNINIT);
    ASSERT(b.state == STATE_UNINIT);
    
    b.connect(t);
    ASSERT(b.state == STATE_CONNECTING);
    b.timerEvent(t);
    ASSERT(b.state == STATE_CONNECTING);
    for(int i = 0; i < 10000 ; i++) {
        b.timerEvent(t);
        t += 1;
    }
    ASSERT(b.state == STATE_UNINIT);
    
    return 0;
    
}

int testRecoverLost() {
    
    Protocol a;
    Protocol b;
    
    uint64_t t = 0;
    a.listen();
    std::vector<ProtocolPacket> fora = b.connect(t);
    std::vector<ProtocolPacket> forb;
    
    std::vector<ProtocolPacket> out;
    
    std::string arecieved;
    std::string brecieved;
    int asentCount = 0;
    int bsentCount = 0;
    int loopCounter = 0;
    while (loopCounter < 500000) {
        t += 1;
        if (asentCount < 500 && a.readyForData()) {
            out = a.sendData("foo",t);
            forb.insert(forb.end(),out.begin(),out.end());
            asentCount += 1;
        }
           
        if (bsentCount < 1000 && b.readyForData()) {
            out = b.sendData("bar",t);
            fora.insert(fora.end(),out.begin(),out.end());
            bsentCount += 1;
        }
        
        if (loopCounter > 10) { //start dropping packets after connection is up
            if  (((loopCounter % 8) == 0) && fora.size() ) {
                fora.pop_back();
            }
            
            if (((loopCounter % 7) == 0) && forb.size()){
                forb.pop_back();
            }
        }
        
        // drop some more packets by not accepting data arbitrarily
        for (std::vector<ProtocolPacket>::iterator it = fora.begin() ; it != fora.end() ; it++ ) {
            std::pair<std::vector<ProtocolPacket>,std::vector<char> > eventResult;
            eventResult = a.packetEvent(*it,t,true);
            out = eventResult.first;
            std::string gotData(eventResult.second.begin(),eventResult.second.end());
            arecieved += gotData;
            forb.insert(forb.end(),out.begin(),out.end());
           
        }
        fora.clear();
        for (std::vector<ProtocolPacket>::iterator it = forb.begin() ; it != forb.end() ; it++ ) {
            std::pair<std::vector<ProtocolPacket>,std::vector<char> > eventResult;
            eventResult = b.packetEvent(*it,t,true);
            out = eventResult.first;
            std::string gotData(eventResult.second.begin(),eventResult.second.end());
            brecieved += gotData;
            fora.insert(fora.end(),out.begin(),out.end());
        }
        forb.clear();
        
        out = a.timerEvent(t);
        forb.insert(forb.end(),out.begin(),out.end());
        out = b.timerEvent(t);
        fora.insert(fora.end(),out.begin(),out.end());
        
        if (asentCount == 500 && bsentCount == 1000 && !a.outgoingDataPacket && !b.outgoingDataPacket) {
            break;
        }
        
        loopCounter += 1;
    }
    ASSERT(a.state == STATE_CONNECTED);
    ASSERT(b.state == STATE_CONNECTED);
    ASSERT(loopCounter != 500000);
    ASSERT(arecieved.size() == 1000*3);
    for(int i = 0 ; i < 1000; i++){
        ASSERT(arecieved.substr(i*3,3) == std::string("bar"));
    }
    for(int i = 0 ; i < 500; i++){
        ASSERT(brecieved.substr(i*3,3) == std::string("foo"));
    }    
    return 0;
}


int testBase64() {
    ASSERT(b64encode("any") == std::string("YW55"));
    ASSERT(b64encode("anyany") == std::string("YW55YW55"));
    ASSERT(b64encode("a") == std::string("YQ=="));
    ASSERT(b64encode("aa") == std::string("YWE="));
    ASSERT(b64encode("aaa") == std::string("YWFh"));
    std::cout << b64encode("aaaa") << std::endl;
    ASSERT(b64encode("aaaa") == std::string("YWFhYQ=="));
    ASSERT(b64encode("aaaaa") == std::string("YWFhYWE="));
    return 1;
}

int main (int argc, char const* argv[]) {
    TEST(testPacketConstructors);
    TEST(testProtocolConstructors);
    TEST(testTimeout);
    TEST(testPinging);
    TEST(testAck);
    TEST(testReadyToSend);
    TEST(testSendData);
    TEST(testDataResending);
    TEST(testListening);
    TEST(testConnecting);
    TEST(testConnect);
    TEST(testRecoverLost);
    
    TEST(testBase64);
    
    std::cout << "Passed " << (totalTests - failedTests) << "/" << totalTests << std::endl;
    return failedTests ? 1 : 0;
}
