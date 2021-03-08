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

bool deskryptorySaPoprawne(int deskryptory[1024], int strumienKontrolny, short liczbaDeskryptorow);
void parametry(int argc, char**argv, double *D_WAIT, unsigned *T_READ, unsigned *C_READ, int *strumienKontrolny, int deskryptory[1024], short *liczbaDeskryptorow);


int main(int argc, char **argv) {
    
    double D_WAIT = 0.5;
    unsigned T_READ = 3;
    unsigned C_READ = 16;
    int strumienKontrolny;
    int deskryptory[1024];
    short liczbaDeskryptorow = 0;

    parametry(argc, argv, &D_WAIT, &T_READ, &C_READ, &strumienKontrolny, deskryptory, &liczbaDeskryptorow);

    int *liczbaCalkowita = (int*)malloc(sizeof(int)); // liczba wysylana do strumienia kontrolnego
    int bajtowWPotoku;

    char bufor[C_READ];

    double czasMiedzyTestami = D_WAIT / 10.0;
    struct timespec tm;
    tm.tv_sec = czasMiedzyTestami;
    tm.tv_nsec = ((long)(czasMiedzyTestami*1000000000)) % 1000000000;

    bool oprozniany[liczbaDeskryptorow];
    for(int i=0;i<liczbaDeskryptorow;i++)
        oprozniany[i] = false;

    if(!deskryptorySaPoprawne(deskryptory, strumienKontrolny, liczbaDeskryptorow)) {
        fprintf(stderr, "Deskryptory niepoprawne\n");
        return 1;
    }
    
    while(1) {
        for(int d=0;d<liczbaDeskryptorow;d++) {
            if(!deskryptorySaPoprawne(deskryptory, strumienKontrolny, liczbaDeskryptorow)) {
                fprintf(stderr, "Deskryptory niepoprawne\n");
                return 1;
            }

            unsigned pojemnoscPotoku = fcntl(deskryptory[d],F_GETPIPE_SZ);
            
            if(ioctl(deskryptory[d], FIONREAD, &bajtowWPotoku)==-1) {
                fprintf(stderr,"ioctl() error\n");
                return 1;
            }

            if(bajtowWPotoku == pojemnoscPotoku) { // potok calkowicie zapelniony
                oprozniany[d] = true;
                int readd = read(deskryptory[d], liczbaCalkowita, sizeof(int));
                bajtowWPotoku -=readd;
                if(write(strumienKontrolny, liczbaCalkowita, sizeof(int)) == -1) { // proces opróżniania jest anonsowany przez wysłanie jednej liczby całkowitej do strumienia kontrolnego
                    fprintf(stderr,"write() error\n");
                    return 1;
                }
            }

            if(oprozniany[d]) {
                for(int odczyt=0;odczyt<T_READ;odczyt++) {
                    if(bajtowWPotoku > 0) {
                        int readd = read(deskryptory[d], bufor, sizeof(bufor));
                        if(readd == -1) {
                            fprintf(stderr,"read() error\n");
                            return 1;
                        }
                        bajtowWPotoku -=readd;
                    }
                    else {
                        oprozniany[d] = false;
                        break;
                    }
                }
            }
        }

        nanosleep(&tm,NULL);
    }
    

    return 0;
}



bool deskryptorySaPoprawne(int deskryptory[1024], int strumienKontrolny, short liczbaDeskryptorow) {
    if(fcntl(strumienKontrolny,F_GETFL) == -1)
        return false;

    unsigned liczbaZamknietych = 0;

    for(int i = 0; i < liczbaDeskryptorow; i++)
        if(fcntl(deskryptory[i],F_GETFL) == -1)
            liczbaZamknietych++;

    if(liczbaZamknietych == liczbaDeskryptorow)
        return false;

    return true;
}


void parametry(int argc, char**argv, double *D_WAIT, unsigned *T_READ, unsigned *C_READ, int *strumienKontrolny, int deskryptory[1024], short *liczbaDeskryptorow) {
    char *dWait = (char*)malloc(20*sizeof(char));
    char *tRead = (char*)malloc(20*sizeof(char));
    char *cRead = (char*)malloc(20*sizeof(char));
    if(!dWait || !tRead || !cRead) {
        fprintf(stderr,"malloc() error\n");
        exit(1);
    }

    dWait = getenv("D_WAIT");
    tRead = getenv("T_READ");
    cRead = getenv("C_READ");

    char *endptr;

    if(dWait != NULL) {
        errno = 0;
        *D_WAIT = strtod(dWait, &endptr);
        if( !((*dWait != '\0') && (*endptr == '\0')) || *D_WAIT < 0.25 || *D_WAIT > 1.5 ) {
            *D_WAIT = 0.5;
        }
    }

    if(tRead != NULL) {
        errno = 0;
        *T_READ = strtol(tRead, &endptr, 0);
        if( !((*tRead != '\0') && (*endptr == '\0')) || *T_READ <= 0 ) {
            *T_READ = 3;
        }
    }

    if(cRead != NULL) {
        errno = 0;
        *C_READ = strtol(cRead, &endptr, 0);
        if( !((*cRead != '\0') && (*endptr == '\0')) || *C_READ <= 4 ) {
            *C_READ = 16;
        }
    }

    int c;
    int fd;
    struct stat *status = (struct stat*)malloc(sizeof(struct stat));
    if(!status) {
        fprintf(stderr,"malloc() error\n");
        exit(1);
    }
    int res;
    
    while ((c = getopt (argc, argv, "-c:")) != -1)
        switch(c) {
            case 'c':
                errno = 0;
                *strumienKontrolny = strtol(optarg, &endptr, 0);
                if( !((*optarg != '\0') && (*endptr == '\0')) || (*strumienKontrolny < 0) ) {
                    fprintf(stderr, "ERROR: niepoprawna wartosc parametru -c\n");
                    exit(1);
                }

                if((res = fcntl(*strumienKontrolny,F_GETFL)) != -1) {
                    fstat(*strumienKontrolny, status);

                    if( !((((res & O_ACCMODE) == 1) || ((res & O_ACCMODE) == 2)) && S_ISFIFO(status->st_mode)) ) {
                        fprintf(stderr, "ERROR: niepoprawna wartosc parametru -c\n");
                        exit(1);
                    }
                }
            
                break;

            default:
                if(*liczbaDeskryptorow < 1024) {
                    
                    errno = 0;
                    fd = strtol(argv[optind - 1], &endptr, 0);
                    if( !((*argv[optind - 1] != '\0') && (*endptr == '\0')) || (*strumienKontrolny < 0) ) {
                        fprintf(stderr, "ERROR: niepoprawna wartosc parametru pozycyjnego\n");
                        exit(1);
                    }

                    // sprawdzenie czy deskryptor jest fifo lub pipe i czy jest otwarty do odczytu

                    if((res = fcntl(fd,F_GETFL)) != -1) {
                        fstat(fd, status);

                        if( (((res & O_ACCMODE) == 0) || ((res & O_ACCMODE) == 2)) && S_ISFIFO(status->st_mode) ) {
                            deskryptory[(*liczbaDeskryptorow)++] = fd;
                        }
                    }
                }
        }
}
