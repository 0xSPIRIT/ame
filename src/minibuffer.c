#include "minibuffer.h"

#include "buffer.h"
#include "util.h"
#include "globals.h"
#include "mark.h"

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
    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
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
                    /* Automatically put next buffer. */
                    char *name;
                    if (prevbuf->next) 
                        name = prevbuf->next->name;
                    else
                        name = headbuf->name;
                    line_type_string(minibuf->start_line, 0, name);
                    minibuf->point.pos = minibuf->start_line->len;
                }
                break;
            }
            case SDLK_g: case SDLK_ESCAPE: {
                if (is_ctrl()) {
                    mark_unset(minibuf->mark);
                    minibuffer_return();
                }
                break;
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
            struct Buffer *buf;
            char buffer_name[256];

            remove_directory(buffer_name, command);
            buf = buffer_allocate(buffer_name);

            int directory_exists = !buffer_load_file(buf, command);
            if (!directory_exists) {
                char cwd[256] = {0};
                get_cwd(cwd);
                sprintf(buf->filename, "%s%s", cwd, command);
                printf("New file name: %s\n", buf->filename);
            }

            prevbuf->next = buf; /* Set the new buffer to the next in the linked list. */
            prevbuf->next->prev = prevbuf;
            prevbuf = prevbuf->next;

            buffer_update_window_title(prevbuf);
            break;
        }
        case STATE_SAVE_FILE_AS: {
            /* Save file to dir given. */
            strcpy(prevbuf->filename, command);
            remove_directory(prevbuf->name, command);

            buffer_save(prevbuf);

            buffer_update_window_title(prevbuf);
            break;
        }
        case STATE_SWITCH_TO_BUFFER: {
            struct Buffer *buf = headbuf;
            while (buf) {
                if (0==strcmp(buf->name, command)) {
                    prevbuf = buf; /* When minibuffer_return() is called, curbuf will be set to prevbuf. */
                    break;
                }
                buf = buf->next;
            }
            /* If we reach here then there is no matching buffer. Create a new buffer. */
            
            break;
        }
    }
    minibuffer_return();
    return 0;
}

void minibuffer_return() {
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