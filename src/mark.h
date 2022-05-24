#ifndef MARK_H_
#define MARK_H_

#include <stdbool.h>

struct Mark {
    struct Buffer *buf; /* The buffer the mark belongs to. */
    struct Point *start, *end;
    bool active;
    bool swapped; /* If true, start = point, if false, end = point */
};

struct Mark *mark_allocate(struct Buffer *buf);
void         mark_deallocate(struct Mark *mark);
void         mark_set(struct Mark *mark);
void         mark_unset(struct Mark *mark);
char        *mark_get_text(struct Mark *mark);
int          mark_get_length(struct Mark *mark);
void         mark_cut_text(struct Mark *mark);
void         mark_delete_text(struct Mark *mark);
void         mark_draw(struct Mark *mark);
void         mark_swap_ends_if(struct Mark *mark);
void         mark_update(struct Mark *mark);

#endif /* MARK_H_ */