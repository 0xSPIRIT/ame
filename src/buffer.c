#include "buffer.h"

#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <dirent.h>

#include "globals.h"
#include "util.h"
#include "mark.h"
#include "modeline.h"
#include "minibuffer.h"
#include "panel.h"
#include "isearch.h"

struct Buffer *curbuf = NULL;
struct Buffer *prevbuf = NULL;
struct Buffer *headbuf = NULL;

unsigned buffer_count = 0; /* Includes the minibuffer. */

struct Buffer *buffer_allocate(char name[BUF_NAME_LEN]) {
    struct Buffer *buf = alloc(1, sizeof(struct Buffer));
    strcpy(buf->name, name);
    buf->start_line = line_allocate(buf);
    buf->line_count = 1;

    buf->search = alloc(1, sizeof(struct Isearch));

    buf->mark = mark_allocate(buf);

    buf->point.line = buf->start_line;
    buf->point.pos = 0;

    buffer_count++;

    return buf;
}

void buffer_deallocate(struct Buffer *buf) {
    struct Line *line, *next;

    mark_deallocate(buf->mark);

    for (line = buf->start_line; line; line = next) {
        next = line->next;
        line_deallocate(line);
    }

    free(buf->search);
    free(buf);
}

static void buffer_draw_point(struct Buffer *buf) {
    int spacing = 3;
    const SDL_Rect dst = {
        buf->x + buf->scroll.x + buf->point.pos * font_w + strlen(buf->point.line->pre_str) * font_w + spacing,
        buf->y + buf->scroll.y + buf->point.line->y * font_h + spacing * buf->point.line->y,
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

    /* For the minibuffer, draw a background so text won't be clipping through. */
    if (buf->is_singular) {
        SDL_Rect r = { buf->x, buf->y, window_width, font_h };
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &r);
    }

    if (buf->mark->active)
        mark_draw(buf->mark);

    for (line = buf->start_line; line; line = line->next) {
        int pos = yoff*3 + yoff*font_h;
        if (pos > -font_h-buf->scroll.y && pos < window_height-buf->scroll.y) { /* Culling */
            if (line == buf->point.line && buf == curbuf && buf != minibuf) { /* Draw a little highlight on current line */
                int w = window_width / panel_count();
                SDL_Rect r = { 
                    buf->x + buf->scroll.x + 3, 
                    buf->y + font_h*yoff + 3*yoff + buf->scroll.y, 
                    w - buf->scroll.x - 6, 
                    font_h
                };
                const int alpha = 255-240;
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
                SDL_RenderFillRect(renderer, &r);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            }

            int i;
            for (i = 0; i < line->hl_count; i++) {
                highlight_update(&line->hls[i]);
                if (line->hls[i].active) {
                    highlight_draw(line->hls[i], 3 + buf->scroll.x, buf->y + font_h*yoff + 3*yoff + buf->scroll.y);
                }
            }

            line_draw(line, yoff, buf->scroll.x, buf->scroll.y);
        }
        amt++;
        yoff++;
    }

    if (buf == curbuf)
        buffer_draw_point(buf);
}

static void buffer_limit_point(struct Buffer *buf) {
    if (buf->point.pos < 0) buf->point.pos = 0;
    if (buf->point.pos > buf->point.line->len) buf->point.pos = buf->point.line->len;
    if (3 + buf->point.pos * font_w < -buf->scroll.target_x) {
        buf->scroll.target_x = -buf->point.pos * font_w;
    }
}

void buffer_handle_input(struct Buffer *buf, SDL_Event *event) {
    if (event->type == SDL_TEXTINPUT) {
        if (buf->destructive) { 
            buf->destructive = false;
            line_delete_chars_range(buf->start_line, 0, buf->start_line->len);
            buf->point.pos = 0;
        }
        if (*(event->text.text) == ' ' && is_ctrl()) goto keydown;

        if (buf->mark->active) {
            mark_delete_text(buf->mark);
            mark_unset(buf->mark);
        }
        line_type(buf->point.line, buf->point.pos++, *(event->text.text));
        
        if (3 + buf->point.pos * font_w > (window_width/panel_count())-buf->scroll.target_x) {
            buf->scroll.target_x = -buf->point.pos * font_w + (window_width/panel_count()) - font_w - 3;
        }
    } else if (event->type == SDL_MOUSEWHEEL) {
        int y = event->wheel.y;
        buf->scroll.target_y += 3 * y * (font_h + 3);
    } else if (event->type == SDL_KEYDOWN) {
  keydown:
        switch (event->key.keysym.sym) {
            /* Ctrl+M and RETURN is synonymous. */
            case SDLK_m: {
                if (is_ctrl()) {
                    goto return_key;
                }
                break;
            }
            case SDLK_RETURN: {
  return_key:
                if (buf->mark->active) {
                    mark_delete_text(buf->mark);
                    mark_unset(buf->mark);
                }
                if (!buf->is_singular) {
                    buffer_newline(buf);
                } else {
                    buf->on_return();
                }
                int pos = buf->point.line->y*3 + buf->point.line->y*font_h;
                if (pos < -font_h-buf->scroll.y || pos > window_height-buf->scroll.y-font_h*2) {
                    buf->scroll.target_y = -font_h+(window_height-font_h*2)-(3*buf->point.line->y + buf->point.line->y * font_h);
                }
                break;
            }
            case SDLK_TAB: {
                if (curbuf == minibuf) break;
                if (is_ctrl()) {
                    struct Buffer *bef = curbuf;
                    if (is_shift()) {
                        if (curbuf->next) 
                            curbuf = curbuf->next;
                        else while (curbuf->prev) curbuf = curbuf->prev;
                    } else {
                        if (curbuf->prev) 
                            curbuf = curbuf->prev;
                        else while (curbuf->next) curbuf = curbuf->next;
                    }

                    if (panel_left == bef) panel_left = curbuf;
                    if (panel_right == bef) panel_right = curbuf;
                } else {
                    line_type_string(buf->point.line, buf->point.pos, "    ");
                    buf->point.pos += 4;
                }
                break;
            }

            case SDLK_SPACE: {
                if (is_ctrl()) {
                    mark_set(buf->mark, false);
                }
                break;
            }
            case SDLK_g: {
                if (is_ctrl()) {
                    mark_unset(buf->mark);
                }
                break;
            }

            case SDLK_u: {
                if (is_ctrl()) {
                    panel_swap_focus();
                }
                break;
            }
            case SDLK_e: {
                if (is_ctrl()) {
                    if (panel_left && panel_right) {
                        if (curbuf == panel_left) panel_left = NULL;
                        else if (curbuf == panel_right) panel_right = NULL;
                    }
                }
                break;
            }

            case SDLK_a: {
                if (is_ctrl()) {
                    buffer_point_to_beginning(buf);
                    mark_set(buf->mark, false);
                    buffer_point_to_end(buf);
                }
                break;
            }

            case SDLK_l: {
                if (is_ctrl()) {
                    buf->scroll.target_y = -buf->point.line->y * (font_h + 3) + window_height/2 - font_h*2;
                }
                break;
            }

            case SDLK_DELETE: {
                if (is_ctrl()) {
                    struct Point point_prev = buf->point;
                    buffer_forward_word(buf);
                    if (point_prev.line == buf->point.line) {
                        line_delete_chars_range(buf->point.line, point_prev.pos, buf->point.pos);
                        buf->point.pos = point_prev.pos;
                    } else {
                        line_remove(point_prev.line);
                        buf->point.pos = 0;
                    }
                } else if (buf->mark->active) {
                    mark_delete_text(buf->mark);
                    mark_unset(buf->mark);
                } else {
                    if (buf->point.pos < buf->point.line->len) {
                        line_delete_char(buf->point.line, buf->point.pos);
                    } else if (buf->point.line->next) {
                        /* Move content of next line to current line, then delete next line. */
                        line_type_string(buf->point.line,
                                         buf->point.pos,
                                         buf->point.line->next->str);
                        line_remove(buf->point.line->next);
                        buffer_limit_point(buf);
                    }
                }
                break;
            }
            case SDLK_BACKSPACE: {
                if (is_ctrl()) {
                    struct Point point_prev = buf->point;
                    buffer_backward_word(buf);
                    if (point_prev.line == buf->point.line)
                        line_delete_chars_range(buf->point.line, buf->point.pos, point_prev.pos);
                } else if (buf->mark->active) {
                    mark_delete_text(buf->mark);
                    mark_unset(buf->mark);
                } else {
                    buffer_backspace(buf);
                }
                buffer_set_edited(buf, true);
                break;
            }
            case SDLK_INSERT: {
                line_debug(buf->point.line);
                break;
            }

            case SDLK_LEFT: {
                mark_set_if_shift(buf->mark);
                if (is_ctrl()) {
                    buffer_backward_word(buf);
                } else {
                    buf->point.pos--;
                    if (buf->point.pos < 0) {
                        if (buf->point.line->prev) {
                            buf->point.line = buf->point.line->prev;
                            buf->point.pos = buf->point.line->len;
                        } else  buf->point.pos = 0;
                    }
                    buffer_limit_point(buf);
                }
                if (3 + buf->point.pos * font_w < -buf->scroll.target_x) {
                    buf->scroll.target_x = -buf->point.pos * font_w;
                }
                break;
            }
            case SDLK_RIGHT: {
                mark_set_if_shift(buf->mark);
                if (is_ctrl()) {
                    buffer_forward_word(buf);
                } else {
                    buf->point.pos++;
                    if (buf->point.pos > buf->point.line->len) {
                        if (buf->point.line->next) {
                            buf->point.line = buf->point.line->next;
                            buf->point.pos = 0;
                        } else  buf->point.pos = buf->point.line->len;
                    }
                }
                if (3 + buf->point.pos * font_w > (window_width/panel_count())-buf->scroll.target_x) {
                    buf->scroll.target_x = -buf->point.pos * font_w;
                }
                break;
            }
            case SDLK_UP: {
                if (!buf->point.line->prev) break;
                mark_set_if_shift(buf->mark);
                if (is_ctrl()) {
                    do {
                        if (!buf->point.line->prev) break;
                        buf->point.line = buf->point.line->prev;
                        buf->point.pos = 0;
                    } while (!line_is_empty(buf->point.line));
                } else {
                    buf->point.line = buf->point.line->prev;
                    buffer_limit_point(buf);
                }
                int pos = buf->point.line->y*3 + buf->point.line->y*font_h;
                if (pos < -font_h-buf->scroll.y || pos > window_height-buf->scroll.y) {
                    buf->scroll.target_y = -(3*buf->point.line->y + buf->point.line->y * font_h);
                }
                break;
            }
            case SDLK_DOWN: {
                if (!buf->point.line->next) break;
                mark_set_if_shift(buf->mark);

                if (is_ctrl()) {
                    do {
                        if (!buf->point.line->next) { buf->point.pos = buf->point.line->len; break; }
                        buf->point.line = buf->point.line->next;
                        buf->point.pos = 0;
                    } while (!line_is_empty(buf->point.line));
                } else {
                    buf->point.line = buf->point.line->next;
                    buffer_limit_point(buf);
                }
                int pos = buf->point.line->y*3 + buf->point.line->y*font_h;
                if (pos < -font_h-buf->scroll.y || pos > window_height-buf->scroll.y-font_h*2) {
                    buf->scroll.target_y = -font_h+(window_height-font_h*2)-(3*buf->point.line->y + buf->point.line->y * font_h);
                }
                break;
            }

            case SDLK_KP_7: {
                if (!(SDL_GetModState() & KMOD_NUM)) {
                    case SDLK_HOME: {
                        mark_set_if_shift(buf->mark);
                        if (is_ctrl()) {
                            buffer_point_to_beginning(buf);
                        } else {
                            /*int i = 0;
                            while (isspace(buf->point.line->str[i++]));
                            buf->point.pos = i-1;*/
                            buf->point.pos = 0;
                            buf->scroll.target_x = 0;
                        }
                        break;
                    }
                } break;
            }
            case SDLK_KP_1: {
                if (!(SDL_GetModState() & KMOD_NUM)) {
                    case SDLK_END: {
                        mark_set_if_shift(buf->mark);
                        if (is_ctrl()) {
                            buffer_point_to_end(buf);
                        } else {
                            buf->point.pos = buf->point.line->len;
                            if (3 + buf->point.pos * font_w > (window_width/panel_count())-buf->scroll.target_x) {
                                buf->scroll.target_x = -buf->point.pos * font_w + (window_width/panel_count()) - font_w - 3;
                            }
                        }
                        break;
                    }
                } break;
            }

            case SDLK_PAGEDOWN: {
                int i;
                buf->scroll.target_y -= window_height - font_h * 2;
                for (i = 0; i < (window_height/(font_h+6)) + 2; i++)
                    if (buf->point.line->next) buf->point.line = buf->point.line->next;
                buffer_limit_point(buf);
                break;
            }
            case SDLK_PAGEUP: {
                int i;
                buf->scroll.target_y += window_height - font_h * 2;
                if (buf->scroll.target_y > 0) buf->scroll.target_y = 0;
                for (i = 0; i < (window_height/(font_h+6)) + 2; i++)
                    if (buf->point.line->prev) buf->point.line = buf->point.line->prev;
                buffer_limit_point(buf);
                break;
            }

            case SDLK_c: {
                if (is_ctrl() && buf->mark->active) {
                    char *text = mark_get_text(buf->mark);
                    SDL_SetClipboardText(text);
                    free(text);
                }
                break;
            }
            case SDLK_x: {
                if (is_ctrl() && buf->mark->active) {
                    mark_cut_text(buf->mark);
                    mark_unset(buf->mark);
                }
                break;
            }
            case SDLK_v: {
                if (is_ctrl()) {
                    if (buf->mark->active) {
                        mark_delete_text(buf->mark);
                        mark_unset(buf->mark);
                    }
                    buffer_paste_text(buf);
                }
                break;
            }
        }
    } 
    mark_update(buf->mark);

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

void buffer_backspace(struct Buffer *buf) {
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
}

void buffer_newline(struct Buffer *buf) {
    if (!buf->point.line->next) {
        buf->point.line->next = line_allocate(buf);
        buf->point.line->next->y = buf->point.line->y+1;
        buf->point.line->next->prev = buf->point.line;
        if (buf->point.pos == buf->point.line->len) {
            buf->point.line = buf->point.line->next;
            buf->point.pos = 0;
        } else {
            line_type_string(buf->point.line->next, 0, buf->point.line->str + buf->point.pos);
            line_delete_chars_range(buf->point.line, buf->point.pos, buf->point.line->len);
            buf->point.line = buf->point.line->next;
        }
    } else {
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
    }
    buffer_set_edited(buf, true);
    buf->line_count++;
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

    
    int y = 3*buf->point.line->y + buf->point.line->y * font_h;
    if (y < -buf->scroll.target_y || y > window_height-font_h*2-buf->scroll.target_y) { 
        buf->scroll.target_y = -font_h+window_height-font_h*2-y;
    }
    if (3 + buf->point.pos * font_w > window_width-buf->scroll.target_x) {
        buf->scroll.target_x = -buf->point.pos * font_w + window_width - font_w - 3;
    }


    SDL_free(clipboard);
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

int buffer_load_file(struct Buffer *buf, char *file) {
    FILE *fp = fopen(file, "r");
    char directory[256] = {0};

    if (!fp) {
        return 1;
    }

    strcpy(buf->filename, file);
    remove_directory(buf->name, file);

    isolate_directory(directory, file);
    chdir(directory);

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        int i;
        int len = strlen(line);
        for (i = 0; i < len; i++) {
            if (line[i] == '\n') continue;
            line_type(buf->point.line, buf->point.pos++, line[i]);
        }
        buffer_newline(buf);
    }

    fclose(fp);

    /* Remove the extra newline generated from the while loop. */
    if (line_is_empty(buf->point.line)) {
        line_remove(buf->point.line);
        buf->point.line = buf->point.line->prev;
    }

    buffer_set_edited(buf, false);

    buf->point.line = buf->start_line;
    buf->point.pos = 0;

    return 0;
}

void buffer_set_edited(struct Buffer *buf, bool edited) {
    /* Don't save if the file starts with a * */
    if (buf->name[0] == '*') {
        return;
    }
    buf->edited = edited;
    const char *title = SDL_GetWindowTitle(window);
    if (edited && title[0] != '*') {
        char new_title[256] = "*";
        strcat(new_title, title);
        SDL_SetWindowTitle(window, new_title);
    } else if (!edited && title[0] == '*') {
        char new_title[256];
        strcpy(new_title, title+1);
        SDL_SetWindowTitle(window, new_title);
    }
}

void buffer_reset_completion(struct Buffer *buf) {
    buf->is_completing = false;
    buf->completion = 0;
    memset(buf->completion_original, 0, 256);
}

void buffer_kill(struct Buffer *buf) {
    if (!buf->prev && !buf->next) return;

    if (curbuf == buf) {
        if (buf->next) curbuf = buf->next;
        if (buf->prev) curbuf = buf->prev;
    }
    if (prevbuf == buf) {
        if (buf->next) prevbuf = buf->next;
        if (buf->prev) prevbuf = buf->prev;
    }

    if (buf->prev) {
        buf->prev->next = buf->next;
    }
    if (buf->next) {
        buf->next->prev = buf->prev;
    }
    buffer_deallocate(buf);
}

bool buffer_is_scrolling(struct Buffer *buf) {
    const float EPSILON = 0.001f;
    float dx = fabs(buf->scroll.x - buf->scroll.target_x);
    float dy = fabs(buf->scroll.y - buf->scroll.target_y);
    if (dx < EPSILON) buf->scroll.x = buf->scroll.target_x;
    if (dy < EPSILON) buf->scroll.y = buf->scroll.target_y;
    return dx > EPSILON || dy > EPSILON;
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

void buffer_goto_line(struct Buffer *buf, int line) {
    for (buf->point.line = buf->start_line; buf->point.line; buf->point.line = buf->point.line->next) {
        if (buf->point.line->y == line) break;
    }
}

static const char breakchars[] = " (){}[];\\\'\":/?,.<>=-_+";

static bool is_break(char c) {
    unsigned i;
    for (i = 0; i < strlen(breakchars); i++) {
        if (c == breakchars[i]) return true;
    }
    return false;
}

/* Moves to the end of the current word. If in spaces, step forward
   until you get to a word then go to the end of that. */
void buffer_forward_word(struct Buffer *buf) {
    if (buf->point.pos == buf->point.line->len && buf->point.line->next) {
        buf->point.line = buf->point.line->next;
        buf->point.pos = 0; return;
    }
    while (buf->point.pos < buf->point.line->len && is_break(buf->point.line->str[buf->point.pos])) buf->point.pos++;
    while (buf->point.pos < buf->point.line->len && !is_break(buf->point.line->str[buf->point.pos])) buf->point.pos++;
}

/* Moves to the beginning of the current word. If in spaces, step backward
   until you get to a word then go to the start of that. */
void buffer_backward_word(struct Buffer *buf) {
    if (buf->point.pos == 0 && buf->point.line->prev) {
        buf->point.line = buf->point.line->prev;
        buf->point.pos = buf->point.line->len; return;
    }

    if (!is_break(buf->point.line->str[buf->point.pos] && is_break(buf->point.line->str[buf->point.pos-1]))) buf->point.pos--;

    while (buf->point.pos > 0 && is_break(buf->point.line->str[buf->point.pos])) buf->point.pos--;
    while (buf->point.pos > 0 && !is_break(buf->point.line->str[buf->point.pos-1])) buf->point.pos--;
}

void buffer_point_to_beginning(struct Buffer *buf) {
    buf->point.line = buf->start_line;
    buf->point.pos = 0;
    buf->scroll.target_y = 0;
    buf->scroll.target_x = 0;
}

void buffer_point_to_end(struct Buffer *buf) {
    while (buf->point.line->next) {
        buf->point.line = buf->point.line->next;
    }
    buf->point.pos = buf->point.line->len;
    if (3*buf->point.line->y + buf->point.line->y * font_h > window_height - font_h*2 - buf->scroll.y) {
        buf->scroll.target_y = -font_h+(window_height-font_h*2)-(3*buf->point.line->y + buf->point.line->y * font_h);
    }
}

struct Line *line_allocate(struct Buffer *buf) {
    struct Line *line = alloc(1, sizeof(struct Line));
    line->buf = buf;
    line->cap = 16;
    line->str = alloc(line->cap, sizeof(char));
    return line;
}

void line_deallocate(struct Line *line) {
    free(line->str);
    free(line);
}

void line_remove(struct Line *line) {
    struct Line *l;

    if (line == line->buf->start_line) {
        line->buf->start_line = line->next;
        line->buf->start_line->prev = NULL;
    } else {
        line->prev->next = line->next;
        if (line->next) {
            line->next->prev = line->prev;
        }
    }

    for (l = line->buf->start_line; l; l = l->next) {
        int prev = -1;
        if (l->prev) prev = l->prev->y;
        l->y = prev+1;
    }

    line_deallocate(line);
    line->buf->line_count--;
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

void line_draw(struct Line *line, int yoff, int scroll_x, int scroll_y) {
    if (line->len == 0 && strlen(line->pre_str) == 0) return;

    int spacing = 3;

    int pre_width = 0;

    if (strlen(line->pre_str) > 0) {
        SDL_Surface *pre_surf = TTF_RenderText_Blended(font, line->pre_str, (SDL_Color){0, 0, 255, 255});
        SDL_Texture *pre_texture = SDL_CreateTextureFromSurface(renderer, pre_surf);

        pre_width = pre_surf->w;

        const SDL_Rect dst = (SDL_Rect){
            line->buf->x + scroll_x + spacing,
            line->buf->y + scroll_y + yoff * spacing + yoff * font_h,
            pre_surf->w, 
            pre_surf->h
        };
        SDL_RenderCopy(renderer, pre_texture, NULL, &dst);
        SDL_FreeSurface(pre_surf);
        SDL_DestroyTexture(pre_texture);
    }

    if (line->len > 0) {
        SDL_Color col = (SDL_Color){0, 0, 0, 255};
        if (line->buf->destructive) {
            col.a = 127;
        }
        SDL_Surface *surf = TTF_RenderText_Blended(font, line->str, col);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);

        const SDL_Rect dst = (SDL_Rect){
            line->buf->x + scroll_x + spacing + pre_width, 
            line->buf->y + scroll_y + yoff * spacing + yoff * font_h,
            surf->w, 
            surf->h
        };
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(texture);
    }
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

bool line_is_empty(struct Line *line) {
    char *c = line->str;
    while (*c) {
        if (!isspace(*c))
            return false;
        ++c;
    }
    return true;
}
