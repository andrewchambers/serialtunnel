#include <string>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <signal.h>
#include <time.h>

#include "protocol.h"



/* Code from ncat
 * Split a command line into an array suitable for handing to execv.
 *
 * A note on syntax: words are split on whitespace and '\' escapes characters.
 * '\\' will show up as '\' and '\ ' will leave a space, combining two
 * words.  Examples:
 * "ncat\ experiment -l -k" will be parsed as the following tokens:
 * "ncat experiment", "-l", "-k".
 * "ncat\\ -l -k" will be parsed as "ncat\", "-l", "-k"
 * See the test program, test/test-cmdline-split to see additional cases.
 */
static char **cmdline_split(const char *cmdexec)
{
    const char *ptr;
    char *cur_arg, **cmd_args;
    int max_tokens = 0, arg_idx = 0, ptr_idx = 0;

    /* Figure out the maximum number of tokens needed */
    ptr = cmdexec;
    while (*ptr) {
        // Find the start of the token
        while (('\0' != *ptr) && isspace((int) (unsigned char) *ptr))
            ptr++;
        if ('\0' == *ptr)
            break;
        max_tokens++;
        // Find the start of the whitespace again
        while (('\0' != *ptr) && !isspace((int) (unsigned char) *ptr))
            ptr++;
    }

    /* The line is not empty so we've got something to deal with */
    cmd_args = (char **) malloc(sizeof(char *) * (max_tokens + 1));
    cur_arg = (char *) calloc(sizeof(char), strlen(cmdexec));
    
    if(!cmd_args || !cur_arg) {
        std::cerr << "Out of memory. terminating." << std::endl;
        exit(1);
    }

    /* Get and copy the tokens */
    ptr = cmdexec;
    while (*ptr) {
        while (('\0' != *ptr) && isspace((int) (unsigned char) *ptr))
            ptr++;
        if ('\0' == *ptr)
            break;

        while (('\0' != *ptr) && !isspace((int) (unsigned char) *ptr)) {
            if ('\\' == *ptr) {
                ptr++;
                if ('\0' == *ptr)
                    break;

                cur_arg[ptr_idx] = *ptr;
                ptr_idx++;
                ptr++;

                if ('\\' != *(ptr - 1)) {
                    while (('\0' != *ptr) && isspace((int) (unsigned char) *ptr))
                        ptr++;
                }
            } else {
                cur_arg[ptr_idx] = *ptr;
                ptr_idx++;
                ptr++;
            }
        }
        cur_arg[ptr_idx] = '\0';

        cmd_args[arg_idx] = strdup(cur_arg);
        cur_arg[0] = '\0';
        ptr_idx = 0;
        arg_idx++;
    }

    cmd_args[arg_idx] = NULL;

    /* Clean up */
    free(cur_arg);

    return cmd_args;
}

//more code borrowed from ncat
void subexec(const char *cmdexec,int * childpid, int * childin,int * childout) {
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
        
        char ** cmdline;
        cmdline = cmdline_split(cmdexec);
        execv(cmdline[0],cmdline);
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
        int r, n_r,n_w;
        now = getNow();
        if (p.getState() == STATE_UNINIT) {
            std::cerr << "Connection terminated." << std::endl;
            break;
        }

        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        
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
        
        struct timeval tv;
        
        tv.tv_sec  = 0;
        tv.tv_usec = 1000; 
        
        r = select(maxfd + 1, &readfds, &writefds, NULL, &tv);
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
            
            eventRet = p.dataEvent(out,now);
            
            bufferedProtocolData.insert(bufferedProtocolData.end(),eventRet.first.begin(),eventRet.first.end());
            bufferedData.insert(bufferedData.end(),eventRet.second.begin(),eventRet.second.end());
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
        
        out = p.timerEvent(now);
        bufferedProtocolData.insert(bufferedProtocolData.end(),out.begin(),out.end());
        
    }
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

    while ((opt = getopt(argc, argv, "sc:")) != -1) {
        switch (opt) {
        case 'c':
            subcommand = optarg;
            break;
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
        subexec(subcommand.c_str(),&childpid,&childin,&childout);
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
                subexec(subcommand.c_str(),&childpid,&childin,&childout);
                proxy_forever(p,out,STDIN_FILENO,STDOUT_FILENO,childin,childout);
                return 0;
            }
            
        }
        
    }
    return 0;
}
