#version 460 core

layout (location = 0) out vec2 outUV;

vec4 fsTrianglePosition(int vtx) {
    float x = -1.0 + float((vtx & 1) << 2);
    float y = -1.0 + float((vtx & 2) << 1);
    return vec4(x, y, 0.0, 1.0);
}
vec2 fsTriangleUV(int vtx) {
    float u = (vtx == 1) ? 2.0 : 0.0; // 0, 2, 0
    float v = (vtx == 2) ? 2.0 : 0.0; // 0, 0, 2
    return vec2(u, v);
}

void main()
{
	gl_Position = fsTrianglePosition(gl_VertexID);
    outUV = fsTriangleUV(gl_VertexID);
}

