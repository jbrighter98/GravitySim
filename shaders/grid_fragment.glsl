out vec4 FragColor;

uniform mat4 invView;
uniform mat4 invProj;
uniform vec2 u_resolution;

void main() {
    // 1. Map pixel coordinates (0 to Width) to NDC (-1 to 1)
    vec2 ndc = (gl_FragCoord.xy / u_resolution) * 2.0 - 1.0;

    // 2. "Unproject" to find where this pixel is in the WORLD
    // Multiplying by the inverse of our camera matrices reverses the camera move
    vec4 worldPos = invView * invProj * vec4(ndc, 0.0, 1.0);
    
    // 3. Draw a line every 1.0 unit
    vec2 grid = abs(fract(worldPos.xy - 0.5) - 0.5) / fwidth(worldPos.xy);
    float line = min(grid.x, grid.y);

    // 4. Create the final color (Gray)
    float alpha = 1.0 - smoothstep(0.0, 1.0, line);
    
    // Don't draw if it's not a line (makes it transparent for the background)
    if (alpha < 0.1) discard;

    FragColor = vec4(0.5, 0.5, 0.5, alpha * 0.5); 
    //FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}