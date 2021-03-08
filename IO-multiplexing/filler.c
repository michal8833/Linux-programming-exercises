#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <poll.h>

bool deskryptorySaPoprawne(int deskryptory[1024], short liczbaDeskryptorow);
void parametry(int argc, char **argv, double *MUX_DELAY, float *timeout, int d[1024], short *liczbaDeskryptorow);
void odnalezieniePoprawnychDeskryptorow(int deksryptory[1024], short liczbaDeskryptorow, short *liczbaWlasciwychDeskryptorow);
void strumienieGotowe(int res, short liczbaWlasciwychDeskryptorow, struct pollfd *deskryptory, int *bufor);


int main(int argc, char **argv) {

    double MUX_DELAY = 7.5; // centysekundy
    float timeout; // milisekundy
    int d[1024];
    short liczbaDeskryptorow = 1024;
    short liczbaWlasciwychDeskryptorow = 0;
    int res;

    parametry(argc, argv, &MUX_DELAY, &timeout, d, &liczbaDeskryptorow);

    odnalezieniePoprawnychDeskryptorow(d, liczbaDeskryptorow, &liczbaWlasciwychDeskryptorow);

    if(!deskryptorySaPoprawne(d, liczbaWlasciwychDeskryptorow)) {
        fprintf(stderr, "Deskryptory niepoprawne\n");
        return 1;
    }

    int *bufor = (int*)malloc(sizeof(int));
    if(!bufor) {
        fprintf(stderr,"malloc() error\n");
        return 1;
    }

    int bajtowWPotoku;

    double przerwaMiedzyObiegami = MUX_DELAY / 100.0;
    struct timespec tm;
    tm.tv_sec = przerwaMiedzyObiegami;
    tm.tv_nsec = ((long)(przerwaMiedzyObiegami*1000000000)) % 1000000000;

    struct pollfd deskryptory[liczbaWlasciwychDeskryptorow];
    deskryptory[0].fd = 0;
    deskryptory[0].events = POLLIN;
    for(int i = 1; i < liczbaWlasciwychDeskryptorow; i++) {
        deskryptory[i].fd = d[i-1];
        deskryptory[i].events = POLLOUT;
    }

    while(1) {
        if(!deskryptorySaPoprawne(d, liczbaWlasciwychDeskryptorow)) {
            fprintf(stderr, "Deskryptory niepoprawne\n");
            return 1;
        }

        res = poll(deskryptory, liczbaWlasciwychDeskryptorow, timeout);

        if(res > 0) {
            strumienieGotowe(res, liczbaWlasciwychDeskryptorow, deskryptory, bufor);
        }
        else if(res == 0) {
            for(int i=1;i<liczbaWlasciwychDeskryptorow;i++) {
                if(ioctl(deskryptory[i].fd, FIONREAD, &bajtowWPotoku)==-1) {
                    fprintf(stderr,"ioctl() error\n");
                    return 1;
                }

                // przywrocenie strumienia do obslugi
                if(bajtowWPotoku == 0) {
                    deskryptory[i].fd = d[i-1];
                }
            }
        }

        nanosleep(&tm,NULL);
    }

    return 0;
}



bool deskryptorySaPoprawne(int deskryptory[1024], short liczbaDeskryptorow) {
    for(int i = 0; i < liczbaDeskryptorow; i++)
        if(fcntl(deskryptory[i],F_GETFL) == -1)
            return false;

    return true;
}


void parametry(int argc, char **argv, double *MUX_DELAY, float *timeout, int d[1024], short *liczbaDeskryptorow) {
    char *muxDelay = (char*)malloc(20*sizeof(char));
    if(!muxDelay) {
        fprintf(stderr,"malloc() error\n");
        exit(1);
    }

    muxDelay = getenv("MUX_DELAY");

    char *endptr;

    if(muxDelay != NULL) {
        errno = 0;
        *MUX_DELAY = strtod(muxDelay, &endptr);
        if( !((*muxDelay != '\0') && (*endptr == '\0')) || *MUX_DELAY < 0 ) {
            *MUX_DELAY = 7.5;
        }
    }

    if(argc < 2 || argc > 3) {
        fprintf(stderr, "Nieprawidlowa liczba parametrow\n");
        exit(1);
    }

    errno = 0;
    strtol(argv[1], &endptr, 0);
    if(*endptr == '.') {
        *timeout = strtof(argv[1], &endptr);
        if( !((*argv[1] != '\0') && (*endptr == '\0')) || *timeout < 0 ) {
            fprintf(stderr, "Niepoprawny parametr\n");
            exit(1);
        }
        *timeout *= 125; // wartosc podawana w jednostkach 1/8s, zmienna timeout okresla liczbe milisekund

        if(argc == 3) {
            *liczbaDeskryptorow = strtod(argv[2], &endptr);
            if( !((*argv[1] != '\0') && (*endptr == '\0')) || *liczbaDeskryptorow < 0 ) {
                *liczbaDeskryptorow = 1024;
            }
        }
    }
    else {
        *liczbaDeskryptorow = strtod(argv[1], &endptr);
        if( !((*argv[1] != '\0') && (*endptr == '\0')) || *liczbaDeskryptorow < 0 ) {
            *liczbaDeskryptorow = 1024;
        }

        *timeout = strtof(argv[2], &endptr);
        if( !((*argv[1] != '\0') && (*endptr == '\0')) || *timeout < 0 ) {
            fprintf(stderr, "Niepoprawny parametr\n");
            exit(1);
        }
        *timeout *= 125; // wartosc podawana w jednostkach 1/8s, zmienna timeout okresla liczbe milisekund
    }
}

void odnalezieniePoprawnychDeskryptorow(int deksryptory[1024], short liczbaDeskryptorow, short *liczbaWlasciwychDeskryptorow) {
    int res;

    for(int fd = 3; fd < liczbaDeskryptorow+3; fd++) {
        struct stat *status = (struct stat*)malloc(sizeof(struct stat));
        if(!status) {
            fprintf(stderr,"malloc() error\n");
            exit(1);
        }

        fstat(fd, status);


        if((res = fcntl(fd,F_GETFL)) != -1) {
            if( (((res & O_ACCMODE) == 1) || ((res & O_ACCMODE) == 2)) && S_ISFIFO(status->st_mode) ) {
                deksryptory[(*liczbaWlasciwychDeskryptorow)++] = fd;
            }
            else
                close(fd);
        }
    }
}

void strumienieGotowe(int res, short liczbaWlasciwychDeskryptorow, struct pollfd *deskryptory, int *bufor) {

    for(int i=0;i<liczbaWlasciwychDeskryptorow;i++) {
        if((deskryptory[i].revents & POLLERR) != 0 || (deskryptory[i].revents & POLLHUP) != 0 || (deskryptory[i].revents & POLLNVAL) != 0) {
            fprintf(stderr, "Blad deskryptora\n");
            exit(1);
        }
        
        if((deskryptory[i].revents & POLLOUT) != 0) {
            *bufor = deskryptory[i].fd;
            if(write(deskryptory[i].fd, bufor, sizeof(int)) == -1) {
                fprintf(stderr, "write() error\n");
                exit(1);
            }
        }

        if(((deskryptory[i].revents & POLLIN) != 0) && res == 1) { // strumien kontrolny jest gotowy do uzycia i nie mozna wykonac operacji zapisu
            if(read(deskryptory[i].fd, bufor, sizeof(int)) == -1) {
                fprintf(stderr, "read() error\n");
                exit(1);
            }

            // wylaczenie deskryptora z obslugi
            for(int i=0;i<3;i++) {
                if(deskryptory[i].fd == *bufor)
                    deskryptory[i].fd = -1;
            }

        }
    }
}
