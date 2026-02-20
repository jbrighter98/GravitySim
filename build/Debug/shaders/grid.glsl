#version 460 core
out vec4 FragColor;

uniform mat4 invView;
uniform mat4 invProj;
uniform float zoom;

void main() {
    // 1. Convert screen pixel back to world coordinates
    vec2 ndc = (gl_FragCoord.xy / vec2(1200, 800)) * 2.0 - 1.0; // Use actual window width/height
    vec4 worldPos = invView * invProj * vec4(ndc, 0.0, 1.0);
    
    // 2. Create the grid pattern
    float gridSize = 1.0; 
    vec2 grid = abs(fract(worldPos.xy / gridSize - 0.5) - 0.5) / fwidth(worldPos.xy / gridSize);
    float line = min(grid.x, grid.y);
    
    // 3. Draw thin gray lines
    float color = 1.0 - min(line, 1.0);
    if (color < 0.1) discard; // Only keep the lines, transparent elsewhere
    
    FragColor = vec4(0.2, 0.2, 0.2, color * 0.5); // Gray with some alpha
}