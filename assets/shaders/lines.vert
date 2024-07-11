#version 460 core

struct LineVertex {
    vec3 position;
    float unused;
    vec4 color;
};

layout(location = 0) uniform mat4 vp;

layout(binding = 0, std430) readonly buffer LineVertexData {
    LineVertex vertices[];
};

layout (location = 0) out vec4 outColor;

void main()
{
    LineVertex v = vertices[gl_VertexID];
    gl_Position = vp * vec4(v.position, 1.0);
    outColor = v.color;
}

