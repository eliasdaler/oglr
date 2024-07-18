#include "DebugRenderer.h"

#include <glm/gtc/type_ptr.hpp>

#include "AABB.h"
#include "Camera.h"
#include "FrustumCulling.h"

void DebugRenderer::init()
{
    linesDrawState = gfx::GlobalState{
        .depthTestEnabled = false,
        .depthWriteEnabled = false,
        .cullingEnabled = false,
        .blendEnabled = true,
    };

    GLint uboAlignment;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uboAlignment);

    linesShader =
        gfx::loadShaderProgram("assets/shaders/lines.vert", "assets/shaders/lines.frag", "lines");
    assert(linesShader);

    const auto lineVertexSize = gfx::getAlignedSize(sizeof(LineVertex), uboAlignment);
    const auto bufSize = lineVertexSize * MAX_LINES * 2;
    linesBuffer = gfx::allocateBuffer(bufSize, 0, "lines");
    lines.reserve(MAX_LINES * 2);
}

void DebugRenderer::cleanup()
{
    glDeleteBuffers(1, &linesBuffer.buffer);
    glDeleteProgram(linesShader);
}

void DebugRenderer::beginDrawing()
{
    lines.clear();
}

void DebugRenderer::addLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
{
    lines.push_back(LineVertex{.pos = from, .color = color});
    lines.push_back(LineVertex{.pos = to, .color = color});
}

void DebugRenderer::addLine(
    const glm::vec3& from,
    const glm::vec3& to,
    const glm::vec4& fromColor,
    const glm::vec4& toColor)
{
    lines.push_back(LineVertex{.pos = from, .color = fromColor});
    lines.push_back(LineVertex{.pos = to, .color = toColor});
}

void DebugRenderer::addQuadLines(
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c,
    const glm::vec3& d,
    const glm::vec4& color)
{
    addLine(a, b, color);
    addLine(b, c, color);
    addLine(c, d, color);
    addLine(d, a, color);
}

void DebugRenderer::addAABBLines(const AABB& aabb, const glm::vec4& color)
{
    auto points = std::vector<glm::vec3>{
        // upper quad
        {aabb.min.x, aabb.min.y, aabb.min.z},
        {aabb.max.x, aabb.min.y, aabb.min.z},
        {aabb.max.x, aabb.min.y, aabb.max.z},
        {aabb.min.x, aabb.min.y, aabb.max.z},
        // lower quad
        {aabb.min.x, aabb.max.y, aabb.min.z},
        {aabb.max.x, aabb.max.y, aabb.min.z},
        {aabb.max.x, aabb.max.y, aabb.max.z},
        {aabb.min.x, aabb.max.y, aabb.max.z},
    };
    std::vector<std::array<std::size_t, 4>> quadIndices = {
        {0, 1, 2, 3},
        {4, 5, 6, 7},
        {1, 2, 6, 5},
        {0, 3, 7, 4},
    };
    for (const auto& quad : quadIndices) {
        addQuadLines(points[quad[0]], points[quad[1]], points[quad[2]], points[quad[3]], color);
    }
}

void DebugRenderer::addFrustumLines(const glm::mat4& viewProj, const glm::vec4& nearFarPlaneColor)
{
    const auto corners = util::calculateFrustumCornersWorldSpace(viewProj);

    // left plane
    addQuadLines(corners[4], corners[5], corners[1], corners[0], glm::vec4{1.f, 1.f, 0.f, 1.f});
    // right plane
    addQuadLines(corners[7], corners[6], corners[2], corners[3], glm::vec4{1.f, 1.f, 0.f, 1.f});
    // near plane
    addQuadLines(corners[0], corners[1], corners[2], corners[3], nearFarPlaneColor);
    // far plane
    addQuadLines(corners[4], corners[5], corners[6], corners[7], nearFarPlaneColor);

#if 0
    // draw normals
    {
        const auto normalLength = 0.25f;
        const auto normalColor = glm::vec4{1.f, 0.0f, 1.0f, 1.f};
        const auto normalColorEnd = glm::vec4{0.f, 1.f, 1.f, 1.f};
        const auto frustum = util::createFrustumFromCamera(camera);

        const auto npc = (corners[0] + corners[1] + corners[2] + corners[3]) / 4.f;
        addLine(npc, npc + frustum.nearFace.n * normalLength, normalColor, normalColorEnd);

        const auto fpc = (corners[4] + corners[5] + corners[6] + corners[7]) / 4.f;
        addLine(fpc, fpc + frustum.farFace.n * normalLength, {1.f, 0.f, 0.f, 1.f}, normalColorEnd);

        const auto lpc = (corners[4] + corners[5] + corners[1] + corners[0]) / 4.f;
        addLine(lpc, lpc + frustum.leftFace.n * normalLength, normalColor, normalColorEnd);

        const auto rpc = (corners[7] + corners[6] + corners[2] + corners[3]) / 4.f;
        addLine(rpc, rpc + frustum.rightFace.n * normalLength, normalColor, normalColorEnd);

        const auto bpc = (corners[0] + corners[4] + corners[7] + corners[3]) / 4.f;
        addLine(bpc, bpc + frustum.bottomFace.n * normalLength, normalColor, normalColorEnd);

        const auto tpc = (corners[1] + corners[5] + corners[6] + corners[2]) / 4.f;
        addLine(tpc, tpc + frustum.topFace.n * normalLength, normalColor, normalColorEnd);
    }
#endif
}

void DebugRenderer::render(const Camera& camera)
{
    static const int VP_UNIFORM_BINDING = 0;
    static const int LINE_VERTEX_DATA_BINDING = 0;

    // realloc buffer if run out of space
    while (sizeof(LineVertex) * lines.size() > linesBuffer.size) {
        glDeleteBuffers(1, &linesBuffer.buffer);
        linesBuffer = gfx::allocateBuffer(linesBuffer.size * 2, nullptr, "lines");
    }

    glNamedBufferSubData(linesBuffer.buffer, 0, sizeof(LineVertex) * lines.size(), lines.data());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, LINE_VERTEX_DATA_BINDING, linesBuffer.buffer);

    glUseProgram(linesShader);
    const auto vp = camera.getViewProj();
    glUniformMatrix4fv(VP_UNIFORM_BINDING, 1, GL_FALSE, glm::value_ptr(vp));
    glDrawArrays(GL_LINES, 0, lines.size());
}
