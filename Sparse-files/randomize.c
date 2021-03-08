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
#include <stdbool.h>
#include <time.h>

void interpretujArgumentyJakoLiczby(char *optarg, size_t bytesToDistribute, int file, off_t fileSize);
void interpretujArgumentyJakoTekst(char *optarg, size_t bytesToDistribute, int file, off_t fileSize);
void writeByteToRandomField(char **byte, int file, off_t fileSize);

void errorHandler(char *functionName);

int main(int argc, char **argv) {

    int file;
    off_t fileSize;
    bool format; // liczba - true, tekst - false

    bool pathGiven = false;
    bool formatGiven = false;

    size_t bytesToDistribute = 0;

    int c;

    srand(time(NULL));

    while ((c = getopt (argc, argv, "-s:f:")) != -1)
        switch(c) {
            case 's':
                pathGiven = true;

                file = open(optarg, O_RDWR);
                if(file == -1) errorHandler("open");
                fileSize = lseek(file, 0, SEEK_END);
                if(lseek(file, 0, SEEK_SET) == -1 || fileSize == -1) errorHandler("lseek");
                break;

            case 'f':
                formatGiven = true;

                if(strcmp("liczba", optarg) == 0) {
                    format = true;
                }
                else if(strcmp("tekst", optarg) == 0) {
                    format = false;
                }
                else {
                    fprintf(stderr, "ERROR: niepoprawna wartosc parametru -f\n");
                }
                break;

            default:
                    if(!pathGiven || !formatGiven) {
                        fprintf(stderr, "ERROR: nalezy podac wartosci parametrow -s oraz -f\n");
                        return 1;
                    }
                    
                    if(format) { // liczba
                        interpretujArgumentyJakoLiczby(optarg, bytesToDistribute, file, fileSize);
                    }
                    else { // tekst
                        interpretujArgumentyJakoTekst(optarg, bytesToDistribute, file, fileSize);
                    }
        }


    return 0;
}


void writeByteToRandomField(char **byte, int file, off_t fileSize) {
    unsigned int random;
    unsigned char *field = (unsigned char*)malloc(sizeof(unsigned char));

    do{
        random = rand()%(fileSize+1);
        if(lseek(file, random, SEEK_SET) == -1) errorHandler("lseek");
        if(read(file, field, sizeof(field)) == -1) errorHandler("read");
    }while(*field != '\0');

    if(lseek(file, random, SEEK_SET) == -1) errorHandler("lseek");
    int written = write(file, *byte, sizeof(char));
    if(written == -1) errorHandler("write");
}



void interpretujArgumentyJakoLiczby(char *optarg, size_t bytesToDistribute, int file, off_t fileSize) {
    char *endptr;
    long val;

    bytesToDistribute++;
    if(bytesToDistribute > fileSize) {
        fprintf(stderr, "ERROR: ilosc wartosci do rozmieszczenia przekracza rozmiar pliku\n");
        exit(1);
    }

    unsigned char byteToWrite;
    char *bytePtr = (char*)malloc(sizeof(unsigned char));

    char *ptr = optarg;
    if(*ptr == '0' && *(ptr+1) == 'x') { // podstawa 16
        val = strtol(optarg, &endptr, 16);
        if( !((*optarg != '\0') && (*endptr == '\0') && val >= 0 && val <= 255 ) ) {
            fprintf(stderr, "ERROR: wartosc parametru pozycyjnego powinna byc liczba calkowita z zakresu 0-255\n");
            exit(1);
        }

        byteToWrite = (unsigned char)val;
    }
    else if(*ptr == '0') { // podstawa 8
        val = strtol(optarg, &endptr, 8);
        if( !((*optarg != '\0') && (*endptr == '\0') && val >= 0 && val <= 255 ) ) {
            fprintf(stderr, "ERROR: wartosc parametru pozycyjnego powinna byc liczba calkowita z zakresu 0-255\n");
            exit(1);
        }

        byteToWrite = (unsigned char)val;
    }
    else { // podstawa 10
        val = strtol(optarg, &endptr, 0);
        if( !((*optarg != '\0') && (*endptr == '\0') && val >= 0 && val <= 255 ) ) {
            fprintf(stderr, "ERROR: wartosc parametru pozycyjnego powinna byc liczba calkowita z zakresu 0-255\n");
            exit(1);
        }

        byteToWrite = (unsigned char)val;
    }

    if(byteToWrite == 0) {
        fprintf(stderr, "ERROR: program nie akceptuje wartosci 0\n");
        exit(1);
    }

    *bytePtr = byteToWrite;
    
    writeByteToRandomField(&bytePtr, file, fileSize);
}



void interpretujArgumentyJakoTekst(char *optarg, size_t bytesToDistribute, int file, off_t fileSize) {
    char *charPtr = optarg;
                        
    while(*charPtr != '\0') {
        bytesToDistribute++;
        if(bytesToDistribute > fileSize) {
            fprintf(stderr, "ERROR: ilosc wartosci do rozmieszczenia przekracza rozmiar pliku\n");
            exit(1);
        }
        
        writeByteToRandomField(&charPtr, file, fileSize);

        charPtr++;
    }
}

void errorHandler(char *functionName) {
    fprintf(stderr, "ERROR: blad funkcji %s\n", functionName);
    exit(1);
}
