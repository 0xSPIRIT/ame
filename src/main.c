#include <stdio.h>
#include <stdbool.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "globals.h"
#include "buffer.h"
#include "util.h"

int main(int argc, char **argv) {
    bool running = true;
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    char title[256];
    sprintf(title, "ame - %s", argv[1]);
    window = SDL_CreateWindow(title,
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              window_width,
                              window_height,
                              SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

    struct Buffer *buf = buffer_allocate(argv[1]);
    buffer_load_file(buf, argv[1]);

    font = TTF_OpenFont("consola.ttf", 18);
    TTF_SizeText(font, " ", &font_w, &font_h);

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
            buffer_handle_input(buf, &event);
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        buffer_draw(buf);
        SDL_RenderPresent(renderer);

        buf->y_scroll = lerp(buf->y_scroll, buf->target_y_scroll, 0.5);

        Uint32 end = SDL_GetTicks();
        Uint32 d = end-start;
        timer += d;
        if (timer >= 1000) {
            printf("FPS: %d\n", fps);
            fps = 0;
            timer = 0;
        } else {
            fps++;
        }
    }

    buffer_deallocate(buf);

    TTF_CloseFont(font);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);

    SDL_Quit();
    TTF_Quit();

    return 0;
}