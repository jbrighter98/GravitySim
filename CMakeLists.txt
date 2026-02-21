cmake_minimum_required(VERSION 3.10)
project(GravitySim VERSION 1.0)

# 1. Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    # Tell Emscripten to use WebGL 2 and bundle the shaders
    set(EMSCRIPTEN_LINK_FLAGS "-s USE_GLFW=3 -s FULL_ES3=1 --preload-file shaders")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}")
endif()


# 2. Find necessary packages (GLFW)
# On Windows/MinGW, ensure GLFW is in your PATH or CMAKE_PREFIX_PATH
include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.8 # Or the latest stable tag
)

# This prevents GLFW from building its own documentation and tests
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# 3. Include directories
# This tells the compiler where to look for your .h/.hpp files
include_directories(include)
# If using GLM as header-only, add its path here too
# include_directories(deps/glm) 

find_package(OpenGL REQUIRED)

# 4. Identify Source Files

set(IMGUI_SOURCES
    deps/imgui.cpp
    deps/imgui_draw.cpp
    deps/imgui_widgets.cpp
    deps/imgui_tables.cpp
    deps/imgui_demo.cpp
    deps/imgui_impl_glfw.cpp
    deps/imgui_impl_opengl3.cpp
)

set(SOURCES 
    src/main.cpp 
    src/glad.c
    ${IMGUI_SOURCES}
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC deps)


# 5. Create the Executable

# 6. Link Libraries
# OpenGL is provided by the OS; glfw is the windowing library
target_link_libraries(${PROJECT_NAME} glfw OpenGL::GL)

# 7. Copy Shaders to Build Folder (Quality of Life)
# This ensures your .glsl files are next to your .exe so they load correctly
file(COPY shaders/ DESTINATION ${CMAKE_BINARY_DIR}/shaders)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/shaders"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders")
