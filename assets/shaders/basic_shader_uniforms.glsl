#include "light.glsl"
#include "vertex.glsl"

layout (binding = 0, std140) uniform GlobalSceneData
{
    mat4 projection;
    mat4 view;
    vec4 cameraPos;

    vec3 ambientColor;
    float ambientIntensity;

    Light sunLight;
    Light pointLight;
    Light spotLight;
};

layout (binding = 1, std140) uniform PerObjectData
{
    mat4 model;
    vec4 props; // x - alpha, yzw - unused
};

layout(binding = 2, std430) readonly buffer VertexData {
    Vertex vertices[];
};
