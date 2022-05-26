#include <stdio.h>
#include <stdbool.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "globals.h"
#include "buffer.h"
#include "minibuffer.h"
#include "util.h"

int main(int argc, char **argv) {
    bool running = true;
    char buffer_name[256], file_name[256];
    
    if (argc == 2) {
        strcpy(file_name, argv[1]);
        remove_directory(buffer_name, file_name);
    } else {
        strcpy(buffer_name, "*scratch*");
    }

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    char title[512];
    sprintf(title, "%s - ame", buffer_name);
    window = SDL_CreateWindow(title,
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              window_width,
                              window_height,
                              SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        font = TTF_OpenFont("consola.ttf", 18);
    TTF_SizeText(font, " ", &font_w, &font_h);

    headbuf = buffer_allocate(buffer_name);
    if (argc == 2) {
        buffer_load_file(headbuf, file_name);
    }
    curbuf = headbuf;

    minibuffer_allocate();
    prevbuf = minibuf;

    printf("Font width: %d, Font height: %d\n", font_w, font_h);

    Uint32 start = 0;
    Uint32 timer = 0;
    int fps = 0;
    while (running) {
        start = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                window_width = event.window.data1;
                window_height = event.window.data2;
            }
            buffer_handle_input(curbuf, &event);
            minibuffer_handle_input(&event);
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        /* Draw the main buffer, and the minibuffer. */
        if (curbuf != minibuf) {
            buffer_draw(curbuf);
            buffer_draw(prevbuf);
        } else {
            buffer_draw(prevbuf);
            buffer_draw(curbuf);
        }

        SDL_RenderPresent(renderer);

        /* TODO: Put these in a better place (in the handle_input method?) */
        curbuf->scroll.y = lerp(curbuf->scroll.y, curbuf->scroll.target_y, 0.25);
        curbuf->scroll.x = lerp(curbuf->scroll.x, curbuf->scroll.target_x, 0.25);

        Uint32 end = SDL_GetTicks();
        Uint32 d = end-start;
        timer += d;
        if (timer >= 1000) {
            fps = 0;
            timer = 0;
        } else {
            fps++;
        }
    }

    struct Buffer *buf = headbuf;
    while (buf) {
        struct Buffer *next = buf->next;
        buffer_deallocate(buf);
        buf = next;
    }
    minibuffer_deallocate();

    TTF_CloseFont(font);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);

    SDL_Quit();
    TTF_Quit();

    return 0;
}