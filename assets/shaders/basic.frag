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

// Thanks to Jasper for supplying this function
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
    // OpenGL stuff:
    uv.y = 1 - uv.y;
}

float calculateOcclusionPoint(vec3 fragPos, vec3 lightPos, float NoL, uint startIndex, vec4 pointLightProjBR) {
    uint faceIndex;
    vec2 uv;
    float currentDepth;

    vec3 fragToLight = fragPos - lightPos;
    calcPointLightLookupInfo(fragToLight, faceIndex, uv, currentDepth);

    // Calculate depth in light's space
    float proj22 = pointLightProjBR.x;
    float proj32 = pointLightProjBR.y;
    float proj23 = pointLightProjBR.z;
    float proj33 = pointLightProjBR.w;
    float Z = -currentDepth;
    float z = Z * proj22 + proj32;
    float w = Z * proj23 + proj33;
    float depthBufferZ = (z/w) * 0.5 + 0.5; // from [-1;1] to [0;1]

    float bias = 0.001 * tan(acos(NoL));
    bias = clamp(bias, 0.0, 0.1);

	return texture(shadowMapTex, vec4(uv, startIndex + faceIndex, depthBufferZ - bias));
}

void main()
{
    vec3 fragPos = inPos;
    vec3 n = normalize(inNormal);
    vec3 v = normalize(cameraPos.xyz - fragPos);
    vec3 diffuse = texture(tex, inUV).rgb;

    fragColor.rgb = vec3(0.f, 0.f, 0.f);

    fragColor.rgb += calculateLight(fragPos, n, v, diffuse, sunLight, 1.0);

    // other lights
    vec2 screenSize = vec2(1280, 960);
    float TILE_SIZE = 64;
    vec2 screenCoord = gl_FragCoord.xy;
    screenCoord.y = screenSize.y - screenCoord.y;
    float tilesX = ceil(screenSize.x / TILE_SIZE);
    float tileIdx = floor(screenCoord.x / TILE_SIZE) +
                    floor(screenCoord.y / TILE_SIZE) * tilesX;

    LightTileData td = tileLightData[int(tileIdx)];
    int lightsTotal = 0;
    for (int i = 0; i < 16; i++) {
        int idx = td.lightIdx[i];
        if (idx == -1) { continue; }
        ++lightsTotal;

        Light light = lights[idx];

        float occlusion = 1.0;
        if (light.shadowMapIdx != SHADOW_MAP_ARRAY_LAYERS) {
            float NoL = dot(n, normalize(light.position - fragPos));
            if (light.type == LIGHT_TYPE_SPOT) {
                occlusion = calculateOcclusion(fragPos,
                        lightSpaceTMs[light.lightSpaceTMsIdx], NoL, light.shadowMapIdx);
            } else if (light.type == LIGHT_TYPE_POINT) {
                occlusion = calculateOcclusionPoint(fragPos, light.position,
                        NoL, light.shadowMapIdx, light.pointLightProjBR);
            }
        }

        fragColor.rgb += calculateLight(fragPos, n, v, diffuse, lights[idx], occlusion);
    }

    fragColor.rgb += diffuse * ambientColor * ambientIntensity;

#if 0
    if (lightsTotal != 0) {
        float level = min(float(lightsTotal) / 16.f, 1.f) * 3.14159265/2.;
        vec3 col;
        col.r = sin(level);
        col.g = sin(level*2.);
        col.b = cos(level);
        fragColor.rgb = mix(fragColor.rgb, col, 0.75);
    }
#endif


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

    if (td.lightIdx[0] == -1) {
        // fragColor.r *= 5.0;
    }

    fragColor.a = props.x;
}
