#include "GLDebugCallback.h"

#include <format>
#include <glad/gl.h>
#include <iostream>

namespace
{
void GLDebugMessageCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* msg,
    const void* data)
{
    const char* sourceStr{nullptr};
    const char* typeStr{nullptr};
    const char* severityStr{nullptr};

    switch (source) {
    case GL_DEBUG_SOURCE_API:
        sourceStr = "API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        sourceStr = "WINDOW SYSTEM";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        sourceStr = "SHADER COMPILER";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        sourceStr = "THIRD PARTY";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        sourceStr = "APPLICATION";
        break;
    case GL_DEBUG_SOURCE_OTHER:
        sourceStr = "OTHER";
        break;
    default:
        sourceStr = "UNKNOWN";
        break;
    }

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        typeStr = "ERROR";
        break;

    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        typeStr = "DEPRECATED BEHAVIOR";
        break;

    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        typeStr = "UDEFINED BEHAVIOR";
        break;

    case GL_DEBUG_TYPE_PORTABILITY:
        typeStr = "PORTABILITY";
        break;

    case GL_DEBUG_TYPE_PERFORMANCE:
        typeStr = "PERFORMANCE";
        break;

    case GL_DEBUG_TYPE_OTHER:
        typeStr = "OTHER";
        break;

    case GL_DEBUG_TYPE_MARKER:
        typeStr = "MARKER";
        break;

    default:
        typeStr = "UNKNOWN";
        break;
    }

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        severityStr = "HIGH";
        break;

    case GL_DEBUG_SEVERITY_MEDIUM:
        severityStr = "MEDIUM";
        break;

    case GL_DEBUG_SEVERITY_LOW:
        severityStr = "LOW";
        break;

    case GL_DEBUG_SEVERITY_NOTIFICATION:
        severityStr = "NOTIFICATION";
        break;

    default:
        severityStr = "UNKNOWN";
        break;
    }

    std::cout << std::
            format("{}: {}: {}, raised from {}: {}\n", id, typeStr, severityStr, sourceStr, msg);
}
} // end of anonymous namespace

namespace gl
{
void enableDebugCallback()
{
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(GLDebugMessageCallback, nullptr);

    // don't care about shaders being successfully compiled
    glDebugMessageControl(
        GL_DEBUG_SOURCE_SHADER_COMPILER,
        GL_DEBUG_TYPE_OTHER,
        GL_DEBUG_SEVERITY_NOTIFICATION,
        0,
        nullptr,
        GL_FALSE);

    glDebugMessageControl(
        GL_DEBUG_SOURCE_APPLICATION,
        GL_DONT_CARE,
        GL_DEBUG_SEVERITY_NOTIFICATION,
        0,
        nullptr,
        GL_FALSE);
}
}
