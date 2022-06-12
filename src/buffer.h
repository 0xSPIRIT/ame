#ifndef BUFFER_H_
#define BUFFER_H_

#define BUF_NAME_LEN 256
#define SPACING 3

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "highlight.h"

extern struct Buffer *headbuf, *curbuf, *prevbuf;
extern unsigned buffer_count; /* Includes the minibuffer. */

struct Point {
    struct Line *line; /* The row */
    int pos;           /* The column */
};

struct ScrollBar {
    float x, y;          /* Scroll of the view in the buffer. */
    int target_x, target_y;
};

struct Buffer {
    struct Buffer *prev, *next; /* Linked list of all the buffers. */

    char name[BUF_NAME_LEN];
    char filename[BUF_NAME_LEN];

    int indent_mode;         /* 1 is tab, 0 is spaces. */

    int x, y;                /* Offsets for the entire buffer. */

    struct Line *start_line; /* Doubly linked list of lines */
    int line_count;
    struct Point point;      /* Location of the point (cursor) */
    bool edited;             /* Flag to show if buffer is edited */
    struct ScrollBar scroll;
    struct Mark *mark;       /* Area of selection (called the mark) */

    bool is_completing;            /* Did we just hit tab to complete? Used to cycle through completions. */
    int completion;                /* Amount of cycles into the completion. */
    char completion_original[256]; /* The original completion to compare against while tab-ing through. */

    bool destructive;        /* Is the text wiped away after typing? */
    bool is_singular;        /* Singular means a special buffer where there's only one line, 
                                and something important happens at RETURN. */
    int singular_state;      /* The state of the one-lined special buffer (minibuffer.) */
    int (*on_return)();      /* Function that executes once at RETURN if is_singular=true */

    struct Isearch *search;  /* Incremental search data. */
};

struct Buffer *buffer_allocate(char name[BUF_NAME_LEN]);
void           buffer_deallocate(struct Buffer *buf);
void           buffer_draw(struct Buffer *buf);
void           buffer_handle_input(struct Buffer *buf, SDL_Event *event);
void           buffer_newline(struct Buffer *buf);
void           buffer_paste_text(struct Buffer *buf);
void           buffer_save(struct Buffer *buf);
int            buffer_load_file(struct Buffer *buf, char *file);
void           buffer_set_edited(struct Buffer *buf, bool edited);
void           buffer_debug(struct Buffer *buf);
void           buffer_backspace(struct Buffer *buf);
void           buffer_reset_completion(struct Buffer *buf);
void           buffer_kill(struct Buffer *buf);
bool           buffer_is_scrolling(struct Buffer *buf);
void           buffer_goto_line(struct Buffer *buf, int line);
void           buffer_auto_indent(struct Buffer *buf);
void           buffer_type_tab(struct Buffer *buf);
void           buffer_remove_tab(struct Buffer *buf);

void buffer_forward_word(struct Buffer *buf);
void buffer_backward_word(struct Buffer *buf);

void buffer_point_to_beginning(struct Buffer *buf);
void buffer_point_to_end(struct Buffer *buf);

struct Line {
    struct Line *prev;
    struct Line *next;

    struct Buffer *buf;        /* Buffer the line belongs to */
    int y;                     /* Line number */

    char *str;                 /* Dynamically allocated array of chars */
    int len, cap;

    char pre_str[256];         /* String that displays before the main string. 
                                  Used in minibuffer for prompts. */
    struct Highlight hls[255]; /* A highlight, used in search. */
    int hl_count;              /* Amount of highlights in the line. */
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
