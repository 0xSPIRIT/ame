#include "util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int sign(int n) {
    if (n < 0) {
        return -1;
    } else if (n > 0) {
        return 1;
    }
    return 0;
}

float lerp(float a, float b, float n) {
    return a + n*(b-a);
}

void *_alloc(size_t num, size_t size, char *file, int line) {
    void *ptr = calloc(num, size);
    if (!ptr) {
        fprintf(stderr, "Memory allocation error in file %s and line %d!\nAborting...\n", file, line);
        exit(1);
    }
    return ptr;
}

void remove_directory(char *dst, char *src) {
    int start = strlen(src)-1;
    while (src[start] != '\\' && src[start] != '/') {
        start--;
        if (start < 0) {
            strcpy(dst, src); return;
        }
    }
    start++;
    strcpy(dst, src + start);
}

void isolate_directory(char *dst, char *src) {
    int start = strlen(src)-1;
    while (src[start] != '\\' && src[start] != '/') {
        start--;
        if (start < 0) {
            strcpy(dst, src); return;
        }
    }
    start++;
    
    strncpy(dst, src, start);
    puts(dst);
}

void get_cwd(char *dst) {
    getcwd(dst, 200);
    int len = strlen(dst);
    dst[len] = '\\';
    dst[len+1] = 0;
}