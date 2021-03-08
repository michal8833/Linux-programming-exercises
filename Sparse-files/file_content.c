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

void parseArgs(int argc, char **argv, int *file, off_t *fileSize, bool *specialParameterIsSet, bool *pathGiven);
void printBlockInformation(unsigned char value, unsigned long blockSize);
void gainAndPrintBlockInformation(unsigned char *firstCharInBlock, unsigned long *appearances, unsigned char *field);

void printFileContent(int file, off_t fileSize, unsigned char *firstCharInBlock, unsigned long *appearances, unsigned char *field);
void printFileContentAndReportHoles(int file, off_t fileSize, unsigned int *totalBytesRead, unsigned char *firstCharInBlock, unsigned long *appearances, unsigned char *field);

void errorHandler(char *functionName);

int main(int argc, char **argv) {

    int file = -1;
    off_t fileSize;

    bool specialParameterIsSet = false;
    bool pathGiven = false;

    parseArgs(argc, argv, &file, &fileSize, &specialParameterIsSet, &pathGiven);

    if(file == -1) {
        fprintf(stderr, "ERROR: nalezy podac sciezke do pliku\n");
        return 1;
    }

    unsigned int totalBytesRead = 0;
    unsigned char firstCharInBlock;
    unsigned long appearances = 0;
    unsigned char *field = (unsigned char*)malloc(sizeof(unsigned char));

    if(lseek(file, 0, SEEK_SET) == -1) errorHandler("lseek");
    
    if(specialParameterIsSet) {
        printFileContent(file, fileSize, &firstCharInBlock, &appearances, field);
    }
    else {
       printFileContentAndReportHoles(file, fileSize, &totalBytesRead, &firstCharInBlock, &appearances, field);
    }


    return 0;
}


void parseArgs(int argc, char **argv, int *file, off_t *fileSize, bool *specialParameterIsSet, bool *pathGiven) {
    int c;

    while ((c = getopt (argc, argv, "-!::")) != -1)
        switch(c) {
            case '!':
                *specialParameterIsSet = true;

                if(optarg != 0) {
                    fprintf(stderr, "ERROR: parametr -! nie przyjmuje zadnej wartosci\n");
                    exit(1);
                }
                break;
            default:
                if(*pathGiven) {
                    fprintf(stderr, "ERROR: nalezy podac jedna sciezke do pliku\n");
                    exit(1);
                }

                *pathGiven = true;

                *file = open(optarg, O_RDONLY);
                if(*file == -1) errorHandler("open");
                if(lseek(*file, 0, SEEK_SET) == -1) errorHandler("lseek");
                *fileSize = lseek(*file, 0, SEEK_END);
                if(lseek(*file, 0, SEEK_SET) == -1 || *fileSize == -1) errorHandler("lseek");
        }
}

void printBlockInformation(unsigned char value, unsigned long blockSize) {
    if(value >= 32 && value <= 126) // znaki drukowalne
        printf("%c %lu\n", value, blockSize);
    else
        printf("%d %lu\n", value, blockSize);
}

void gainAndPrintBlockInformation(unsigned char *firstCharInBlock, unsigned long *appearances, unsigned char *field) {
    if(*appearances == 0) {
        *firstCharInBlock = *field;
        *appearances = 1;
    }
    else {
        if(*field == *firstCharInBlock) {
            (*appearances)++;
        }
        else {
            // wypisanie informacji o bloku jednakowych wartości
            printBlockInformation(*firstCharInBlock, *appearances);

            *firstCharInBlock = *field;
            *appearances = 1;
        }
    }
}

void printFileContent(int file, off_t fileSize, unsigned char *firstCharInBlock, unsigned long *appearances, unsigned char *field) {
    for(int i=0;i<fileSize;i++) {
        int readd = read(file, field, sizeof(char));
        if(readd == -1) errorHandler("read");
        
        gainAndPrintBlockInformation(firstCharInBlock, appearances, field);
    }

    // wypisanie informacji o bloku jednakowych wartości
    printBlockInformation(*firstCharInBlock, *appearances);
}

void printFileContentAndReportHoles(int file, off_t fileSize, unsigned int *totalBytesRead, unsigned char *firstCharInBlock, unsigned long *appearances, unsigned char *field) {
     while(*totalBytesRead < fileSize) {
        errno = 0;
        off_t res = lseek(file, *totalBytesRead, SEEK_DATA);
        if(res == -1 && errno == ENXIO) { 
            break;
        }

        if(res != *totalBytesRead) { // wykryto dziure
            //wypisanie informacji o poprzednim bloku
            if(*appearances != 0) {
                printBlockInformation(*firstCharInBlock, *appearances);
            }

            //wypisanie informacji o dziurze
            printf("<NULL> %lu\n", res - *totalBytesRead);
            *totalBytesRead += res - *totalBytesRead;
            *appearances = 0;
        }

        int readd = read(file, field, sizeof(char));
        if(readd == -1) errorHandler("read");

        *totalBytesRead += readd;

        gainAndPrintBlockInformation(firstCharInBlock, appearances, field);
    }

    //wypisanie informacji o poprzednim bloku
    if(*appearances != 0) {
        printBlockInformation(*firstCharInBlock, *appearances);
    }

    //wypisanie informacji o ewentualnej dziurze na końcu pliku
    *appearances = 0;
    while(*totalBytesRead < fileSize) {
        int readd = read(file, field, sizeof(char));
        if(readd == -1) errorHandler("read");
        *totalBytesRead += readd;
        (*appearances)++;
    }
    
    if(*appearances != 0)
        printf("<NULL> %lu\n", *appearances);
}

void errorHandler(char *functionName) {
    fprintf(stderr, "ERROR: blad funkcji %s\n", functionName);
    exit(1);
}
