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
    vec2 padding;
};

#endif // LIGHT_GLSL
