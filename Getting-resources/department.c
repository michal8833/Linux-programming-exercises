#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <stdbool.h>

void parametry(int argc, char **argv, double *t, bool *tGiven, unsigned *n, char **z, char **p, char **sciezka);
void stworzeniePotomkow(unsigned n, int pipeFD[2], char *z, char *p, char *sciezka);
void usunNiewlasciwePliki(double czasKoncaWyscigu, char *poczatekNazwyPotomka, char *nazwaZasobu);
bool pasujeDoWzorca(char *nazwaPliku, char *poczatekNazwyPotomka, char *nazwaZasobu);
void error(char *nazwaFunkcji);


int main(int argc, char **argv) {

    struct timespec timespecKoniecWyscigu;
    double czasKoncaWyscigu;

    int pipeFD[2];
    pipe(pipeFD);

    double t;
    bool tGiven = false;
    unsigned n = 16;
    char *z = (char*)malloc(20*sizeof(char));
    char *p = (char*)malloc(20*sizeof(char));
    if(!z || !p) error("malloc");
    *p = *z = '\0';
    char *sciezka = NULL;

    parametry(argc, argv, &t, &tGiven, &n, &z, &p, &sciezka);

    if(!tGiven || *z == '\0' || *p == '\0') {
        fprintf(stderr,"ERROR: nalezy podac wartosci parametrow -t, -z oraz -p\n");
        return 1;
    }

    errno = 0;
    if(mkfifo(sciezka,0777) == -1 && errno != EEXIST) error("mkfifo");

    stworzeniePotomkow(n, pipeFD, z, p, sciezka);

    char *bytes = (char*)malloc(n*sizeof(char));
    if(!bytes) error("malloc");
    for(int i=0;i<n;i++) {
        bytes[i] = 97;
    }

    close(pipeFD[0]);

    if(write(pipeFD[1], bytes, n*sizeof(char)) == -1) error("write"); // rodzic startuje wyścig

    close(pipeFD[1]);

    struct timespec tm;
    tm.tv_sec = t;
    tm.tv_nsec = ((long)(t*1000000000)) % 1000000000;
    nanosleep(&tm,NULL); // czekanie na koniec wyścigu

    clock_gettime(CLOCK_REALTIME, &timespecKoniecWyscigu);

    int fifo;
    if((fifo = open(sciezka, O_RDONLY | O_NONBLOCK)) == -1) error("open"); // otwarcie pliku fifo kończy wyścig

    czasKoncaWyscigu = (timespecKoniecWyscigu.tv_sec * 1000.0) + (timespecKoniecWyscigu.tv_nsec / 1000000.0); // czas w milisekundach

    for(int i=0;i<n;i++) {
        if(wait(NULL) == -1) error("wait");
    }

    usunNiewlasciwePliki(czasKoncaWyscigu, p, z);

    close(fifo);
    free(z);
    free(p);
    free(bytes);

    return 0;
}





void usunNiewlasciwePliki(double czasKoncaWyscigu, char *poczatekNazwyPotomka, char *nazwaZasobu) {
    struct dirent *pDirent;
    DIR *pDir;

    char *zawartoscKatalogu[500];
    unsigned liczbaPlikowWKatalogu = 0;

    struct stat *status = (struct stat*)malloc(sizeof(struct stat));
    if(!status) error("malloc");

    pDir = opendir ("."); // otwieram katalog bieżący po zakończeniu wyścigu
    if(pDir == NULL) error("opendir");

    while((pDirent = readdir(pDir)) != NULL) {
        zawartoscKatalogu[liczbaPlikowWKatalogu++] = pDirent->d_name;
    }

    for(int plik = 0; plik<liczbaPlikowWKatalogu; plik++) {
        // usunięcie plików o nazwach "property*"

        char substr[9];
        memcpy(substr, zawartoscKatalogu[plik], 8);
        substr[8] = '\0';

        if(strcmp("property", substr) == 0) {
            if(remove(zawartoscKatalogu[plik]) == -1) error("remove");
        }

        // usunięcie plików o nazwach pasujących do wzorca "<nazwa_potomka>_<nazwa_zasobu>mine.#<nr>" powstałych po zakończeniu wyścigu

        stat(zawartoscKatalogu[plik], status);
        double czasPowstaniaPliku = (status->st_mtim.tv_sec*1000.0) + (status->st_mtim.tv_nsec/1000000.0); // czas w milisekundach


        if(czasPowstaniaPliku > czasKoncaWyscigu && pasujeDoWzorca(zawartoscKatalogu[plik], poczatekNazwyPotomka, nazwaZasobu))
            if(remove(zawartoscKatalogu[plik]) == -1) error("remove");
    }

    free(status);
}

bool pasujeDoWzorca(char *nazwaPliku, char *poczatekNazwyPotomka, char *nazwaZasobu) {
    unsigned dlugoscPoczatkuNazwyPotomka = strlen(poczatekNazwyPotomka);
    unsigned dlugoscNazwyZasobu = strlen(nazwaZasobu);

    char poczatek_nazwy_potomka[dlugoscPoczatkuNazwyPotomka + 1];
    memcpy(poczatek_nazwy_potomka, nazwaPliku, dlugoscPoczatkuNazwyPotomka);
    poczatek_nazwy_potomka[dlugoscPoczatkuNazwyPotomka] = '\0';

    if(strcmp(poczatekNazwyPotomka, poczatek_nazwy_potomka) == 0) {
        char *charPtr = nazwaPliku + dlugoscPoczatkuNazwyPotomka - 1;
        while(*charPtr != '_')
            charPtr++;
        charPtr++;

        char substr[dlugoscNazwyZasobu + 6 + 1];
        memcpy(substr, nazwaPliku, dlugoscNazwyZasobu + 6);
        substr[dlugoscNazwyZasobu + 6] = '\0';

        char *drugaCzescNazwy = (char*)malloc((dlugoscNazwyZasobu + 6 + 1) * sizeof(char));
        sprintf(drugaCzescNazwy, "%smine.#", nazwaZasobu);

        if(strcmp(substr, drugaCzescNazwy)) {
            char *endptr;
            long val;

            charPtr += dlugoscNazwyZasobu + 6;

            errno = 0;
            val = strtol(charPtr, &endptr, 0);
            if( (*charPtr != '\0') && (*endptr == '\0') && !(errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ) {
                return true;
            } 
        }
    }

    return false;
}

void parametry(int argc, char **argv, double *t, bool *tGiven, unsigned *n, char **z, char **p, char **sciezka) {
    int c;
    char *endptr;

    while ((c = getopt (argc, argv, "-t:n:z:p:")) != -1)
        switch(c) {
            case 't':
                errno = 0;
                *t = strtod(optarg, &endptr);
                if( !((*optarg != '\0') && (*endptr == '\0')) || *t < 0 ) {
                    fprintf(stderr, "ERROR: niepoprawna wartosc parametru -t\n");
                    exit(1);
                }

                *tGiven = true;
            
                break;

            case 'n':
                errno = 0;
                *n = strtol(optarg, &endptr, 0);
                if( !((*optarg != '\0') && (*endptr == '\0')) || (*n <= 4) ) {
                    fprintf(stderr, "ERROR: niepoprawna wartosc parametru -n\n");
                    exit(1);
                }
            
                break;
            
            case 'z':
                strcpy(*z, optarg);
                break;

            case 'p':
                strcpy(*p, optarg);
                break;

            default:
                if(*sciezka == NULL)
                    *sciezka = argv[optind - 1];
                else {
                    fprintf(stderr, "ERROR: nieoczekiwany parametr %s\n", argv[optind - 1]);
                    exit(1);
                }
        }

    if(*sciezka == NULL) { // ustawienie domyślnej wartości parametru "ścieżka"
        srand(time(NULL));
        char *nazwaPliku = (char*)malloc(30*sizeof(char));
        if(!nazwaPliku) error("malloc");

        while(1) {
            sprintf(nazwaPliku, "/tmp/fifo%d", rand());
            int fd = open(nazwaPliku, O_RDWR | O_CREAT | O_EXCL, 0222);
            if(fd != -1) { // plik nie istnieje, więc został utworzony
                if(remove(nazwaPliku) == -1) error("remove"); // usuwam plik i wiem, że w katalogu /tmp nie ma pliku o nazwie "nazwaPliku"
                
                *sciezka = nazwaPliku;
                break;
            }
        }
    }
}

void stworzeniePotomkow(unsigned n, int pipeFD[2], char *z, char *p, char *sciezka) {
 for(int potomek=0;potomek<n;potomek++) {
        char** args;

        pid_t pid=fork();

        switch(pid) {
            case -1:
                error("fork");

            case 0:
                close(pipeFD[1]);

                if(dup2(pipeFD[0],4) == -1) error("dup2");

                close(pipeFD[0]);
                
                args=(char**)malloc(4*sizeof(char*));

                char *paramZ = (char*)malloc(20*sizeof(char));
                char *paramS = (char*)malloc(20*sizeof(char));
                char *nazwaProcesu = (char*)malloc(20*sizeof(char));

                if(!args || !paramZ || !paramS || !nazwaProcesu) error("malloc");

                sprintf(paramZ, "-z%s", z);
                sprintf(paramS, "-s%s", sciezka);
                sprintf(nazwaProcesu, "%s%d", p, potomek);

                args[0] = nazwaProcesu;
                args[1] = paramZ;
                args[2] = paramS;
                args[3] = NULL;

                if(execvp("./poszukiwacz",args)==-1) error("execvp");

                break;

            default:
                break;
        }
    }
}

void error(char *nazwaFunkcji) {
    fprintf(stderr,"%s() error\n", nazwaFunkcji);
    exit(1);
}