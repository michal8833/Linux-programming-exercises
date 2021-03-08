#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <stdbool.h>
#include <signal.h>

void argumenty(int argc, char ** argv);
void pierwszyEtap();
void skorygowanie_w_k(size_t dlugoscKomunikatu);
void drugiEtap();
void stworzINastawBudzik();
void formatDatyICzasu(char *buf, struct timespec *ts);

void SIGCONT_handl( int sig, siginfo_t * info, void * data );
void reg_handler_potomek();

void SIGCHLD_handl( int sig, siginfo_t * info, void * data );
void SIGWINCH_handl( int sig, siginfo_t * info, void * data );
void reg_handler_rodzic();

pid_t pidPotomka;
struct timespec znacznikCzasu;
long roznicaMiedzyZnacznikami = -1;

float T; //decysekundy
int p;
int w;
int k;

const char * RESTPOS = "\e[u";
const char * JUMPPOS = "\e[%d;%df";
const char * FMT[] = {"\e[0m", "\e[1;31m", "\e[1;32m"};

const size_t dlugoscKomunikatu = strlen("YYY-MM-DD HH:MM:SS.s") + 1;


int main(int argc, char ** argv) {

    argumenty(argc, argv);

    pierwszyEtap();

    pid_t pid = fork();

    struct timespec ts;
    struct timespec ts2 = {.tv_sec=0, .tv_nsec=300000000};

    char napis[dlugoscKomunikatu];

    switch(pid) {
        case -1:
            perror("fork");

        case 0:

            reg_handler_potomek();
            stworzINastawBudzik();

            while(1) {
                clock_gettime(CLOCK_REALTIME, &ts);
                formatDatyICzasu(napis, &ts);
                skorygowanie_w_k(dlugoscKomunikatu);
                printf(JUMPPOS, w+1, k+4);
                printf("%s%s\n", FMT[2], napis);

                nanosleep(&ts2, NULL); 
            }
            
            return 0;

        default:

            pidPotomka = pid;
            drugiEtap();
            break;
    }

    while(roznicaMiedzyZnacznikami == -1);

    printf(JUMPPOS, w, k);
    printf("%sSmierc potomka. Roznica miedzy znacznikami: %ldns\n", FMT[1], roznicaMiedzyZnacznikami);
    fputs(RESTPOS, stdout);
    puts(FMT[0]);
    fflush(stdout);

    return 0;
}




void pierwszyEtap() {
    struct timespec ts;
    char napis[100];
    size_t dlugoscNapisu;

    for(int i=p; i>0; i--) {
        ts.tv_sec = T/10;
        ts.tv_nsec=(long)((T/10)*1000000000)%1000000000;
        nanosleep(&ts, NULL);
        
        sprintf(napis, "%sOdliczanie #%d   do konca: %d interwalow", FMT[1], -(i-p) + 1, i-1);
        dlugoscNapisu = strlen(napis);
        skorygowanie_w_k(dlugoscNapisu);
        printf(JUMPPOS, w, k);
        printf("%s\n", napis);
    }

    // wyczyszczenie miejsca wypisania komunikatow
    
    for(int i=0;i<dlugoscNapisu;i++)
        napis[i] = ' ';
    skorygowanie_w_k(dlugoscNapisu);
    printf(JUMPPOS, w, k);
    printf("%s\n", napis);
}

void drugiEtap() {
    reg_handler_rodzic();
}

void stworzINastawBudzik() {
    struct sigevent se = {.sigev_notify=SIGEV_SIGNAL, .sigev_signo=SIGSTOP};
    timer_t budzik;

    timer_create(CLOCK_REALTIME, &se, &budzik);

    struct timespec tm = {.tv_sec=T/10, .tv_nsec=(long)((T/10)*1000000000)%1000000000};
    struct itimerspec tspec = {.it_interval=tm, .it_value=tm};
    timer_settime(budzik, 0, &tspec, NULL);
}

void SIGCONT_handl( int sig, siginfo_t * info, void * data ) {
    static unsigned liczbaSIGCONT = 0;
    liczbaSIGCONT += 1;

    if(liczbaSIGCONT == p) {
        exit(0);
    }

    stworzINastawBudzik();
}

void reg_handler_potomek() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = SIGCONT_handl;
    if( sigaction(SIGCONT, &sa, NULL) == -1 ) {
        perror("sigaction");
        exit(9);
    }
}

void SIGCHLD_handl( int sig, siginfo_t * info, void * data ) {
    if(info->si_code == CLD_EXITED) { //proces potomny zakonczony
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        roznicaMiedzyZnacznikami = (ts.tv_sec * 1000000000) + ts.tv_nsec - ((znacznikCzasu.tv_sec * 1000000000) + znacznikCzasu.tv_nsec);
    }
    else if(info->si_status == SIGSTOP) { //proces potomny zatrzymany
        struct timespec ts = {.tv_sec=1, .tv_nsec=0};
        nanosleep(&ts, NULL);

        kill(pidPotomka, SIGCONT);
    }
    else if(info->si_status == SIGCONT) { //proces potomny wznowiony
        clock_gettime(CLOCK_REALTIME, &znacznikCzasu); 
    }
}

void SIGWINCH_handl( int sig, siginfo_t * info, void * data ) {
    skorygowanie_w_k(dlugoscKomunikatu);
}

void reg_handler_rodzic() {
    struct sigaction sa1;
    sigemptyset(&sa1.sa_mask);

    sa1.sa_flags = SA_SIGINFO | SA_NOCLDWAIT;
    sa1.sa_sigaction = SIGCHLD_handl;
    if( sigaction(SIGCHLD, &sa1, NULL) == -1 ) {
        perror("sigaction");
        exit(9);
    }

    struct sigaction sa2;
    sigemptyset(&sa2.sa_mask);

    sa2.sa_flags = SA_SIGINFO;
    sa2.sa_sigaction = SIGWINCH_handl;
    if( sigaction(SIGWINCH, &sa2, NULL) == -1 ) {
        perror("sigaction");
        exit(9);
    }
}

void argumenty(int argc, char ** argv) {
    if(argc != 2) {
        printf("Niepoprawna liczba argumentow.\n");
        exit(1);
    }

    char *endptr;
    char *ptr = argv[1];

    if(*ptr != '\0') {
        T = strtof(ptr, &endptr);
        if( *endptr == '\0' || T < 0 ) {
            fprintf(stderr, "ERROR: niepoprawna wartosc parametru\n");
            exit(1);
        }

        ptr = endptr;

        if(*ptr != ':') {
            fprintf(stderr, "ERROR: niepoprawna wartosc parametru\n");
            exit(1);
        }

        ptr++;

        p = strtol(ptr, &endptr, 0);
        if( *endptr == '\0' || p < 0 ) {
            fprintf(stderr, "ERROR: niepoprawna wartosc parametru\n");
            exit(1);
        }

        ptr = endptr;

        if(*ptr != ':') {
            fprintf(stderr, "ERROR: niepoprawna wartosc parametru\n");
            exit(1);
        }

        ptr++;

        w = strtol(ptr, &endptr, 0);
        if( *endptr == '\0' || w < 0 ) {
            fprintf(stderr, "ERROR: niepoprawna wartosc parametru\n");
            exit(1);
        }

        ptr = endptr;

        if(*ptr != ',') {
            fprintf(stderr, "ERROR: niepoprawna wartosc parametru\n");
            exit(1);
        }
        
        ptr++;

        k = strtol(ptr, &endptr, 0);
        if( !((*ptr != '\0') && (*endptr == '\0')) || k < 0 ) {
            fprintf(stderr, "ERROR: niepoprawna wartosc parametru\n");
            exit(1);
        }
    }
}

void skorygowanie_w_k(size_t dlugoscKomunikatu) {
    struct winsize sz;
    ioctl(1, TIOCGWINSZ, &sz);
    if(k+dlugoscKomunikatu > sz.ws_col)
        k -= (dlugoscKomunikatu - (sz.ws_col-k));
    if(w > sz.ws_row)
        w = sz.ws_row - 2;
}

void formatDatyICzasu(char *buf, struct timespec *ts) {
    struct tm t;

    if(localtime_r(&(ts->tv_sec), &t) == NULL)
        exit(1);

    sprintf(buf, "%d-%d-%d %d:%d:%d.%ld", t.tm_year+1900, t.tm_mon+1, t.tm_yday+1, t.tm_hour, t.tm_min, t.tm_sec, ts->tv_nsec/100000000);
}
