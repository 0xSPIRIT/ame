#ifndef MODELINE_H_
#define MODELINE_H_

/* The modeline is the line at the top or
   bottom of the screen that displays information
   about the current buffer. I'm calling it the
   "mode line" because that's what emacs calls it
   so I'm used to that. */

#include "buffer.h"

void buffer_modeline_draw(struct Buffer *buf);

#endif /* MODELINE_H_ */