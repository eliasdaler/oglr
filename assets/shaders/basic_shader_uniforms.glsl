#include "light.glsl"
#include "vertex.glsl"

layout (binding = 0, std140) uniform CameraData
{
    mat4 projection;
    mat4 view;
    vec4 cameraPos;
};

#define MAX_LIGHTS 32

layout (binding = 1, std140) uniform LightData
{
    vec3 ambientColor;
    float ambientIntensity;

    Light sunLight;
    mat4 spotLightSpaceTM;

    Light lights[MAX_LIGHTS];
};

layout (binding = 2, std140) uniform PerObjectData
{
    mat4 model;
    vec4 props; // x - alpha, yzw - unused
};

layout(binding = 3, std430) readonly buffer VertexData {
    Vertex vertices[];
};