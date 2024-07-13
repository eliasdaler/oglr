#version 460 core

#include "basic_shader_uniforms.glsl"
#include "lighting.glsl"

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

layout (location = 0) out vec4 fragColor;

layout (location = 1) uniform sampler2D tex;
layout (location = 2) uniform sampler2D goboTex;

void main()
{
    vec3 fragPos = inPos;
    vec3 n = normalize(inNormal);
    vec3 v = normalize(cameraPos.xyz - fragPos);
    vec3 diffuse = texture(tex, inUV).rgb;

    fragColor.rgb = calculateLight(fragPos, n, v, diffuse, sunLight);
    fragColor.rgb += calculateLight(fragPos, n, v, diffuse, pointLight);
    // fragColor.rgb += calculateLight(fragPos, n, v, diffuse, spotLight);

    // ambient
    fragColor.rgb += diffuse * ambientColor * ambientIntensity;

    // gobo
    vec4 fragPosLightSpace = spotLightSpaceTM * vec4(fragPos, 1.f);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; // from [-1;1] to [0;1]
    if (projCoords.x > 0 && projCoords.x < 1 &&
        projCoords.y > 0 && projCoords.y < 1 &&
        projCoords.z > 0 && projCoords.z < 1) {
        vec3 goboLight = calculateLight(fragPos, n, v, diffuse, spotLight);
        goboLight *= texture(goboTex, projCoords.xy).rgb;
        fragColor.rgb += goboLight;
    }

    fragColor.a = props.x;
}
