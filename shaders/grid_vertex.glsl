layout (location = 0) in vec3 aPos;

void main() {
    // We pass the position directly. 
    // Since our vertices are -1 to 1, this covers the whole screen.
    gl_Position = vec4(aPos, 1.0);
}