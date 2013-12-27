#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>

/* this program disables echo and sets the baud rate on the specified tty
   it then redirects all stdin into the tty and gets all its stdout from the tty 
 */

static struct termios orig_termios;  /* TERMinal I/O Structure */
static int toreset = -1;

int tty_reset() {
    /* flush and reset */
    
    if(toreset != -1) {
        if (tcsetattr(toreset,TCSAFLUSH,&orig_termios) < 0) 
            return -1;
    }
    return 0;
}


/* exit handler for tty reset */
void tty_atexit() {
    tty_reset();
}




void sig_handler(int sig) {
    tty_reset();
    exit(1);
}



static int max(int x,int y) {
    if(x > y)
        return x;
    return y;
}


static int write_all(int fd, uint8_t buff[],unsigned int sz) {
    
    uint8_t * p = buff;
    
    int amnt;
    
    while(sz) {
        amnt = write(fd,p,sz);
        if(amnt <= 0) {
            return -1;
        }
        
        p += amnt;
        sz -= amnt;
    }
    
    return 0;
}


struct SpeedTab_e {
    char * str; 
    speed_t speed;
} ;

static struct SpeedTab_e bauds[] = {
    {"300",B300},	
    {"600",B600},	
    {"1200",B1200},	
    {"1800",B1800},	
    {"2400",B2400},
    {"4800",B4800},
    {"9600",B9600},
    {"19200",B19200},
    {"38400",B38400},
    {"57600",B57600},
    {"115200",B115200},
    {"230400",B230400},
    {"460800",B460800},
    {"500000",B500000},
    {"576000",B576000},
    {"921600",B921600},
    {"1000000",B1000000},
    {"1152000",B1152000},
    {"1500000",B1500000},
    {"2000000",B2000000},
    {NULL, 0}
};


static
speed_t str2speed(char * s) {
    struct SpeedTab_e * e = bauds;
    while(e->str) {
        if(strcmp(e->str,s) == 0) {
            return e->speed;
        }
        
        e++;
    }
    return 0;
}


int 
main (int argc, char *argv[])
{
    
    int err;
    int n_r;
    
    if(argc < 3) {
        fprintf(stderr,"Bad args\n");
        exit(1);
    }

    int serport = open(argv[1],O_RDWR);
    
    if(serport < 0) {
        perror("cannot open serial port!");
        exit(1);
    }
    

    /* check that input is from a tty */
    if (! isatty(serport)) {
        fprintf(stderr,"%s not on a tty!",argv[1]);
        exit(1);
    }

    /* store current tty settings in orig_termios */
    if (tcgetattr(serport,&orig_termios) < 0) {
        perror("can't get tty settings");
        exit(1);
    }

    /* register the tty reset with the exit handler */
    if (atexit(tty_atexit) != 0) {
        perror("atexit: can't register tty reset");
        exit(1);
    }
    
    toreset = serport;
    
    signal(SIGINT, sig_handler);
    
    struct termios newTermios;
    
    
    newTermios = orig_termios;
    
    newTermios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    
    speed_t s = str2speed(argv[2]);
    
    if(s == 0) {
        fprintf(stderr,"invalid baud rate %s\n",argv[2]);
        exit(1);
    }
    
    if(cfsetspeed(&newTermios,s) < 0) {
        perror("cant set port speed");
        exit(1);
    }
    
    if (tcsetattr(serport,TCSANOW,&newTermios) < 0) {
        perror("can't set new tty settings");
        exit(1);
    }
    
    uint8_t buff[4096];
    
    for(;;) {
        fd_set readfds;
        
        FD_ZERO(&readfds);
        
        FD_SET(STDIN_FILENO,&readfds);
        FD_SET(serport,&readfds);
        
        int maxfd = max(STDIN_FILENO,serport);
        
        err = select(maxfd+1,&readfds,NULL,NULL,NULL);
        
        if(err < 0) {
            break;
        }
        
        if(FD_ISSET(STDIN_FILENO,&readfds)) {
            n_r = read(STDIN_FILENO,buff,sizeof(buff));
            if(n_r <= 0) {
                break;
            }
            
            if(write_all(serport,buff,n_r) != 0) {
                break;
            }
        }
        
        if(FD_ISSET(serport,&readfds)) {
            n_r = read(serport,buff,sizeof(buff));
            if(n_r <= 0) {
                break;
            }
            
            if(write_all(STDOUT_FILENO,buff,n_r) != 0) {
                break;
            }
        }
        
        
    }
    
    close(serport);
    return 0;
}
