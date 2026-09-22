@echo off
g++ ray2.cpp -I. -L"C:/raylib/raylib/src" -lraylib -lopengl32 -lgdi32 -lwinmm -o ray2.exe
ray2.exe