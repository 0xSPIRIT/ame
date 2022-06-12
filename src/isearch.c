#include "isearch.h"

#include "globals.h"
#include "mark.h"
#include "util.h"

void buffer_isearch_goto_matching(struct Buffer *buf, char *str) {
    struct Line *line;

    if (!strlen(str)) return;

    strcpy(buf->search->str, str);

    /* Destroy previous highlights so we can update it properly. */
    for (line = buf->point.line; line; line = line->next) {
        int i, c = line->hl_count;
        for (i = 0; i < c; i++)
            highlight_stop(&line->hls[i]);
    }

    for (line = buf->point.line; line; line = line->next) {
        int start = 0;
        if (line == buf->point.line) {
            start = buf->point.pos+1;
            if (start >= line->len) continue;
        }

        while (start < line->len) {
            char *match = stristr(line->str + start, str);
            if (match) {
                buf->point.line = line;
                buf->point.pos = match - line->str;
                start = buf->point.pos+1;
    
                highlight_set(&line->hls[line->hl_count], line, (SDL_Color){0, 64, 127, 255}, buf->point.pos, strlen(str), true);
    
                int pos = buf->point.line->y*3 + buf->point.line->y*font_h;
                if (pos < -font_h-buf->scroll.y || pos > window_height-buf->scroll.y-font_h*2) {
                    buf->scroll.target_y = -font_h+(window_height/2 - font_h*2)-(3*buf->point.line->y + buf->point.line->y * font_h);
                }
                goto end_of_buffer_isearch_goto_matching;
            } else {
                start++;
            }
        }
    }
  end_of_buffer_isearch_goto_matching:
    if (buf->point.pos > buf->point.line->len) {
        buf->point.pos = buf->point.line->len;
    }
}

void buffer_isearch_mark_matching(struct Buffer *buf, char *str) {
    struct Line *line;
    struct Point temp = buf->point;
    bool first = true;

    if (!strlen(str)) return;

    strcpy(buf->search->str, str);

    /* Destroy previous marks so we can update it properly. */
    for (line = buf->point.line; line; line = line->next) {
        int i, c = line->hl_count;
        for (i = 0; i < c; i++)
            highlight_stop(&line->hls[i]);
    }

    for (line = buf->point.line; line; line = line->next) {
        int start = 0;
        SDL_Color col;

        if (line == buf->point.line) {
            start = buf->point.pos+1;
            if (start >= line->len) continue;
        }
        while (start < line->len) {
            char *match = stristr(line->str + start, str);
            if (match) {
                if (first) {
                    col = (SDL_Color){202, 127, 235, 255};
                    first = false;
    
                    /* If the first one is offscreen, center the screen onto it. */
                    int pos = line->y*3 + line->y*font_h;
                    if (pos < -font_h-buf->scroll.y || pos > window_height-buf->scroll.y-font_h*2) {
                        buf->scroll.target_y = -font_h+(window_height/2 - font_h*2)-(3*line->y + line->y * font_h);
                    }
                } else {
                    col = (SDL_Color){0, 127, 255, 255};
                }
    
                buf->point.line = line;
                buf->point.pos = match - line->str;
                start = buf->point.pos+1;
    
                highlight_set(&line->hls[line->hl_count], line, col, buf->point.pos, strlen(str), false);
            } else {
                start++;
            }
        }
    }

    buf->point = temp;
}
