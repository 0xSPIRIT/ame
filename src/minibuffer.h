#ifndef MINIBUFFER_H_
#define MINIBUFFER_H_

/* The "minibuffer" is an area under the
   modeline where you can type in commands,
   such as opening or saving files. It's named
   "minibuffer" because that's what it's named
   in emacs. */

#include "buffer.h"

enum MinibufferState {
    STATE_NONE,
    STATE_LOAD_FILE,
    STATE_SAVE_FILE_AS,
    STATE_SWITCH_TO_BUFFER
};

extern struct Buffer *minibuf;

void minibuffer_allocate();
void minibuffer_deallocate();
void minibuffer_handle_input(SDL_Event *event);
int  minibuffer_execute();
void minibuffer_return();
void minibuffer_reset();

#endif /* MINIBUFFER_H_ */