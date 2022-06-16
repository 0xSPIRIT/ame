#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

extern TTF_Font *font;
extern SDL_Window *window;
extern SDL_Renderer *renderer;

extern Uint32 mouse;
extern int mx, my;

extern int window_width, window_height;
extern int font_w, font_h;

extern double dt; /* Difference in milliseconds between this frame and the last. */

extern const SDL_Color BG, POINT;

#endif /* GLOBALS_H_ */
