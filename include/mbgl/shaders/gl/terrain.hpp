// Generated code, do not modify this file!
#pragma once
#include <mbgl/shaders/shader_source.hpp>

namespace mbgl {
namespace shaders {

template <>
struct ShaderSource<BuiltIn::TerrainShader, gfx::Backend::Type::OpenGL> {
    static constexpr const char* name = "TerrainShader";
    static constexpr const char* vertex = R"(out vec2 v_uv;
out float v_elevation;

layout (std140) uniform TerrainDrawableUBO {
    highp mat4 u_matrix;
};

layout (std140) uniform TerrainEvaluatedPropsUBO {
    highp float u_exaggeration;
};

layout (location = 0) in vec2 a_pos;

uniform sampler2D u_dem;

void main() {
    // Convert vertex position to normalized texture coordinates [0, 1]
    // The mesh was generated with coordinates from 0 to EXTENT (8192)
    vec2 pos = vec2(a_pos);
    v_uv = pos / 8192.0;

    // Sample DEM texture to get raw RGBA values
    vec4 demSample = texture(u_dem, v_uv);

    // Decode Mapbox Terrain RGB format to get elevation in meters
    // Format: height = -10000 + ((R*256*256 + G*256 + B) * 0.1)
    // DEM values are in range [0, 1] so convert back to [0, 255]
    float r = demSample.r * 255.0;
    float g = demSample.g * 255.0;
    float b = demSample.b * 255.0;

    // Calculate elevation in meters
    float elevationMeters = -10000.0 + ((r * 256.0 * 256.0 + g * 256.0 + b) * 0.1);

    // Apply exaggeration for visible relief (default: 1.0, can be set higher for dramatic effect)
    v_elevation = elevationMeters * u_exaggeration;

    // Create 3D position with elevation as Z coordinate
    gl_Position = u_matrix * vec4(pos.x, pos.y, v_elevation, 1.0);
    //gl_Position = vec4((v_uv.x-0.5), (v_uv.y-0.5), 0.0, 1.0);
}
)";
    static constexpr const char* fragment = R"(in vec2 v_uv;
in float v_elevation;

uniform sampler2D u_map;

void main() {
    // Sample the map texture (render-to-texture output) for the surface color
    // Note: Y-coordinate is flipped (1.0 - y) to match OpenGL convention
    vec4 mapColor = texture(u_map, vec2(v_uv.x, 1.0 - v_uv.y));

    // If map texture has valid data, use it; otherwise fall back to elevation-based coloring
    // Check if alpha is > 0 to detect valid map data
    if (mapColor.a > 0.01) {
        fragColor = vec4(1.0 - mapColor.x, 1.0 - mapColor.y, 1.0 - mapColor.z, 1.0);
        fragColor = vec4(vec3(mapColor), 1.0);
        return;
    }

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

    fragColor =  vec4(1.0 - color.x, 1.0 - color.y, 1.0 - color.z, 1.0);
    
#if defined(OVERDRAW_INSPECTOR)
    fragColor = vec4(1.0);
#endif
}
)";
};

} // namespace shaders
} // namespace mbgl
