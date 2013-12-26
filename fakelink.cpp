#include <unistd.h>
#include <sys/select.h>
#include <stdint.h>
#include <cstdlib>
#include <string>
#include <stdio.h>
#include <iostream>



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
        perror(cmdexec[0]);
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


static int write_loop(int fd, uint8_t *buf, uint32_t size)
{
    uint8_t *p;
    int n;

    p = buf;
    while (p - buf < size) {
        n = write(fd, p, size - (p - buf));
        if (n == -1) {
            break;
        }
        p += n;
    }
    if((p - buf) > size) {
        return -1;
    }
    return 0;
}


void doCorruption(uint8_t * data,uint32_t size,double errRate) {
    for(uint32_t i = 0; i < size; i++) {
        double r = (double)rand();
        if(r < (errRate * RAND_MAX)) {
            data[i] = rand();            
        }
    }
}

int main(int argc, char * argv[]) {
    
    int opt;
    bool corrupt = false;
    double err_rate;
    
    uint32_t bps;
    bool speedlimit = false;    
    
    while ((opt = getopt(argc, argv, "e:s:")) != -1) {
        switch (opt) {
        case 'e':
            corrupt = true;
            err_rate = atof(optarg);
            break;
        case 's':
            speedlimit = true;
            bps = atoi(optarg); 
            break;
        default: /* '?' */
            std::cerr << "Bad arguments." << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    
    
    int childpid,childin,childout;
    
    subexec(&argv[optind],&childpid,&childin,&childout);
    
    uint8_t buff[64]; //deliberately small so we can control speeds finely
    
    int maxfd = (STDIN_FILENO > childout) ? STDIN_FILENO : childout;
    
    for (;;) {
        
        fd_set fds;
        int r, n_r;

        FD_ZERO(&fds);
        FD_SET(childout, &fds);
        FD_SET(STDIN_FILENO, &fds);

        r = select(maxfd + 1, &fds, NULL, NULL, NULL);
        
        if(r < 0) {
            break;
        }
        
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            n_r = read(STDIN_FILENO, buff, sizeof(buff));
            if (n_r <= 0) {
                break;
            }
            
            
            if(corrupt) {
                doCorruption(buff,n_r,err_rate);
            }
            
            if(speedlimit) {
                usleep((n_r * 1000000)  / bps);
            }
            
            if(write_loop(childin, buff, n_r) != 0) {
                break;
            }
        }
        if (FD_ISSET(childout, &fds)) {
            n_r = read(childout, buff, sizeof(buff));
            if (n_r <= 0) {
                break;
            }
            
            if(corrupt) {
                doCorruption(buff,n_r,err_rate);
            }
            
            if(speedlimit) {
                usleep((n_r * 1000000)  / bps);
            }
            
            if(write_loop(STDOUT_FILENO, buff, n_r) != 0) {
                break;
            }
        }
    }
    
}
