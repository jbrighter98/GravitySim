# GravitySim

This is a 2D gravity simulation made use C++ and OpenGL.
I made use of select files from the imgui github. [Source]([https://example.com](https://github.com/ocornut/imgui))

It is a work in progress. This was made and built useing CMake for VSCode.

## Building

This program is designed to build on Windows 11 using CMake (Linux probably works too, I have not confirmed)

It is also designed to build for web using Enscripten. Here is the following steps to build that worked for me.

Outside project directory:

Clone the Enscripten Repo 
```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
```

Install the latest tools
```
./emsdk install latest
./emsdk activate latest
```

Set the environment variables 
```
./emsdk_env.bat
```

NOTE: Activating and running the bat file need to both be done every time you open a new terminal (for Windows 11)

Go into the GravitySim directory, and run the following to build in the `web_build` folder
```
em++ src/main.cpp src/glad.c deps/imgui.cpp deps/imgui_demo.cpp deps/imgui_impl_glfw.cpp deps/imgui_impl_opengl3.cpp deps/imgui_tables.cpp deps/imgui_widgets.cpp deps/imgui_draw.cpp -o web_build/GravitySimRun.html -s USE_GLFW=3 -s FULL_ES3=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 --preload-file shaders -I./include -I./deps
```
