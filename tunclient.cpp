#include <string>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <signal.h>
#include <time.h>

#include "protocol.h"
#include "packets.h"



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

//returns 0 on success
static int write_loop(int fd, uint8_t *buf, unsigned int size)
{
    uint8_t  *p;
    int n;
    unsigned int sent = 0;

    p = buf;
    while (sent < size) {
        n = write(fd, p, size - (sent));
        if (n == -1) {
            break;
        }
        sent += n;
        p += n;
    }
    if (sent == size) {
        return 0;
    }
    return 1;
}

uint64_t getNow() {
    struct timespec ts;
    
    if(clock_gettime(CLOCK_REALTIME,&ts)) {
        std::cerr << "error failed to get system time\n.";
        exit(1);
    }
    
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    
    
}

void proxy_forever(int childin, int childout) {
    
    int maxfd = STDIN_FILENO;
    
    maxfd = max(STDIN_FILENO,maxfd);
    maxfd = max(childout,maxfd);
    
    Protocol p;
    PacketBuilder pb;
    
    std::vector<ProtocolPacket> packets;
    std::vector<uint8_t> data;
    
    int64_t now = getNow();
    packets = p.connect(now);
    for(std::vector<ProtocolPacket>::iterator it = packets.begin() ; it != packets.end() ; it++) {
        std::vector<uint8_t> encoded = encodePacket(*it);
        data.insert(data.begin(),encoded.begin(),encoded.end());
    }
    
    uint8_t  buff[4096];
    
    for (;;) {
        fd_set fds;
        int r, n_r;
        now = getNow();
        if (p.getState() == STATE_UNINIT) {
            break;
        }

        FD_ZERO(&fds);
        
        if(p.readyForData()) {
            FD_SET(STDIN_FILENO, &fds);
        }
        
        FD_SET(childout, &fds);
        
        struct timeval tv;
        
        tv.tv_sec  = 0;
        tv.tv_usec = 1000; 
        
        r = select(maxfd + 1, &fds, NULL, NULL, &tv);
        if (r == -1) {
            break;
        }
        
        
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            n_r = read(STDIN_FILENO, buff, sizeof(buff));
            if (n_r <= 0) {
                break;
            }
        }
        
        if(write_loop(childin, &data.front(), data.size()) != 0) {
            break;
        }
        
        data.clear();
        
        if (FD_ISSET(childout, &fds)) {
            n_r = read(childout, buff, sizeof(buff));
            if (n_r <= 0) {
                break;
            }
            
            packets = pb.addData(buff,n_r);
            std::pair<std::vector<ProtocolPacket>,std::vector<uint8_t> > reply;
            for(std::vector<ProtocolPacket>::iterator it = packets.begin() ; it != packets.end() ; it++) {
                reply = p.packetEvent(*it,now,true);
                std::vector<uint8_t> reply_raw = encodePackets(reply.first);
                data.insert(data.end(),reply_raw.begin(),reply_raw.end());
            }
            if (reply.second.size()) {
                if(write_loop(STDOUT_FILENO, &reply.second.front(), reply.second.size()) != 0) {
                    break;
                }
            }
        }
    }
    
    close(childin);
    close(childout);
    
    exit(0);
}


int
main(int argc, char *argv[]) {
    
    std::string subcommand;
    
    int opt;

    while ((opt = getopt(argc, argv, "c:")) != -1) {
        switch (opt) {
        case 'c':
            subcommand = optarg;
            break;
        default: /* '?' */
            std::cerr << "Bad arguments." << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    
    
    
    int childpid,childin,childout;
    subexec(subcommand.c_str(),&childpid,&childin,&childout);
    proxy_forever(childin,childout);
    
    
    return 0;
}
