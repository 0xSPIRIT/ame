#ifndef BUFFER_H_
#define BUFFER_H_

#define BUF_NAME_LEN 256

#include <stdbool.h>
#include <SDL2/SDL.h>

struct Point {
    struct Line *line; /* The row */
    int pos;           /* The column */
};

struct ScrollBar {
    float x, y;          /* Scroll of the view in the buffer. */
    int target_x, target_y;
};

struct Buffer {
    char name[BUF_NAME_LEN];
    char filename[BUF_NAME_LEN];
    struct Line *start_line; /* Doubly linked list of lines */
    struct Point point;      /* Location of the point (cursor) */
    bool edited;             /* Flag to show if buffer is edited */
    struct ScrollBar scroll;
    struct Mark *mark;       /* Area of selection (called the mark) */
};

struct Buffer *buffer_allocate(char name[BUF_NAME_LEN]);
void           buffer_deallocate(struct Buffer *buf);
void           buffer_draw(struct Buffer *buf);
void           buffer_handle_input(struct Buffer *buf, SDL_Event *event);
void           buffer_newline(struct Buffer *buf);
void           buffer_paste_text(struct Buffer *buf);
void           buffer_save(struct Buffer *buf);
void           buffer_load_file(struct Buffer *buf, char *file);
void           buffer_set_edited(struct Buffer *buf, bool edited);
void           buffer_debug(struct Buffer *buf);
void           buffer_backspace(struct Buffer *buf);

void buffer_forward_word(struct Buffer *buf);
void buffer_backward_word(struct Buffer *buf);

struct Line {
    struct Line *prev;
    struct Line *next;

    struct Buffer *buf;   /* Buffer the line belongs to */
    int y;                /* Line number */

    char *str;            /* Dynamically allocated array of chars */
    int len, cap;
};

struct Line *line_allocate(struct Buffer *buf);
void         line_deallocate(struct Line *line);
void         line_remove(struct Line *line);
void         line_type(struct Line *line, int pos, char c);
void         line_type_string(struct Line *line, int pos, char *str);
void         line_delete_char(struct Line *line, int pos);
void         line_delete_chars_range(struct Line *line, int start, int end);
void         line_draw(struct Line *line, int yoff, int x_scroll, int y_scroll);
void         line_debug(struct Line *line);
bool         line_is_empty(struct Line *line);

#endif /* BUFFER_H_ */
