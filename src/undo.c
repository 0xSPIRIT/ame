#include "undo.h"

#include "util.h"

struct BufferState *undo_start, *undo_head;
int type_amt = 0;

struct BufferState *buffer_save_undo_state(struct Buffer *buf) {
    struct BufferState *state = alloc(1, sizeof(struct BufferState));
    state->cap = 512;
    state->len = 0;
    state->str = alloc(state->cap, 1);
    state->point = *buffer_curr_point(buf);
    
    struct Line *line;
    for (line = buf->start_line; line; line = line->next) {
        state->len += line->len;
        while (state->len >= state->cap) {
            state->cap *= 2;
            state->str = realloc(state->str, state->cap);
        }
        strcpy(state->str, line->str);
    }
    
    if (!undo_start) {
        undo_start = state;
        state->prev = NULL;
    } else {
        undo_head->next = state;
        state->prev = undo_head;
    }
    
    undo_head = state;
    return state;
}

void buffer_undo(struct Buffer *buf) {
    if (!undo_head->prev) return;
    buffer_set_to_state(buf, undo_head);
    struct BufferState *prev = undo_head->prev;
    dealloc(undo_head->str);
    dealloc(undo_head);
    undo_head = prev;
    undo_head->next = NULL;
    type_amt = 0;
}

void buffer_set_to_state(struct Buffer *buf, struct BufferState *state) {
    struct Line *line, *next;
    for (line = buf->start_line; line; line = next) {
        next = line->next;
        line_deallocate(line);
    }
    buf->start_line = line_allocate(buf);

    line = buf->start_line;

    char *str = state->str;        
    while (*str) {
        if (*str == '\n') {
            buffer_newline(buf);
        } else {
            line_type(line, line->len, *str, 0);
        }
        ++str;
    }
    
    *buffer_curr_point(buf) = state->point;
}

void check_type_amt_undo(struct Buffer *buf) {
    type_amt++;
    if (type_amt >= UNDO_TYPE_AMOUNT) {
        buffer_save_undo_state(buf);
        type_amt = 0;
    }
}
