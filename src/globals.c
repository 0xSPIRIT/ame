#include "globals.h"

TTF_Font *font;
SDL_Window *window;
SDL_Renderer *renderer;

int window_width = 900, window_height = 600;
int font_w, font_h;

Uint32 mouse;
int mx, my;

double dt;

const SDL_Color BG = {0, 0, 0, 255};
const SDL_Color POINT = {255, 255, 255, 255};
