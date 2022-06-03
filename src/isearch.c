#include "isearch.h"

#include "globals.h"
#include "mark.h"

void buffer_isearch_goto_matching(struct Buffer *buf, char *str) {
    struct Line *line;

    strcpy(buf->search->str, str);

    for (line = buf->point.line; line; line = line->next) {
        int start = 0;
        if (line == buf->point.line) {
            start = buf->point.pos+1;
            if (start >= line->len) continue;
        }

        char *match = strstr(line->str + start, str);
        if (match) {
            printf("Occurence at Line %d and Character %d... [%s].\n", line->y+1, (int) (match - line->str), match);
            buf->point.line = line;
            buf->point.pos = match - line->str;

            highlight_set(&line->hl, buf->point.pos, strlen(str));

            int pos = buf->point.line->y*3 + buf->point.line->y*font_h;
            if (pos < -font_h-buf->scroll.y || pos > window_height-buf->scroll.y-font_h*2) {
                buf->scroll.target_y = -font_h+(window_height-font_h*2)-(3*buf->point.line->y + buf->point.line->y * font_h);
            }
            break;
        }
    }
}