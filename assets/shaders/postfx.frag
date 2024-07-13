#version 460 core

layout (location = 0) in vec2 inUV;

layout (location = 0) uniform sampler2D tex;

layout (location = 0) out vec4 fragColor;

void main()
{
    vec3 texColor = texture(tex, inUV).rgb;

    // B/W effect
    // float average = 0.2126 * texColor.r + 0.7152 * texColor.g + 0.0722 * texColor.b;
    // fragColor = vec4(average, average, average, 1.0);

    fragColor.rgb = texColor;
}

