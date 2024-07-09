#version 460 core

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
};

layout (binding = 0, std140) uniform GlobalSceneData
{
    mat4 projection;
    mat4 view;
};

layout (binding = 1, std140) uniform PerObjectData
{
    mat4 model;
};

layout(binding = 2, std430) readonly buffer VertexData {
    Vertex vertices[];
};


layout (location = 0) out vec2 outUV;

void main()
{
    Vertex v = vertices[gl_VertexID];
    gl_Position = projection * view * model * vec4(v.position, 1.0);
    outUV = vec2(v.uv_x, v.uv_y);
}
