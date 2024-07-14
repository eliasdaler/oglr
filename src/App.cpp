#include "App.h"

#include <chrono>
#include <iostream>

#include <glad/gl.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp> // lerp for vec3

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

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

constexpr auto CAMERA_DATA_BINDING = 0;
constexpr auto LIGHT_DATA_BINDING = 1;
constexpr auto PER_OBJECT_DATA_BINDING = 2;
constexpr auto VERTEX_DATA_BINDING = 3;

template<typename T, typename RNGType>
std::size_t chooseRandomElementIndex(const std::vector<T>& v, RNGType& rng)
{
    if (v.empty()) {
        return 0;
    }
    std::uniform_int_distribution<std::size_t> dist{0, v.size() - 1};
    return dist(rng);
}

template<typename T, typename RNGType>
T chooseRandomElement(const std::vector<T>& v, RNGType& rng)
{
    if (v.empty()) {
        return T{};
    }
    return v.at(chooseRandomElementIndex(v, rng));
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

glm::vec2 calculateSpotLightScaleOffset(float innerConeAngle, float outerConeAngle)
{
    // See KHR_lights_punctual spec - formulas are taken from it
    glm::vec2 scaleOffset;
    scaleOffset.x = 1.f / std::max(0.001f, std::cos(innerConeAngle) - std::cos(outerConeAngle));
    scaleOffset.y = -std::cos(outerConeAngle) * scaleOffset.x;
    return scaleOffset;
}

GPULightData toGPULightData(
    const glm::vec3& pos,
    const glm::vec3& dir,
    const Light& light,
    bool castShadow)
{
    return GPULightData{
        .position = pos,
        .intensity = light.intensity,
        .dir = dir,
        .range = light.range,
        .color = glm::vec3(light.color),
        .type = light.type,
        .scaleOffset = light.scaleOffset,
        .props = {(float)castShadow, 0.f},
    };
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

    { // imgui init
        ImGui::CreateContext();
        ImGui_ImplSDL2_InitForOpenGL(window, glContext);
        ImGui_ImplOpenGL3_Init("#version 460 core");
    }

    { // shaders
        worldShader = gfx::
            loadShaderProgram("assets/shaders/basic.vert", "assets/shaders/basic.frag", "world");
        assert(worldShader);

        depthOnlyShader = gfx::loadShaderProgram("assets/shaders/basic.vert", "", "depth_only");
        assert(depthOnlyShader);

        solidColorShader = gfx::loadShaderProgram(
            "assets/shaders/basic.vert", "assets/shaders/solid_color.frag", "world");
        assert(solidColorShader);

        postFXShader = gfx::loadShaderProgram(
            "assets/shaders/fullscreen_tri.vert", "assets/shaders/postfx.frag", "postfx");
        assert(postFXShader);
    }

    { // allocate scene data buffer
        const auto cameraDataSize = gfx::getAlignedSize(sizeof(CameraData), uboAlignment);
        const auto lightDataSize = gfx::getAlignedSize(sizeof(LightData), uboAlignment);
        const auto perObjectDataElementSize =
            gfx::getAlignedSize(sizeof(PerObjectData), uboAlignment);
        const auto bufSize = cameraDataSize * MAX_CAMERAS_IN_UBO +
                             lightDataSize * MAX_LIGHS_IN_UBO + perObjectDataElementSize * 100;
        sceneDataBuffer = gfx::allocateBuffer(bufSize, nullptr, "sceneData");

        sceneData.resize(bufSize);
        cameraDataUboOffsets.resize(MAX_CAMERAS_IN_UBO);
        lightsUboOffsets.resize(MAX_LIGHS_IN_UBO);
    }

    // we still need an empty VAO even for vertex pulling
    glGenVertexArrays(1, &vao);

    debugRenderer.init();

    const auto texturesToLoad = {
        "assets/images/texture1.png",
        "assets/images/texture2.png",
        "assets/images/texture3.png",
        "assets/images/texture5.png",
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
        meshes.push_back(gfx::uploadMeshToGPU(util::getPlaneMesh(100, 50)));

        // TODO: something better?
        randomSpawnMeshes = {0, 1};
        randomSpawnTextures = {0, 1};
        cubeMeshIdx = 0;
        planeMeshIdx = 2;
    }

    { // init camera
        const auto fovX = 45.f;
        const auto zNear = 0.1f;
        const auto zFar = 1000.f;
        camera.init(fovX, zNear, zFar, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT);
        camera.setPosition({0.f, 2.5f, -10.f});
    }

    { // init test camera
        const auto fovX = 45.f;
        const auto zNear = 0.1f;
        const auto zFar = 7.5f;
        testCamera.init(fovX, zNear, zFar, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT);
        // testCamera.setPosition({2.f, 3.f, -3.f});
        testCamera.setPosition({0.f, 3.f, -3.f});
        testCamera.lookAt(glm::vec3{0.f, 0.f, 0.f});
    }

    // ground plane
    spawnObject({}, planeMeshIdx, 2, 1.f);
    // some cubes
    spawnObject({0.f, 1.f, 0.f}, cubeMeshIdx, 0, 1.0f);
    spawnObject({0.f, 1.f, 2.5f}, cubeMeshIdx, 1, 0.5f);
    spawnObject({0.f, 1.f, 5.f}, cubeMeshIdx, 0, 1.f);
    spawnObject({0.f, 1.f, 7.5f}, cubeMeshIdx, 1, 1.f);
    // star
    // spawnObject({0.f, 0.1f, 5.0f}, 1, 0, 1.f);
    // spawnObject({0.f, 0.3f, 7.5f}, 1, 1, 1.f);

    { // init lights
        ambientColor = glm::vec3{0.3f, 0.65f, 0.8f};
        ambientIntensity = 0.1f;

        sunLightDir = glm::normalize(glm::vec3(1.f, -1.f, 1.f));
        sunLight = Light{
            .type = LIGHT_TYPE_DIRECTIONAL,
            .color = glm::vec4{0.65f, 0.4f, 0.3f, 1.f},
            .intensity = 0.5f,
        };

        pointLightRotateOrigin = {0.f, 2.5f, 1.25f};
        pointLightRotateAngle = 0.f;
        pointLightRotateRadius = 5.f;

        pointLightPosition = {0.f, 2.5f, 1.25f};
        pointLight = Light{
            .type = LIGHT_TYPE_POINT,
            .color = glm::vec4{0.1f, 0.75f, 0.3f, 1.f},
            .intensity = 10.f,
            .range = 20.f,
        };

        spotLightPosition = {-3.f, 3.5f, 2.f};
        spotLightDir = glm::normalize(glm::vec3(1.f, -1.f, 1.f));

        // spotLightPosition = {-3.f, 5.0f, 2.f};
        // spotLightDir = glm::normalize(glm::vec3(0.f, -1.f, 0.f));

        spotLight = Light{
            .type = LIGHT_TYPE_SPOT,
            .color = glm::vec4{1.f, 1.f, 1.f, 1.f},
            .intensity = 1.f,
            .range = 20.f,
            .scaleOffset = calculateSpotLightScaleOffset(glm::radians(20.f), glm::radians(30.f)),
        };

        {
            const auto fovX = glm::radians(60.f);
            const auto zNear = 0.1f;
            const auto zFar = spotLight.range;
            spotLightCamera.init(fovX, zNear, zFar, 1.f);
            spotLightCamera.setPosition(spotLightPosition);

            if (std::abs(glm::dot(spotLightDir, math::GLOBAL_UP_DIR)) > 0.9999f) {
                spotLightCamera.setHeading(
                    glm::quatLookAt(-spotLightDir, math::GLOBAL_FORWARD_DIR));
            } else {
                spotLightCamera.setHeading(glm::quatLookAt(-spotLightDir, math::GLOBAL_UP_DIR));
            }
        }
    }

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

    postFXDrawState = gfx::GlobalState{
        .depthTestEnabled = false,
        .depthWriteEnabled = false,
        .cullingEnabled = false,
        .blendEnabled = false,
    };

    wireframesDrawState = gfx::GlobalState{
        .depthTestEnabled = false,
        .depthWriteEnabled = false,
        .cullingEnabled = false,
        .blendEnabled = true,
    };

    { // FBO setup
        glCreateFramebuffers(1, &mainDrawFBO);

        // Color buffer
        glCreateTextures(GL_TEXTURE_2D, 1, &mainDrawColorTexture);
        glTextureStorage2D(mainDrawColorTexture, 1, GL_RGB8, WINDOW_WIDTH, WINDOW_HEIGHT);
        glTextureParameteri(mainDrawColorTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(mainDrawColorTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Depth buffer
        glCreateTextures(GL_TEXTURE_2D, 1, &mainDrawDepthTexture);
        glTextureStorage2D(
            mainDrawDepthTexture, 1, GL_DEPTH_COMPONENT32F, WINDOW_WIDTH, WINDOW_HEIGHT);

        // Attach buffers
        glNamedFramebufferTexture(mainDrawFBO, GL_COLOR_ATTACHMENT0, mainDrawColorTexture, 0);
        glNamedFramebufferTexture(mainDrawFBO, GL_DEPTH_ATTACHMENT, mainDrawDepthTexture, 0);

        // Set draw buffers
        static const GLenum draw_buffers[]{GL_COLOR_ATTACHMENT0};
        glNamedFramebufferDrawBuffers(mainDrawFBO, 1, draw_buffers);
    }

    { // shadow map setup
        glCreateFramebuffers(1, &shadowMapFBO);

        // Depth buffer
        glCreateTextures(GL_TEXTURE_2D, 1, &shadowMapDepthTexture);
        glTextureStorage2D(
            shadowMapDepthTexture, 1, GL_DEPTH_COMPONENT32F, shadowMapSize, shadowMapSize);

        float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTextureParameteri(shadowMapDepthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(shadowMapDepthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        // Attach buffers
        glNamedFramebufferTexture(shadowMapFBO, GL_DEPTH_ATTACHMENT, shadowMapDepthTexture, 0);
    }
}

void App::cleanup()
{
    for (const auto& mesh : meshes) {
        glDeleteBuffers(1, &mesh.indexBuffer.buffer);
        glDeleteBuffers(1, &mesh.vertexBuffer.buffer);
    }
    glDeleteTextures(textures.size(), textures.data());
    glDeleteBuffers(1, &sceneDataBuffer.buffer);

    glDeleteVertexArrays(1, &vao);

    glDeleteProgram(postFXShader);
    glDeleteProgram(solidColorShader);
    glDeleteProgram(depthOnlyShader);
    glDeleteProgram(worldShader);

    glDeleteFramebuffers(1, &mainDrawFBO);

    debugRenderer.cleanup();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

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
                ImGui_ImplSDL2_ProcessEvent(&event);
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            update(dt);
            ImGui::Render();

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

    // animate point light
    pointLightRotateAngle += 1.5f * dt;
    pointLightPosition.x =
        pointLightRotateOrigin.x + std::cos(pointLightRotateAngle) * pointLightRotateRadius;
    pointLightPosition.z =
        pointLightRotateOrigin.z + std::sin(pointLightRotateAngle) * pointLightRotateRadius;

    ImGui::Begin("Debug");
    ImGui::Text("Total objects: %d", (int)objects.size());
    ImGui::Text("Drawn objects: %d", (int)(opaqueDrawList.size() + transparentDrawList.size()));
    ImGui::Text("Drawn objects (shadow map): %d", (int)shadowMapOpaqueDrawList.size());
    ImGui::Checkbox("Use test camera for culling", &useTestCameraForCulling);
    ImGui::Checkbox("Draw AABBs", &drawAABBs);
    ImGui::Checkbox("Draw wireframes", &drawWireframes);
    ImGui::Text("Spotlight culled: %d", (int)spotLightCulled);
    if (ImGui::Button("Update test camera")) {
        testCamera = camera;
    }
    ImGui::End();
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

    glBindVertexArray(vao);

    {
        GL_DEBUG_GROUP("Shadow pass");
        gfx::setGlobalState(opaqueDrawState);
        renderShadowMap();
    }

    {
        gfx::setGlobalState(frameStartState);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mainDrawFBO);
        glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        // clear fbo buffers
        GLfloat clearColor[4]{0.f, 0.f, 0.f, 1.f};
        glClearNamedFramebufferfv(mainDrawFBO, GL_COLOR, 0, clearColor);
        GLfloat depthValue{1.f};
        glClearNamedFramebufferfv(mainDrawFBO, GL_DEPTH, 0, &depthValue);

        // object texture will be read from TU0
        glProgramUniform1i(worldShader, FRAG_TEXTURE_UNIFORM_LOC, 0);

        // gobo texture will be read from TU1
        constexpr auto GOBO_TEXTURE_UNIFORM_LOC = 2;
        constexpr auto GOBO_TEX_ID = 3;
        glProgramUniform1i(worldShader, GOBO_TEXTURE_UNIFORM_LOC, 1);
        glBindTextureUnit(1, textures[3]);

        // shadow map
        constexpr auto SHADOW_MAP_TEXTURE_UNIFORM_LOC = 3;
        glProgramUniform1i(worldShader, SHADOW_MAP_TEXTURE_UNIFORM_LOC, 2);
        glBindTextureUnit(2, shadowMapDepthTexture);

        glUseProgram(worldShader);

        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            CAMERA_DATA_BINDING,
            sceneDataBuffer.buffer,
            cameraDataUboOffsets[0],
            sizeof(CameraData));
        glDepthFunc(GL_LEQUAL);
        for (int i = 0; i < lightsUboOffsets.size(); ++i) {
            glBindBufferRange(
                GL_UNIFORM_BUFFER,
                LIGHT_DATA_BINDING,
                sceneDataBuffer.buffer,
                lightsUboOffsets[i],
                sizeof(LightData));

            {
                GL_DEBUG_GROUP("Opaque pass");
                gfx::setGlobalState(opaqueDrawState);
                if (i != 0) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_ONE);
                }
                renderSceneObjects(opaqueDrawList);
            }

            {
                GL_DEBUG_GROUP("Transparent pass");
                gfx::setGlobalState(transparentDrawState);
                renderSceneObjects(transparentDrawList);
            }
        }
    }
    glDepthFunc(GL_LESS);

    // restore default FBO
    // we'll draw everything into it
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    {
        GL_DEBUG_GROUP("Post FX");
        static const auto POSTFX_FRAG_TEXTURE_UNIFORM_LOC = 0;

        gfx::setGlobalState(postFXDrawState);
        glUseProgram(postFXShader);

        glBindTextureUnit(0, mainDrawColorTexture);
        glProgramUniform1i(postFXShader, POSTFX_FRAG_TEXTURE_UNIFORM_LOC, 0);

        // draw fullscreen triangle
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    {
        GL_DEBUG_GROUP("Debug primitives");
        renderDebugObjects();
    }

    {
        GL_DEBUG_GROUP("Draw ImGui");
        glDisable(GL_FRAMEBUFFER_SRGB); // kinda cringe, but works
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glEnable(GL_FRAMEBUFFER_SRGB);
    }

    SDL_GL_SwapWindow(window);
}

void App::generateDrawList()
{
    drawList.clear();

    for (std::size_t i = 0; i < objects.size(); ++i) {
        auto& object = objects[i];
        if (object.alpha == 0.f) {
            continue;
        }

        // recalculate world AABB
        const auto& meshAABB = meshes[object.meshIdx].aabb;
        const auto tm = object.transform.asMatrix();
        object.worldAABB = util::calculateWorldAABB(meshAABB, tm);

        const auto distToCamera = glm::length(camera.getPosition() - object.transform.position);
        drawList.push_back(DrawInfo{
            .objectIdx = i,
            .distToCamera = distToCamera,
        });
    }

    uploadSceneData();

    // separate into opaque and transparent lists
    opaqueDrawList.clear();
    transparentDrawList.clear();
    const auto mainCameraFrustum = getFrustum();
    for (const auto& drawInfo : drawList) {
        const auto& object = objects[drawInfo.objectIdx];
        if (!util::isInFrustum(mainCameraFrustum, object.worldAABB)) {
            continue;
        }

        if (object.alpha == 1.f) {
            opaqueDrawList.push_back(drawInfo);
        } else {
            transparentDrawList.push_back(drawInfo);
        }
    }

    sortDrawList(opaqueDrawList, SortOrder::FrontToBack);
    sortDrawList(transparentDrawList, SortOrder::BackToFront);

    { // recalc spotlight AABB
        const auto spotLightPoints = util::calculateFrustumCornersWorldSpace(spotLightCamera);
        std::vector<glm::vec3> points;
        points.assign(spotLightPoints.begin(), spotLightPoints.end());
        spotLightAABB = util::calculateAABB(points);
    }

    spotLightCulled = !util::isInFrustum(mainCameraFrustum, spotLightAABB);

    // shadow map draw list
    shadowMapOpaqueDrawList.clear();
    if (!spotLightCulled) {
        const auto spotLightFrustum = util::createFrustumFromCamera(spotLightCamera);
        for (const auto& drawInfo : drawList) {
            const auto& object = objects[drawInfo.objectIdx];
            if (object.alpha == 1.f && util::isInFrustum(spotLightFrustum, object.worldAABB)) {
                shadowMapOpaqueDrawList.push_back(drawInfo);
            }
        }
    }
}

void App::uploadSceneData()
{
    sceneData.clear();

    // global scene data
    const auto projection = camera.getProjection();
    const auto view = camera.getView();

    cameraDataUboOffsets.clear();

    // main cam
    auto cd = CameraData{
        .projection = projection,
        .view = view,
        .cameraPos = glm::vec4{camera.getPosition(), 0.f},
    };
    cameraDataUboOffsets.push_back(sceneData.append(cd, uboAlignment));

    // spot light cam
    cd = CameraData{
        .projection = spotLightCamera.getProjection(),
        .view = spotLightCamera.getView(),
        .cameraPos = glm::vec4{spotLightCamera.getPosition(), 0.f},
    };
    cameraDataUboOffsets.push_back(sceneData.append(cd, uboAlignment));

    lightsUboOffsets.clear();
    lightsUboOffsets.push_back(sceneData.append(LightData{
        .light = toGPULightData({}, sunLightDir, sunLight, false),
    }));
    lightsUboOffsets.push_back(sceneData.append(LightData{
        .light = toGPULightData(pointLightPosition, {}, pointLight, false),
    }));
    if (!spotLightCulled) {
        lightsUboOffsets.push_back(sceneData.append(LightData{
            .light = toGPULightData(spotLightPosition, spotLightDir, spotLight, true),
            .lightSpaceTM = spotLightCamera.getViewProj(),
        }));
    }

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
    while (sceneData.getData().size() > sceneDataBuffer.size) {
        glDeleteBuffers(1, &sceneDataBuffer.buffer);
        sceneDataBuffer = gfx::allocateBuffer(sceneDataBuffer.size * 2, nullptr, "sceneData");
        std::cout << "Reallocated UBO, new size = " << sceneData.getData().size() << std::endl;
    }

    // upload new data
    glNamedBufferSubData(
        sceneDataBuffer.buffer, 0, sceneData.getData().size(), sceneData.getData().data());
}

void App::renderShadowMap()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, shadowMapFBO);
    glViewport(0, 0, shadowMapSize, shadowMapSize);
    // clear shadow map
    GLfloat depthValue{1.f};
    glClearNamedFramebufferfv(shadowMapFBO, GL_DEPTH, 0, &depthValue);

    glBindBufferRange(
        GL_UNIFORM_BUFFER,
        CAMERA_DATA_BINDING,
        sceneDataBuffer.buffer,
        cameraDataUboOffsets[1],
        sizeof(CameraData));

    glUseProgram(depthOnlyShader);

    renderSceneObjects(shadowMapOpaqueDrawList);
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

        glBindTextureUnit(0, textures[object.textureIdx]);
        glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
    }
}

void App::renderDebugObjects()
{
    debugRenderer.beginDrawing();
    if (drawAABBs) {
        for (auto& object : objects) {
            debugRenderer.addAABBLines(object.worldAABB, glm::vec4{1.f, 0.f, 1.f, 1.f});
        }
    }

    // debugRenderer.addFrustumLines(spotLightCamera);
    debugRenderer.addFrustumLines(testCamera);

    if (drawAABBs) {
        debugRenderer.addAABBLines(spotLightAABB, glm::vec4{1.f, 0.f, 1.f, 1.f});
    }

    { // world origin
        debugRenderer.addLine(
            glm::vec3{0.f, 0.f, 0.f}, glm::vec3{1.f, 0.f, 0.f}, glm::vec4{1.f, 0.f, 0.f, 1.f});
        debugRenderer.addLine(
            glm::vec3{0.f, 0.f, 0.f}, glm::vec3{0.f, 1.f, 0.f}, glm::vec4{0.f, 1.f, 0.f, 1.f});
        debugRenderer.addLine(
            glm::vec3{0.f, 0.f, 0.f}, glm::vec3{0.f, 0.f, 1.f}, glm::vec4{0.f, 0.f, 1.f, 1.f});
    }

    // spot light
    debugRenderer.addLine(
        spotLightPosition,
        spotLightPosition + spotLightDir * 1.f,
        glm::vec4{1.f, 0.f, 0.f, 1.f},
        glm::vec4{0.f, 1.f, 0.f, 1.f});

    debugRenderer.render(camera);

    if (drawWireframes) {
        gfx::setGlobalState(wireframesDrawState);
        renderWireframes(drawList);
    }
}

void App::renderWireframes(const std::vector<DrawInfo>& drawList)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUseProgram(solidColorShader);

    Frustum frustum = getFrustum();
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
        .meshIdx = chooseRandomElement(randomSpawnMeshes, rng),
        .textureIdx = chooseRandomElement(randomSpawnTextures, rng),
    };

    std::uniform_real_distribution<float> posDist{-10.f, 10.f};
    std::uniform_real_distribution<float> posDistY{0.1f, 10.f};
    // random position
    object.transform.position.x = posDist(rng);
    object.transform.position.y = posDistY(rng);
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

void App::spawnObject(
    const glm::vec3& pos,
    std::size_t meshIdx,
    std::size_t textureIdx,
    float alpha)
{
    Transform transform{.position = pos};
    ObjectData object{
        .transform = transform,
        .meshIdx = meshIdx,
        .textureIdx = textureIdx,
        .alpha = alpha,
    };
    objects.push_back(object);
}

Frustum App::getFrustum() const
{
    return util::createFrustumFromCamera(useTestCameraForCulling ? testCamera : camera);
}
