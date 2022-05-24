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

    struct Buffer *buf = buffer_allocate(buffer_name);
    if (argc == 2) {
        buffer_load_file(buf, file_name);
    }
    curbuf = buf;

    minibuffer_allocate();

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

        buffer_draw(buf);
        buffer_draw(minibuf);

        SDL_RenderPresent(renderer);

        buf->scroll.y = lerp(buf->scroll.y, buf->scroll.target_y, 0.5);
        buf->scroll.x = lerp(buf->scroll.x, buf->scroll.target_x, 0.5);

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

    buffer_deallocate(buf);
    minibuffer_deallocate();

    TTF_CloseFont(font);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);

    SDL_Quit();
    TTF_Quit();

    return 0;
}