#include "mark.h"

#include <SDL2/SDL.h>

#include "globals.h"
#include "buffer.h"
#include "util.h"

struct Mark *mark_allocate(struct Buffer *buf) {
    struct Mark *mark = alloc(1, sizeof(struct Mark));
    mark->buf = buf;
    return mark;
}

void mark_deallocate(struct Mark *mark) {
    free(mark->start);
    free(mark);
}

void mark_set(struct Mark *mark) {
    if (mark->start) free(mark->start);
    mark->start = alloc(1, sizeof(struct Point));
    memcpy(mark->start, &mark->buf->point, sizeof(mark->buf->point));
    mark->end = &mark->buf->point;
    mark->active = true;
}

void mark_unset(struct Mark *mark) {
    mark->start = mark->end = NULL;
    mark->active = false;
}

char *mark_get_text(struct Mark *mark) {
    struct Line *line;
    char *text = alloc(mark_get_length(mark), sizeof(char));

    mark_swap_ends_if(mark);
    for (line = mark->start->line; line != mark->end->line->next; line = line->next, strcat(text, (char[2]){'\n',0})) {
        int x = 0, e = line->len;
        if (line == mark->start->line) {
            x = mark->start->pos;
            e = line->len;
        }
        if (line == mark->end->line) {
            e = mark->end->pos;
        }
        for (; x < e; x++) {
            strcat(text, (char[2]){line->str[x], 0});
        }
    }
    /* Remove trailing 0. */
    text[strlen(text)-1] = 0;

    return text;
}

int mark_get_length(struct Mark *mark) {
    struct Line *line;
    int len = 0;
    for (line = mark->start->line; line != mark->end->line->next; line = line->next) {
        int x = 0, e = line->len;
        if (line == mark->start->line) {
            x = mark->start->pos;
            e = line->len;
        }
        if (line == mark->end->line) {
            e = mark->end->pos;
        }
        len += e-x + 1; /* +1 for \n */
    }
    return len;
}

void mark_cut_text(struct Mark *mark) {
    mark_swap_ends_if(mark);
    memcpy(&mark->buf->point, mark->end, sizeof(struct Point));
    while (true) {
        buffer_backspace(mark->buf);
        if (mark->buf->point.line == mark->start->line && mark->buf->point.pos == mark->start->pos) {
            break;
        }
    }
}

void mark_draw(struct Mark *mark) {
    struct Line *line;
    int yoff;
    
    if (!mark->start->line || !mark->end->line) return;

    yoff = mark->start->line->y;
    mark_swap_ends_if(mark);

    SDL_SetRenderDrawColor(renderer, 153, 211, 224, 255);
    for (line = mark->start->line; line != mark->end->line->next; line = line->next) {
        int x = 0, w = line->len;

        int pos = yoff*3 + yoff*font_h;
        if (pos < -font_h - mark->buf->scroll.y || pos >= window_height - mark->buf->scroll.y) { /* Culling */
            yoff++;
            continue;
        }
        if (line == mark->start->line) {
            x = mark->start->pos;
            w = line->len - x;
        }
        if (line == mark->end->line) {
            w = mark->end->pos - x;
        }
        SDL_Rect selection = {
            mark->buf->scroll.x + 3 + x * font_w, 
            mark->buf->scroll.y + pos-3,
            w * font_w, font_h+6
        };
        SDL_RenderFillRect(renderer, &selection);
        yoff++;
    }
}

void mark_swap_ends_if(struct Mark *mark) {
    struct Point *temp = mark->start;
    if (mark->start->line->y > mark->end->line->y || 
       (mark->start->line == mark->end->line && mark->start->pos > mark->end->pos)) {
        mark->start = mark->end;
        mark->end = temp;
    }
}