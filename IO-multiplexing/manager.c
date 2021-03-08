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

typedef struct A {
    float D_WAIT;
    int T_READ;
    int C_READ;
}A;

void parametry(int argc, char **argv, int pipes[64], int fifos[64], unsigned *pipesNo, unsigned *fifosNo, A **a, unsigned *aNo, float *opoznienie, float *D_WAIT, int *T_READ, int *C_READ);
void utworzeniePotomkaMuxpmp(float opoznienie);
void utworzeniePotomkaApoc(int strumienKontrolny, int *fds, unsigned fdsNo, A *a, int apoc);


int main(int argc, char **argv) {

    int pipes[64] = {-1};
    int fifos[64] = {-1};

    unsigned pipesNo = 0;
    unsigned fifosNo = 0;
    struct A *a;
    unsigned aNo = 0;

    float opoznienie = 0;
    float D_WAIT;
    int T_READ;
    int C_READ;

    parametry(argc, argv, pipes, fifos, &pipesNo, &fifosNo, &a, &aNo, &opoznienie, &D_WAIT, &T_READ, &C_READ);

    if(mkfifo("fifo0",0777) == -1 && errno != EEXIST) {
        fprintf(stderr, "mkfifo() error\n");
        return 1;
    }

    int strumienKontrolny;
    if((strumienKontrolny = open("fifo0", O_RDWR | O_NONBLOCK)) == -1) {
        fprintf(stderr, "open() error\n");
        return 1;
    }

    int fd;

    int fifoN = 1;

    for(int apoc=0;apoc<aNo;apoc++) {
        int fds[fifos[apoc] + pipes[apoc]];
        unsigned fdsNo = 0;

        for(int f=0;f<fifos[apoc];f++) { // tworze pliki fifo dla danego procesu "apoc"
            errno = 0;

            char *nazwa = (char*)malloc(30*sizeof(char));
            if(!nazwa) {
                fprintf(stderr,"malloc() error\n");
                return 1;
            }

            sprintf(nazwa,"fifo%d", fifoN++);

            if(mkfifo(nazwa,0777) == -1 && errno != EEXIST) {
                fprintf(stderr, "mkfifo() error\n");
                return 1;
            }

            if((fd = open(nazwa, O_RDWR | O_NONBLOCK)) == -1) {
                fprintf(stderr, "open() error\n");
                return 1;
            }

            fds[fdsNo++] = fd;
        }

        for(int f=0;f<pipes[apoc];f++) { // tworze potoki dla danego procesu "apoc"
            int pipeFD[2];
            pipe(pipeFD);

            fds[fdsNo++] = pipeFD[0];
        }

        utworzeniePotomkaApoc(strumienKontrolny, fds, fdsNo, a, apoc);

        for(int i=fifos[apoc];i<fdsNo;i++) { // zamkniecie koncow czytania z potokow
            close(fds[i]);
        }
    }

    utworzeniePotomkaMuxpmp(opoznienie);    


    return 0;
}



void parametry(int argc, char **argv, int pipes[64], int fifos[64], unsigned *pipesNo, unsigned *fifosNo, A **a, unsigned *aNo, float *opoznienie, float *D_WAIT, int *T_READ, int *C_READ) {
    char *endptr;
    long val;
    char *ptr;

    char *PIPES = (char*)malloc(50*sizeof(char));
    char *FIFOS = (char*)malloc(50*sizeof(char));
    if(!PIPES || !FIFOS) {
        fprintf(stderr,"malloc() error\n");
        exit(1);
    }

    PIPES = getenv("PIPES");
    FIFOS = getenv("FIFOS");

    ptr = PIPES;
    while(ptr != NULL) {
        errno = 0;
        val = strtol(ptr, &endptr, 0);
        if( (*endptr != ',' && *endptr != '\0') || val < 0 ) {
            fprintf(stderr, "Nieprawidlowy format zmiennej srodowiskowej PIPES\n");
            exit(1);
        }

        pipes[(*pipesNo)++] = val;

        if(*endptr == ',') {
            ptr = endptr + 1;
        }
        else
            break;
    }

    ptr = FIFOS;
    while(ptr != NULL) {
        errno = 0;
        val = strtol(ptr, &endptr, 0);
        if( (*endptr != ',' && *endptr != '\0') || val < 0 ) {
            fprintf(stderr, "Nieprawidlowy format zmiennej srodowiskowej FIFOS\n");
            exit(1);
        }

        fifos[(*fifosNo)++] = val;

        if(*endptr == ',') {
            ptr = endptr + 1;
        }
        else
            break;
    }

    // uzupelnienie krotszej listy zerami

    if(*pipesNo < *fifosNo) {
        while(*pipesNo != *fifosNo) {
            pipes[(*pipesNo)++] = 0;
        }
    }
    else if(*pipesNo > *fifosNo) {
        while(*pipesNo != *fifosNo) {
            fifos[(*fifosNo)++] = 0;
        }
    }

    *a = (struct A*)malloc(*fifosNo*sizeof(struct A));
    if(!(*a)) {
        fprintf(stderr,"malloc() error\n");
        exit(1);
    }

    int c;

    while ((c = getopt (argc, argv, "-m:a:")) != -1)
        switch(c) {
            case 'm':

                *opoznienie = strtof(optarg, &endptr);
                if( !((*optarg != '\0') && (*endptr == '\0')) || (*opoznienie < 0) ) {
                    fprintf(stderr, "ERROR: niepoprawna wartosc parametru -m\n");
                    exit(1);
                }

                break;

            case 'a':

                *D_WAIT = 0.5;
                *T_READ = 3;
                *C_READ = 16;
                
                ptr = optarg;

                *D_WAIT = strtof(ptr, &endptr);
                if( *ptr == '\0' || *endptr != ',' || val < 0 ) {
                    fprintf(stderr, "ERROR: nieprawidlowa wartosc parametru -a\n");
                    exit(1);
                }

                ptr = endptr + 1;

                *T_READ = strtol(ptr, &endptr, 0);
                if( *ptr == '\0' || *endptr != ',' || val < 0 ) {
                    fprintf(stderr, "ERROR: nieprawidlowa wartosc parametru -a\n");
                    exit(1);
                }

                ptr = endptr + 1;

                *C_READ = strtol(ptr, &endptr, 0);
                if( *ptr == '\0' || *endptr != '\0' || val < 0 ) {
                    fprintf(stderr, "ERROR: nieprawidlowa wartosc parametru -a\n");
                    exit(1);
                }

                (*a)[*aNo].D_WAIT = *D_WAIT;
                (*a)[*aNo].T_READ = *T_READ;
                (*a)[*aNo].C_READ = *C_READ;
            
                (*aNo)++;

                break;

            default:
                fprintf(stderr, "ERROR: niepoprawny parametr\n");
                exit(1);
        }
}

void utworzeniePotomkaMuxpmp(float opoznienie) {
    char** args;

    pid_t pid=fork();

    switch(pid) {
        case -1:
            fprintf(stderr, "fork() error\n");
            exit(1);

        case 0:
            
            args=(char**)malloc((3)*sizeof(char*));
            if(!args) {
                fprintf(stderr,"malloc() error\n");
                exit(1);
            }

            char *paramTimeout = (char*)malloc(20*sizeof(char));
            if(!paramTimeout) {
                fprintf(stderr,"malloc() error\n");
                exit(1);
            }

            sprintf(paramTimeout,"%f",opoznienie);

            args[0] = "./muxpmp";
            args[1] = paramTimeout;
            args[2] = "0<fifo0";
            args[3] = NULL;

            if(execvp("./muxpmp",args)==-1) {
                fprintf(stderr, "execvp() error\n");
                exit(1);
            }

            break;

        default:
            break;
    }
}

void utworzeniePotomkaApoc(int strumienKontrolny, int *fds, unsigned fdsNo, A *a, int apoc) {
    char** args;

    pid_t pid=fork();

    switch(pid) {
        case -1:
            fprintf(stderr, "fork() error\n");
            exit(1);

        case 0:

            args=(char**)malloc((3+fdsNo)*sizeof(char*));
            if(!args) {
                fprintf(stderr,"malloc() error\n");
                exit(1);
            }

            for(int i=0;i<fdsNo;i++) {
                char *param = (char*)malloc(20*sizeof(char));
                sprintf(param,"%d",fds[i]);

                args[i+2] = param;
            }

            char *param = (char*)malloc(20*sizeof(char));
            if(!param) {
                fprintf(stderr,"malloc() error\n");
                exit(1);
            }

            sprintf(param,"-c%d",strumienKontrolny);

            args[0] = "./apoc";
            args[1] = param;
            args[3+fdsNo-1] = NULL;

            // ustawienie zmiennych srodowiskowych

            char *dWait = (char*)malloc(20*sizeof(char));
            char *tRead = (char*)malloc(20*sizeof(char));
            char *cRead = (char*)malloc(20*sizeof(char));
            if(!dWait || !tRead || !cRead) {
                fprintf(stderr,"malloc() error\n");
                exit(1);
            }

            sprintf(dWait, "%f", a[apoc].D_WAIT);
            sprintf(tRead, "%d", a[apoc].T_READ);
            sprintf(cRead, "%d", a[apoc].C_READ);

            setenv("D_WAIT", dWait, 1);
            setenv("T_READ", tRead, 1);
            setenv("C_READ", cRead, 1);

            if(execvp("./apoc",args)==-1) {
                fprintf(stderr, "execvp() error\n");
                exit(1);
            }

            break;

        default:
            break;
    }
}