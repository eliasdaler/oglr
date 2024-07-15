#version 460 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

void main()
{
    gl_FragDepth = length(vec3(inPos) - vec3(3.f, 3.5f, 2.f)) / 20.0;
}

