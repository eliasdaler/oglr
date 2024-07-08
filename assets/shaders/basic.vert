#version 460 core

struct Vertex {
    vec4 position;
    vec2 uv;
    vec2 padding;
};

layout (binding = 0, std140) uniform SceneData
{
    mat4 projection;
    mat4 view;
};

layout(binding = 1, std430) readonly buffer VertexData {
    Vertex vertices[];
};


layout (location = 0) uniform mat4 model;

layout (location = 0) out vec2 outUV;

void main()
{
   vec3 pos = vec3(vertices[gl_VertexID].position);
   gl_Position = projection * view * model * vec4(pos, 1.0);
   outUV = vertices[gl_VertexID].uv;
}
