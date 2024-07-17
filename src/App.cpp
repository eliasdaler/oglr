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

std::array<int, MAX_AFFECTING_LIGHTS> getClosestLights(
    const glm::vec3& objPos,
    const std::vector<CPULightData>& lights)
{
    struct LightDist {
        std::size_t idx;
        float dist;
    };
    std::vector<LightDist> dists;

    // NOTE: currentLightIdx is used there, not "i", because
    // we're only uploading non-culled lights to our UBO
    // and the indices in PerObjectData.lightIdx should match that
    std::size_t currentLightIdx = 0;
    for (std::size_t i = 0; i < lights.size(); ++i) {
        if (lights[i].culled) {
            continue;
        }
        auto ld = LightDist{
            .idx = currentLightIdx,
            .dist = glm::length(lights[i].position - objPos),
        };
        // HACK: always include spot lights for image stability
        if (lights[i].light.type == LIGHT_TYPE_SPOT) {
            ld.dist = 0;
        }
        dists.push_back(ld);
        ++currentLightIdx;
    }

    std::sort(dists.begin(), dists.end(), [](const auto& d1, const auto& d2) {
        return d1.dist <= d2.dist;
    });

    std::array<int, MAX_AFFECTING_LIGHTS> lightIdx;

    // select closes MAX_AFFECTING_LIGHTS
    // and if not enough - add (MAX_LIGHTS_IN_UBO + 1), indicating "no light"
    for (int i = 0; i < MAX_AFFECTING_LIGHTS; ++i) {
        if (i < dists.size()) {
            lightIdx[i] = dists[i].idx;
        } else {
            lightIdx[i] = MAX_LIGHTS_IN_UBO + 1;
        }
    }
    return lightIdx;
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
    rng.seed(4);

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
        const auto cameraDataSize = gfx::getAlignedSize(sizeof(UBOCameraData), uboAlignment);
        const auto lightDataSize = gfx::getAlignedSize(sizeof(UBOLightData), uboAlignment);
        const auto perObjectDataElementSize =
            gfx::getAlignedSize(sizeof(UBOPerObjectData), uboAlignment);
        const auto bufSize =
            cameraDataSize * MAX_CAMERAS_IN_UBO + lightDataSize + perObjectDataElementSize * 100;
        sceneDataBuffer = gfx::allocateBuffer(bufSize, nullptr, "sceneData");
        sceneData.resize(bufSize);
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

        randomSpawnMeshes = {0, 1};
        randomSpawnTextures = {0, 1};
        cubeMeshIdx = 0;
        startMeshIdx = 1;
        planeMeshIdx = 2;
    }

    { // init camera
        const auto fovX = 45.f;
        const auto zNear = 0.1f;
        const auto zFar = 1000.f;
        camera.init(fovX, zNear, zFar, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT);
        camera.setPosition({0.f, 2.5f, -10.f});
        camera.lookAt({0.f, 4.f, -1.f});
    }

    testCamera = camera;

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

        // depth texture array
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &shadowMapDepthTexture);
        glTextureStorage3D(
            shadowMapDepthTexture,
            1,
            GL_DEPTH_COMPONENT32F,
            shadowMapSize,
            shadowMapSize,
            SHADOW_MAP_ARRAY_LAYERS);

        glTextureParameteri(shadowMapDepthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(shadowMapDepthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(shadowMapDepthTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(shadowMapDepthTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(
            shadowMapDepthTexture, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

        // Attach buffers
        glNamedFramebufferTextureLayer(
            shadowMapFBO, GL_DEPTH_ATTACHMENT, shadowMapDepthTexture, 0, 0);
    }

    { // int point light cameras
        const auto aspect = 1.f; // shadow maps are square
        const auto nearPlane = 0.1f;
        const auto pointLightMaxRange = 20.f;
        const auto farPlane = pointLightMaxRange;

        // <front, up>
        static const std::array<std::pair<glm::vec3, glm::vec3>, 6> shadowDirections{{
            {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}}, // posx
            {{-1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}}, // negx
            {{0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}, // posy
            {{0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}}, // negy
            {{0.0, 0.0, 1.0}, {0.0, 1.0, 0.0}}, // posz
            {{0.0, 0.0, -1.0}, {0.0, 1.0, 0.0}}, // negz
        }};

        for (int i = 0; i < 6; ++i) {
            auto& camera = pointLightShadowMapCameras[i];
            camera.setHeading(
                glm::quatLookAt(shadowDirections[i].first, shadowDirections[i].second));
            camera.init(glm::radians(90.0f), nearPlane, farPlane, aspect);
        }
    }

    initScene();
}

void App::initScene()
{
    // ground plane
    spawnObject({}, planeMeshIdx, planeMeshIdx, 1.f);

    { // walls / ceiling
        spawnObject({0.f, 7.5f, 0.f}, planeMeshIdx, 2, 1.f);
        objects.back().transform.heading =
            glm::angleAxis(glm::radians(180.f), glm::vec3{0.f, 0.f, 1.f});

        spawnObject({7.5f, 0.f, 0.f}, planeMeshIdx, 2, 1.f);
        objects.back().transform.heading =
            glm::angleAxis(glm::radians(90.f), glm::vec3{0.f, 0.f, 1.f});
        spawnObject({-5.f, 0.f, 0.f}, planeMeshIdx, 2, 1.f);
        objects.back().transform.heading =
            glm::angleAxis(glm::radians(-90.f), glm::vec3{0.f, 0.f, 1.f});

        spawnObject({0.f, 0.f, 10.f}, planeMeshIdx, 2, 1.f);
        objects.back().transform.heading =
            glm::angleAxis(glm::radians(-90.f), glm::vec3{1.f, 0.f, 0.f});

        spawnObject({0.f, 0.f, -4.f}, planeMeshIdx, 2, 1.f);
        objects.back().transform.heading =
            glm::angleAxis(glm::radians(90.f), glm::vec3{1.f, 0.f, 0.f});
    }

    // some cubes
    spawnObject({0.f, 1.f, 0.f}, cubeMeshIdx, 0, 1.0f);
    spawnObject({0.f, 1.f, 2.5f}, cubeMeshIdx, 1, 1.f);
    spawnObject({0.f, 1.f, 5.f}, cubeMeshIdx, 0, 1.f);
    spawnObject({0.f, 1.f, 7.5f}, cubeMeshIdx, 1, 1.f);

    spawnObject({6.f, 1.f, 2.5f}, cubeMeshIdx, 1, 1.f);
    spawnObject({6.f, 1.f, 5.f}, cubeMeshIdx, 0, 1.f);
    // star
    spawnObject({3.f, 6.0f, 2.0f}, startMeshIdx, 0, 1.f);

    spawnObject({-1.f, 4.0f, 4.0f}, startMeshIdx, 1, 1.f);
    objects.back().transform.heading =
        glm::angleAxis(glm::radians(90.f), glm::vec3{1.f, 0.5f, 0.f});

    { // init lights
        ambientColor = glm::vec3{0.3f, 0.65f, 0.8f};
        ambientIntensity = 0.1f;

        sunLightDir = glm::normalize(glm::vec3(1.f, -1.f, 1.f));
        sunLight = Light{
            .type = LIGHT_TYPE_DIRECTIONAL,
            .color = glm::vec4{0.65f, 0.4f, 0.3f, 1.f},
            .intensity = 0.15f,
        };

        // generate some random floating point lights
        std::uniform_real_distribution<float> posXZDist(-3.f, 3.f);
        std::uniform_real_distribution<float> posYDist(3.f, 5.0f);
        std::uniform_real_distribution<float> rangeDist(20.f, 20.f);
        std::uniform_real_distribution<float> colorDist(0.2f, 0.9f);
        std::uniform_real_distribution<float> rotationRadiusDist(1.f, 2.f);
        std::uniform_real_distribution<float> rotationSpeedDist(-1.5f, 1.5f);
        for (std::size_t i = 0; i < 4; ++i) {
            lights.push_back(CPULightData{
                .position = {posXZDist(rng), posYDist(rng), posXZDist(rng)},
                .light =
                    Light{
                        .type = LIGHT_TYPE_POINT,
                        .color = {colorDist(rng), colorDist(rng), colorDist(rng), 1.f},
                        .intensity = 10.f,
                        .range = rangeDist(rng),
                    },
                .rotationOrigin = {posXZDist(rng), posYDist(rng), posXZDist(rng)},
                .rotationRadius = rotationRadiusDist(rng),
                .rotationSpeed = rotationSpeedDist(rng),
                .castsShadow = true,
            });
        }

        addSpotLight(
            {-3.f, 3.5f, 2.f}, // pos
            glm::normalize(glm::vec3(1.f, -1.f, 1.f)), // dir
            Light{
                .type = LIGHT_TYPE_SPOT,
                .color = glm::vec4{1.f, 1.f, 1.f, 1.f},
                .intensity = 1.f,
                .range = 20.f,
                .innerConeAngle = glm::radians(20.f),
                .outerConeAngle = glm::radians(30.f),
            },
            true // cast shadow
        );

        addSpotLight(
            {2.f, 5.0f, -1.f}, // pos
            glm::normalize(glm::vec3(-0.5f, -1.f, 0.75f)), // dir
            Light{
                .type = LIGHT_TYPE_SPOT,
                .color = glm::vec4{1.f, 0.f, 1.f, 1.f},
                .intensity = 1.f,
                .range = 30.f,
                .innerConeAngle = glm::radians(20.f),
                .outerConeAngle = glm::radians(30.f),
            },
            true // cast shadow
        );
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
        // object.transform.heading *= glm::angleAxis(rotationSpeed * dt, glm::vec3{1.f, 1.f,
        // 0.f});
    }

    // animate lights
    for (auto& light : lights) {
        if (light.rotationSpeed == 0.f) {
            continue;
        }
        light.rotationAngle += light.rotationSpeed * dt;
        light.position.x =
            light.rotationOrigin.x + std::cos(light.rotationAngle) * light.rotationRadius;
        light.position.y = light.rotationOrigin.y;
        light.position.z =
            light.rotationOrigin.z + std::sin(light.rotationAngle) * light.rotationRadius;
    }

    ImGui::Begin("Debug");

    ImGui::Text("Total objects: %d", (int)objects.size());
    ImGui::Text("Drawn objects: %d", (int)(opaqueDrawList.size() + transparentDrawList.size()));

    ImGui::Text("Total lights: %d", (int)lights.size() + 1); // + lights + dir light
    int numLightsCulled = 0;
    for (const auto& light : lights) {
        if (light.culled) {
            ++numLightsCulled;
        }
    }
    ImGui::Text("Lights culled : %d", numLightsCulled);

    // ImGui::Text("Drawn objects (shadow map): %d", (int)shadowMapOpaqueDrawList.size());
    ImGui::Checkbox("Use test camera for culling", &useTestCameraForCulling);
    ImGui::Checkbox("Draw AABBs", &drawAABBs);
    ImGui::Checkbox("Draw wireframes", &drawWireframes);
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
        moveVector += camera.getRight() * moveStickState.x;
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
        rotationVelocity.y = -rotateStickState.y * rotatePitchSpeed;

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
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, shadowMapFBO);
        glViewport(0, 0, shadowMapSize, shadowMapSize);
        glUseProgram(depthOnlyShader);
        for (const auto& light : lights) {
            if (light.shadowMapDrawListIdx == MAX_SHADOW_CASTING_LIGHTS) {
                continue;
            }

            if (light.light.type == LIGHT_TYPE_SPOT) {
                renderSpotLightShadowMap(light);
            } else if (light.light.type == LIGHT_TYPE_POINT) {
                renderPointLightShadowMap(light);
            }
        }
    }

    { // draw scene
        GL_DEBUG_GROUP("Draw world");
        gfx::setGlobalState(frameStartState);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mainDrawFBO);
        glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        // clear fbo buffers
        GLfloat clearColor[4]{0.f, 0.f, 0.f, 0.f};
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

        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            CAMERA_DATA_BINDING,
            sceneDataBuffer.buffer,
            mainCameraUboOffset,
            sizeof(UBOCameraData));
        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            LIGHT_DATA_BINDING,
            sceneDataBuffer.buffer,
            lightDataUboOffset,
            sizeof(UBOLightData));

        glUseProgram(worldShader);

        {
            GL_DEBUG_GROUP("Opaque pass");
            gfx::setGlobalState(opaqueDrawState);
            renderSceneObjects(opaqueDrawList);
        }

        {
            GL_DEBUG_GROUP("Transparent pass");
            gfx::setGlobalState(transparentDrawState);
            renderSceneObjects(transparentDrawList);
        }
    }

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

    const auto mainCameraFrustum = getFrustum();

    // cull lights
    for (auto& light : lights) {
        light.culled = shouldCullLight(mainCameraFrustum, light);
    }

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
            .lightIdx = getClosestLights(object.transform.position, lights),
        });
    }

    // important to do it here - draw lists rely on UBO offsets produces by this function
    uploadSceneData();

    // separate into opaque and transparent lists
    opaqueDrawList.clear();
    transparentDrawList.clear();
    for (auto& drawInfo : drawList) {
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

    generateShadowMapDrawList();
}

void App::generateShadowMapDrawList()
{
    for (auto& dl : shadowMapOpaqueDrawLists) {
        dl.clear();
    }

    std::size_t shadowMapDrawListIdx = 0;
    for (auto& light : lights) {
        bool skipShadowDrawing = false;
        if (!light.castsShadow || light.culled) {
            skipShadowDrawing = true;
        }
        if (light.light.type == LIGHT_TYPE_SPOT &&
            shadowMapDrawListIdx >= MAX_SHADOW_CASTING_LIGHTS) {
            // 1 slice won't fit into remaining space
            skipShadowDrawing = true;
        }
        if (light.light.type == LIGHT_TYPE_POINT &&
            shadowMapDrawListIdx + 5 >= SHADOW_MAP_ARRAY_LAYERS) {
            // 6 slices won't fit into remaining space
            skipShadowDrawing = true;
        }

        if (skipShadowDrawing) {
            light.shadowMapDrawListIdx = MAX_SHADOW_CASTING_LIGHTS;
            light.shadowMapIdx = SHADOW_MAP_ARRAY_LAYERS;
            continue;
        }

        if (light.light.type == LIGHT_TYPE_SPOT) {
            light.shadowMapDrawListIdx = shadowMapDrawListIdx;
            light.shadowMapIdx = shadowMapDrawListIdx;

            const auto spotLightFrustum =
                util::createFrustumFromVPMatrix(light.lightSpaceProj * light.lightSpaceView);
            for (const auto& drawInfo : drawList) {
                const auto& object = objects[drawInfo.objectIdx];
                if (object.alpha == 1.f && util::isInFrustum(spotLightFrustum, object.worldAABB)) {
                    shadowMapOpaqueDrawLists[shadowMapDrawListIdx].push_back(drawInfo);
                }
            }
            ++shadowMapDrawListIdx;
        } else if (light.light.type == LIGHT_TYPE_POINT) {
            light.shadowMapDrawListIdx = shadowMapDrawListIdx;
            light.shadowMapIdx = shadowMapDrawListIdx;

            for (int i = 0; i < 6; ++i) {
                auto cam = pointLightShadowMapCameras[i];
                cam.setPosition(light.position);
                const auto frustum = util::createFrustumFromVPMatrix(cam.getViewProj());
                for (const auto& drawInfo : drawList) {
                    const auto& object = objects[drawInfo.objectIdx];
                    if (object.alpha == 1.f && util::isInFrustum(frustum, object.worldAABB)) {
                        shadowMapOpaqueDrawLists[shadowMapDrawListIdx].push_back(drawInfo);
                    }
                }
                ++shadowMapDrawListIdx;
            }
        }
    }
}

void App::uploadSceneData()
{
    sceneData.clear();

    std::size_t currentCameraIdxUbo = 0;

    // main camera
    const auto cd = UBOCameraData{
        .projection = camera.getProjection(),
        .view = camera.getView(),
        .cameraPos = glm::vec4{camera.getPosition(), 0.f},
    };
    mainCameraUboOffset = sceneData.append(cd, uboAlignment);
    ++currentCameraIdxUbo;

    // light "cameras"
    for (auto& light : lights) {
        if (!light.castsShadow || light.culled) {
            continue;
        }

        if (currentCameraIdxUbo >= MAX_CAMERAS_IN_UBO) {
            continue;
        }

        if (light.light.type == LIGHT_TYPE_SPOT) {
            const auto cd = UBOCameraData{
                .projection = light.lightSpaceProj,
                .view = light.lightSpaceView,
                .cameraPos = glm::vec4{light.position, 0.f},
            };
            light.camerasUboOffset = sceneData.append(cd, uboAlignment);
            ++currentCameraIdxUbo;
        } else if (light.light.type == LIGHT_TYPE_POINT) {
            for (int i = 0; i < 6; ++i) {
                auto cam = pointLightShadowMapCameras[i];
                cam.setPosition(light.position);
                const auto cd = UBOCameraData{
                    .projection = cam.getProjection(),
                    .view = cam.getView(),
                    .cameraPos = glm::vec4{cam.getPosition(), 0.f},
                };
                const auto offset = sceneData.append(cd, uboAlignment);
                if (i == 0) {
                    light.camerasUboOffset = offset;
                }
                ++currentCameraIdxUbo;
            }
        }
    }

    auto ld = UBOLightData{
        // ambient
        .ambientColor = glm::vec3{ambientColor},
        .ambientIntensity = ambientIntensity,
        .sunLight = toGPULightData({}, sunLightDir, sunLight),
    };

    // light space TMs (spot lights)
    std::size_t currentTMIdx = 0;
    for (auto& light : lights) {
        if (!light.castsShadow || light.culled) {
            continue;
        }
        if (currentTMIdx >= MAX_SHADOW_CASTING_LIGHTS) {
            light.lightSpaceTMsIdx = MAX_SHADOW_CASTING_LIGHTS;
            continue;
        }

        ld.lightSpaceTMs[currentTMIdx] = light.lightSpaceProj * light.lightSpaceView;
        light.lightSpaceTMsIdx = currentTMIdx;

        ++currentTMIdx;
    }

    // lights
    assert(lights.size() <= MAX_LIGHTS_IN_UBO);
    std::size_t currentLightIndex = 0;
    for (const auto& light : lights) {
        if (light.culled) {
            continue;
        }

        auto gpuLD = toGPULightData(light.position, light.direction, light.light);
        if (light.castsShadow) {
            gpuLD.lightSpaceTMsIdx = light.lightSpaceTMsIdx;
            gpuLD.shadowMapIdx = light.shadowMapIdx;
        }
        if (light.castsShadow && light.light.type == LIGHT_TYPE_POINT) {
            // all point light cameras have the same projection
            const auto& m = pointLightShadowMapCameras[0].getProjection();
            gpuLD.pointLightProjBR = glm::vec4{m[2][2], m[3][2], m[2][3], m[3][3]};
        }

        ld.lights[currentLightIndex] = gpuLD;
        ++currentLightIndex;
    }

    lightDataUboOffset = sceneData.append(ld, uboAlignment);

    // per object data
    for (auto& drawInfo : drawList) {
        const auto& object = objects[drawInfo.objectIdx];
        const auto d = UBOPerObjectData{
            .model = object.transform.asMatrix(),
            .props = glm::vec4{object.alpha, 0.f, 0.f, 0.f},
            .lightIdx = drawInfo.lightIdx,
        };
        drawInfo.uboOffset = sceneData.append(d, uboAlignment);
    }

    // reallocate buffer if needed
    while (sceneData.getData().size() > sceneDataBuffer.size) {
        glDeleteBuffers(1, &sceneDataBuffer.buffer);
        sceneDataBuffer = gfx::allocateBuffer(sceneDataBuffer.size * 2, nullptr, "sceneData");
        std::cout << "Reallocated UBO, new size = " << sceneData.getData().size() << std::endl;
    }

    // upload to GPU
    glNamedBufferSubData(
        sceneDataBuffer.buffer, 0, sceneData.getData().size(), sceneData.getData().data());
}

void App::renderSpotLightShadowMap(const CPULightData& lightData)
{
    assert(!lightData.culled);
    assert(lightData.castsShadow);
    assert(lightData.lightSpaceTMsIdx != MAX_SHADOW_CASTING_LIGHTS);
    assert(lightData.shadowMapDrawListIdx != SHADOW_MAP_ARRAY_LAYERS);

    glNamedFramebufferTextureLayer(
        shadowMapFBO, GL_DEPTH_ATTACHMENT, shadowMapDepthTexture, 0, lightData.shadowMapIdx);

    // clear
    GLfloat depthValue{1.f};
    glClearNamedFramebufferfv(shadowMapFBO, GL_DEPTH, 0, &depthValue);

    glBindBufferRange(
        GL_UNIFORM_BUFFER,
        CAMERA_DATA_BINDING,
        sceneDataBuffer.buffer,
        lightData.camerasUboOffset,
        sizeof(UBOCameraData));

    renderSceneObjects(shadowMapOpaqueDrawLists[lightData.shadowMapDrawListIdx]);
}

void App::renderPointLightShadowMap(const CPULightData& lightData)
{
    assert(!lightData.culled);
    assert(lightData.castsShadow);
    assert(lightData.shadowMapDrawListIdx != SHADOW_MAP_ARRAY_LAYERS);

    const auto alignedCameraDataSize = gfx::getAlignedSize(sizeof(UBOCameraData), uboAlignment);
    for (int i = 0; i < 6; ++i) {
        glNamedFramebufferTextureLayer(
            shadowMapFBO,
            GL_DEPTH_ATTACHMENT,
            shadowMapDepthTexture,
            0,
            lightData.shadowMapIdx + i);

        // clear
        GLfloat depthValue{1.f};
        glClearNamedFramebufferfv(shadowMapFBO, GL_DEPTH, 0, &depthValue);

        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            CAMERA_DATA_BINDING,
            sceneDataBuffer.buffer,
            lightData.camerasUboOffset + i * alignedCameraDataSize,
            sizeof(UBOCameraData));

        renderSceneObjects(shadowMapOpaqueDrawLists[lightData.shadowMapDrawListIdx + i]);
    }
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
            sizeof(UBOPerObjectData));

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

    for (const auto& light : lights) {
        debugRenderer.addLine(
            light.position,
            light.position + glm::vec3{0.f, 0.1f, 0.f},
            glm::vec4{1.f, 1.f, 0.f, 1.f});
    }

    // debugRenderer.addFrustumLines(spotLightCamera);
    // debugRenderer.addFrustumLines(testCamera);

    {
        // const auto lightPos = lights[0].position;
        // debugRenderer.addLine(lightPos, testFragPos, glm::vec4{0.f, 0.f, 1.f, 1.f});

        /* const auto v = glm::normalize(testFragPos - testLightPos);
        debugRenderer.addLine({}, v * 5.f, glm::vec4{1.f, 0.f, 1.f, 1.f});
        debugRenderer.addLine({}, v * 1.f, glm::vec4{0.f, 1.f, 1.f, 1.f});

        float originLength = 0.25f;
        debugRenderer
            .addLine(v, v + glm::vec3{1.f, 0.f, 0.f} * originLength, glm::vec4{1.f, 0.f,
        0.f, 1.f}); debugRenderer .addLine(v, v + glm::vec3{0.f, 1.f, 0.f} * originLength,
        glm::vec4{0.f, 1.f, 0.f, 1.f}); debugRenderer .addLine(v, v + glm::vec3{0.f, 0.f, 1.f} *
        originLength, glm::vec4{0.f, 0.f, 1.f, 1.f});

        const auto cubeAABB = AABB{
            .min = glm::vec3(glm::vec3{-1.f}),
            .max = glm::vec3(glm::vec3{1.f}),
        };
        debugRenderer.addAABBLines(cubeAABB, glm::vec4{1.f, 1.f, 0.f, 1.f}); */
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
    for (const auto& light : lights) {
        if (light.light.type == LIGHT_TYPE_SPOT) {
            debugRenderer.addLine(
                light.position,
                light.position + light.direction * 1.f,
                glm::vec4{1.f, 0.f, 0.f, 1.f},
                glm::vec4{0.f, 1.f, 0.f, 1.f});
        } else if (light.light.type == LIGHT_TYPE_POINT) {
            /* const auto lightAABB = AABB{
                .min = glm::vec3(light.position - glm::vec3(light.light.range)),
                .max = glm::vec3(light.position + glm::vec3(light.light.range))};
            debugRenderer.addAABBLines(lightAABB, light.light.color);
            debugRenderer.addLine(
                light.position,
                light.position + glm::vec3{0.f, 1.f, 0.f},
                glm::vec4{0.f, 1.f, 0.f, 1.f}); */
        }
    }

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
            sizeof(UBOPerObjectData));

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

void App::addSpotLight(
    const glm::vec3& pos,
    const glm::vec3& dir,
    const Light& light,
    bool castShadow)
{
    auto ld = CPULightData{
        .position = pos,
        .direction = dir,
        .light = light,
        .castsShadow = castShadow,
    };

    const auto spotLightCamera = makeSpotLightCamera(pos, dir, light.range, light.outerConeAngle);
    ld.lightSpaceProj = spotLightCamera.getProjection();
    ld.lightSpaceView = spotLightCamera.getView();

    lights.push_back(std::move(ld));
}
