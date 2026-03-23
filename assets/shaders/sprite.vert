#version 450

layout(push_constant) uniform PushConstants {
    vec2 center;
    vec2 size;
    vec4 color;
    vec2 screen_size;
} pc;

layout(location = 0) out vec4 out_color;

void main() {
    vec2 offsets[6] = vec2[](
        vec2(-0.5, -0.5),
        vec2(0.5, -0.5),
        vec2(-0.5, 0.5),
        vec2(-0.5, 0.5),
        vec2(0.5, -0.5),
        vec2(0.5, 0.5)
    );

    vec2 pixel = pc.center + (offsets[gl_VertexIndex] * pc.size);
    vec2 clip = vec2(
        (2.0 * pixel.x / pc.screen_size.x) - 1.0,
        (2.0 * pixel.y / pc.screen_size.y) - 1.0
    );

    gl_Position = vec4(clip, 0.0, 1.0);
    out_color = pc.color;
}
