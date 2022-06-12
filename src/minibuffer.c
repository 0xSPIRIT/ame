#include "minibuffer.h"

#include "buffer.h"
#include "util.h"
#include "globals.h"
#include "mark.h"
#include "panel.h"
#include "isearch.h"
#include "replace.h"

#include <dirent.h>

struct Buffer *minibuf;

void minibuffer_allocate() {
    minibuf = buffer_allocate("*minibuffer*");
    minibuf->x = 0;
    minibuf->y = window_height - font_h;
    minibuf->is_singular = true;
    minibuf->on_return = minibuffer_execute;
}

void minibuffer_deallocate() {
    buffer_deallocate(minibuf);
}

void minibuffer_handle_input(SDL_Event *event) {
    minibuf->y = window_height - font_h;
    minibuf->scroll.target_y = minibuf->scroll.y = 0;

    if (minibuf->singular_state == STATE_ISEARCH) {
        buffer_isearch_mark_matching(prevbuf, minibuf->start_line->str);
    }

    if (event->type == SDL_TEXTINPUT) {
        buffer_reset_completion(minibuf);
    }
    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym != SDLK_TAB && !is_shift() && !is_ctrl()) {
            buffer_reset_completion(minibuf);
        }

        switch (event->key.keysym.sym) {
            case SDLK_TAB: {
                if (curbuf == minibuf) {
                    int direction = is_shift() ? -1 : 1;
                    minibuffer_attempt_autocomplete(direction);
                }
                break;
            }
            case SDLK_o: {
                if (is_ctrl() && !curbuf->is_singular) {
                    prevbuf = curbuf;
                    curbuf = minibuf;
                    minibuf->singular_state = STATE_LOAD_FILE;
                    strcpy(minibuf->start_line->pre_str, "Open File: ");
                    /* add cwd by default */
                    char cwd[256] = {0};
                    get_cwd(cwd);
                    line_type_string(minibuf->start_line, 0, cwd);
                    minibuf->point.pos = minibuf->start_line->len;
                }
                break;
            }
            case SDLK_f: {
                if (is_ctrl()) {
                    if (!curbuf->is_singular) {
                        prevbuf = curbuf;
                        curbuf = minibuf;
                        minibuf->singular_state = STATE_ISEARCH;
                        strcpy(minibuf->start_line->pre_str, "Isearch: ");
                        if (strlen(prevbuf->search->str)) {
                            line_type_string(minibuf->start_line, 0, prevbuf->search->str);
                            minibuf->destructive = true;
                        }
                        minibuf->point.pos = minibuf->start_line->len;
                    } else {
                        /* Go to the first match, then move one position ahead if possible, then mark the new matches. */
                        buffer_isearch_goto_matching(prevbuf, minibuf->start_line->str);
                        prevbuf->point.pos++;
                        if (prevbuf->point.pos >= prevbuf->point.line->len && prevbuf->point.line->next) {
                            prevbuf->point.line = prevbuf->point.line->next;
                        }
                        buffer_isearch_mark_matching(prevbuf, minibuf->start_line->str);
                    }
                }
                break;
            }
            
            case SDLK_h: {
                if (!curbuf->is_singular) {
                    if (is_ctrl()) {
                        prevbuf = curbuf;
                        curbuf = minibuf;
                        minibuf->singular_state = STATE_FIND;
                        strcpy(minibuf->start_line->pre_str, "Find: ");
                    } else if (is_alt() && (!panel_left || !panel_right)) {
                        if (curbuf == panel_left) panel_right = curbuf;
                        else if (curbuf == panel_right) panel_left = curbuf;
                    }
                }
                break;
            }
            
            case SDLK_q: {
                if (!curbuf->is_singular && is_ctrl()) {
                    prevbuf = curbuf;
                    curbuf = minibuf;
                    minibuf->singular_state = STATE_QUERY_FIND;
                    strcpy(minibuf->start_line->pre_str, "(Query) Find: ");
                }
                break;
            }
            
            case SDLK_s: {
                if (is_ctrl() && !is_shift() && !curbuf->is_singular) {
                    if (strlen(curbuf->filename) > 0) {
                        if (curbuf->edited) buffer_save(curbuf);
                    } else {
                        goto save_file_as;
                    }
                } else if (is_ctrl() && is_shift() && !curbuf->is_singular) {
  save_file_as:
                    minibuffer_reset();
                    prevbuf = curbuf;
                    curbuf = minibuf;
                    minibuf->singular_state = STATE_SAVE_FILE_AS;
                    strcpy(minibuf->start_line->pre_str, "Save File As: ");
                    /* Add CWD by default. */
                    char cwd[256] = {0};
                    get_cwd(cwd);
                    line_type_string(minibuf->start_line, 0, cwd);
                    minibuf->point.pos = minibuf->start_line->len;
                }
                break;
            }
            case SDLK_b: {
                if (is_ctrl() && !curbuf->is_singular) {
                    prevbuf = curbuf;
                    curbuf = minibuf;
                    minibuf->singular_state = STATE_SWITCH_TO_BUFFER;
                    strcpy(minibuf->start_line->pre_str, "Switch to buffer: ");
                }
                break;
            }

            case SDLK_k: { 
                if (!curbuf->is_singular && is_ctrl() && is_shift()) {
                    prevbuf = curbuf;
                    curbuf = minibuf;
                    minibuf->singular_state = STATE_KILL_BUFFER;
                    strcpy(minibuf->start_line->pre_str, "Kill buffer: ");
                }
                break;
            }

            case SDLK_w: {
                if (is_ctrl() && !curbuf->is_singular) {
                    if (curbuf->edited) {
                        prevbuf = curbuf;
                        curbuf = minibuf;
                        minibuf->singular_state = STATE_KILL_CURRENT_BUFFER;
                        strcpy(minibuf->start_line->pre_str, "Discard changes and kill buffer? (y/n): ");
                    } else {
                        struct Buffer *buf = curbuf;
                        buffer_kill(curbuf);
                        if (panel_left == buf) panel_left = curbuf;
                        if (panel_right == buf) panel_right = curbuf;
                    }
                }
                break;
            }
            case SDLK_g: {
                if (is_ctrl()) {
                    mark_unset(minibuf->mark);
                    minibuffer_return();

                    struct Line *line;
                    /* Destroy previous highlights. */
                    for (line = curbuf->point.line; line; line = line->next) {
                        int i, c = line->hl_count;
                        for (i = 0; i < c; i++)
                            highlight_stop(&line->hls[i]);
                    }
                } else if (is_alt()) {
                    prevbuf = curbuf;
                    curbuf = minibuf;
                    minibuf->singular_state = STATE_GOTO_LINE;
                    strcpy(minibuf->start_line->pre_str, "Go to line: ");
                }
                break;
            }

            case SDLK_a: {
                if (is_alt()) {
                    if (curbuf == panel_left) panel_right = NULL;
                    if (curbuf == panel_right) panel_left = NULL;
                } break;
            }
            case SDLK_e: {
                if (is_alt() && panel_left && panel_right) {
                    if (curbuf == panel_left) panel_left = NULL;
                    if (curbuf == panel_right) panel_right = NULL;
                } break;
            }
        }
    }
}

/* Take the command from minibuffer, split it by space, then parse. */
int minibuffer_execute() {
    char *command = minibuf->start_line->str;

    /* NOTE: Curbuf is the minibuf so we must use prevbuf to get the real main buffer. */
    switch (minibuf->singular_state) {
        case STATE_LOAD_FILE: {
            /* Create a new buffer, load file, then add it to the linked list of buffers. */
            if (!strchr(command, '\\') && !strchr(command, '/')) return 0;

            struct Buffer *buf;
            char buffer_name[256];

            /* Check if file already exists in opened buffers. If so, switch to it. */
            struct Buffer *a;
            for (a = headbuf; a; a = a->next) {
                if (0==strcmp(a->filename, command)) {
                    if (prevbuf == panel_left) panel_left = a;
                    if (prevbuf == panel_right) panel_right = a;
                    prevbuf = a;
                    goto end;
                }
            }

            remove_directory(buffer_name, command);
            buf = buffer_allocate(buffer_name);
            if (panel_left == prevbuf) panel_left = buf;
            else if (panel_right == prevbuf) panel_right = buf;

            int directory_exists = !buffer_load_file(buf, command);
            if (!directory_exists) {
                strcpy(buf->filename, command);
                printf("New file name: %s\n", buf->filename);
            }

            /* Set the new buffer to the next in the linked list, and fix up the pointers. */
            buf->next = prevbuf->next;
            buf->prev = prevbuf;
            prevbuf->next = buf;
            prevbuf = prevbuf->next;

            break;
        }
        case STATE_SAVE_FILE_AS: {
            /* Save file to dir given. */
            strcpy(prevbuf->filename, command);
            remove_directory(prevbuf->name, command);

            buffer_save(prevbuf);
            break;
        }
        case STATE_FIND: {
            if (strlen(command) >= 1024) return 0;
            strcpy(find, command);
            minibuf->singular_state = STATE_REPLACE;
            strcpy(minibuf->start_line->pre_str, "Replace: ");
            memset(minibuf->start_line->str, 0, minibuf->start_line->cap);
            minibuf->start_line->len = 0;
            minibuf->point.pos = 0;
            return 0; /* Don't go to end of function, where it will reset. */
        }
        case STATE_REPLACE: {
            if (strlen(command) >= 1024) return 0;
            strcpy(replace, command);
            buffer_replace_matching(prevbuf, find, replace, true);
            break;
        }
        case STATE_QUERY_FIND: {
            if (strlen(command) >= 1024) return 0;
            strcpy(find, command);
            minibuf->singular_state = STATE_QUERY_REPLACE;
            strcpy(minibuf->start_line->pre_str, "(Query) Replace: ");
            memset(minibuf->start_line->str, 0, minibuf->start_line->cap);
            minibuf->start_line->len = 0;
            minibuf->point.pos = 0;
            return 0; /* Don't go to end of function, where it will reset. */
        }
        case STATE_QUERY_REPLACE: {
            if (strlen(command) >= 1024) return 0;
            strcpy(replace, command);
            minibuf->singular_state = STATE_QUERY;

            buffer_isearch_mark_matching(prevbuf, find); /* Mark the next one */
/*            buffer_isearch_goto_matching(prevbuf, find); */
            
            char msg[3000] = {0};
            sprintf(msg, "(Query) Replace %s with %s? (y/n): ", find, replace);
            
            strcpy(minibuf->start_line->pre_str, msg);
            memset(minibuf->start_line->str, 0, minibuf->start_line->cap);
            minibuf->start_line->len = 0;
            minibuf->destructive = true;
            line_type(minibuf->start_line, 0, 'y');
            minibuf->point.pos = 1;
            return 0; /* Don't go to end of function, where it will reset. */
        }
        case STATE_QUERY: {
            int match = 0;
            
            if (0==strcmp(command, "y")) {
                match = buffer_replace_matching(prevbuf, find, replace, false);
            } else {
                buffer_isearch_goto_matching(prevbuf, find);
                match = 1; /* Setting this so it will still return. */
            }
            
            buffer_isearch_mark_matching(prevbuf, find); /* Mark the next one */

            memset(minibuf->start_line->str, 0, minibuf->start_line->cap);
            minibuf->start_line->len = 0;
            minibuf->point.pos = 0;

            minibuf->destructive = true;
            line_type(minibuf->start_line, 0, 'y');
            minibuf->point.pos = 1;

            if (!match) {
                /* Destroy previous highlights upon exit */
                struct Line *line;
                for (line = prevbuf->point.line; line; line = line->next) {
                    int i, c = line->hl_count;
                    for (i = 0; i < c; i++)
                        highlight_stop(&line->hls[i]);
                }
                break;
            } else {
                return 0;
            }
        }
        case STATE_SWITCH_TO_BUFFER: {
            struct Buffer *buf = headbuf;
            while (buf) {
                if (0==strcmp(buf->name, command)) {
                    if (prevbuf == panel_left) {
                        panel_left = buf;
                    } else if (prevbuf == panel_right) {
                        panel_right = buf;
                    }
                    prevbuf = buf; /* When minibuffer_return() is called, curbuf will be set to prevbuf. */
                    break;
                }
                buf = buf->next;
            }
            /* If we reach here then there is no matching buffer. Create a new buffer. */
            break;
        }
        case STATE_KILL_CURRENT_BUFFER: {
            if (*command == 'y') {
                buffer_kill(prevbuf);
            } else if (*command == 'n') {
            } else {
                strcpy(minibuf->start_line->pre_str, "Discard changes and kill buffer? (y/n) [Must be y or n]: ");
                return 0;
            }
            break;
        }
        case STATE_KILL_BUFFER: {
            struct Buffer *buf = headbuf;
            while (buf) {
                if (0==strcmp(buf->name, command)) {
                    buffer_kill(buf);
                    break;
                }
                buf = buf->next;
            }
            break;
        }
        case STATE_GOTO_LINE: {
            int line = atoi(curbuf->start_line->str) - 1;
            if (line < 0) line = 0;
            buffer_goto_line(prevbuf, line);
            int pos = prevbuf->point.line->y*3 + prevbuf->point.line->y*font_h;
            if (pos < -font_h-prevbuf->scroll.y || pos > window_height-prevbuf->scroll.y-font_h*2) {
                prevbuf->scroll.target_y = -font_h+(window_height-font_h*2)-(3*prevbuf->point.line->y + prevbuf->point.line->y * font_h);
            }
            break;
        }
        case STATE_ISEARCH: {
            buffer_isearch_goto_matching(prevbuf, minibuf->start_line->str);
            break;
        }
    }
 end:
    minibuffer_return();
    return 0;
}

void minibuffer_return() {
    buffer_reset_completion(minibuf);
    minibuf->destructive = false;
    if (curbuf == minibuf) {
        minibuffer_reset();
        curbuf = prevbuf;
        prevbuf = minibuf;
    }
}

void minibuffer_reset() {
    memset(minibuf->start_line->str, 0, minibuf->start_line->cap);
    minibuf->start_line->len = 0;
    minibuf->point.pos = 0;
    memset(minibuf->start_line->pre_str, 0, 255);
}

void minibuffer_attempt_autocomplete(int direction) {
    char *str = minibuf->start_line->str;

    char possibilities[MAX_COMPLETION][256];
    int i = 0;

    bool is_initial = !minibuf->is_completing; /* Is this the intial completion? */

    switch (minibuf->singular_state) {
        case STATE_LOAD_FILE: case STATE_SAVE_FILE_AS: {
            char dirname[256] = {0};
            char filename[256] = {0};
            DIR *d;
            struct dirent *dir;

            isolate_directory(dirname, str);

            if (minibuf->is_completing) {
                strcpy(filename, minibuf->completion_original);
            } else {
                minibuf->is_completing = true;
                remove_directory(filename, str);
                strcpy(minibuf->completion_original, filename);
            }

            d = opendir(dirname);
            if (d) {
                while ((dir = readdir(d)) != NULL) {
                    if (0==strcmp(dir->d_name, ".") || 0==strcmp(dir->d_name, "..")) continue;
                    if (strlen(filename) == 0) {
                        strcpy(possibilities[i++], dir->d_name);
                        continue;
                    }
                    if (string_begins_with(dir->d_name, filename)) {
                        strcpy(possibilities[i++], dir->d_name);
                    }
                }
                closedir(d);
            }

            if (i == 0) return;

            /* If it's an initial, we want to see the first element no matter direction. We don't want to skip the first one*/
            if (!is_initial) {
                minibuf->completion += direction;
                if (minibuf->completion >= i) {
                    minibuf->completion = 0;
                }
                if (minibuf->completion < 0) {
                    minibuf->completion = i-1;
                }
            }

            memset(minibuf->start_line->str, 0, minibuf->start_line->cap);
            minibuf->start_line->len = 0;
            char new[256] = {0};
            strcat(new, dirname);
            strcat(new, possibilities[minibuf->completion]);
            line_type_string(minibuf->start_line, 0, new);
            minibuf->point.pos = minibuf->start_line->len;

            break;
        }
        case STATE_SWITCH_TO_BUFFER: case STATE_KILL_BUFFER: {
            /* If there is one option, then choose that. Otherwise print the possibilities. */
            struct Buffer *buf;
            int i = 0;

            if (minibuf->is_completing) {
                strcpy(str, minibuf->completion_original);
            } else {
                minibuf->is_completing = true;
                strcpy(minibuf->completion_original, str);
            }

            for (buf = headbuf; buf; buf = buf->next) {
                if (buf == prevbuf) continue; /* Obviously don't switch to the currently opened buffer. */
                if (strlen(str) == 0) {
                    strcpy(possibilities[i++], buf->name);
                    continue;
                }
                if (string_begins_with(buf->name, str)) {
                    strcpy(possibilities[i++], buf->name);
                }
            }

            if (i == 0) return;

            /* If it's an initial, we want to see the first element no matter direction. We don't want to skip the first one*/
            if (!is_initial) {
                minibuf->completion += direction;
                if (minibuf->completion >= i) {
                    minibuf->completion = 0;
                }
                if (minibuf->completion < 0) {
                    minibuf->completion = i-1;
                }
            }

            memset(minibuf->start_line->str, 0, minibuf->start_line->cap);
            minibuf->start_line->len = 0;
            line_type_string(minibuf->start_line, 0, possibilities[minibuf->completion]);
            minibuf->point.pos = minibuf->start_line->len;
            
            break;
        }
    }
}
