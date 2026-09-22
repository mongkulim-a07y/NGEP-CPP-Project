@echo off
g++ gui_main.cpp -I. -L"C:/raylib/raylib/src" -lraylib -lopengl32 -lgdi32 -lwinmm -o gui_app.exe
gui_app.exe 