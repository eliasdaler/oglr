#version 460 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

layout (location = 0) out vec4 fragColor;

layout (location = 1) uniform sampler2D tex;

#define DIRECTIONAL_LIGHT_TYPE 0
#define POINT_LIGHT_TYPE 1
#define SPOT_LIGHT_TYPE 2

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

layout (binding = 0, std140) uniform GlobalSceneData
{
    mat4 projection;
    mat4 view;
    vec4 cameraPos;

    vec4 ambientColorAndIntensity;

    Light sunLight;
    Light pointLight;
    Light spotLight;
};

layout (binding = 1, std140) uniform PerObjectData
{
    mat4 model;
    vec4 props; // x - alpha, yzw - unused
};

vec3 blinnPhongBRDF(vec3 diffuse, vec3 n, vec3 v, vec3 l, vec3 h) {
    vec3 Fd = diffuse;

    // specular
    float shininess = 128.0;
    vec3 specularColor = diffuse * 0.5;
    float NoH = clamp(dot(n, h), 0, 1);
    vec3 Fr = specularColor * pow(NoH, shininess);

    return Fd + Fr;
}

float calculateDistanceAttenuation(float dist, float range)
{
    float d = clamp(1.0 - pow((dist/range), 4.0), 0.0, 1.0);
    return d / (dist*dist);
}

// See KHR_lights_punctual spec - formulas are taken from it
float calculateAngularAttenuation(
        vec3 lightDir, vec3 l,
        vec2 scaleOffset) {
    float cd = dot(lightDir, l);
    float angularAttenuation = clamp(cd * scaleOffset.x + scaleOffset.y, 0.0, 1.0);
    angularAttenuation *= angularAttenuation;
    return angularAttenuation;
}

float calculateAttenuation(vec3 pos, vec3 l, Light light) {
    float dist = length(light.position - pos);
    float atten = calculateDistanceAttenuation(dist, light.range);
    if (light.type == SPOT_LIGHT_TYPE) {
        atten = calculateAngularAttenuation(light.dir, l, light.scaleOffset);
    }
    return atten;
}

vec3 calculateLight(vec3 fragPos, vec3 n, vec3 v, vec3 diffuse, Light light) {
    vec3 l = -light.dir;
    float atten = 1.0;

    if (light.type != DIRECTIONAL_LIGHT_TYPE) {
        l = normalize(light.position - fragPos);
        atten = calculateAttenuation(fragPos, l, light);
    }

    vec3 h = normalize(v + l);
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    vec3 fr = blinnPhongBRDF(diffuse, n, v, l, h);
    return (fr * light.color) * (light.intensity * atten * NoL);
}

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
    vec3 ambientColor = ambientColorAndIntensity.rgb;
    float ambientIntensity = ambientColorAndIntensity.a;
    // fragColor.rgb += diffuse * ambientColor * ambientIntensity;

    fragColor.a = props.x;
}
