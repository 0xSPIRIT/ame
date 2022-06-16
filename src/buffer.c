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
    int i;
    struct Buffer *buf = alloc(1, sizeof(struct Buffer));
    
    strcpy(buf->name, name);
    buf->start_line = line_allocate(buf);
    buf->line_count = 1;
    buf->view_count = 2;
    buf->curview = 0;

    for (i = 0; i < buf->view_count; i++) {
        buf->views[i].search = alloc(1, sizeof(struct Isearch));
    }

    for (i = 0; i < buf->view_count; i++) {
        buf->views[i].mark = mark_allocate(buf);
    }
    
    for (i = 0; i < buf->view_count; i++) {
        buf->views[i].point.line = buf->start_line;
        buf->views[i].point.pos = 0;
    }

    buffer_count++;

    return buf;
}

void buffer_deallocate(struct Buffer *buf) {
    struct Line *line, *next;
    int i;

    for (i = 0; i < buf->view_count; i++) {
        mark_deallocate(buf->views[i].mark);
    }

    for (line = buf->start_line; line; line = next) {
        next = line->next;
        line_deallocate(line);
    }

    for (i = 0; i < buf->view_count; i++) {
        dealloc(buf->views[i].search);
    }
    dealloc(buf);
}

static void buffer_draw_point(struct Buffer *buf, bool is_active) {
    int tab_offset = 0;
    int i;

    for (i = 0; i < buffer_curr_point(buf)->pos; i++) {
        if (buffer_curr_point(buf)->line->str[i] == '\t') tab_offset += font_w * (4-1); /* -1 to remove the offset that's already there. */
    }
        
    const SDL_Rect dst = {
        tab_offset + buf->x + buffer_curr_scroll(buf)->x + buffer_curr_point(buf)->pos * font_w + strlen(buffer_curr_point(buf)->line->pre_str) * font_w + SPACING,
        buf->y + buffer_curr_scroll(buf)->y + buffer_curr_point(buf)->line->y * font_h + SPACING * buffer_curr_point(buf)->line->y,
        font_w,
        font_h
    };

    if (is_active) {
        SDL_SetRenderDrawColor(renderer, POINT.r, POINT.g, POINT.b, 255);
        SDL_RenderDrawRect(renderer, &dst);
    } else {
        SDL_SetRenderDrawColor(renderer, POINT.r, POINT.g, POINT.b, 127);
        SDL_RenderFillRect(renderer, &dst);
    }
}

/* real_view corresponds to the actual curview value. At this point, buffer_draw
 * is called from panel.c where curview is set depending on the current panel
 * being drawn. We still want the real curview value to check if the current
 * focus is actually on the left or right panel.
 */
void buffer_draw(struct Buffer *buf, int real_view) {
    struct Line *line;
    int yoff = 0;
    int amt = 0;
    
    /* For the minibuffer, draw a background so text won't be clipping through. */
    if (buf->is_singular) {
        SDL_Rect r = { buf->x, buf->y, window_width, font_h };
        SDL_SetRenderDrawColor(renderer, BG.r, BG.g, BG.b, 255);
        SDL_RenderFillRect(renderer, &r);
    }

    if (buffer_curr_mark(buf)->active)
        mark_draw(buffer_curr_mark(buf));

    for (line = buf->start_line; line; line = line->next) {
        int pos = yoff*SPACING + yoff*font_h;
        if (pos > -font_h-buffer_curr_scroll(buf)->y && pos < window_height-buffer_curr_scroll(buf)->y) { /* Culling */
            if (line == buffer_curr_point(buf)->line && buf == curbuf && buf != minibuf) { /* Draw a little highlight on current line */
                if (panel_left == panel_right && real_view != buf->curview) goto after_highlight;
                
                int w = window_width / panel_count();
                SDL_Rect r = { 
                    buf->x + buffer_curr_scroll(buf)->x + SPACING, 
                    buf->y + font_h*yoff + SPACING*yoff + buffer_curr_scroll(buf)->y - SPACING/2,
                    w - buffer_curr_scroll(buf)->x - SPACING*2,
                    font_h + SPACING/2
                };
                SDL_SetRenderDrawColor(renderer, POINT.r, POINT.g, POINT.b, 40);
                SDL_RenderFillRect(renderer, &r);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            }
            
  after_highlight:;
            
            int i;
            for (i = 0; i < line->hl_count; i++) {
                highlight_update(&line->hls[i]);
                if (line->hls[i].active) {
                    highlight_draw(line->hls[i], SPACING + buffer_curr_scroll(buf)->x, buf->y + font_h*yoff + SPACING*yoff + buffer_curr_scroll(buf)->y);
                }
            }

            line_draw(line, yoff, buffer_curr_scroll(buf)->x, buffer_curr_scroll(buf)->y);
        }
        amt++;
        yoff++;
    }

    bool is_active = buf == curbuf;
    if (panel_left == panel_right && buf->curview != real_view) is_active = false;
    buffer_draw_point(buf, is_active);
}

void buffer_limit_point(struct Buffer *buf) {
    if (buffer_curr_point(buf)->pos < 0) buffer_curr_point(buf)->pos = 0;
    if (buffer_curr_point(buf)->pos > buffer_curr_point(buf)->line->len) buffer_curr_point(buf)->pos = buffer_curr_point(buf)->line->len;
    if (SPACING + buffer_curr_point(buf)->pos * font_w < -buffer_curr_scroll(buf)->target_x) {
        buffer_curr_scroll(buf)->target_x = -buffer_curr_point(buf)->pos * font_w;
    }
    
    if (buffer_curr_point(buf)->line->y >= buf->line_count) {
        buffer_curr_point(buf)->line = buf->start_line;
        while (buffer_curr_point(buf)->line->y < buf->line_count-1) {
            buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next;
        }
    }
}

void buffer_handle_input(struct Buffer *buf, SDL_Event *event) {
    if (event->type == SDL_TEXTINPUT) {
        if (buf->destructive) { 
            buf->destructive = false;
            line_delete_chars_range(buf->start_line, 0, buf->start_line->len);
            buffer_curr_point(buf)->pos = 0;
        }
        if (*(event->text.text) == ' ' && is_ctrl()) goto keydown;
        if (*(event->text.text) == '}' && line_is_empty(buffer_curr_point(buf)->line)) buffer_remove_tab(buf);

        if (buffer_curr_mark(buf)->active) {
            mark_delete_text(buffer_curr_mark(buf));
            mark_unset(buffer_curr_mark(buf));
        }
        line_type(buffer_curr_point(buf)->line, buffer_curr_point(buf)->pos++, *(event->text.text));
        
        if (SPACING + buffer_curr_point(buf)->pos * font_w > (window_width/panel_count())-buffer_curr_scroll(buf)->target_x) {
            buffer_curr_scroll(buf)->target_x = -buffer_curr_point(buf)->pos * font_w + (window_width/panel_count()) - font_w - SPACING;
        }
    } else if (event->type == SDL_MOUSEWHEEL) {
        int y = event->wheel.y;
        buffer_curr_scroll(buf)->target_y += SPACING * y * (font_h + SPACING);
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
                if (buffer_curr_mark(buf)->active) {
                    mark_delete_text(buffer_curr_mark(buf));
                    mark_unset(buffer_curr_mark(buf));
                }
                if (!buf->is_singular) {
                    buffer_newline(buf);
                    buffer_auto_indent(buf);
                } else {
                    buf->on_return();
                }
                int pos = buffer_curr_point(buf)->line->y*SPACING + buffer_curr_point(buf)->line->y*font_h;
                if (pos < -font_h-buffer_curr_scroll(buf)->y || pos > window_height-buffer_curr_scroll(buf)->y-font_h*2) {
                    buffer_curr_scroll(buf)->target_y = -font_h+(window_height-font_h*2)-(SPACING*buffer_curr_point(buf)->line->y + buffer_curr_point(buf)->line->y * font_h);
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
                    buffer_type_tab(buf);
                }
                break;
            }

            case SDLK_SPACE: {
                if (is_ctrl()) {
                    mark_set(buffer_curr_mark(buf), false);
                }
                break;
            }
            case SDLK_g: {
                if (is_ctrl()) {
                    mark_unset(buffer_curr_mark(buf));
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
                    mark_set(buffer_curr_mark(buf), true);
                    buffer_point_to_end(buf);
                }
                break;
            }

            case SDLK_l: {
                if (is_ctrl()) {
                    buffer_curr_scroll(buf)->target_y = -buffer_curr_point(buf)->line->y * (font_h + SPACING) + window_height/2 - font_h*2;
                }
                break;
            }

            case SDLK_DELETE: {
                if (is_ctrl()) {
                    struct Point point_prev = *buffer_curr_point(buf);
                    buffer_forward_word(buf);
                    if (point_prev.line == buffer_curr_point(buf)->line) {
                        line_delete_chars_range(buffer_curr_point(buf)->line, point_prev.pos, buffer_curr_point(buf)->pos);
                        buffer_curr_point(buf)->pos = point_prev.pos;
                    } else {
                        line_remove(point_prev.line);
                        buffer_curr_point(buf)->pos = 0;
                    }
                } else if (buffer_curr_mark(buf)->active) {
                    mark_delete_text(buffer_curr_mark(buf));
                    mark_unset(buffer_curr_mark(buf));
                } else {
                    if (buffer_curr_point(buf)->pos < buffer_curr_point(buf)->line->len) {
                        line_delete_char(buffer_curr_point(buf)->line, buffer_curr_point(buf)->pos);
                    } else if (buffer_curr_point(buf)->line->next) {
                        /* Move content of next line to current line, then delete next line. */
                        line_type_string(buffer_curr_point(buf)->line,
                                         buffer_curr_point(buf)->pos,
                                         buffer_curr_point(buf)->line->next->str);
                        line_remove(buffer_curr_point(buf)->line->next);
                        buffer_limit_point(buf);
                    }
                }
                buffer_set_edited(buf, true);
                break;
            }
            case SDLK_BACKSPACE: {
                if (is_ctrl()) {
                    struct Point point_prev = *buffer_curr_point(buf);
                    buffer_backward_word(buf);
                    if (point_prev.line == buffer_curr_point(buf)->line)
                        line_delete_chars_range(buffer_curr_point(buf)->line, buffer_curr_point(buf)->pos, point_prev.pos);
                } else if (buffer_curr_mark(buf)->active) {
                    mark_delete_text(buffer_curr_mark(buf));
                    mark_unset(buffer_curr_mark(buf));
                } else {
                    buffer_backspace(buf);
                }
                buffer_set_edited(buf, true);
                break;
            }
            case SDLK_INSERT: {
                line_debug(buffer_curr_point(buf)->line);
                break;
            }

            case SDLK_LEFT: {
                mark_set_if_shift(buffer_curr_mark(buf));
                if (is_ctrl()) {
                    buffer_backward_word(buf);
                } else {
                    buffer_curr_point(buf)->pos--;
                    if (buffer_curr_point(buf)->pos < 0) {
                        if (buffer_curr_point(buf)->line->prev) {
                            buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->prev;
                            buffer_curr_point(buf)->pos = buffer_curr_point(buf)->line->len;
                        } else  buffer_curr_point(buf)->pos = 0;
                    }
                    buffer_limit_point(buf);
                }
                if (SPACING + buffer_curr_point(buf)->pos * font_w < -buffer_curr_scroll(buf)->target_x) {
                    buffer_curr_scroll(buf)->target_x = -buffer_curr_point(buf)->pos * font_w;
                }
                break;
            }
            case SDLK_RIGHT: {
                mark_set_if_shift(buffer_curr_mark(buf));
                if (is_ctrl()) {
                    buffer_forward_word(buf);
                } else {
                    buffer_curr_point(buf)->pos++;
                    if (buffer_curr_point(buf)->pos > buffer_curr_point(buf)->line->len) {
                        if (buffer_curr_point(buf)->line->next) {
                            buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next;
                            buffer_curr_point(buf)->pos = 0;
                        } else  buffer_curr_point(buf)->pos = buffer_curr_point(buf)->line->len;
                    }
                }
                if (SPACING + buffer_curr_point(buf)->pos * font_w > (window_width/panel_count())-buffer_curr_scroll(buf)->target_x) {
                    buffer_curr_scroll(buf)->target_x = -buffer_curr_point(buf)->pos * font_w;
                }
                break;
            }
            case SDLK_UP: {
                if (!buffer_curr_point(buf)->line->prev) break;
                mark_set_if_shift(buffer_curr_mark(buf));
                if (is_ctrl()) {
                    do {
                        if (!buffer_curr_point(buf)->line->prev) break;
                        buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->prev;
                        buffer_curr_point(buf)->pos = 0;
                    } while (!line_is_empty(buffer_curr_point(buf)->line));
                } else {
                    buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->prev;
                    buffer_limit_point(buf);
                }
                int pos = buffer_curr_point(buf)->line->y*SPACING + buffer_curr_point(buf)->line->y*font_h;
                if (pos < -font_h-buffer_curr_scroll(buf)->y || pos > window_height-buffer_curr_scroll(buf)->y) {
                    buffer_curr_scroll(buf)->target_y = -(SPACING*buffer_curr_point(buf)->line->y + buffer_curr_point(buf)->line->y * font_h);
                }
                break;
            }
            case SDLK_DOWN: {
                if (!buffer_curr_point(buf)->line->next) break;
                mark_set_if_shift(buffer_curr_mark(buf));

                if (is_ctrl()) {
                    do {
                        if (!buffer_curr_point(buf)->line->next) { buffer_curr_point(buf)->pos = buffer_curr_point(buf)->line->len; break; }
                        buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next;
                        buffer_curr_point(buf)->pos = 0;
                    } while (!line_is_empty(buffer_curr_point(buf)->line));
                } else {
                    buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next;
                    buffer_limit_point(buf);
                }
                int pos = buffer_curr_point(buf)->line->y*SPACING + buffer_curr_point(buf)->line->y*font_h;
                if (pos < -font_h-buffer_curr_scroll(buf)->y || pos > window_height-buffer_curr_scroll(buf)->y-font_h*2) {
                    buffer_curr_scroll(buf)->target_y = -font_h+(window_height-font_h*2)-(SPACING*buffer_curr_point(buf)->line->y + buffer_curr_point(buf)->line->y * font_h);
                }
                break;
            }

            case SDLK_KP_7: {
                if (!(SDL_GetModState() & KMOD_NUM)) {
                    case SDLK_HOME: {
                        mark_set_if_shift(buffer_curr_mark(buf));
                        if (is_ctrl()) {
                            buffer_point_to_beginning(buf);
                        } else {
                            /*int i = 0;
                            while (isspace(buffer_curr_point(buf)->line->str[i++]));
                            buffer_curr_point(buf)->pos = i-1;*/
                            buffer_curr_point(buf)->pos = 0;
                            buffer_curr_scroll(buf)->target_x = 0;
                        }
                        break;
                    }
                } break;
            }
            case SDLK_KP_1: {
                if (!(SDL_GetModState() & KMOD_NUM)) {
                    case SDLK_END: {
                        mark_set_if_shift(buffer_curr_mark(buf));
                        if (is_ctrl()) {
                            buffer_point_to_end(buf);
                        } else {
                            buffer_curr_point(buf)->pos = buffer_curr_point(buf)->line->len;
                            if (SPACING + buffer_curr_point(buf)->pos * font_w > (window_width/panel_count())-buffer_curr_scroll(buf)->target_x) {
                                buffer_curr_scroll(buf)->target_x = -buffer_curr_point(buf)->pos * font_w + (window_width/panel_count()) - font_w - SPACING;
                            }
                        }
                        break;
                    }
                } break;
            }

            case SDLK_PAGEDOWN: {
                int i;
                buffer_curr_scroll(buf)->target_y -= window_height - font_h * 2;
                for (i = 0; i < (window_height/(font_h+6)) + 2; i++)
                    if (buffer_curr_point(buf)->line->next) buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next;
                buffer_limit_point(buf);
                break;
            }
            case SDLK_PAGEUP: {
                int i;
                buffer_curr_scroll(buf)->target_y += window_height - font_h * 2;
                if (buffer_curr_scroll(buf)->target_y > 0) buffer_curr_scroll(buf)->target_y = 0;
                for (i = 0; i < (window_height/(font_h+6)) + 2; i++)
                    if (buffer_curr_point(buf)->line->prev) buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->prev;
                buffer_limit_point(buf);
                break;
            }

            case SDLK_c: {
                if (is_ctrl() && buffer_curr_mark(buf)->active) {
                    char *text = mark_get_text(buffer_curr_mark(buf));
                    SDL_SetClipboardText(text);
                    dealloc(text);
                    mark_unset(buffer_curr_mark(buf));
                }
                break;
            }
            case SDLK_x: {
                if (is_ctrl() && buffer_curr_mark(buf)->active) {
                    mark_cut_text(buffer_curr_mark(buf));
                    mark_unset(buffer_curr_mark(buf));
                }
                break;
            }
            case SDLK_v: {
                if (is_ctrl()) {
                    if (buffer_curr_mark(buf)->active) {
                        mark_delete_text(buffer_curr_mark(buf));
                        mark_unset(buffer_curr_mark(buf));
                    }
                    buffer_paste_text(buf);
                }
                break;
            }
        }
    }
    
    mark_update(buffer_curr_mark(buf));

    struct ScrollBar *curbuf_scroll = &curbuf->views[curbuf->curview].scroll;
    struct ScrollBar *prevbuf_scroll = &prevbuf->views[prevbuf->curview].scroll;
            
    curbuf_scroll->y = curbuf_scroll->target_y;
    curbuf_scroll->x = curbuf_scroll->target_x;
    prevbuf_scroll->y = prevbuf_scroll->target_y;
    prevbuf_scroll->x = prevbuf_scroll->target_x;
    
    /* We'll leave this alone for now. Not very important. */
    /*else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int x = (event->button.x + SPACING)/font_w;
        int y = (event->button.y/font_h);
        buffer_curr_point(buf)->pos = x;
        buffer_limit_point(buf);
        int displacement = y - buffer_curr_point(buf)->line->y;
        int dir = sign(displacement);
        if (displacement == 0) return;
        while (displacement) {
            if (dir == -1) {
                if (buffer_curr_point(buf)->line->prev == NULL) return;
                buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->prev;
            } else if (dir == 1) {
                if (buffer_curr_point(buf)->line->next == NULL) return;
                buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next;
            }
            displacement--;
        }
    }*/
}

void buffer_backspace(struct Buffer *buf) {
    if (buffer_curr_point(buf)->pos > 0) {
        line_delete_char(buffer_curr_point(buf)->line, --buffer_curr_point(buf)->pos);
    } else if (buffer_curr_point(buf)->line != buf->start_line) {
        struct Line *prev;
        /* Get line content, move to end of previous line, and delete old line. */
        int bef_len = buffer_curr_point(buf)->line->prev->len;
        line_type_string(buffer_curr_point(buf)->line->prev,
                         buffer_curr_point(buf)->line->prev->len,
                         buffer_curr_point(buf)->line->str);
        prev = buffer_curr_point(buf)->line->prev;
        line_remove(buffer_curr_point(buf)->line);
        buffer_curr_point(buf)->line = prev;
        buffer_curr_point(buf)->pos = bef_len;
        buffer_limit_point(buf);
    }
}

void buffer_newline(struct Buffer *buf) {
    if (!buffer_curr_point(buf)->line->next) {
        buffer_curr_point(buf)->line->next = line_allocate(buf);
        buffer_curr_point(buf)->line->next->y = buffer_curr_point(buf)->line->y+1;
        buffer_curr_point(buf)->line->next->prev = buffer_curr_point(buf)->line;
        if (buffer_curr_point(buf)->pos == buffer_curr_point(buf)->line->len) {
            buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next;
            buffer_curr_point(buf)->pos = 0;
        } else {
            line_type_string(buffer_curr_point(buf)->line->next, 0, buffer_curr_point(buf)->line->str + buffer_curr_point(buf)->pos);
            line_delete_chars_range(buffer_curr_point(buf)->line, buffer_curr_point(buf)->pos, buffer_curr_point(buf)->line->len);
            buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next;
        }
    } else {
        struct Line *new_line, *old_next;
    
        old_next = buffer_curr_point(buf)->line->next;
        new_line = line_allocate(buf);
    
        line_type_string(new_line, 0, buffer_curr_point(buf)->line->str + buffer_curr_point(buf)->pos);
        int amt_chars_deleted = buffer_curr_point(buf)->line->len - buffer_curr_point(buf)->pos;
        line_delete_chars_range(buffer_curr_point(buf)->line, buffer_curr_point(buf)->pos, buffer_curr_point(buf)->line->len);
    
        new_line->y = old_next->y;
        buffer_curr_point(buf)->line->next = new_line;
        new_line->next = old_next;
        old_next->prev = new_line;
        new_line->prev = buffer_curr_point(buf)->line;
    
        buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next;
        if (amt_chars_deleted > 0) {
            buffer_curr_point(buf)->pos = 0;
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
        line_type(buffer_curr_point(buf)->line, buffer_curr_point(buf)->pos++, *text);
        text++;
    }

    
    int y = SPACING*buffer_curr_point(buf)->line->y + buffer_curr_point(buf)->line->y * font_h;
    if (y < -buffer_curr_scroll(buf)->target_y || y > window_height-font_h*2-buffer_curr_scroll(buf)->target_y) { 
        buffer_curr_scroll(buf)->target_y = -font_h+window_height-font_h*2-y;
    }
    if (SPACING + buffer_curr_point(buf)->pos * font_w > window_width-buffer_curr_scroll(buf)->target_x) {
        buffer_curr_scroll(buf)->target_x = -buffer_curr_point(buf)->pos * font_w + window_width - font_w - SPACING;
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

    char absolute_path[BUF_NAME_LEN] = {0};
    _fullpath(absolute_path, file, BUF_NAME_LEN);

    strcpy(buf->filename, absolute_path); /* Use the absolute path for the stored directory. */
    remove_directory(buf->name, file);

    isolate_directory(directory, file);
    chdir(directory);

    int total_len = 0, total_cap = 2048;
    char *total_string = alloc(total_cap, sizeof(char));

    char line[1024] = {0};
    while (fgets(line, sizeof(line), fp)) {
        int i;
        int len = strlen(line);
        for (i = 0; i < len; i++) {
            if (line[i] == '\n') continue;
            line_type(buffer_curr_point(buf)->line, buffer_curr_point(buf)->pos++, line[i]);
        }
        int line_len = buffer_curr_point(buf)->line->len;

        buffer_newline(buf);

        total_len += line_len+1;
        if (total_len >= total_cap) {
            total_cap *= 2;
            total_string = realloc(total_string, total_cap);
        }
        strcat(total_string, line);
    }

    buf->indent_mode = determine_tabs_indent_method(total_string);

    fclose(fp);

    /* Remove the extra newline generated from the while loop. */
    if (line_is_empty(buffer_curr_point(buf)->line)) {
        line_remove(buffer_curr_point(buf)->line);
        buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->prev;
    }

    buffer_set_edited(buf, false);

    buffer_curr_point(buf)->line = buf->start_line;
    buffer_curr_point(buf)->pos = 0;

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
    
    if (buf->prev) {
        buf->prev->next = buf->next;
    } else {
        headbuf = buf->next; /* If we want the first buffer to be destroyed, we need to reset the head pointer. */
    }
    if (buf->next) {
        buf->next->prev = buf->prev;
    }
    buffer_deallocate(buf);
}

bool buffer_is_scrolling(struct Buffer *buf) {
    struct ScrollBar *scroll = buffer_curr_scroll(buf);
    const float EPSILON = 0.001f;
    float dx = fabs(scroll->x - scroll->target_x);
    float dy = fabs(scroll->y - scroll->target_y);
    if (dx < EPSILON) scroll->x = scroll->target_x;
    if (dy < EPSILON) scroll->y = scroll->target_y;
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
    for (buffer_curr_point(buf)->line = buf->start_line; buffer_curr_point(buf)->line; buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next) {
        if (buffer_curr_point(buf)->line->y == line) break;
    }
    buffer_curr_point(buf)->pos = 0;
}

/* Find current {} level, then add that amount of tabs at current line. */
void buffer_auto_indent(struct Buffer *buf) {
    struct Line *line;
    int indent = 0,
        char_quote = 0,
        string_quote = 0, 
        multi_comment = 0, 
        single_comment = 0;
    
    for (line = buf->start_line; line != buffer_curr_point(buf)->line; line = line->next) {
        int i;
        for (i = 0; i < line->len; i++) {
            if (line->str[i] == '/' && i < line->len-1 && line->str[i+1] == '*') multi_comment = 1;
            if (line->str[i] == '*' && i < line->len-1 && line->str[i+1] == '/') multi_comment = 0;
            if (line->str[i] == '/' && i < line->len-1 && line->str[i+1] == '/') single_comment = 1;
                
            if (multi_comment || single_comment) continue;
            
            if (line->str[i] == '\'' && ((i > 0 && line->str[i-1] != '\\') || (i == 0))) {
                if (!string_quote) char_quote = !char_quote;
            } else if (line->str[i] == '\"' && ((i > 0 && line->str[i-1] != '\\') || (i == 0))) {
                if (!char_quote) string_quote = !string_quote;
            }
            
            if (char_quote || string_quote) continue;
            
            if (line->str[i] == '{') indent++;
            if (line->str[i] == '}') indent--;
        }
        single_comment = 0;
    }

    int i;
    for (i = 0; i < indent; i++) {
        buffer_type_tab(buf);
    }
}

void buffer_type_tab(struct Buffer *buf) {
    if (buf->indent_mode == 0) {
        line_type_string(buffer_curr_point(buf)->line, buffer_curr_point(buf)->pos, "    ");
        buffer_curr_point(buf)->pos += 4;
    } else {
        line_type(buffer_curr_point(buf)->line, buffer_curr_point(buf)->pos, '\t');
        buffer_curr_point(buf)->pos += 1;
    }
}

void buffer_remove_tab(struct Buffer *buf) {
    int temp_pos = buffer_curr_point(buf)->pos;
    buffer_curr_point(buf)->pos = 0;
    if (buf->indent_mode == 0) {
        if (!string_begins_with(buffer_curr_point(buf)->line->str, "    ")) return;
        line_delete_chars_range(buffer_curr_point(buf)->line, 0, 4);
        buffer_curr_point(buf)->pos = temp_pos-4;
    } else {
        if (buffer_curr_point(buf)->line->str[0] != '\t') return;
        line_delete_char(buffer_curr_point(buf)->line, 0);
        buffer_curr_point(buf)->pos = temp_pos-1;
    }
}

struct Point *buffer_curr_point(struct Buffer *buf) {
    return &buf->views[buf->curview].point;
}

struct Mark *buffer_curr_mark(struct Buffer *buf) {
    return buf->views[buf->curview].mark;
}

struct ScrollBar *buffer_curr_scroll(struct Buffer *buf) {
    return &buf->views[buf->curview].scroll;
}

struct Isearch *buffer_curr_search(struct Buffer *buf) {
    return buf->views[buf->curview].search;
}

static const char breakchars[] = " (){}[];\\\'\":/?,.<>=-_+*";

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
    if (buffer_curr_point(buf)->pos == buffer_curr_point(buf)->line->len && buffer_curr_point(buf)->line->next) {
        buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next;
        buffer_curr_point(buf)->pos = 0; return;
    }
    while (buffer_curr_point(buf)->pos < buffer_curr_point(buf)->line->len && is_break(buffer_curr_point(buf)->line->str[buffer_curr_point(buf)->pos])) buffer_curr_point(buf)->pos++;
    while (buffer_curr_point(buf)->pos < buffer_curr_point(buf)->line->len && !is_break(buffer_curr_point(buf)->line->str[buffer_curr_point(buf)->pos])) buffer_curr_point(buf)->pos++;
}

/* Moves to the beginning of the current word. If in spaces, step backward
   until you get to a word then go to the start of that. */
void buffer_backward_word(struct Buffer *buf) {
    if (buffer_curr_point(buf)->pos == 0 && buffer_curr_point(buf)->line->prev) {
        buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->prev;
        buffer_curr_point(buf)->pos = buffer_curr_point(buf)->line->len; return;
    }

    if (!is_break(buffer_curr_point(buf)->line->str[buffer_curr_point(buf)->pos] && is_break(buffer_curr_point(buf)->line->str[buffer_curr_point(buf)->pos-1]))) buffer_curr_point(buf)->pos--;

    while (buffer_curr_point(buf)->pos > 0 && is_break(buffer_curr_point(buf)->line->str[buffer_curr_point(buf)->pos])) buffer_curr_point(buf)->pos--;
    while (buffer_curr_point(buf)->pos > 0 && !is_break(buffer_curr_point(buf)->line->str[buffer_curr_point(buf)->pos-1])) buffer_curr_point(buf)->pos--;
}

void buffer_point_to_beginning(struct Buffer *buf) {
    buffer_curr_point(buf)->line = buf->start_line;
    buffer_curr_point(buf)->pos = 0;
    buffer_curr_scroll(buf)->target_y = 0;
    buffer_curr_scroll(buf)->target_x = 0;
}

void buffer_point_to_end(struct Buffer *buf) {
    while (buffer_curr_point(buf)->line->next) {
        buffer_curr_point(buf)->line = buffer_curr_point(buf)->line->next;
    }
    buffer_curr_point(buf)->pos = buffer_curr_point(buf)->line->len;
    if (SPACING*buffer_curr_point(buf)->line->y + buffer_curr_point(buf)->line->y * font_h > window_height - font_h*2 - buffer_curr_scroll(buf)->y) {
        buffer_curr_scroll(buf)->target_y = -font_h+(window_height-font_h*2)-(SPACING*buffer_curr_point(buf)->line->y + buffer_curr_point(buf)->line->y * font_h);
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
    dealloc(line->str);
    dealloc(line);
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

    line->buf->line_count--;
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

void line_draw(struct Line *line, int yoff, int scroll_x, int scroll_y) {
    if (line->len == 0 && strlen(line->pre_str) == 0) return;

    int pre_width = 0;

    if (strlen(line->pre_str) > 0) {
        SDL_Surface *pre_surf = TTF_RenderText_Blended(font, line->pre_str, (SDL_Color){88, 98, 237, 255});
        SDL_Texture *pre_texture = SDL_CreateTextureFromSurface(renderer, pre_surf);

        pre_width = pre_surf->w;

        const SDL_Rect dst = (SDL_Rect){
            line->buf->x + scroll_x + SPACING,
            line->buf->y + scroll_y + yoff * SPACING + yoff * font_h,
            pre_surf->w, 
            pre_surf->h
        };
        SDL_RenderCopy(renderer, pre_texture, NULL, &dst);
        SDL_FreeSurface(pre_surf);
        SDL_DestroyTexture(pre_texture);
    }

    if (line->len > 0) {
        SDL_Color col = (SDL_Color){255, 255, 255, 255};
        if (line->buf->destructive) {
            col.a = 127;
        }

        /* Convert tabs to spaces before rendering. */
        int i;
        char *draw_string = alloc(line->len * 4 + 1, sizeof(char)); /* Allocating the most needed. */
        for (i = 0; i < line->len; i++) {
            if (line->str[i] == '\t') {
                strcat(draw_string, "    ");
            } else {
                strcat(draw_string, (char[2]){line->str[i], 0});
            }
        }

        SDL_Surface *surf = TTF_RenderText_Blended(font, draw_string, col);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);

        const SDL_Rect dst = (SDL_Rect){
            line->buf->x + scroll_x + SPACING + pre_width, 
            line->buf->y + scroll_y + yoff * SPACING + yoff * font_h,
            surf->w, 
            surf->h
        };
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(texture);

        dealloc(draw_string);
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
