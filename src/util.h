#ifndef UTIL_H_
#define UTIL_H_

#include <stddef.h>

#define alloc(num, size) (_alloc(num, size, __FILE__, __LINE__))
#define dealloc(ptr) (free(ptr))
#define is_ctrl() (SDL_GetModState() & KMOD_LCTRL || SDL_GetModState() & KMOD_RCTRL)
#define is_shift() (SDL_GetModState() & KMOD_LSHIFT || SDL_GetModState() & KMOD_RSHIFT)
#define is_alt() (SDL_GetModState() & KMOD_LALT || SDL_GetModState() & KMOD_RALT)

int sign(int n);
float lerp(float a, float b, float n);
float damp(float a, float b, float smooth, float dt); /* Frame-rate indepdendent damping. */

void *_alloc(size_t num, size_t size, char *file, int line);
void _dealloc(void *ptr);

void remove_directory(char *dst, char *src);
void isolate_directory(char *dst, char *src);
void get_cwd(char *dst);
int string_begins_with(const char *a, const char *b);
int is_directory(const char *path);
char *stristr(const char *str1, const char *str2);
int determine_tabs_indent_method(const char *str);

#endif /* UTIL_H_ */
