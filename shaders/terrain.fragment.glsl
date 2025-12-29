in vec2 v_uv;
in float v_elevation;

void main() {
    // Sample the map texture (render-to-texture output) for the surface color
    // Note: Y-coordinate is flipped (1.0 - y) to match OpenGL convention
    //vec4 mapColor = texture(u_map, vec2(v_uv.x, 1.0 - v_uv.y));

    // If map texture has valid data, use it; otherwise fall back to elevation-based coloring
    // Check if alpha is > 0 to detect valid map data
    //if (mapColor.a > 0.01) {
    //    fragColor = vec4(mapColor);
    //}

    // Fallback: elevation-based color gradient for debugging
    float normalizedElevation = clamp((v_elevation - 500.0) / 3500.0, 0.0, 1.0);

    vec3 color;
    if (normalizedElevation < 0.33) {
        float t = normalizedElevation / 0.33;
        color = mix(vec3(0.2, 0.4, 0.8), vec3(0.3, 0.7, 0.3), t);
    } else if (normalizedElevation < 0.66) {
        float t = (normalizedElevation - 0.33) / 0.33;
        color = mix(vec3(0.3, 0.7, 0.3), vec3(0.6, 0.5, 0.3), t);
    } else {
        float t = (normalizedElevation - 0.66) / 0.34;
        color = mix(vec3(0.6, 0.5, 0.3), vec3(0.95, 0.95, 0.95), t);
    }

    float gridLine = step(0.98, fract(v_uv.x * 4.0)) + step(0.98, fract(v_uv.y * 4.0));
    color = mix(color, vec3(1.0, 1.0, 1.0), gridLine * 0.5);

    fragColor =  vec4(vec3(color), 1.0);
    
#if defined(OVERDRAW_INSPECTOR)
    fragColor = vec4(1.0);
#endif
}
