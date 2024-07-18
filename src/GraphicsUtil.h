#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include <glad/gl.h>

struct CPUMesh;
struct GPUMesh;
struct GPUBuffer;

namespace gfx
{
GLuint loadShaderProgram(
    const std::filesystem::path& vertShaderPath,
    const std::filesystem::path& fragShaderPath,
    const char* debugName);
GLuint compileShader(const std::filesystem::path& path, GLenum shaderType);

GPUBuffer allocateBuffer(
    std::size_t size,
    const void* data = nullptr,
    const char* debugName = nullptr);
// Returns a size of an element which it should have to respect alignment of "align" bytes
// For example:
// * getAlignedSize(192, 256) == 256
// * getAlignedSize(480, 256) == 512
int getAlignedSize(std::size_t elementSize, std::size_t align);

class BumpAllocator {
public:
    template<typename T>
    std::size_t append(const T& obj, std::size_t align = 0)
    {
        return append((void*)(&obj), sizeof(T));
    }

    // returns offset into allocatedData
    std::size_t append(void* data, std::size_t size, std::size_t align = 0);

    void resize(const std::size_t allocatedSize);
    void clear();

    std::span<std::uint8_t> getData() { return {allocatedData.begin(), currentOffset}; }

private:
    std::vector<std::uint8_t> allocatedData;
    std::size_t currentOffset{0};
};

// debug stuff
void setDebugLabel(GLenum identifier, GLuint name, std::string_view label);

struct GLDebugGroup {
    GLDebugGroup(const char* name) { glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name); }
    ~GLDebugGroup() { glPopDebugGroup(); }
};

// more high level functions
GLuint loadTextureFromFile(const std::filesystem::path& path);
GPUMesh uploadMeshToGPU(const CPUMesh& cpuMesh);

struct GlobalState {
    bool depthTestEnabled{true};
    bool depthWriteEnabled{true};
    GLenum depthFunc{GL_LESS};
    bool cullingEnabled{true};
    bool blendEnabled{false};
};

void setGlobalState(const GlobalState& state);

}

#define GL_DEBUG_GROUP(x) gfx::GLDebugGroup g{x};
