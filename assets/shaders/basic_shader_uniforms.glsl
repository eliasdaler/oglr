#include "light.glsl"
#include "vertex.glsl"

layout (binding = 0, std140) uniform CameraData
{
    mat4 projection;
    mat4 view;
    vec4 cameraPos;
};

layout (binding = 1, std140) uniform LightData
{
    vec3 ambientColor;
    float ambientIntensity;

    Light sunLight;
    Light pointLight;
    Light spotLight;

    mat4 spotLightSpaceTM;
};

layout (binding = 2, std140) uniform PerObjectData
{
    mat4 model;
    vec4 props; // x - alpha, yzw - unused
};

layout(binding = 3, std430) readonly buffer VertexData {
    Vertex vertices[];
};
