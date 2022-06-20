#ifndef PANEL_H_
#define PANEL_H_

extern struct Buffer *panel_left, *panel_right;

void panel_swap_focus();
void buffers_draw();
int panel_count();
int is_panel_left(struct Buffer *buf);

#endif /* PANEL_H_* */
