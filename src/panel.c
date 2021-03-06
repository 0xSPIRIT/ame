#include "panel.h"

#include "globals.h"
#include "buffer.h"
#include "minibuffer.h"
#include "modeline.h"

struct Buffer *panel_left  = NULL, 
              *panel_right = NULL;

void panel_swap_focus() {
    if (panel_left == panel_right) {
        curbuf->curview = !curbuf->curview;
        buffer_limit_point(curbuf);
        return;
    }
    
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

    SDL_Texture *left = NULL, *right = NULL;

    left = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, window_width / panel_count(), window_height);
    if (panel_count())
        right = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, window_width / panel_count(), window_height);

    /* Draw the main buffer(s), and the minibuffer. */
    if (panel_left) {
        int temp = panel_left->curview;
        if (panel_left == panel_right) {
            panel_left->curview = 0;
        }
        SDL_SetRenderTarget(renderer, left);

        SDL_SetRenderDrawColor(renderer, BG.r, BG.b, BG.b, 255);
        SDL_RenderClear(renderer);

        panel_left->x = 0;
        buffer_draw(panel_left, temp);

        if (panel_right) {
            const SDL_Rect r = { window_width/2, 0, window_width, window_height };
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
            SDL_RenderFillRect(renderer, &r);
        }
        if (panel_left == panel_right) {
            panel_left->curview = temp;
        }
    }
    if (panel_right) {
        int temp = panel_right->curview;
        if (panel_left == panel_right) {
            panel_right->curview = 1;
        }
        SDL_SetRenderTarget(renderer, right);

        SDL_SetRenderDrawColor(renderer, BG.r, BG.g, BG.b, 255);
        SDL_RenderClear(renderer);

        panel_right->x = 0;

        buffer_draw(panel_right, temp);
        if (panel_left == panel_right) {
            panel_right->curview = temp;
        }
    }

    SDL_SetRenderTarget(renderer, NULL);

    SDL_RenderClear(renderer);

    SDL_Rect left_dst = { 0, 0, window_width / panel_count(), window_height };
    SDL_Rect right_dst = { left_dst.w, 0, window_width / panel_count(), window_height };
    
    SDL_RenderCopy(renderer, left, NULL, &left_dst);
    
    if (right)
        SDL_RenderCopy(renderer, right, NULL, &right_dst);

    if (panel_count() == 2) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer, window_width/2, 0, window_width/2, window_height - font_h*2);
    }

    modeline_draw_rect();

    if (panel_left) {
        int temp = panel_left->curview;
        if (panel_left == panel_right) {
            panel_left->curview = 0;
        }
        
        panel_left->x = 0;
        buffer_modeline_draw(panel_left);
        
        if (panel_left == panel_right) {
            panel_left->curview = temp;
        }
    }
    if (panel_right) {
        int temp = panel_right->curview;
        if (panel_left == panel_right) {
            panel_right->curview = 1;
        }
        
        panel_right->x = window_width/2;
        buffer_modeline_draw(panel_right);
        
        if (panel_left == panel_right) {
            panel_right->curview = temp;
        }
    }

    buffer_draw(minibuf, 0);

    if (left)
        SDL_DestroyTexture(left);
    if (right)
        SDL_DestroyTexture(right);
}

int panel_count() {
    return (panel_left && panel_right) ? 2 : 1;
}

int is_panel_left(struct Buffer *buf) {
    int is_panel_left = buf == panel_left;
    if (panel_left == panel_right) {
        is_panel_left = buf->curview == 0;
    }
    return is_panel_left;
}
