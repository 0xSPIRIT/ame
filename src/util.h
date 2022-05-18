#ifndef UTIL_H_
#define UTIL_H_

#include <stddef.h>

#define alloc(num, size) (_alloc(num, size, __FILE__, __LINE__))
#define is_ctrl() (SDL_GetModState() == KMOD_LCTRL || SDL_GetModState() == KMOD_RCTRL)

int sign(int n);
float lerp(float a, float b, float n);
void *_alloc(size_t num, size_t size, char *file, int line);
void remove_directory(char *dst, char *src);

#endif /* UTIL_H_ */