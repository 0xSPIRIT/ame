#ifndef ISEARCH_H_
#define ISEARCH_H_

#include "buffer.h"

/* Incremental search near identical to emacs' search. */

struct Isearch {
    char str[512];
};

void buffer_isearch_goto_matching(struct Buffer *buf, char *str);

#endif /* ISEARCH_H_ */