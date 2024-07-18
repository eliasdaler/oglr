#include "light.glsl"
#include "vertex.glsl"

#define MAX_SHADOW_CASTING_LIGHTS 32
#define SHADOW_MAP_ARRAY_LAYERS 64
#define LIGHT_TILE_SIZE 64

layout (binding = 0, std140) uniform CameraData
{
    mat4 projection;
    mat4 view;
    vec4 cameraPos;
};

layout (binding = 1, std140) uniform GlobalData
{
    vec4 screenSizeAndUnused;

    vec3 ambientColor;
    float ambientIntensity;

    Light sunLight;
    mat4 lightSpaceTMs[MAX_SHADOW_CASTING_LIGHTS];
};

layout (binding = 2, std140) uniform PerObjectData
{
    mat4 model;
    vec4 props; // x - alpha, yzw - unused
};

layout(binding = 3, std430) readonly buffer VertexData {
    Vertex vertices[];
};

layout(binding = 4, std430) readonly buffer LightsData {
    Light lights[];
};

struct LightTileData {
    int lightIdx[16];
};

layout(binding = 5, std430) readonly buffer TileLightsData {
    LightTileData tileLightData[];
};
