#include "isearch.h"

#include "globals.h"
#include "mark.h"
#include "util.h"

void buffer_isearch_goto_matching(struct Buffer *buf, char *str) {
    struct Line *line;
    struct Isearch *search = buf->views[buf->curview].search;
    struct Point *point = &buf->views[buf->curview].point;
    struct ScrollBar *scroll = &buf->views[buf->curview].scroll;

    if (!strlen(str)) return;

    strcpy(search->str, str);

    /* Destroy previous highlights so we can update it properly. */
    for (line = point->line; line; line = line->next) {
        int i, c = line->hl_count;
        for (i = 0; i < c; i++)
            highlight_stop(&line->hls[i]);
    }

    for (line = point->line; line; line = line->next) {
        int start = 0;
        if (line == point->line) {
            start = point->pos+1;
            if (start >= line->len) continue;
        }

        while (start < line->len) {
            char *match = stristr(line->str + start, str);
            if (match) {
                point->line = line;
                point->pos = match - line->str;
                start = point->pos+1;
    
                highlight_set(&line->hls[line->hl_count], line, (SDL_Color){0, 64, 127, 255}, point->pos, strlen(str), true);
    
                int pos = point->line->y*SPACING + point->line->y*font_h;
                if (pos < -font_h-scroll->y || pos > window_height-scroll->y-font_h*2) {
                    scroll->target_y = -font_h+(window_height/2 - font_h*2)-(SPACING*point->line->y + point->line->y * font_h);
                }
                goto end_of_buffer_isearch_goto_matching;
            } else {
                start++;
            }
        }
    }
  end_of_buffer_isearch_goto_matching:
    if (point->pos > point->line->len) {
        point->pos = point->line->len;
    }
}

void buffer_isearch_mark_matching(struct Buffer *buf, char *str) {
    struct Line *line;

    struct Isearch *search = buf->views[buf->curview].search;
    struct Point *point = &buf->views[buf->curview].point;
    struct ScrollBar *scroll = &buf->views[buf->curview].scroll;
    
    struct Point temp = *point;
    bool first = true;

    if (!strlen(str)) return;

    strcpy(search->str, str);

    /* Destroy previous marks so we can update it properly. */
    for (line = point->line; line; line = line->next) {
        int i, c = line->hl_count;
        for (i = 0; i < c; i++)
            highlight_stop(&line->hls[i]);
    }

    for (line = point->line; line; line = line->next) {
        int start = 0;
        SDL_Color col;

        if (line == point->line) {
            start = point->pos+1;
            if (start >= line->len) continue;
        }
        while (start < line->len) {
            char *match = stristr(line->str + start, str);
            if (match) {
                if (first) {
                    col = (SDL_Color){202, 127, 235, 255};
                    first = false;
    
                    /* If the first one is offscreen, center the screen onto it. */
                    int pos = line->y*SPACING + line->y*font_h;
                    if (pos < -font_h-scroll->y || pos > window_height-scroll->y-font_h*2) {
                        scroll->target_y = -font_h+(window_height/2 - font_h*2)-(SPACING*line->y + line->y * font_h);
                    }
                } else {
                    col = (SDL_Color){0, 127, 255, 255};
                }
    
                point->line = line;
                point->pos = match - line->str;
                start = point->pos+1;
    
                highlight_set(&line->hls[line->hl_count], line, col, point->pos, strlen(str), false);
            } else {
                start++;
            }
        }
    }

    *point = temp;
}
