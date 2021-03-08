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

void parseArgs(int argc, char **argv, size_t *bSize, char **path);

int main(int argc, char **argv) {

    size_t bSize = 0;
    char *path = NULL;

    parseArgs(argc, argv, &bSize, &path);

    if(bSize == 0 || path == NULL) {
        fprintf(stderr, "ERROR: nalezy podac wielkosc i lokalizacje pliku wynikowego\n");
        return 2;
    }

    errno = 0;

    int file = open(path, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);
    if(file == -1) {
        if(errno == EEXIST) {
            printf("ERROR: istnieje plik o podanej nazwie\n");
            return 3;
        }    
        else {
            printf("ERROR: blad funkcji open()\n");
            return 3;
        }
    }

    ftruncate(file, bSize);


    return 0;
}


void parseArgs(int argc, char **argv, size_t *bSize, char **path) {
    int c;
    char *endptr;

    while ((c = getopt (argc, argv, "-r:")) != -1)
        switch(c) {
            case 'r':
                errno = 0;
                *bSize = strtol(optarg, &endptr, 0);
                if( !((*optarg != '\0') && (*endptr == '\0') && !(errno == ERANGE && (*bSize == LONG_MAX || *bSize == LONG_MIN))) ) {
                    fprintf(stderr, "ERROR: niepoprawna wartosc parametru -r\n");
                    exit(1);
                }
                *bSize *= 8*1024; // -r podawany w jednostkach 8kB
                break;

            default:
                if(*path == NULL)
                    *path = argv[optind - 1];
                else {
                    fprintf(stderr, "ERROR: nieoczekiwany parametr %s\n", argv[optind - 1]);
                    exit(1);
                }
        }
}