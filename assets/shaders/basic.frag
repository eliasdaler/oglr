#version 460 core

#include "basic_shader_uniforms.glsl"
#include "lighting.glsl"

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

layout (location = 0) out vec4 fragColor;

layout (location = 1) uniform sampler2D tex;
layout (location = 2) uniform sampler2D goboTex;
layout (location = 3) uniform sampler2DArrayShadow shadowMapTex;

float calculateOcclusion(vec3 fragPos, mat4 lightSpaceTM, float NoL, uint smIndex) {
    vec4 fragPosLightSpace = lightSpaceTM * vec4(fragPos, 1.f);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    projCoords = projCoords * 0.5 + 0.5; // from [-1;1] to [0;1]

    if (projCoords.z >= 1) {
        return 1.0;
    }

    float bias = 0.001 * tan(acos(NoL));
    bias = clamp(bias, 0.0, 0.1);

    float currentDepth = projCoords.z - bias;
    // float closestDepth = texture(shadowMapTex, vec3(projCoords.xy, smIndex)).r;
    // return currentDepth - bias > closestDepth ? 0.0 : 1.0;
	return texture(shadowMapTex, vec4(projCoords.xy, smIndex, currentDepth));
}

vec2 sampleCube(vec3 v, out uint faceIndex)
{
    // https://www.gamedev.net/forums/topic/687535-implementing-a-cube-map-lookup-function/5337472/
    // had to flip x/y axis for OpenGL, though
    vec3 vAbs = abs(v);
    float ma;
    vec2 uv;
    if (vAbs.z >= vAbs.x && vAbs.z >= vAbs.y) {
        faceIndex = v.z < 0.0 ? 5 : 4;
        ma = 0.5 / vAbs.z;
        uv = vec2(v.z < 0.0 ? -v.x : v.x, -v.y);
    } else if (vAbs.y >= vAbs.x) {
        faceIndex = v.y < 0.0 ? 3 : 2;
        ma = 0.5 / vAbs.y;
        uv = vec2(v.x, v.y < 0.0 ? -v.z : v.z);
    } else {
        faceIndex = v.x < 0.0 ? 1 : 0;
        ma = 0.5 / vAbs.x;
        uv = vec2(v.x < 0.0 ? v.z : -v.z, -v.y);
    }

    uv = uv * ma + vec2(0.5f);
    return vec2(1.f) - uv;
}

void calcPointLightLookupInfo(vec3 dir, out uint faceIdx, out vec2 uv, out float depth) {
    depth = max(max(abs(dir.x), abs(dir.y)), abs(dir.z));
    if (dir.x == depth) {
        faceIdx = 0;
        uv = vec2(dir.z, -dir.y) / dir.x;
    } else if (-dir.x == depth) {
        faceIdx = 1;
        uv = vec2(-dir.z, -dir.y) / -dir.x;
    } else if (dir.y == depth) {
        faceIdx = 2;
        uv = vec2(-dir.x, dir.z) / dir.y;
    } else if (-dir.y == depth) {
        faceIdx = 3;
        uv = vec2(-dir.x, -dir.z) / -dir.y;
    } else if (dir.z == depth) {
        faceIdx = 4;
        uv = vec2(-dir.x, -dir.y) / dir.z;
    } else { // if(-dir.z == depth)
        faceIdx = 5;
        uv = vec2(dir.x, -dir.y) / -dir.z;
    }
    uv = uv * vec2(0.5, 0.5) + vec2(0.5, 0.5);
}

float calculateOcclusionPoint(vec3 fragPos, vec3 lightPos, float NoL, uint startIndex) {
    uint faceIndex;
    vec2 uv;
    float currentDepth;

    vec3 fragToLight = fragPos - lightPos;
    calcPointLightLookupInfo(fragToLight, faceIndex, uv, currentDepth);

    // TODO: fix uvs
    uv.y = 1 - uv.y;
    if (faceIndex == 2 || faceIndex == 3) {
        // "up" and "down" have weird orientations currently
        uv.y = 1 - uv.y;
        uv.x = 1 - uv.x;
    }

    float proj22 = -1.0100503;
    float proj32 = -0.201005027;
    float proj23 = -1;
    float proj33 = 0;

    float Z = currentDepth;
    float z = Z*proj22 + proj32;
    float w = Z*proj23 + proj33;

    z = Z*proj22 + proj23;
    w = Z*proj32 + proj33;

    float depthBufferZ = (z/w) * 0.5 + 0.5;

	return texture(shadowMapTex, vec4(uv, startIndex + faceIndex, depthBufferZ));
    // return currentDepth / 100.0;
}

void main()
{
    vec3 fragPos = inPos;
    vec3 n = normalize(inNormal);
    vec3 v = normalize(cameraPos.xyz - fragPos);
    vec3 diffuse = texture(tex, inUV).rgb;

    fragColor.rgb = vec3(0.f, 0.f, 0.f);

    // fragColor.rgb += calculateLight(fragPos, n, v, diffuse, sunLight, 1.0);

    // float NoL = dot(n, normalize(spotLight.position - fragPos));
    // float occlusion = calculateOcclusion(fragPos, spotLightSpaceTM, NoL);
    // fragColor.rgb += calculateLight(fragPos, n, v, diffuse, spotLight, occlusion);

    // other lights
    for (int i = 0; i < MAX_AFFECTING_LIGHTS; i++) {
        int idx = lightIdx[i >> 2][i & 3];
        if (idx > MAX_LIGHTS) { continue; }

        Light light = lights[idx];

        float occlusion = 1.0;
        if (light.lightSpaceTMsIdx != MAX_SHADOW_CASTING_LIGHTS) {
            float NoL = dot(n, normalize(light.position - fragPos));
            if (light.type == LIGHT_TYPE_SPOT) {
                occlusion = calculateOcclusion(fragPos,
                        lightSpaceTMs[light.lightSpaceTMsIdx], NoL, light.lightSpaceTMsIdx);
            } else {
                occlusion = calculateOcclusionPoint(fragPos, light.position,
                        NoL, light.lightSpaceTMsIdx);
                // fragColor.rgb += vec3(occlusion, 0.0, 0.0);
            }
        }

        fragColor.rgb += calculateLight(fragPos, n, v, diffuse, lights[idx], occlusion);
    }

    fragColor.rgb += diffuse * ambientColor * ambientIntensity;

    // fragColor.rgb = fragPos;

    // gobo
    /* vec4 fragPosLightSpace = spotLightSpaceTM * vec4(fragPos, 1.f);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; // from [-1;1] to [0;1]
    if (projCoords.x > 0 && projCoords.x < 1 &&
        projCoords.y > 0 && projCoords.y < 1 &&
        projCoords.z > 0 && projCoords.z < 1) {
        vec3 goboLight = calculateLight(fragPos, n, v, diffuse, spotLight, 1.0);
        goboLight *= texture(goboTex, projCoords.xy).rgb;
        fragColor.rgb += goboLight;
    } */

    fragColor.a = props.x;
}
