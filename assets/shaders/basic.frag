#version 460 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

out vec4 fragColor;

layout (location = 1) uniform sampler2D tex;

layout (binding = 0, std140) uniform GlobalSceneData
{
    mat4 projection;
    mat4 view;
    vec4 cameraPos;

    vec4 sunlightColorAndIntensity;
    vec4 sunlightDirAndUnused;
    vec4 ambientColorAndIntensity;
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

void main()
{
    vec3 lightColor = sunlightColorAndIntensity.rgb;
    float lightIntensity = sunlightColorAndIntensity.a;
    vec3 lightDir = sunlightDirAndUnused.xyz;

    vec3 fragPos = inPos;
    vec3 n = normalize(inNormal);

    vec3 l = -lightDir;
    vec3 v = normalize(cameraPos.xyz - fragPos);
    vec3 h = normalize(v + l);

    float NoL = clamp(dot(n, l), 0.0, 1.0);

    vec3 diffuse = texture(tex, inUV).rgb;
    vec3 fr = blinnPhongBRDF(diffuse, n, v, l, h);
    fragColor.rgb = (fr * lightColor) * (lightIntensity * NoL);

    // ambient
    vec3 ambientColor = ambientColorAndIntensity.rgb;
    float ambientIntensity = ambientColorAndIntensity.a;
    fragColor.rgb += diffuse * ambientColor * ambientIntensity;
}
