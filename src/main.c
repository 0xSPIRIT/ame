#include <stdio.h>
#include <stdbool.h>
#include <dirent.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "globals.h"
#include "buffer.h"
#include "minibuffer.h"
#include "util.h"
#include "panel.h"

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

    window = SDL_CreateWindow("ame",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              window_width,
                              window_height,
                              SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, 0);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    font = TTF_OpenFont("consola.ttf", 19);
    TTF_SizeText(font, " ", &font_w, &font_h);

    headbuf = buffer_allocate(buffer_name);
    if (argc == 2) {
        buffer_load_file(headbuf, file_name);
    }
    curbuf = headbuf;

    minibuffer_allocate();
    prevbuf = minibuf;

    panel_left = curbuf;
    panel_right = NULL;

    printf("Font width: %d, Font height: %d\n", font_w, font_h);

    Uint32 start = 0;

    while (running) {
        SDL_Event event;
        start = SDL_GetPerformanceCounter();

        int is_event = SDL_PollEvent(&event);
        int did_do_event = is_event;
        int is_scroll = buffer_is_scrolling(curbuf);
        
        while (is_event) {
            if (event.type == SDL_QUIT) {
                running = false;
                goto end_of_running_loop;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                window_width = event.window.data1;
                window_height = event.window.data2;
            }

            buffer_handle_input(curbuf, &event);
            
            minibuffer_handle_input(&event);

            is_event = SDL_PollEvent(&event);
        }
        if (did_do_event || is_scroll || animated_highlights_active) {
            SDL_SetRenderDrawColor(renderer, BG.r, BG.g, BG.b, 255);
            SDL_RenderClear(renderer);

            char cwd[256] = {0};
            get_cwd(cwd);

            char curdir[256] = {0};
            isolate_directory(curdir, curbuf->filename);

            if (curbuf != minibuf && 0 != strcmp(cwd, curdir)) {
                chdir(curdir);
            }

            buffers_draw();    

            SDL_RenderPresent(renderer);

            /* dt = ~1 ms. We want to scale the speed of the lerp to the frametime. */

/*            
            curbuf->scroll.y = damp(curbuf->scroll.y, curbuf->scroll.target_y, 0.000001, dt);
            curbuf->scroll.x = damp(curbuf->scroll.x, curbuf->scroll.target_x, 0.000001, dt);

            prevbuf->scroll.y = damp(prevbuf->scroll.y, prevbuf->scroll.target_y, 0.000001, dt);
            prevbuf->scroll.x = damp(prevbuf->scroll.x, prevbuf->scroll.target_x, 0.000001, dt);
*/

            Uint32 end = SDL_GetPerformanceCounter();
            dt = (double)(1000*(end-start)) / SDL_GetPerformanceFrequency();
        }
  end_of_running_loop:;
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
    
    printf("Leaked allocations: %d\n", get_leaked_allocations());

    return 0;
}
