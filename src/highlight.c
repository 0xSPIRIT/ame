#include "highlight.h"

#include "globals.h"
#include "buffer.h"

int animated_highlights_active = 0;

void highlight_draw(struct Highlight hl, int xoff, int yoff) {
    struct SDL_Rect r = { xoff + hl.pos * font_w,  yoff, hl.len * font_w, font_h };
    Uint8 alpha = 255;
    if (hl.is_temp) {
        alpha = (Uint8)(255.0 * (hl.time / hl.total_time));
    }
    SDL_SetRenderDrawColor(renderer, hl.col.r, hl.col.g, hl.col.b, alpha);
    SDL_RenderFillRect(renderer, &r);
}

void highlight_set(struct Highlight *hl, struct Line *line, SDL_Color col, int pos, int len, bool temp) {
    hl->total_time = 1.0;
    hl->time = hl->total_time;
    hl->is_temp = temp;
    hl->pos = pos;
    hl->len = len;
    hl->col = col;
    hl->active = 1;
    hl->line = line;
    line->hl_count++;
    animated_highlights_active++;
}

void highlight_stop(struct Highlight *hl) {
    hl->line->hl_count--;
    memset(hl, 0, sizeof(*hl));
}

void highlight_update(struct Highlight *hl) {
    if (!hl->active || !hl->is_temp) return;

    if (hl->time <= 0) {
        hl->time = 0;
        hl->active = 0;
        animated_highlights_active--;
        hl->line->hl_count--;
    } else {
        hl->time -= dt/1000.0;
    }
}