#version 460 core

#include "basic_shader_uniforms.glsl"

layout (location = 0) out vec3 outPos;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outNormal;

void main()
{
    Vertex v = vertices[gl_VertexID];
    vec4 worldPos = model * vec4(v.position, 1.0);

    gl_Position = projection * view * worldPos;

    outPos = vec3(worldPos);
    outUV = vec2(v.uv_x, v.uv_y);
    outNormal = (model * vec4(v.normal, 0.0)).xyz;
}
