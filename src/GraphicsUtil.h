#pragma once

#include <filesystem>

#include <glad/gl.h>

struct CPUMesh;
struct GPUMesh;

namespace gfx
{
GLuint compileShader(const std::filesystem::path& path, GLenum shaderType);

GLuint allocateBuffer(std::size_t size, const char* debugName);
// Returns a size of an element which it should have to respect uboAlignment
// For example:
// * getUBOArrayElementSize(192, 256) == 256
// * getUBOArrayElementSize(480, 256) == 512
int getUBOArrayElementSize(std::size_t elementSize, int uboAlignment);

// debug stuff
void setDebugLabel(GLenum identifier, GLuint name, std::string_view label);

struct GLDebugGroup {
    GLDebugGroup(const char* name) { glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name); }
    ~GLDebugGroup() { glPopDebugGroup(); }
};

// more high level functions
GLuint loadTextureFromFile(const std::filesystem::path& path);
GPUMesh uploadMeshToGPU(const CPUMesh& cpuMesh);
}

#define GL_DEBUG_GROUP(x) gfx::GLDebugGroup g{x};
