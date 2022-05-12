#include "buffer.h"

#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "globals.h"
#include "util.h"

struct Buffer *buffer_allocate(char name[BUF_NAME_LEN]) {
    struct Buffer *buf = calloc(1, sizeof(struct Buffer));
    strcpy(buf->name, name);
    strcpy(buf->filename, name);
    buf->start_line = line_allocate(buf);

    buf->point.line = buf->start_line;
    buf->point.pos = 0;

    return buf;
}

void buffer_deallocate(struct Buffer *buf) {
    struct Line *line, *next;

    for (line = buf->start_line; line; line = next) {
        next = line->next;
        line_deallocate(line);
    }
    free(buf);
}

static void buffer_draw_point(struct Buffer *buf) {
    int spacing = 3;
    const SDL_Rect dst = {
        buf->point.pos * font_w + spacing,
        buf->y_scroll + buf->point.line->y * font_h + spacing * buf->point.line->y,
        font_w,
        font_h
    };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &dst);
}

void buffer_draw(struct Buffer *buf) {
    struct Line *line;
    int yoff = 0;

    int amt = 0;
    for (line = buf->start_line; line; line = line->next) {
        int pos = yoff*3 + yoff*font_h;
        if (pos > -font_h-buf->y_scroll && pos < window_height-buf->y_scroll) {
            line_draw(line, yoff, buf->y_scroll);
        }
        amt++;
        yoff++;
    }

    buffer_draw_point(buf);
}

static void buffer_limit_point(struct Buffer *buf) {
    if (buf->point.pos < 0) buf->point.pos = 0;
    if (buf->point.pos > buf->point.line->len) buf->point.pos = buf->point.line->len;
}

void buffer_handle_input(struct Buffer *buf, SDL_Event *event) {
    if (event->type == SDL_TEXTINPUT) {
        line_type(buf->point.line, buf->point.pos++, *(event->text.text));
    } else if (event->type == SDL_MOUSEWHEEL) {
        int y = event->wheel.y;
        buf->target_y_scroll += 3 * y * (font_h + 3);
    } else if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_RETURN: {
                buffer_newline(buf);
                break;
            }
            case SDLK_TAB: {
                line_type_string(buf->point.line, buf->point.pos, "    ");
                buf->point.pos += 4;
                break;
            }
            case SDLK_BACKSPACE: {
                if (buf->point.pos > 0) {
                    line_delete_char(buf->point.line, --buf->point.pos);
                } else if (buf->point.line != buf->start_line) {
                    struct Line *prev;
                    /* Get line content, move to end of previous line, and delete old line. */
                    int bef_len = buf->point.line->prev->len;
                    line_type_string(buf->point.line->prev,
                                     buf->point.line->prev->len,
                                     buf->point.line->str);
                    prev = buf->point.line->prev;
                    line_remove(buf->point.line);
                    buf->point.line = prev;
                    buf->point.pos = bef_len;
                    buffer_limit_point(buf);
                }
                break;
            }
            case SDLK_INSERT: {
                line_debug(buf->point.line);
                break;
            }

            case SDLK_LEFT: {
                buf->point.pos--;
                buffer_limit_point(buf);
                break;
            }
            case SDLK_RIGHT: {
                buf->point.pos++;
                buffer_limit_point(buf);
                break;
            }
            case SDLK_UP: {
                if (buf->point.line->prev) {
                    buf->point.line = buf->point.line->prev;
                    buffer_limit_point(buf);
                    int pos = buf->point.line->y*3 + buf->point.line->y*font_h;
                    if (pos < -font_h-buf->y_scroll || pos > window_height-buf->y_scroll) {
                        buf->target_y_scroll = -(3*buf->point.line->y + buf->point.line->y * font_h);
                    }
                }
                break;
            }
            case SDLK_DOWN: {
                if (buf->point.line->next) {
                    buf->point.line = buf->point.line->next;
                    buffer_limit_point(buf);
                    int pos = buf->point.line->y*3 + buf->point.line->y*font_h;
                    if (pos < -font_h-buf->y_scroll || pos > window_height-buf->y_scroll) {
                        buf->target_y_scroll = -font_h+window_height-(3*buf->point.line->y + buf->point.line->y * font_h);
                    }
                }
                break;
            }

            case SDLK_KP_7: {
                if (!(SDL_GetModState() & KMOD_NUM)) {
                    case SDLK_HOME: {
                        if (SDL_GetModState() == KMOD_LCTRL) {
                            buf->point.line = buf->start_line;
                            buf->point.pos = 0;
                            buf->target_y_scroll = 0;
                        }
                        buf->point.pos = 0;
                        break;
                    }
                } break;
            }
            case SDLK_KP_1: {
                if (!(SDL_GetModState() & KMOD_NUM)) {
                    case SDLK_END: {
                        if (SDL_GetModState() == KMOD_LCTRL) {
                            while (buf->point.line->next) {
                                buf->point.line = buf->point.line->next;
                            }
                            buf->point.pos = buf->point.line->len;
                            if (3*buf->point.line->y + buf->point.line->y * font_h > window_height - buf->y_scroll) {
                                buf->target_y_scroll = -font_h+window_height-(3*buf->point.line->y + buf->point.line->y * font_h);
                            }
                        }
                        buf->point.pos = buf->point.line->len;
                        break;
                    }
                } break;
            }

            case SDLK_PAGEDOWN: {
                buf->target_y_scroll -= window_height - font_h * 2;
                break;
            }
            case SDLK_PAGEUP: {
                buf->target_y_scroll += window_height - font_h * 2;
                if (buf->target_y_scroll > 0) buf->target_y_scroll = 0;
                break;
            }

            case SDLK_v: {
                if (SDL_GetModState() == KMOD_LCTRL) {
                    buffer_paste_text(buf);
                }
                break;
            }
            case SDLK_s: {
                if (SDL_GetModState() == KMOD_LCTRL) {
                    buffer_save(buf);
                }
                break;
            }
        }
    } 
    /* We'll leave this alone for now. Not very important. */
    /*else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int x = (event->button.x + 3)/font_w;
        int y = (event->button.y/font_h);
        buf->point.pos = x;
        buffer_limit_point(buf);
        int displacement = y - buf->point.line->y;
        int dir = sign(displacement);
        if (displacement == 0) return;
        while (displacement) {
            if (dir == -1) {
                if (buf->point.line->prev == NULL) return;
                buf->point.line = buf->point.line->prev;
            } else if (dir == 1) {
                if (buf->point.line->next == NULL) return;
                buf->point.line = buf->point.line->next;
            }
            displacement--;
        }
    }*/
}

void buffer_newline(struct Buffer *buf) {
    if (buf->point.line->next) { /* Do we have more lines after the current line? */
        struct Line *new_line, *old_next;

        old_next = buf->point.line->next;
        new_line = line_allocate(buf);

        line_type_string(new_line, 0, buf->point.line->str + buf->point.pos);
        int amt_chars_deleted = buf->point.line->len - buf->point.pos;
        line_delete_chars_range(buf->point.line, buf->point.pos, buf->point.line->len);

        new_line->y = old_next->y;
        buf->point.line->next = new_line;
        new_line->next = old_next;
        old_next->prev = new_line;
        new_line->prev = buf->point.line;

        buf->point.line = buf->point.line->next;
        if (amt_chars_deleted > 0) {
            buf->point.pos = 0;
        }
     
        buffer_limit_point(buf);

        /* Change all the line numbers for each subsequent line. */
        struct Line *l;
        for (l = old_next; l; l = l->next) {
            l->y++;
        }
    } else { /* Current line is the last line in the buffer. */
        buf->point.line->next = line_allocate(buf);
        buf->point.line->next->y = buf->point.line->y+1;
        buf->point.line->next->prev = buf->point.line;
        buf->point.line = buf->point.line->next;
        buf->point.pos = 0;
    }
    buffer_set_edited(buf, true);
}

/* Pastes text at point. */
void buffer_paste_text(struct Buffer *buf) {
    char *clipboard = SDL_GetClipboardText();
    char *text = clipboard;

    while (*text) {
        if (*text == '\r' || *text == '\n') {
            buffer_newline(buf);
            if (*text == '\r' && *(text+1) == '\n') {
                text += 2;
            } else {
                text++;
            }
            continue;
        }
        line_type(buf->point.line, buf->point.pos++, *text);
        text++;
    }
    SDL_free(clipboard);
    buf->point.pos = buf->point.line->len;
    buf->target_y_scroll = -font_h+window_height-(3*buf->point.line->y + buf->point.line->y * font_h);
}

void buffer_save(struct Buffer *buf) {
    FILE *fp = fopen(buf->filename, "w");

    struct Line *line;
    for (line = buf->start_line; line; line = line->next) {
        fputs(line->str, fp); 
        fputs("\n", fp);
    }

    fclose(fp);
    buffer_set_edited(buf, false);
}

void buffer_load_file(struct Buffer *buf, char *file) {
    FILE *fp = fopen(file, "r");
    if (!fp) {
        return;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        int i;
        int len = strlen(line);
        for (i = 0; i < len-1; i++) { /* -1 to get rid of \n */
            line_type(buf->point.line, buf->point.pos++, line[i]);
        }
        buffer_newline(buf);
    }

    fclose(fp);

    buffer_set_edited(buf, false);
}

void buffer_set_edited(struct Buffer *buf, bool edited) {
    buf->edited = edited;
    const char *title = SDL_GetWindowTitle(window);
    if (edited && title[0] != '*') {
        char new_title[256] = "*";
        strcat(new_title, title);
        puts(new_title);
        SDL_SetWindowTitle(window, new_title);
    } else if (!edited && title[0] == '*') {
        char new_title[256];
        strcpy(new_title, title+1);
        puts(new_title);
        SDL_SetWindowTitle(window, new_title);
    }
}

void buffer_debug(struct Buffer *buf) {
    struct Line *l;
    int i = 0;
    printf("\n");
    for (l = buf->start_line; l; l = l->next) {
        printf("Line #%d:\n  y: %d,\n  ptr: %p,\n  prev: %p,\n  next: %p,\n  text: \"%s\",\n  len: %d,\n  cap: %d.\n\n",
                i++, l->y, l, l->prev, l->next, l->str, l->len, l->cap);
    }
}

struct Line *line_allocate(struct Buffer *buf) {
    struct Line *line = calloc(1, sizeof(struct Line));
    line->buf = buf;
    line->cap = 16;
    line->str = calloc(line->cap, sizeof(char));
    return line;
}

void line_deallocate(struct Line *line) {
    free(line->str);
    free(line);
}

void line_remove(struct Line *line) {
    struct Line *l;

    line->prev->next = line->next;
    if (line->next) {
        line->next->prev = line->prev;
    }

    for (l = line->buf->start_line->next; l; l = l->next) {
        l->y = l->prev->y+1;
    }

    line_deallocate(line);
}

void line_type(struct Line *line, int pos, char c) {
    /* Shift everything from pos to len, then insert c. */
    int i;

    for (i = line->len-1; i >= pos; i--) {
        line->str[i+1] = line->str[i];
    }
    line->str[pos] = c;
    line->len++;
    buffer_set_edited(line->buf, true);

    if (line->len >= line->cap) {
        int j;

        line->cap *= 2;
        line->str = realloc(line->str, line->cap * sizeof(char));

        /* Zero-out the newly allocated region. */
        for (j = line->len; j < line->cap; j++) {
            line->str[j] = 0;
        }
    }
}

void line_type_string(struct Line *line, int pos, char *str) {
    while (*str) {
        line_type(line, pos++, *str);
        ++str;
    }
}

void line_delete_char(struct Line *line, int pos) {
    int i;
    for (i = pos; i <= line->len; i++) {
        line->str[i] = line->str[i+1];
    }
    line->str[--line->len] = 0;
    /* Perhaps allocate a smaller space if len <= 1/2 cap? */
}

void line_delete_chars_range(struct Line *line, int start, int end) {
    int count = end-start;
    while (count) {
        line_delete_char(line, start);
        count--;
    }
}

void line_draw(struct Line *line, int yoff, int y_scroll) {
    if (line->len == 0) return;

    int spacing = 3;

    SDL_Surface *surf = TTF_RenderText_Blended(font, line->str, (SDL_Color){0, 0, 0, 255});
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);

    const SDL_Rect dst = (SDL_Rect){
        spacing, 
        y_scroll + yoff * spacing + yoff * font_h,
        surf->w, 
        surf->h
    };
    SDL_RenderCopy(renderer, texture, NULL, &dst);

    SDL_FreeSurface(surf);
    SDL_DestroyTexture(texture);
}

void line_debug(struct Line *line) {
    printf("Line #%d: ", line->y);
    int i;
    for (i = 0; i < line->cap; i++) {
        printf("%d ", line->str[i]);
    }
    printf("\n");
    for (i = 0; i < line->cap; i++) {
        printf("%c ", line->str[i]);
    }
    printf("\n");
}