#ifndef REPLACE_H_
#define REPLACE_H_

#include "buffer.h"

extern char find[1024];
extern char replace[1024];

int buffer_replace_matching(struct Buffer *buf, char *find, char *replace, bool all);

#endif /* REPLACE_H_ */
