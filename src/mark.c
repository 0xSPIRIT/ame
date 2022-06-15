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
    dealloc(mark->start);
    dealloc(mark);
}

void mark_set(struct Mark *mark, bool shift_select) {
    if (mark->start) dealloc(mark->start);
    if (mark->end) dealloc(mark->end);
    mark->start = alloc(1, sizeof(struct Point));
    mark->end = alloc(1, sizeof(struct Point));
    
    struct Point *buf_point = &mark->buf->views[mark->buf->curview].point;
    memcpy(mark->start, buf_point, sizeof(*buf_point));
    memcpy(mark->end, buf_point, sizeof(*buf_point));
    mark->active = true;
    mark->shift_select = shift_select;
}

void mark_update(struct Mark *mark) {
    struct Point *buf_point = &mark->buf->views[mark->buf->curview].point;
    if (mark->active) {
        if (mark->swapped)
            memcpy(mark->start, buf_point, sizeof(*buf_point));
        else
            memcpy(mark->end, buf_point, sizeof(*buf_point));
    }
}

void mark_unset(struct Mark *mark) {
    if (mark->start) dealloc(mark->start);
    if (mark->end) dealloc(mark->end);
    mark->start = mark->end = NULL;
    mark->active = false;
    mark->shift_select = false;
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
        len += e-x + 2; /* +2 for \n and \0 */
    }
    return len;
}

void mark_delete_text(struct Mark *mark) {
    struct Point *buf_point = &mark->buf->views[mark->buf->curview].point;
    *buf_point = *mark->end;
    while (true) {
        buffer_backspace(mark->buf);
        if (buf_point->line == mark->start->line && buf_point->pos == mark->start->pos) {
            break;
        }
    }
}

void mark_cut_text(struct Mark *mark) {
    /* Set clipboard text. */
    char *text = mark_get_text(mark);
    SDL_SetClipboardText(text);    

    /* Delete the text in the selected region. */
    mark_delete_text(mark);

    dealloc(text);
}

void mark_draw(struct Mark *mark) {
    struct Line *line;
    int yoff;
    struct ScrollBar *scroll = &mark->buf->views[mark->buf->curview].scroll;
    
    if (!mark->start->line || !mark->end->line) return;

    mark_swap_ends_if(mark);
    yoff = mark->start->line->y;

    SDL_SetRenderDrawColor(renderer, 64, 85, 200, 255);
    for (line = mark->start->line; line != mark->end->line->next; line = line->next) {
        int x = 0, w = line->len;

        int pos = yoff*3 + yoff*font_h;
        if (pos < -font_h - scroll->y || pos >= window_height - scroll->y) { /* Culling */
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
        x += strlen(line->pre_str);

        int tab_offset = 0;
        int tab_width_offset = 0;
        int i;

        if (line == mark->start->line) {
            for (i = 0; i < mark->start->pos; i++) {
                if (line->str[i] == '\t') tab_offset += font_w * (4-1); /* -1 to remove the offset that's already there. */
            }
        } else if (line == mark->end->line) {
            for (i = 0; i < mark->end->pos; i++) {
                if (line->str[i] == '\t') tab_offset += font_w * 4; /* -1 to remove the offset that's already there. */
            }
        } else {
            for (i = 0; i < line->len; i++) {
                if (line->str[i] == '\t') tab_width_offset += font_w * 4;
            }
        }

        SDL_Rect selection = {
            tab_offset + mark->buf->x + scroll->x + 3 + x * font_w, 
            mark->buf->y + scroll->y + pos-3,
            tab_width_offset + w * font_w, font_h+6
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
        mark->swapped = !mark->swapped;
    }
}

void mark_set_if_shift(struct Mark *mark) {
    if (is_shift()) {
        if (!mark->active) mark_set(mark, true);
    } else if (mark->shift_select) {
        mark_unset(mark);
    }
}
