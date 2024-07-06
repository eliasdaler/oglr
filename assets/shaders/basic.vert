#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;

layout (location = 0) uniform mat4 vp;
layout (location = 1) uniform mat4 model;

layout (location = 0) out vec2 outUV;

void main()
{
   gl_Position = vp * model * vec4(aPos.x, aPos.y, aPos.z, 1.0);
   outUV = aUV;
}
