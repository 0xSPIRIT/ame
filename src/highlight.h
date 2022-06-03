#ifndef HIGHLIGHT_H_
#define HIGHLIGHT_H_

struct Highlight {
    int active;
    float time, total_time;
    int pos, len;
};

extern int animated_highlights_active;

void highlight_set(struct Highlight *hl, int pos, int len);
void highlight_update(struct Highlight *hl);
void highlight_draw(struct Highlight hl, int xoff, int yoff);

#endif /* HIGHLIGHT_H_ */