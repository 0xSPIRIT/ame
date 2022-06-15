#include "util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <sys/stat.h>

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

static int num_allocs = 0;
void *_alloc(size_t num, size_t size, char *file, int line) {
    void *ptr = calloc(num, size);
    if (!ptr) {
        fprintf(stderr, "Memory allocation error in file %s and line %d!\nAborting...\n", file, line);
        exit(1);
    }
    num_allocs++;
    return ptr;
}

int get_leaked_allocations() {
    return num_allocs;
}

void _dealloc(void *ptr, char *file, int line) {
    free(ptr);
    num_allocs--;
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

/* Taken from https://stackoverflow.com/a/27304609 */
char* stristr(const char* str1, const char* str2) {
    const char* p1 = str1;
    const char* p2 = str2;
    const char* r = *p2 == 0 ? str1 : 0;

    while (*p1 != 0 && *p2 != 0) {
        if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2)) {
            if (r == 0) {
                r = p1;
            }
            p2++;
        } else {
            p2 = str2;
            if (r != 0) {
                p1 = r+1;
            }

            if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2)) {
                r = p1;
                p2++;
            } else {
                r = 0;
            }
        }
        p1++;
    }
    return *p2 == 0 ? (char*)r : 0 ;
}

int is_directory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

/* Check which is more common at the start of lines: tabs or spaces.
 * If the file is longer than a certain amount, just go up to that
 * point.
 */
int determine_tabs_indent_method(const char *str) {
    int indent = 0;
    int is_start_of_line = 1;
    int i = 0;
    while (*str && i < 1500) {
        if (*str == '\n') {
            is_start_of_line = 1;
            ++str; continue;
        }
        if (is_start_of_line) {
            if (*str == '\t')     indent++;
            else if (*str == ' ') indent--;
            is_start_of_line = 0;
        }
        ++str;
        ++i;
    }
    return indent > 0;
}
