#version 460 core

#include "basic_shader_uniforms.glsl"
#include "lighting.glsl"

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

layout (location = 0) out vec4 fragColor;

layout (location = 1) uniform sampler2D tex;

void main()
{
    vec3 fragPos = inPos;
    vec3 n = normalize(inNormal);
    vec3 v = normalize(cameraPos.xyz - fragPos);
    vec3 diffuse = texture(tex, inUV).rgb;

    fragColor.rgb = calculateLight(fragPos, n, v, diffuse, sunLight);
    fragColor.rgb += calculateLight(fragPos, n, v, diffuse, pointLight);
    fragColor.rgb += calculateLight(fragPos, n, v, diffuse, spotLight);

    // ambient
    fragColor.rgb += diffuse * ambientColor * ambientIntensity;

    fragColor.a = props.x;
}
