#ifndef HIGHLIGHT_H_
#define HIGHLIGHT_H_

#include <stdbool.h>
#include <SDL2/SDL.h>

struct Highlight {
    int active;
    float time, total_time;
    bool is_temp; /* Does this one fade away? */
    int pos, len;
    SDL_Color col;
    struct Line *line;
};

extern int animated_highlights_active;

void highlight_set(struct Highlight *hl, struct Line *line, SDL_Color col, int pos, int len, bool temp);
void highlight_stop(struct Highlight *hl);
void highlight_update(struct Highlight *hl);
void highlight_draw(struct Highlight hl, int xoff, int yoff);

#endif /* HIGHLIGHT_H_ */