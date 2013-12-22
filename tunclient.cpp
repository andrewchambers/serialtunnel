#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <signal.h>
#include <time.h>

#include "protocol.h"



//more code borrowed from ncat
void subexec(char * cmdexec[],int * childpid, int * childin,int * childout) {
    int child_stdin[2];
    int child_stdout[2];
    int pid;

    if (pipe(child_stdin) == -1 || pipe(child_stdout) == -1) {
        perror("Can't create child pipes");
        exit(1);
    }


    if ( (pid = fork()) < 0) {
	    perror("fork error");
	    exit(1);
    } else if ( pid == 0 ) {
        close(child_stdin[1]);
        close(child_stdout[0]);

        if(dup2(child_stdin[0], STDIN_FILENO) < 0) {
            perror("dup2 failed");
            exit(1);
        }
        
        if(dup2(child_stdout[1], STDOUT_FILENO) < 0) {
            perror("dup2 failed");
            exit(1);
        }
        
        execvp(cmdexec[0],cmdexec);
        std::cerr << "error starting command " << cmdexec[0] << std::endl;
        exit(1);
        
	    /* do child stuff */
    } else {
	    close(child_stdin[0]);
        close(child_stdout[1]);
	    /* do parent stuff */
	    *childin = child_stdin[1];
	    *childout = child_stdout[0];
    }
}

static int max(int a, int b) {
    if (a > b) {
        return a;
    }
    return b;
}

uint64_t getNow() {
    struct timespec ts;
    
    if(clock_gettime(CLOCK_REALTIME,&ts)) {
        std::cerr << "error failed to get system time\n.";
        exit(1);
    }
    
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}


void proxy_forever(Protocol & p, std::vector<uint8_t> & initialProtoData ,int protoin,int protoout,int datain, int dataout) {
    
    int maxfd = datain;
    
    maxfd = max(datain,maxfd);
    maxfd = max(dataout,maxfd);
    maxfd = max(protoout,maxfd);
    maxfd = max(protoin,maxfd);
    
    int64_t now = getNow();
    
    std::vector<uint8_t> out;
    std::vector<uint8_t> bufferedProtocolData;
    std::vector<uint8_t> bufferedData;
    
    bufferedProtocolData = initialProtoData;
    
    uint8_t  buff[4096];
    
    
    for (;;) {
        fd_set readfds;
        fd_set writefds;
        fd_set errfds;
        
        int r, n_r,n_w;
        now = getNow();
        if (p.getState() == STATE_UNINIT) {
            std::cerr << "Connection terminated." << std::endl;
            break;
        }

        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&errfds);
        
        int doDataIn = p.readyForData();
        int doBufferedOut = bufferedData.size() > 0;
        int doBufferedProtoOut = bufferedProtocolData.size() > 0;
        
        FD_SET(protoin, &readfds);
        
        if(doDataIn) {
            FD_SET(datain, &readfds);
        }
        
        if(doBufferedProtoOut) {
            FD_SET(protoout,&writefds);
        }
        
        if(doBufferedOut) {
            FD_SET(dataout,&writefds);
        }
        
        FD_SET(datain,&errfds);
        FD_SET(dataout,&errfds);
        FD_SET(protoin,&errfds);
        FD_SET(protoout,&errfds);
        
        struct timeval tv;
        
        tv.tv_sec  = 0;
        tv.tv_usec = 1000; 
        
        r = select(maxfd + 1, &readfds, &writefds, &errfds, &tv);
        if (r == -1) {
            break;
        }
        
        
        if(doDataIn) {
            if (FD_ISSET(datain, &readfds)) {
                
                if(!p.readyForData()) {
                    std::cerr << "BUG: bad assertion. not ready for data." << std::endl;
                    exit(1);
                }
                n_r = read(datain, buff, sizeof(buff));
                if (n_r <= 0) {
                    break;
                }
                
                out = p.sendData(std::vector<uint8_t>(buff,buff+n_r),now);
                bufferedProtocolData.insert(bufferedProtocolData.end(),out.begin(),out.end());
            }
        }
        
        if (FD_ISSET(protoin, &readfds)) {
            n_r = read(protoin, buff, sizeof(buff));
            if (n_r <= 0) {
                break;
            }
            
            out = std::vector<uint8_t>(buff,buff+n_r);
            
            std::pair<std::vector<uint8_t>,std::vector<uint8_t> > eventRet;
            
            eventRet = p.dataEvent(out,now,true);
            
            bufferedProtocolData.insert(bufferedProtocolData.end(),eventRet.first.begin(),eventRet.first.end());
            
            if(eventRet.second.size()) {
                bufferedData.insert(bufferedData.end(),eventRet.second.begin(),eventRet.second.end());
            }
        }
    
        if(doBufferedOut) {
            if (FD_ISSET(dataout, &writefds)) {
                if(!bufferedData.size()) {
                        std::cerr << "BUG: bad assertion. not for sending buffered data." << std::endl;
                        exit(1);
                }
                
                n_w = write(dataout,&bufferedData.front(),bufferedData.size());
                
                if(n_w <= 0) {
                    break;
                }
                //inefficient due to vector realloc but w.e. , our cpu is faster than data link
                bufferedData.erase(bufferedData.begin(),bufferedData.begin()+n_w);
                
            }
        }
        
        if(doBufferedProtoOut) {
            if (FD_ISSET(protoout, &writefds)) {
                if(!bufferedProtocolData.size()) {
                    std::cerr << "BUG: bad assertion. not for sending protocol buffered data." << std::endl;
                    exit(1);
                }
                
                n_w = write(protoout,&bufferedProtocolData.front(),bufferedProtocolData.size());
                
                if(n_w <= 0) {
                    break;
                }
                
                //inefficient due to vector realloc but w.e. , our cpu is faster than data link
                bufferedProtocolData.erase(bufferedProtocolData.begin(),bufferedProtocolData.begin()+n_w);
            }
        }
        
        if( FD_ISSET(datain,&errfds)
            || FD_ISSET(dataout,&errfds)
            || FD_ISSET(protoin,&errfds)
            || FD_ISSET(protoout,&errfds) ) {
            std::cerr << "Closing - fd error\n";
            break;
        }
        
        out = p.timerEvent(now);
        bufferedProtocolData.insert(bufferedProtocolData.end(),out.begin(),out.end());
    }
    std::cerr << "closing connection\n";
    close(protoout);
    close(protoin);
    
    close(datain);
    close(dataout);
    
    exit(0);
}


int
main(int argc, char *argv[]) {
    
    std::string subcommand;
    
    int opt;
    int server = 0;

    while ((opt = getopt(argc, argv, "s")) != -1) {
        switch (opt) {
        case 's':
            server = 1;
            break;
        default: /* '?' */
            std::cerr << "Bad arguments." << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    
    
    
    int childpid,childin,childout;
    
    Protocol p;
    
    if(!server) {
        subexec(&argv[optind],&childpid,&childin,&childout);
        std::vector<uint8_t> initVec;
        initVec = p.connect(getNow());
        proxy_forever(p,initVec,childout,childin,STDIN_FILENO,STDOUT_FILENO);
    } else {
        int n_r;
        uint8_t buff[4096];
        p.listen();
        std::cerr << "listening for connection.\n";
        while(1) {
            n_r = read(STDIN_FILENO,buff,sizeof(buff));
            if(n_r <= 0) {
                std::cerr << "std in abruptly closed" << std::endl;
                exit(1);
            }
            std::vector<uint8_t> out(buff,buff+n_r);
            
            out = p.dataEvent(out,getNow()).first;
            
            if(p.getState() != STATE_LISTENING) {
                std::cerr << "Connection established\n";
                subexec(&argv[optind],&childpid,&childin,&childout);
                proxy_forever(p,out,STDIN_FILENO,STDOUT_FILENO,childout,childin);
                return 0;
            }
            
        }
        
    }
    return 0;
}
