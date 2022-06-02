#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

extern TTF_Font *font;
extern SDL_Window *window;
extern SDL_Renderer *renderer;

extern int window_width, window_height;
extern int font_w, font_h;

extern double dt; /* Difference in milliseconds between this frame and the last. */

#endif /* GLOBALS_H_ */