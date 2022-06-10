#include "replace.h"

#include <string.h>

#include "util.h"
#include "globals.h"

char find[1024] = {0};
char replace[1024] = {0};

void buffer_replace_matching(struct Buffer *buf, char *find, char *replace) {
    struct Line *line;

    unsigned find_len = strlen(find);
    unsigned replace_len = strlen(replace);
    
    if (!find_len) return;

    for (line = buf->point.line; line; line = line->next) {
        int start = 0;
        if (line == buf->point.line) {
            start = buf->point.pos+1;
            if (start >= line->len) continue;
        }

        while (start < line->len) {
            char *match = stristr(line->str + start, find);
            if (match) {
                buf->point.line = line;
                buf->point.pos = match - line->str;
                start = buf->point.pos+1;
                
                line_delete_chars_range(line, buf->point.pos, buf->point.pos + find_len);
                line_type_string(line, buf->point.pos, replace);
                    
                highlight_set(&line->hls[line->hl_count], line, (SDL_Color){0, 64, 127, 255}, buf->point.pos, replace_len, true);
    
                int pos = buf->point.line->y*3 + buf->point.line->y*font_h;
                if (pos < -font_h-buf->scroll.y || pos > window_height-buf->scroll.y-font_h*2) {
                    buf->scroll.target_y = -font_h+(window_height/2 - font_h*2)-(3*buf->point.line->y + buf->point.line->y * font_h);
                }
            } else {
                start++;
            }
        }
    }
}
