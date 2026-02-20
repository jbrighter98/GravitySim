#version 460 core
out vec4 FragColor;
in vec2 TexCoords; // Pass these from vertex shader (range 0 to 1)

uniform vec4 uColor;
uniform float uGlowStrength; // Control how "bright" the glow is

void main() {
    // Calculate distance from center (0.5, 0.5)
    float dist = distance(TexCoords, vec2(0.5));
    
    // Create a smooth radial falloff
    float alpha = smoothstep(0.5, 0.0, dist);
    
    // Apply a power curve to make the center much brighter than the edges
    alpha = pow(alpha, 3.0); 

    FragColor = vec4(uColor.rgb * uGlowStrength, alpha * uColor.a);
}