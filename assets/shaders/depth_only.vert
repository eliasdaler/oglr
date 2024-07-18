#version 460 core

#include "basic_shader_uniforms.glsl"

invariant gl_Position;

void main()
{
    Vertex v = vertices[gl_VertexID];
    vec4 worldPos = model * vec4(v.position, 1.0);

    gl_Position = projection * view * worldPos;
}

