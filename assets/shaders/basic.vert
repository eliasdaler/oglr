#version 460 core

struct Vertex {
    vec4 position;
    vec2 uv;
    vec2 padding;
};

layout(binding = 0, std430) readonly buffer ssbo1 {
    Vertex vertices[];
};

layout (location = 0) uniform mat4 vp;
layout (location = 1) uniform mat4 model;

layout (location = 0) out vec2 outUV;

void main()
{
   vec3 pos = vec3(vertices[gl_VertexID].position);
   gl_Position = vp * model * vec4(pos, 1.0);
   outUV = vertices[gl_VertexID].uv;
}
