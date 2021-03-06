#include "modeline.h"

#include "globals.h"

void modeline_draw_rect() {
    SDL_Rect mode_rect = {
        0, window_height - font_h*2,
        window_width, font_h
    };

    SDL_SetRenderDrawColor(renderer, 255-240, 255-240, 255-240, 255);
    SDL_RenderFillRect(renderer, &mode_rect);
    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
    SDL_RenderDrawRect(renderer, &mode_rect);
}

void buffer_modeline_draw(struct Buffer *buf) {
    char text[512] = {0};
    char line_string[64] = {0};

    sprintf(line_string, "L%d/%d", buf->views[buf->curview].point.line->y+1, buf->line_count);

    strcat(text, buf->name);
    if (buf->edited)
        strcat(text, (char[2]){'*', 0});
    strcat(text, "     ");
    strcat(text, line_string);

    SDL_Surface *surf = TTF_RenderText_Blended(font, text, (SDL_Color){255, 255, 255, 255});
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
    const SDL_Rect dst = (SDL_Rect){
        buf->x + 6, window_height + 1 - font_h*2,
        surf->w, surf->h
    };
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(texture);
}
