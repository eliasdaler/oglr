#include "App.h"

#include <chrono>
#include <iostream>

#include <glad/gl.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp> // lerp for vec3

#include "FrustumCulling.h"
#include "GLDebugCallback.h"
#include "GlobalAxes.h"
#include "GraphicsUtil.h"
#include "Meshes.h"

namespace
{
constexpr auto WINDOW_WIDTH = 1280;
constexpr auto WINDOW_HEIGHT = 960;

constexpr auto FRAG_TEXTURE_UNIFORM_LOC = 1;

constexpr auto GLOBAL_SCENE_DATA_BINDING = 0;
constexpr auto PER_OBJECT_DATA_BINDING = 1;
constexpr auto VERTEX_DATA_BINDING = 2;

template<typename T, typename RNGType>
std::size_t chooseRandomElement(const std::vector<T>& v, RNGType& rng)
{
    if (v.empty()) {
        return 0;
    }
    std::uniform_int_distribution<std::size_t> dist{0, v.size() - 1};
    return dist(rng);
}

// getStickState({negX, posX}, {negY, posY})
glm::vec2 getStickState(
    std::pair<SDL_Scancode, SDL_Scancode> xAxis,
    std::pair<SDL_Scancode, SDL_Scancode> yAxis)
{
    const Uint8* state = SDL_GetKeyboardState(nullptr);
    glm::vec2 dir;
    if (state[xAxis.first]) {
        dir.x -= 1.f;
    }
    if (state[xAxis.second]) {
        dir.x += 1.f;
    }
    if (state[yAxis.first]) {
        dir.y -= 1.f;
    }
    if (state[yAxis.second]) {
        dir.y += 1.f;
    }
    return dir;
}

enum class SortOrder {
    FrontToBack,
    BackToFront,
};

void sortDrawList(std::vector<DrawInfo>& drawList, SortOrder sortOrder)
{
    std::sort(
        drawList.begin(),
        drawList.end(),
        [sortOrder](const DrawInfo& draw1, const DrawInfo& draw2) {
            if (sortOrder == SortOrder::BackToFront) {
                return draw1.distToCamera > draw2.distToCamera;
            }
            return draw1.distToCamera <= draw2.distToCamera;
        });
}

} // end of anonymous namespace

void App::start()
{
    init();
    run();
    cleanup();
}

void App::init()
{
    rng = std::mt19937{randomDevice()};

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        printf("SDL could not initialize. SDL Error: %s\n", SDL_GetError());
        std::exit(1);
    }

    window = SDL_CreateWindow(
        "App",
        // pos
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        // size
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_SetWindowResizable(window, SDL_TRUE);

    // create gl context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);

    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
    SDL_GL_SetSwapInterval(1);

    // glad
    int gl_version = gladLoaderLoadGL();
    if (!gl_version) {
        std::cout << "Unable to load GL.\n";
        std::exit(1);
    }

    gl::enableDebugCallback();
    glEnable(GL_FRAMEBUFFER_SRGB);

    // make lines thicker (won't work everywhere, but whatever)
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(2.f);

    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uboAlignment);

    { // shaders
        worldShader = gfx::
            loadShaderProgram("assets/shaders/basic.vert", "assets/shaders/basic.frag", "world");
        assert(worldShader);
        solidColorShader = gfx::loadShaderProgram(
            "assets/shaders/basic.vert", "assets/shaders/solid_color.frag", "world");
        assert(solidColorShader);
        linesShader = gfx::
            loadShaderProgram("assets/shaders/lines.vert", "assets/shaders/lines.frag", "lines");
        assert(linesShader);
    }

    { // allocate scene data buffer
        const auto globalSceneDataSize = gfx::getAlignedSize(sizeof(GlobalSceneData), uboAlignment);
        const auto perObjectDataElementSize =
            gfx::getAlignedSize(sizeof(PerObjectData), uboAlignment);
        const auto bufSize = globalSceneDataSize + perObjectDataElementSize * 100;
        sceneDataBuffer = gfx::allocateBuffer(bufSize, nullptr, "sceneData");
        sceneData.resize(bufSize);
    }

    { // allocate scene data buffer
        const auto lineVertexSize = gfx::getAlignedSize(sizeof(LineVertex), uboAlignment);
        const auto bufSize = lineVertexSize * MAX_LINES * 2;
        linesBuffer = gfx::allocateBuffer(bufSize, 0, "lines");
        lines.reserve(MAX_LINES * 2);
    }

    // we still need an empty VAO even for vertex pulling
    glGenVertexArrays(1, &vao);

    const auto texturesToLoad = {
        "assets/images/texture1.png",
        "assets/images/texture2.png",
    };
    for (const auto& texturePath : texturesToLoad) {
        auto texture = gfx::loadTextureFromFile(texturePath);
        if (texture == 0) {
            std::exit(1);
        }
        textures.push_back(texture);
    }

    { // load meshes
        meshes.push_back(gfx::uploadMeshToGPU(util::getCubeMesh()));
        meshes.push_back(gfx::uploadMeshToGPU(util::getStarMesh()));
    }

    // initial state
    glClearDepth(1.f);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    { // init camera
        const auto fovX = 45.f;
        const auto zNear = 0.1f;
        const auto zFar = 1000.f;
        camera.init(fovX, zNear, zFar, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT);
        camera.setPosition({0.f, 0.5f, -10.f});
    }

    { // init test camera
        const auto fovX = 45.f;
        const auto zNear = 0.1f;
        const auto zFar = 7.5f;
        testCamera.init(fovX, zNear, zFar, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT);
        testCamera.setPosition({2.f, 3.f, -3.f});
        // testCamera.setPosition({0.f, 0.f, -5.f});
        testCamera.lookAt(glm::vec3{0.f, 0.f, 0.f});
    }

    auto numObjectsToSpawn = dist(rng);
    numObjectsToSpawn = 0;
    for (int i = 0; i < numObjectsToSpawn; ++i) {
        generateRandomObject();
    }

    spawnCube({0.f, 0.f, 0.f}, 0, 0.5f);
    spawnCube({0.f, 0.f, 2.5f}, 1, 0.7f);
    spawnCube({0.f, 0.f, 5.0f}, 0, 1.f);
    spawnCube({0.f, 0.f, 7.5f}, 0, 0.2f);

    // init lights
    sunlightColor = glm::vec3{0.65, 0.4, 0.3};
    sunlightIntensity = 1.75f;
    sunlightDir = glm::normalize(glm::vec3(1.0, -1.0, 1.0));

    ambientColor = glm::vec3{0.3, 0.65, 0.8};
    ambientIntensity = 0.2f;

    frameStartState = gfx::GlobalState{
        .depthTestEnabled = false,
        .depthWriteEnabled = true,
        .cullingEnabled = false,
        .blendEnabled = false,
    };

    opaqueDrawState = gfx::GlobalState{
        .depthTestEnabled = true,
        .depthWriteEnabled = true,
        .cullingEnabled = true,
        .blendEnabled = false,
    };

    transparentDrawState = gfx::GlobalState{
        .depthTestEnabled = true,
        .depthWriteEnabled = false,
        .cullingEnabled = true,
        .blendEnabled = true,
    };

    linesDrawState = gfx::GlobalState{
        .depthTestEnabled = false,
        .depthWriteEnabled = false,
        .cullingEnabled = false,
        .blendEnabled = true,
    };
}

void App::cleanup()
{
    glDeleteTextures(textures.size(), textures.data());
    for (const auto& mesh : meshes) {
        glDeleteBuffers(1, &mesh.indexBuffer.buffer);
        glDeleteBuffers(1, &mesh.vertexBuffer.buffer);
    }
    glDeleteBuffers(1, &sceneDataBuffer.buffer);

    glDeleteVertexArrays(1, &vao);

    glDeleteProgram(linesShader);
    glDeleteProgram(solidColorShader);
    glDeleteProgram(worldShader);

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void App::run()
{
    // Fix your timestep! game loop
    const float FPS = 60.f;
    const float dt = 1.f / FPS;

    auto prevTime = std::chrono::high_resolution_clock::now();
    float accumulator = dt; // so that we get at least 1 update before render

    isRunning = true;
    while (isRunning) {
        const auto newTime = std::chrono::high_resolution_clock::now();
        frameTime = std::chrono::duration<float>(newTime - prevTime).count();

        accumulator += frameTime;
        prevTime = newTime;

        // moving average
        float newFPS = 1.f / frameTime;
        if (newFPS == std::numeric_limits<float>::infinity()) {
            // can happen when frameTime == 0
            newFPS = 0;
        }
        avgFPS = std::lerp(avgFPS, newFPS, 0.1f);

        if (accumulator > 10 * dt) { // game stopped for debug
            accumulator = dt;
        }

        while (accumulator >= dt) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    isRunning = false;
                    return;
                }
            }

            update(dt);
            accumulator -= dt;
        }

        render();

        if (frameLimit) {
            // Delay to not overload the CPU
            const auto now = std::chrono::high_resolution_clock::now();
            const auto frameTime = std::chrono::duration<float>(now - prevTime).count();
            if (dt > frameTime) {
                SDL_Delay(static_cast<std::uint32_t>(dt - frameTime));
            }
        }
    }
}

void App::update(float dt)
{
    handleFreeCameraControls(dt);

    // spawn random objects
    timer += dt;
    if (timer >= timeToSpawnNewObject) {
        timer = 0.f;
        generateRandomObject();
    }

    // rotate objects
    static const auto rotationSpeed = glm::radians(45.f);
    for (auto& object : objects) {
        // object.transform.heading *= glm::angleAxis(rotationSpeed * dt, glm::vec3{1.f, 1.f, 0.f});
    }

    lines.clear();
    for (auto& object : objects) {
        addAABBLines(object.worldAABB, glm::vec4{1.f, 0.f, 1.f, 1.f});
    }

    addFrustumLines(testCamera);
}

void App::handleFreeCameraControls(float dt)
{
    { // move
        const glm::vec3 cameraWalkSpeed = {10.f, 5.f, 10.f};

        const glm::vec2 moveStickState =
            getStickState({SDL_SCANCODE_A, SDL_SCANCODE_D}, {SDL_SCANCODE_W, SDL_SCANCODE_S});

        const glm::vec2 moveUpDownState =
            getStickState({SDL_SCANCODE_Q, SDL_SCANCODE_E}, {SDL_SCANCODE_W, SDL_SCANCODE_S});

        glm::vec3 moveVector{};
        moveVector += camera.getForward() * (-moveStickState.y);
        moveVector += camera.getRight() * (-moveStickState.x);
        moveVector += math::GLOBAL_UP_DIR * moveUpDownState.x;

        auto pos = camera.getPosition();
        pos += moveVector * cameraWalkSpeed * dt;
        camera.setPosition(pos);
    }

    { // rotate view
        const float rotateYawSpeed{1.75f};
        const float rotatePitchSpeed{1.f};

        const glm::vec2 rotateStickState = getStickState(
            {SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT}, {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN});

        glm::vec2 rotationVelocity;
        rotationVelocity.x = -rotateStickState.x * rotateYawSpeed;
        rotationVelocity.y = rotateStickState.y * rotatePitchSpeed;

        const auto dYaw = glm::angleAxis(rotationVelocity.x * dt, math::GLOBAL_UP_DIR);
        const auto dPitch = glm::angleAxis(rotationVelocity.y * dt, math::GLOBAL_RIGHT_DIR);
        const auto newHeading = dYaw * camera.getHeading() * dPitch;
        camera.setHeading(newHeading);
    }
}

void App::render()
{
    generateDrawList();

    // clear default FBO color and depth
    gfx::setGlobalState(frameStartState);
    glClearColor(97.f / 255.f, 120.f / 255.f, 159.f / 255.f, 1.0f);
    glClearDepth(1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(vao);
    { // render objects
        // object texture will be read from TU0
        glProgramUniform1i(worldShader, FRAG_TEXTURE_UNIFORM_LOC, 0);

        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            GLOBAL_SCENE_DATA_BINDING,
            sceneDataBuffer.buffer,
            sceneDataUboOffset,
            sizeof(GlobalSceneData));

        glUseProgram(worldShader);

        {
            GL_DEBUG_GROUP("Opaque pass");
            gfx::setGlobalState(opaqueDrawState);
            gfx::setGlobalState({
                .depthWriteEnabled = true,
                .blendEnabled = false,
            });
            renderSceneObjects(opaqueDrawList);
        }

        {
            GL_DEBUG_GROUP("Transparent pass");
            gfx::setGlobalState(transparentDrawState);
            renderSceneObjects(transparentDrawList);
        }
    }

    {
        GL_DEBUG_GROUP("Debug primitives");
        gfx::setGlobalState(linesDrawState);

        renderLines();
        renderWireframes(drawList);
    }

    SDL_GL_SwapWindow(window);
}

void App::generateDrawList()
{
    Frustum frustum = util::createFrustumFromCamera(testCamera);

    drawList.clear();

    for (std::size_t i = 0; i < objects.size(); ++i) {
        auto& object = objects[i];
        if (object.alpha == 0.f) {
            continue;
        }

        object.worldAABB = calculateWorldAABB(object);
        if (!util::isInFrustum(frustum, object.worldAABB)) {
            // continue;
        }

        const auto distToCamera = glm::length(camera.getPosition() - object.transform.position);
        drawList.push_back(DrawInfo{
            .objectIdx = i,
            .distToCamera = distToCamera,
        });
    }

    uploadSceneData();

    opaqueDrawList.clear();
    transparentDrawList.clear();
    for (const auto& drawInfo : drawList) {
        if (objects[drawInfo.objectIdx].alpha == 1.f) {
            opaqueDrawList.push_back(drawInfo);
        } else {
            transparentDrawList.push_back(drawInfo);
        }
    }

    sortDrawList(opaqueDrawList, SortOrder::FrontToBack);
    sortDrawList(transparentDrawList, SortOrder::BackToFront);
}

void App::uploadSceneData()
{
    sceneData.clear();

    // global scene data
    const auto projection = camera.getProjection();
    const auto view = camera.getView();
    const auto d = GlobalSceneData{
        .projection = projection,
        .view = view,
        .cameraPos = glm::vec4{camera.getPosition(), 0.f},
        .sunlightColorAndIntensity = glm::vec4{sunlightColor, sunlightIntensity},
        .sunlightDirAndUnused = glm::vec4{sunlightDir, 0.f},
        .ambientColorAndIntensity = glm::vec4{ambientColor, ambientIntensity},
    };
    sceneDataUboOffset = sceneData.append(d, uboAlignment);

    // per object data
    for (auto& drawInfo : drawList) {
        const auto& object = objects[drawInfo.objectIdx];
        const auto d = PerObjectData{
            .model = object.transform.asMatrix(),
            .props = glm::vec4{object.alpha, 0.f, 0.f, 0.f},
        };
        drawInfo.uboOffset = sceneData.append(d, uboAlignment);
    }

    // reallocate buffer if needed
    // const auto sceneDataSize = sizeof(PerObjectData) * sceneData.size();
    while (sceneData.getData().size() > sceneDataBuffer.size) {
        glDeleteBuffers(1, &sceneDataBuffer.buffer);
        sceneDataBuffer = gfx::allocateBuffer(sceneDataBuffer.size * 2, nullptr, "sceneData");
        std::cout << "Reallocated UBO, new size = " << sceneData.getData().size() << std::endl;
    }

    // upload new data
    glNamedBufferSubData(
        sceneDataBuffer.buffer, 0, sceneData.getData().size(), sceneData.getData().data());
}

void App::renderSceneObjects(const std::vector<DrawInfo>& drawList)
{
    for (const auto& drawInfo : drawList) {
        const auto& object = objects[drawInfo.objectIdx];
        const auto& mesh = meshes[object.meshIdx];
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, VERTEX_DATA_BINDING, mesh.vertexBuffer.buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer.buffer);

        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            PER_OBJECT_DATA_BINDING,
            sceneDataBuffer.buffer,
            drawInfo.uboOffset,
            sizeof(PerObjectData));

        // glBindTextureUnit(0, textures[object.textureIdx]);
        glBindTextureUnit(0, textures[object.alpha != 1.f ? 0 : 1]);
        glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
    }
}

void App::renderWireframes(const std::vector<DrawInfo>& drawList)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUseProgram(solidColorShader);

    Frustum frustum = util::createFrustumFromCamera(testCamera);

    for (const auto& drawInfo : drawList) {
        const auto& object = objects[drawInfo.objectIdx];
        const auto& mesh = meshes[object.meshIdx];
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, VERTEX_DATA_BINDING, mesh.vertexBuffer.buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer.buffer);

        bool isInFrustum = util::isInFrustum(frustum, object.worldAABB);
        const auto color =
            isInFrustum ? glm::vec4{0.f, 1.f, 0.f, 1.f} : glm::vec4{1.f, 0.f, 0.f, 1.f};
        glUniform4fv(0, 1, glm::value_ptr(color));

        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            PER_OBJECT_DATA_BINDING,
            sceneDataBuffer.buffer,
            drawInfo.uboOffset,
            sizeof(PerObjectData));

        glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void App::generateRandomObject()
{
    ObjectData object{
        .meshIdx = chooseRandomElement(meshes, rng),
        .textureIdx = chooseRandomElement(textures, rng),
    };

    std::uniform_real_distribution<float> posDist{-10.f, 10.f};
    // random position
    object.transform.position.x = posDist(rng);
    object.transform.position.y = posDist(rng);
    object.transform.position.z = posDist(rng);

    // add some random rotation
    std::uniform_real_distribution<float> angleDist{-glm::pi<float>(), glm::pi<float>()};
    auto xRot = glm::angleAxis(angleDist(rng), glm::vec3{1.f, 0.f, 0.f});
    auto zRot = glm::angleAxis(angleDist(rng), glm::vec3{0.f, 0.f, 1.f});
    object.transform.heading = zRot * xRot;

    // std::uniform_real_distribution<float> scaleDist{0.5f, 1.5f};
    // object.transform.scale = glm::vec3{scaleDist(rng)};

    // decide if object should be opaque or not at random
    std::uniform_int_distribution<int> opaqueDist{0, 1};
    bool isOpaque = (bool)opaqueDist(rng);
    object.alpha = isOpaque ? 1.0 : 0.75f;

    objects.push_back(object);
}

void App::spawnCube(const glm::vec3& pos, std::size_t textureIdx, float alpha)
{
    ObjectData object{
        .meshIdx = 0,
        .textureIdx = textureIdx,
        .alpha = alpha,
    };
    object.transform.position = pos;
    objects.push_back(object);
}

void App::addLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
{
    lines.push_back(LineVertex{.pos = from, .color = color});
    lines.push_back(LineVertex{.pos = to, .color = color});
}

void App::addLine(
    const glm::vec3& from,
    const glm::vec3& to,
    const glm::vec4& fromColor,
    const glm::vec4& toColor)
{
    lines.push_back(LineVertex{.pos = from, .color = fromColor});
    lines.push_back(LineVertex{.pos = to, .color = toColor});
}

void App::addQuadLines(
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

void App::addAABBLines(const AABB& aabb, const glm::vec4& color)
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

void App::addFrustumLines(const Camera& camera)
{
    const auto corners = util::calculateFrustumCornersWorldSpace(camera);

    // left plane
    addQuadLines(corners[4], corners[5], corners[1], corners[0], glm::vec4{1.f, 1.f, 0.f, 1.f});
    // right plane
    addQuadLines(corners[7], corners[6], corners[2], corners[3], glm::vec4{1.f, 1.f, 0.f, 1.f});
    // near plane
    addQuadLines(corners[0], corners[1], corners[2], corners[3], glm::vec4{0.f, 1.f, 1.f, 1.f});
    // far plane
    addQuadLines(corners[4], corners[5], corners[6], corners[7], glm::vec4{0.f, 1.f, 1.f, 1.f});

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
}

AABB App::calculateWorldAABB(const ObjectData& object)
{
    auto& aabb = meshes[object.meshIdx].aabb;
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
    const auto tm = object.transform.asMatrix();
    for (auto& point : points) {
        point = glm::vec3(tm * glm::vec4{point, 1.f});
    }
    return util::calculateAABB(points);
}

void App::renderLines()
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
