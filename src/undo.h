#ifndef UNDO_H_
#define UNDO_H_

/* This is a sort of "dumb" undo, where the state of the entire buffer
 * is stored at intervals of action, eg. typing, pasting, etc.
 * Upon Ctrl+Z, the entire buffer is destroyed, then re-typed line-by-line
 * according to the previous undo state.
 *
 * Yeah, pretty inefficient. If you're interested in making a better system,
 * maybe store only the changed lines or something, then remaking the buffer
 * based on that.
 * 
 * I'm not too concerned with this though, as text files are relatively small.
 */

#include "buffer.h"

#define UNDO_TYPE_AMOUNT 8

struct BufferState {
    struct Buffer *buf; /* What buffer are we taking the state of? */
    char *str; /* Contents of a buffer. */
    int len, cap;
    struct Point point;
    
    struct BufferState *prev, *next; /* Linked list of undos. */
};

/* Undo head would be the most recent buffer state. */
extern struct BufferState *undo_start, *undo_head;
extern int type_amt;

struct BufferState *buffer_save_undo_state(struct Buffer *buf);
void buffer_undo(struct Buffer *buf);
void buffer_set_to_state(struct Buffer *buf, struct BufferState *state);

void check_type_amt_undo(struct Buffer *buf);

#endif /* UNDO_H_ */
