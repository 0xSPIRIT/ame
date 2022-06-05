@echo off
gcc *.c -std=c99 -D_BSD_SOURCE -Wall -Wextra -D_BSD_SOURCE -lSDL2 -lSDL2main -lSDL2_ttf -o ..\bin\ame.exe
