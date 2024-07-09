#version 460 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

out vec4 fragColor;

layout (location = 1) uniform sampler2D tex;

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
    vec3 lightColor = vec3(0.5, 0.6, 0.9);
    float lightIntensity = 2.0;
    vec3 cameraPos = vec3(-5.f, 10.f, -50.f);
    vec3 lightDir = normalize(vec3(1.0, -1.0, 1.0));

    vec3 fragPos = inPos;
    vec3 n = normalize(inNormal);

    vec3 l = -lightDir;
    vec3 v = normalize(cameraPos - fragPos);
    vec3 h = normalize(v + l);

    float NoL = clamp(dot(n, l), 0.0, 1.0);

    vec3 diffuse = texture(tex, inUV).rgb;
    vec3 fr = blinnPhongBRDF(diffuse, n, v, l, h);
    fragColor.rgb = (fr * lightColor) * (lightIntensity * NoL);

    vec3 ambientColor = vec3(0.3, 0.65, 0.8);
    float ambientIntensity = 0.3;
    fragColor.rgb += diffuse * ambientColor * ambientIntensity;
}
