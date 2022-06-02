#include "panel.h"

#include "globals.h"
#include "buffer.h"
#include "minibuffer.h"
#include "modeline.h"

struct Buffer *panel_left  = NULL, 
              *panel_right = NULL;

void panel_swap_focus() {
    if (curbuf == panel_left && panel_right) {
        curbuf = panel_right;
    } else if (curbuf == panel_right && panel_left) {
        curbuf = panel_left;
    }
}

void buffers_draw() {
    if (panel_right && !panel_left) {
        panel_left = panel_right;
        panel_right = NULL;
    }

    /* Draw the main buffer(s), and the minibuffer. */
    if (panel_left) {
        panel_left->x = 0;
        buffer_draw(panel_left);
    }
    if (panel_right) {
        panel_right->x = window_width/2;
        SDL_Rect r = { panel_right->x, 0, window_width/2, window_height };
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &r);

        buffer_draw(panel_right);
    }

    modeline_draw_rect();

    if (panel_left) {
        panel_left->x = 0;
        buffer_modeline_draw(panel_left);
    }
    if (panel_right) {
        panel_right->x = window_width/2;
        buffer_modeline_draw(panel_right);
    }

    buffer_draw(minibuf);
}