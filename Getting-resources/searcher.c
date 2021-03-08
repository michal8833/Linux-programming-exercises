#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

void parametry(int argc, char **argv, char **z, char **s);
void zdobywanieZasobow(short *k, char *nazwyUtworzonychPlikow[90], char *z, char *s);
void zarejestrowanieFunkcjiZmieniajacychNazwy(short k, char *nazwyUtworzonychPlikow[90], char *z, char* nazwaProcesu);
void ustanawianieWlasnosci(int status, void *arg);
void error(char *nazwaFunkcji);

struct NazwyPlikow {
    char *staraNazwa;
    char *nowaNazwa;
};


int main(int argc, char **argv) {

    char *z = (char*)malloc(20*sizeof(char));
    char *s = (char*)malloc(20*sizeof(char));
    if(!z || !s) error("malloc");

    char *nazwaProcesu = argv[0];
    
    parametry(argc, argv, &z, &s);

    char *bufor = (char*)malloc(sizeof(char));
    if(!bufor) error("malloc");

    if(read(4, bufor, sizeof(char)) == -1) error("read"); // potomek czeka na start wyścigu

    short k = 1;
    char *nazwyUtworzonychPlikow[90];
    
    zdobywanieZasobow(&k, nazwyUtworzonychPlikow, z, s); 

    zarejestrowanieFunkcjiZmieniajacychNazwy(k, nazwyUtworzonychPlikow, z, nazwaProcesu);

    free(z);
    free(s);
    free(bufor);

    return 0;
}


void zdobywanieZasobow(short *k, char *nazwyUtworzonychPlikow[90], char *z, char *s) {
    int fd;
    struct timespec times[2];

    for(int i=10;i<100;i++) {
        char *nazwaPliku = (char*)malloc(30*sizeof(char));
        if(!nazwaPliku) error("malloc");

        sprintf(nazwaPliku, "property_0%d.%smine", i, z);

        errno = 0;
        int fifo = open(s, O_WRONLY | O_NONBLOCK); // proces sprawdza, czy nadal ma przyzwolenie na działanie
        if( fifo != -1 ) { 
            close(fifo);
            exit(0);
        }
        else if(fifo == -1 && errno != ENXIO)
            error("open");

        errno = 0;
        fd = open(nazwaPliku, O_RDWR | O_CREAT | O_EXCL, 0222);
        if(fd != -1) { // plik nie istnieje
            clock_gettime(CLOCK_REALTIME, &times[0]);
            times[1].tv_sec = times[0].tv_sec;
            times[1].tv_nsec = times[0].tv_nsec;
            utimensat(AT_FDCWD, nazwaPliku, times, 0); // ustawienie ostatniego czasu dostępu i modyfikcji pliku na obecny czas

            ftruncate(fd, (*k)*384); // nadanie plikowi rozmiaru

            nazwyUtworzonychPlikow[(*k)-1] = nazwaPliku;
            (*k)++;

            close(fd);
        }
        else if(fd == -1 && errno != EEXIST)
            error("open");
    }
}

void zarejestrowanieFunkcjiZmieniajacychNazwy(short k, char *nazwyUtworzonychPlikow[90], char *z, char* nazwaProcesu) {
    for(int i=0;i<k-1;i++) {
        struct NazwyPlikow *nazwy = (struct NazwyPlikow*)malloc(sizeof(struct NazwyPlikow));
        if(!nazwy) error("malloc");

        nazwy->staraNazwa = (char*)malloc(30*sizeof(char));
        nazwy->nowaNazwa = (char*)malloc(30*sizeof(char));
        if(!nazwy->staraNazwa || !nazwy->nowaNazwa) error("malloc");

        strcpy(nazwy->staraNazwa, nazwyUtworzonychPlikow[i]);
        sprintf(nazwy->nowaNazwa, "%s_%smine.#%d", nazwaProcesu, z, i+1);

        on_exit(ustanawianieWlasnosci, nazwy);
    }
}

void ustanawianieWlasnosci(int status, void *arg) {
    struct NazwyPlikow *nazwy = (struct NazwyPlikow*)arg;

    if(rename(nazwy->staraNazwa, nazwy->nowaNazwa) == -1) {
        fprintf(stderr,"rename() error\n");
    }

    free(nazwy->staraNazwa);
    free(nazwy->nowaNazwa);
    free(nazwy);
}

void parametry(int argc, char **argv, char **z, char **s) {
    int c;

    while ((c = getopt (argc, argv, "-z:s:")) != -1)
        switch(c) {
            case 'z':
                strcpy(*z, optarg);
                break;

            case 's':
                strcpy(*s, optarg);
                break;

            default:
                exit(1);
        }
}

void error(char *nazwaFunkcji) {
    fprintf(stderr,"%s() error\n", nazwaFunkcji);
    exit(1);
}
