#include "highlight.h"

#include "globals.h"

int animated_highlights_active = 0;

void highlight_draw(struct Highlight hl, int xoff, int yoff) {
    struct SDL_Rect r = { xoff + hl.pos * font_w,  yoff, hl.len * font_w, font_h };
    SDL_SetRenderDrawColor(renderer, 0, 127, 255, (Uint8)(255.0 * (hl.time / hl.total_time)));
    SDL_RenderFillRect(renderer, &r);
}

void highlight_set(struct Highlight *hl, int pos, int len) {
    hl->total_time = 1.0;
    hl->time = hl->total_time;
    hl->pos = pos;
    hl->len = len;
    hl->active = 1;
    animated_highlights_active++;
}

void highlight_update(struct Highlight *hl) {
    if (!hl->active) return;

    if (hl->time <= 0) {
        hl->time = 0;
        hl->active = 0;
        animated_highlights_active--;
    } else {
        hl->time -= dt/1000.0;
    }
}