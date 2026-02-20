#version 460 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 model; // Move the circle
uniform mat4 projection; // Handle window stretching

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}