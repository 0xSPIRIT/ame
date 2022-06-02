#include "util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

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

float damp(float a, float b, float smooth, float dt) {
    return lerp(a, b, 1-pow(smooth, dt/1000.));
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
}

void get_cwd(char *dst) {
    getcwd(dst, 200);
    int len = strlen(dst);
    dst[len] = '\\';
    dst[len+1] = 0;
}

int string_begins_with(const char *a, const char *b) {
   if(strncmp(a, b, strlen(b)) == 0) return 1;
   return 0;
}