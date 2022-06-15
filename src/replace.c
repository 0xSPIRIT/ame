#include "replace.h"

#include <string.h>

#include "util.h"
#include "globals.h"

char find[1024] = {0};
char replace[1024] = {0};

int buffer_replace_matching(struct Buffer *buf, char *find, char *replace, bool all) {
    struct Line *line;

    struct Point *point = &buf->views[buf->curview].point;
    
    unsigned find_len = strlen(find);
    unsigned replace_len = strlen(replace);
    
    int amt = 0;
    
    if (!find_len) return 0;

    for (line = point->line; line; line = line->next) {
        int start = 0;
        if (line == point->line) {
            start = point->pos+1;
            if (start >= line->len) continue;
        }

        while (start < line->len) {
            char *match = stristr(line->str + start, find);
            if (match) {
                point->line = line;
                point->pos = match - line->str;
                start = point->pos+1;
                
                line_delete_chars_range(line, point->pos, point->pos + find_len);
                line_type_string(line, point->pos, replace);
                    
                highlight_set(&line->hls[line->hl_count], line, (SDL_Color){0, 64, 127, 255}, point->pos, replace_len, true);
    
                int pos = point->line->y*3 + point->line->y*font_h;
                if (pos < -font_h-buf->views[buf->curview].scroll.y || pos > window_height-buf->views[buf->curview].scroll.y-font_h*2) {
                    buf->views[buf->curview].scroll.target_y = -font_h+(window_height/2 - font_h*2)-(3*point->line->y + point->line->y * font_h);
                }
                
                amt++;
                if (!all) return 1;
            } else {
                start++;
            }
        }
    }
    return amt;
}

