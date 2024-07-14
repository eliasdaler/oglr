#version 460 core

#include "basic_shader_uniforms.glsl"
#include "lighting.glsl"

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

layout (location = 0) out vec4 fragColor;

layout (location = 1) uniform sampler2D tex;
layout (location = 2) uniform sampler2D goboTex;
layout (location = 3) uniform sampler2D shadowMapTex;

float calculateOcclusion(vec3 fragPos, mat4 lightSpaceTM, float NoL) {
    vec4 fragPosLightSpace = lightSpaceTM * vec4(fragPos, 1.f);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    projCoords = projCoords * 0.5 + 0.5; // from [-1;1] to [0;1]

    if (projCoords.z >= 1) {
        return 1.0;
    }

    float bias = 0.001 * tan(acos(NoL));
    bias = clamp(bias, 0.0, 0.1);

    float currentDepth = projCoords.z;
    float closestDepth = texture(shadowMapTex, projCoords.xy).r;
    float visibility = 1.0;
    return currentDepth - bias > closestDepth ? 0.0 : 1.0;
}

void main()
{
    vec3 fragPos = inPos;
    vec3 n = normalize(inNormal);
    vec3 v = normalize(cameraPos.xyz - fragPos);
    vec3 diffuse = texture(tex, inUV).rgb;

    float occlusion = 1.0;
    if (light.props.x == 1.0) {
        float NoL = dot(n, normalize(light.position - fragPos));
        occlusion = calculateOcclusion(fragPos, lightSpaceTM, NoL);
    }

    fragColor.rgb = calculateLight(fragPos, n, v, diffuse, light, occlusion);

    fragColor.a = props.x;
}
