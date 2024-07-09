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
    vec4 cameraPos;

    vec4 sunlightColorAndIntensity;
    vec4 sunlightDirAndUnused;
    vec4 ambientColorAndIntensity;
};

layout (binding = 1, std140) uniform PerObjectData
{
    mat4 model;
};

layout(binding = 2, std430) readonly buffer VertexData {
    Vertex vertices[];
};

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
