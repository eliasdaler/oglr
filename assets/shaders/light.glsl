#ifndef LIGHT_GLSL
#define LIGHT_GLSL


#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

struct Light {
    vec3 position;
    float intensity;

    vec3 dir; // directional only
    float range; // point light only

    vec3 color;
    int type;

    vec2 scaleOffset; // spot light only
    // index into LightData.lightSpaceTMs if == MAX_SHADOW_CASTING_LIGHTS - no shadow
    uint lightSpaceTMsIdx;
    uint shadowMapIdx;

    vec4 pointLightProjBR; // bottom-right 4 elements of point light camera projection matrix
                         // (.x == m[2][2], .y = m[3][2], .z = m[2][3], .w = m[3][3] )
};

#endif // LIGHT_GLSL
