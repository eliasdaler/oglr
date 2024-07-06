#version 460 core

layout (location = 0) in vec2 inUV;
out vec4 fragColor;

layout (location = 2) uniform sampler2D tex;

void main()
{
   fragColor = texture(tex, inUV);
}
