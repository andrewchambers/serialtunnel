#include "protocol.h"



class PacketBuilder {
    
    public:
       std::tr1::shared_ptr<ProtocolPacket> addData(std::vector<char> & data);
    
    private:
        std::vector<char> buffered;
}