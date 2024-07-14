#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#include "light.glsl"

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
    float cd = dot(-lightDir, l);
    float angularAttenuation = clamp(cd * scaleOffset.x + scaleOffset.y, 0.0, 1.0);
    angularAttenuation *= angularAttenuation;
    return angularAttenuation;
}

float calculateAttenuation(vec3 pos, vec3 l, Light light) {
    float dist = length(light.position - pos);
    float atten = calculateDistanceAttenuation(dist, light.range);
    if (light.type == LIGHT_TYPE_SPOT) {
        atten = calculateAngularAttenuation(light.dir, l, light.scaleOffset);
    }
    return atten;
}

vec3 calculateLight(vec3 fragPos, vec3 n, vec3 v, vec3 diffuse, Light light, float occlusion) {
    vec3 l = -light.dir;
    float atten = 1.0;

    if (light.type != LIGHT_TYPE_DIRECTIONAL) {
        l = normalize(light.position - fragPos);
        atten = calculateAttenuation(fragPos, l, light);
    }

    vec3 h = normalize(v + l);
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    vec3 fr = blinnPhongBRDF(diffuse, n, v, l, h);
    return (fr * light.color) * (light.intensity * atten * NoL * occlusion);
}


#endif // LIGHTING_GLSL
