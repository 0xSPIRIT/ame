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
                if (is_ctrl() && curbuf != minibuf) {
                    prevbuf = curbuf;
                    curbuf = minibuf;
                }
                break;
            }
            case SDLK_g: {
                if (is_ctrl()) {
                    mark_unset(minibuf->mark);
                    if (curbuf == minibuf) {
                        minibuffer_return();
                    }
                }
                break;
            }
            case SDLK_ESCAPE: {
                if (curbuf == minibuf) {
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
    puts(command);
    line_delete_chars_range(minibuf->start_line, 0, minibuf->start_line->len);
    minibuf->point.pos = 0;
    minibuffer_return();
    return 0;
}

void minibuffer_return() {
    curbuf = prevbuf;
    prevbuf = minibuf;
}