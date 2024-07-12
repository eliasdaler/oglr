#include "GraphicsUtil.h"

#include "CPUMesh.h"
#include "GPUMesh.h"

#include "ImageLoader.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

namespace
{
std::string readFileToString(const std::filesystem::path& path)
{
    // open file
    std::ifstream f(path);
    if (!f.good()) {
        std::cerr << "Failed to open shader file from " << path << std::endl;
        return {};
    }
    // read whole file into string buffer
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

} // end of anonymous namespace

namespace gfx
{
GLuint loadShaderProgram(
    const std::filesystem::path& vertShaderPath,
    const std::filesystem::path& fragShaderPath,
    const char* debugName)
{
    const auto vertexShader = gfx::compileShader(vertShaderPath, GL_VERTEX_SHADER);
    if (vertexShader == 0) {
        return 0;
    }

    const auto fragShader = gfx::compileShader(fragShaderPath, GL_FRAGMENT_SHADER);
    if (fragShader == 0) {
        return 0;
    }

    GLuint shaderProgram = glCreateProgram();
    gfx::setDebugLabel(GL_PROGRAM, shaderProgram, debugName);

    // link
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragShader);
    glLinkProgram(shaderProgram);

    // check for linking errors
    int success{};
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetShaderiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(logLength + 1, '\0');
        glGetProgramInfoLog(shaderProgram, logLength, NULL, &log[0]);
        std::cout << "Shader linking failed: " << log << std::endl;
        std::exit(1);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragShader);

    return shaderProgram;
}

GLuint compileShader(const std::filesystem::path& path, GLenum shaderType)
{
    GLint shader = glCreateShader(shaderType);

    const auto sourceStr = readFileToString(path);
    const char* sourceCStr = sourceStr.c_str();
    glShaderSource(shader, 1, &sourceCStr, NULL);

    glCompileShader(shader);

    // check for shader compile errors
    int success{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength{};
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(logLength + 1, '\0');
        glGetShaderInfoLog(shader, logLength, NULL, &log[0]);
        std::cout << "Failed to compile shader " << path << ": " << log << std::endl;
        return 0;
    }
    setDebugLabel(GL_SHADER, shader, path.string());
    return shader;
}

GPUBuffer allocateBuffer(std::size_t size, const void* data, const char* debugName)
{
    GLuint buffer;
    glCreateBuffers(1, &buffer);
    glNamedBufferStorage(buffer, (GLsizeiptr)size, data, GL_DYNAMIC_STORAGE_BIT);
    if (debugName) {
        setDebugLabel(GL_BUFFER, buffer, debugName);
    }
    return {
        .buffer = buffer,
        .size = size,
    };
}

int getAlignedSize(std::size_t elementSize, std::size_t align)
{
    if (align == 0) {
        return elementSize;
    }

    if (elementSize < align) {
        return align;
    }
    if (elementSize % align == 0) {
        return elementSize;
    }
    return ((elementSize / align) + 1) * align;
}

std::size_t BumpAllocator::append(void* data, std::size_t size, std::size_t align)
{
    while (currentOffset + size > allocatedData.size()) {
        resize(allocatedData.size() * 2);
    }

    std::memcpy(allocatedData.data() + currentOffset, data, size);

    const auto prevOffset = currentOffset;
    currentOffset += getAlignedSize(size, align);
    return prevOffset;
}

void BumpAllocator::clear()
{
    currentOffset = 0;
}

void BumpAllocator::resize(const std::size_t newSize)
{
    if (newSize > allocatedData.size()) {
        allocatedData.resize(newSize);
    }
}

void setDebugLabel(GLenum identifier, GLuint name, std::string_view label)
{
    glObjectLabel(identifier, name, label.size(), label.data());
}

GLuint loadTextureFromFile(const std::filesystem::path& path)
{
    const auto imageData = util::loadImage(path);
    if (!imageData.pixels) {
        std::cout << "Failed to load image from " << path << "\n";
        return 0;
    }

    GLuint texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);

    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLfloat maxAniso;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    glTextureParameterf(texture, GL_TEXTURE_MAX_ANISOTROPY, std::min(8.f, maxAniso));

    const auto maxExtent = std::max(imageData.width, imageData.height);
    const auto mipLevels = (std::uint32_t)std::floor(std::log2(maxExtent)) + 1;

    glTextureStorage2D(texture, mipLevels, GL_SRGB8_ALPHA8, imageData.width, imageData.height);
    glTextureSubImage2D(
        texture,
        0,
        0,
        0,
        imageData.width,
        imageData.height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        imageData.pixels);

    glGenerateTextureMipmap(texture);

    return texture;
}

GPUMesh uploadMeshToGPU(const CPUMesh& cpuMesh)
{
    std::vector<GPUVertex> vertices;
    vertices.reserve(cpuMesh.vertices.size());
    for (const auto& vr : cpuMesh.vertices) {
        vertices.push_back(GPUVertex{
            .position = vr.position,
            .uv_x = vr.uv.x,
            .normal = vr.normal,
            .uv_y = vr.uv.y,
        });
    }

    auto vertexBuffer = allocateBuffer(sizeof(GPUVertex) * vertices.size(), vertices.data());
    auto indexBuffer =
        allocateBuffer(sizeof(std::uint32_t) * cpuMesh.indices.size(), cpuMesh.indices.data());

    return GPUMesh{
        .vertexBuffer = vertexBuffer,
        .indexBuffer = indexBuffer,
        .numIndices = static_cast<std::uint32_t>(cpuMesh.indices.size()),
        .aabb = util::calculateAABB(cpuMesh),
    };
}

void setGlobalState(const GlobalState& state)
{
    if (state.depthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    glDepthMask(state.depthWriteEnabled ? GL_TRUE : GL_FALSE);

    if (state.cullingEnabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK); // TODO: allow to specify face?
    } else {
        glDisable(GL_CULL_FACE);
    }

    if (state.blendEnabled) {
        glEnable(GL_BLEND);
        // TODO: allow to specify blend func?
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

} // end of namespace gfx
