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

void parseArgs(int argc, char **argv, int *file, off_t *fileSize, unsigned int *ante, unsigned int *post, bool *pathGiven, bool *anteGiven, bool *postGiven);

void findSecondValue(int file, unsigned char *field, unsigned char *secondValue);
void findFirstValue(int file, unsigned char *field, off_t holeOffset, unsigned char *firstValue);

void rozrostAnte(int file, off_t holeOffset, size_t holeSize, unsigned ante, unsigned char *value);
void rozrostPost(int file, off_t holeOffset, size_t holeSize, unsigned post, unsigned char *value);
void rozrost(int file, off_t holeOffset, size_t holeSize, unsigned ante, unsigned post, unsigned char *firstValue, unsigned char *secondValue);

void errorHandler(char *functionName);

int main(int argc, char **argv) {

    int file;
    off_t fileSize;

    bool pathGiven = false;
    bool anteGiven = false;
    bool postGiven = false;

    unsigned int ante = 0;
    unsigned int post = 0;

    parseArgs(argc, argv, &file, &fileSize, &ante, &post, &pathGiven, &anteGiven, &postGiven);

    if(!pathGiven || (!anteGiven && !postGiven)) {
        fprintf(stderr, "ERROR: nalezy podac sciezke do pliku i przynajmniej jednen parametr <ekspansja>\n");
        return 3;
    }

    unsigned char *firstValue = (unsigned char*)malloc(sizeof(unsigned char));
    unsigned char *secondValue = (unsigned char*)malloc(sizeof(unsigned char));
    off_t currentOffset;
    off_t holeOffset;
    size_t holeSize;
    unsigned char *field = (unsigned char*)malloc(sizeof(unsigned char));


    // dziura na poczatku pliku
    off_t res = lseek(file, 0, SEEK_DATA);
    if(res == -1) errorHandler("lseek");

    holeOffset = 0;
    holeSize = res;

    findSecondValue(file, field, secondValue); // szukanie wartosci dla rozrostu ante

    rozrostAnte(file, holeOffset, holeSize, ante, secondValue);
    
    // pozostale dziury

    while(1) {
        // szukanie wartosci dla rozrostu post
        errno = 0;
        currentOffset = lseek(file, 0, SEEK_CUR);
        if(currentOffset == -1) errorHandler("lseek");
        holeOffset = lseek(file, currentOffset, SEEK_HOLE);
        if(holeOffset == -1 && errno == ENXIO) {
            break;
        }
        else if(holeOffset == -1) errorHandler("lseek");

        findFirstValue(file, field, holeOffset, firstValue);
        
        // szukanie wartosci dla rozrostu ante
        errno = 0;
        res = lseek(file, holeOffset, SEEK_DATA);
        if(res == -1 && errno == ENXIO) {
            break;
        }
        else if(res == -1) errorHandler("lseek");

        holeSize = res - holeOffset;

        findSecondValue(file, field, secondValue);
        
        // rozrost
        rozrost(file, holeOffset, holeSize, ante, post, firstValue, secondValue); 
    }

    // dziura na koncu pliku
    
    findFirstValue(file, field, holeOffset, firstValue); // szukanie wartosci dla rozrostu post

    currentOffset = lseek(file, 0, SEEK_CUR);
    if(currentOffset == -1) errorHandler("lseek");
    holeOffset = lseek(file, currentOffset, SEEK_HOLE);
    if(holeOffset == -1) errorHandler("lseek");
    holeSize = fileSize - holeOffset;
    rozrostPost(file, holeOffset, holeSize, post, firstValue);
    

    return 0;
}


void rozrostAnte(int file, off_t holeOffset, size_t holeSize, unsigned ante, unsigned char *value) {
    off_t res;
    unsigned int bytesToWrite = ante <= holeSize ? ante : holeSize;
    errno = 0;
    if((res = lseek(file, holeOffset + holeSize - ante, SEEK_SET)) == -1 && errno == EINVAL) return;
    else if(res == -1) errorHandler("lseek");
    for(int f=0;f<bytesToWrite;f++) {
        if(write(file, value, sizeof(char)) == -1) errorHandler("write");
    }
}

void rozrostPost(int file, off_t holeOffset, size_t holeSize, unsigned post, unsigned char *value) {
    off_t res;
    unsigned int bytesToWrite = post <= holeSize ? post : holeSize;
    errno = 0;
    if((res = lseek(file, holeOffset, SEEK_SET)) == -1 && errno == EINVAL) {
        if(lseek(file, holeOffset + holeSize + 1, SEEK_SET) == -1) errorHandler("lseek");
        return;
    }
    else if(res == -1) errorHandler("lseek");
    for(int f=0;f<bytesToWrite;f++) {
        if(write(file, value, sizeof(char)) == -1) errorHandler("write");
    }
    if(lseek(file, holeOffset + holeSize + 1, SEEK_SET) == -1) errorHandler("lseek");
}

void rozrost(int file, off_t holeOffset, size_t holeSize, unsigned ante, unsigned post, unsigned char *firstValue, unsigned char *secondValue) {
    if(ante != 0 && post == 0) { // rozrost ante
        rozrostAnte(file, holeOffset, holeSize, ante, secondValue);
    }
    else if(ante == 0 && post != 0) { // rozrost post
        rozrostPost(file, holeOffset, holeSize, post, firstValue);
    }
    else { // rozrost ambo
        unsigned newAnte, newPost;

        if(ante + post > holeSize) { // nowe obszary zachodzą na siebie
            newAnte = newPost = holeSize / 2;
        }
        else {
            newAnte = ante;
            newPost = post;
        }

        rozrostAnte(file, holeOffset, holeSize, newAnte, secondValue);
        rozrostPost(file, holeOffset, holeSize, newPost, firstValue);
    }
}

void parseArgs(int argc, char **argv, int *file, off_t *fileSize, unsigned int *ante, unsigned int *post, bool *pathGiven, bool *anteGiven, bool *postGiven) {
    char *endptr;
    long val;

    char *ptr;

    int c;

    while ((c = getopt (argc, argv, "-s:")) != -1) {
        switch(c) {
            case 's':
                *pathGiven = true;

                *file = open(optarg, O_RDWR);
                if(*file == -1) errorHandler("open");
                if(lseek(*file, 0, SEEK_SET) == -1) errorHandler("lseek");
                *fileSize = lseek(*file, 0, SEEK_END);
                if(lseek(*file, 0, SEEK_SET) == -1 || *fileSize == -1) errorHandler("lseek");
                break;

            default:
                if(*anteGiven && *postGiven) {
                    fprintf(stderr, "ERROR: kolejny parametr <ekspansja> niedozwolony\n");
                    exit(2);
                }

                ptr = optarg;

                char kierunek[4];
                for(int i=0;i<4;i++) {
                    kierunek[i] = ptr[i];
                }
                
                if(ptr[4] != ':') {
                    fprintf(stderr, "ERROR: nieprawidlowy parametr\n");
                    exit(2);
                }

                ptr += 5;

                val = strtol(ptr, &endptr, 0);

                switch(*endptr) {
                    case '\0':
                    case 'B':
                        break;

                    case 'b':
                        if(*(endptr+1) == 'b')
                            val *= 512;
                        else {
                            fprintf(stderr, "ERROR: nieprawidlowy parametr\n");
                            exit(2);
                        }

                        break;

                    case 'K':
                        val *= 1024;
                        break;

                    default:
                        fprintf(stderr, "ERROR: nieprawidlowy parametr\n");
                        exit(2);
                }


                if(strcmp(kierunek, "ante") == 0) {
                    if(*anteGiven) {
                        fprintf(stderr, "ERROR: kolejny parametr <ekspansja> niedozwolony\n");
                        exit(2);
                    }
                    if(*postGiven && *post == val) {
                        fprintf(stderr, "ERROR: kolejny parametr <ekspansja> niedozwolony\n");
                        exit(2);
                    }
                    *ante = val;
                    *anteGiven = true;
                }
                else if(strcmp(kierunek, "post") == 0) {
                    if(*postGiven) {
                        fprintf(stderr, "ERROR: kolejny parametr <ekspansja> niedozwolony\n");
                        exit(2);
                    }
                    if(*anteGiven && *ante == val) {
                        fprintf(stderr, "ERROR: kolejny parametr <ekspansja> niedozwolony\n");
                        exit(2);
                    }
                    *post = val;
                    *postGiven = true;
                }
                else if(strcmp(kierunek, "ambo") == 0) {
                    *ante = *post = val;
                    *anteGiven = true;
                    *postGiven = true;
                }
                else {
                    fprintf(stderr, "ERROR: niepoprawna wartosc parametru\n");
                    exit(2);
                }
        }
    }
}

void findSecondValue(int file, unsigned char *field, unsigned char *secondValue) {
    do {
        int readd = read(file, field, sizeof(char));
        if(readd == -1) errorHandler("read");
    }while(*field == '\0');

    *secondValue = *field;
}

void findFirstValue(int file, unsigned char *field, off_t holeOffset, unsigned char *firstValue) {
    unsigned i = 1;
        
    do {
        if(lseek(file, holeOffset - i, SEEK_SET) == -1) errorHandler("lseek");
        i++;
        int readd = read(file, field, sizeof(char));
        if(readd == -1) errorHandler("read");
    }while(*field == '\0');

    *firstValue = *field;
}

void errorHandler(char *functionName) {
    fprintf(stderr, "ERROR: blad funkcji %s\n", functionName);
    exit(1);
}
