#include "App.h"

#include <chrono>
#include <iostream>

#include <glad/gl.h>

#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp> // lerp for vec3

#include "GLDebugCallback.h"
#include "ImageLoader.h"
#include "Meshes.h"

namespace
{
constexpr auto WINDOW_WIDTH = 1280;
constexpr auto WINDOW_HEIGHT = 960;

constexpr auto FRAG_TEXTURE_UNIFORM_LOC = 1;

constexpr auto GLOBAL_SCENE_DATA_BINDING = 0;
constexpr auto PER_OBJECT_DATA_BINDING = 1;
constexpr auto VERTEX_DATA_BINDING = 2;

void setDebugLabel(GLenum identifier, GLuint name, std::string_view label)
{
    glObjectLabel(identifier, name, label.size(), label.data());
}

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
        std::cout << "Failed to compile shader:" << log << std::endl;
        return 0;
    }
    setDebugLabel(GL_SHADER, shader, path.string());
    return shader;
}

// will add ability to pass params later
GLuint loadTextureFromFile(const std::filesystem::path& path)
{
    const auto imageData = util::loadImage(path);
    if (!imageData.pixels) {
        std::cout << "Failed to load image from " << path << "\n";
        return 0;
    }

    GLuint texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);

    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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

GLuint allocateBuffer(std::size_t size, const char* debugName)
{
    GLuint buffer;
    glCreateBuffers(1, &buffer);
    setDebugLabel(GL_BUFFER, buffer, "sceneData");
    glNamedBufferStorage(buffer, (GLsizeiptr)size, nullptr, GL_DYNAMIC_STORAGE_BIT);
    return buffer;
}

int getUBOArrayElementSize(std::size_t elementSize, int uboAlignment)
{
    if (elementSize < uboAlignment) {
        return uboAlignment;
    }
    if (elementSize % uboAlignment == 0) {
        return elementSize;
    }
    return ((elementSize / uboAlignment) + 1) * uboAlignment;
}

template<typename T, typename RNGType>
std::size_t chooseRandomElement(const std::vector<T>& v, RNGType& rng)
{
    if (v.empty()) {
        return 0;
    }
    std::uniform_int_distribution<std::size_t> dist{0, v.size() - 1};
    return dist(rng);
}

GPUMesh uploadMeshToGPU(const util::CPUMesh& cpuMesh)
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

    GLuint vertexBuffer;
    glCreateBuffers(1, &vertexBuffer);
    glNamedBufferStorage(
        vertexBuffer, sizeof(GPUVertex) * vertices.size(), vertices.data(), GL_DYNAMIC_STORAGE_BIT);

    GLuint indexBuffer;
    glCreateBuffers(1, &indexBuffer);
    glNamedBufferStorage(
        indexBuffer,
        sizeof(std::uint32_t) * cpuMesh.indices.size(),
        cpuMesh.indices.data(),
        GL_DYNAMIC_STORAGE_BIT);

    return GPUMesh{
        .vertexBuffer = vertexBuffer,
        .indexBuffer = indexBuffer,
        .numIndices = static_cast<std::uint32_t>(cpuMesh.indices.size()),
    };
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

}

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

    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uboAlignment);

    { // shaders
        const auto vertexShader = compileShader("assets/shaders/basic.vert", GL_VERTEX_SHADER);
        if (vertexShader == 0) {
            std::exit(1);
        }

        const auto fragShader = compileShader("assets/shaders/basic.frag", GL_FRAGMENT_SHADER);
        if (fragShader == 0) {
            std::exit(1);
        }

        shaderProgram = glCreateProgram();
        setDebugLabel(GL_PROGRAM, shaderProgram, "shader");

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
    }

    { // allocate scene data buffer
        globalSceneDataSize = getUBOArrayElementSize(sizeof(GlobalSceneData), uboAlignment);
        perObjectDataElementSize = getUBOArrayElementSize(sizeof(PerObjectData), uboAlignment);
        allocatedBufferSize = globalSceneDataSize + perObjectDataElementSize * 100;
        sceneDataBuffer = allocateBuffer(allocatedBufferSize, "sceneData");
    }

    // we still need an empty VAO even for vertex pulling
    glGenVertexArrays(1, &vao);

    const auto texturesToLoad = {
        "assets/images/texture1.png",
        "assets/images/texture2.png",
    };
    for (const auto& texturePath : texturesToLoad) {
        auto texture = loadTextureFromFile(texturePath);
        if (texture == 0) {
            std::exit(1);
        }
        textures.push_back(texture);
    }

    { // load meshes
        meshes.push_back(uploadMeshToGPU(util::getCubeMesh()));
        meshes.push_back(uploadMeshToGPU(util::getStarMesh()));
    }

    // initial state
    glClearDepth(1.f);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    { // init camera
        const auto fovX = 45.f;
        const auto zNear = 0.1f;
        const auto zFar = 1000.f;
        camera.init(fovX, zNear, zFar, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT);
        camera.setPosition(glm::vec3{-5.f, 10.f, -50.0f});
        // camera.lookAt(glm::vec3{0.f, 2.f, 0.f});
    }

    auto numObjectsToSpawn = dist(rng);
    numObjectsToSpawn = 10;
    numObjectsToSpawn = 0;
    for (int i = 0; i < numObjectsToSpawn; ++i) {
        generateRandomObject();
    }

    spawnCube({0.f, 0.f, 0.f}, 0);
    spawnCube({0.f, 0.f, 2.5f}, 1);
    spawnCube({0.f, 0.f, 5.0f}, 0);

    camera.setPosition({0.f, 0.5f, -10.f});

    // init lights
    sunlightColor = glm::vec3{0.65, 0.4, 0.3};
    sunlightIntensity = 1.5f;
    sunlightDir = glm::normalize(glm::vec3(1.0, -1.0, 1.0));

    ambientColor = glm::vec3{0.3, 0.65, 0.8};
    ambientIntensity = 0.2f;
}

void App::cleanup()
{
    glDeleteTextures(textures.size(), textures.data());
    for (const auto& mesh : meshes) {
        glDeleteBuffers(1, &mesh.indexBuffer);
        glDeleteBuffers(1, &mesh.vertexBuffer);
    }
    glDeleteBuffers(1, &sceneDataBuffer);

    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(shaderProgram);

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

    // rotate objects
    static const auto rotationSpeed = glm::radians(45.f);
    for (auto& object : objects) {
        // object.transform.heading *= glm::angleAxis(rotationSpeed * dt, glm::vec3{1.f, 1.f, 0.f});
    }

    timer += dt;
    if (timer >= timeToSpawnNewCube) {
        timer = 0.f;
        generateRandomObject();
    }
}

void App::handleFreeCameraControls(float dt)
{
    const auto GLOBAL_UP_DIR = glm::vec3{0.f, 1.f, 0.f};
    const auto GLOBAL_FRONT_DIR = glm::vec3{0.f, 0.f, 1.f};
    const auto GLOBAL_RIGHT_DIR = glm::vec3{1.f, 0.f, 0.f};

    { // move
        const glm::vec3 cameraWalkSpeed = {10.f, 5.f, 10.f};

        const glm::vec2 moveStickState =
            getStickState({SDL_SCANCODE_A, SDL_SCANCODE_D}, {SDL_SCANCODE_W, SDL_SCANCODE_S});

        const glm::vec2 moveUpDownState =
            getStickState({SDL_SCANCODE_Q, SDL_SCANCODE_E}, {SDL_SCANCODE_W, SDL_SCANCODE_S});

        glm::vec3 moveVector{};
        moveVector += camera.getFront() * (-moveStickState.y);
        moveVector += camera.getRight() * (-moveStickState.x);
        moveVector += GLOBAL_UP_DIR * moveUpDownState.x;

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

        const auto dYaw = glm::angleAxis(rotationVelocity.x * dt, GLOBAL_UP_DIR);
        const auto dPitch = glm::angleAxis(rotationVelocity.y * dt, GLOBAL_RIGHT_DIR);
        const auto newHeading = dYaw * camera.getHeading() * dPitch;
        camera.setHeading(newHeading);
    }
}

void App::render()
{
    glClearColor(97.f / 255.f, 120.f / 255.f, 159.f / 255.f, 1.0f);
    glClearDepth(1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    uploadSceneData();

    glBindVertexArray(vao);
    {
        // object texture will be read from TU0
        glProgramUniform1i(shaderProgram, FRAG_TEXTURE_UNIFORM_LOC, 0);

        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            GLOBAL_SCENE_DATA_BINDING,
            sceneDataBuffer,
            0,
            sizeof(GlobalSceneData));

        glUseProgram(shaderProgram);

        // sort by distance to ther camera (decreasing)
        std::vector<std::size_t> sortedIdx(objects.size());
        std::iota(sortedIdx.begin(), sortedIdx.end(), 0);
        std::sort(sortedIdx.begin(), sortedIdx.end(), [this](std::size_t a, std::size_t b) {
            const auto lengthA = glm::length(camera.getPosition() - objects[a].transform.position);
            const auto lengthB = glm::length(camera.getPosition() - objects[b].transform.position);
            return lengthA > lengthB;
        });

        // draw front to back
        for (std::size_t i = 0; i < sortedIdx.size(); ++i) {
            auto objectIdx = sortedIdx[i];
            const auto& object = objects[objectIdx];
            const auto& mesh = meshes[object.meshIdx];
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, VERTEX_DATA_BINDING, mesh.vertexBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);

            glBindBufferRange(
                GL_UNIFORM_BUFFER,
                PER_OBJECT_DATA_BINDING,
                sceneDataBuffer,
                globalSceneDataSize + objectIdx * perObjectDataElementSize,
                sizeof(PerObjectData));

            glBindTextureUnit(0, textures[object.textureIdx]);
            glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
        }
    }

    SDL_GL_SwapWindow(window);
}

void App::uploadSceneData()
{
    sceneData.clear();
    const auto projection = camera.getProjection();
    const auto view = camera.getView();

    const auto sceneDataSize = globalSceneDataSize + perObjectDataElementSize * objects.size();
    sceneData.resize(sceneDataSize);

    int currentOffset = 0;

    // global scene data
    const auto d = GlobalSceneData{
        .projection = projection,
        .view = view,
        .cameraPos = glm::vec4{camera.getPosition(), 0.f},
        .sunlightColorAndIntensity = glm::vec4{sunlightColor, sunlightIntensity},
        .sunlightDirAndUnused = glm::vec4{sunlightDir, 0.f},
        .ambientColorAndIntensity = glm::vec4{ambientColor, ambientIntensity},
    };
    std::memcpy(sceneData.data() + currentOffset, &d, sizeof(GlobalSceneData));
    currentOffset += globalSceneDataSize;

    // per object data
    for (const auto& object : objects) {
        const auto d = PerObjectData{
            .model = object.transform.asMatrix(),
        };
        std::memcpy(sceneData.data() + currentOffset, &d, sizeof(PerObjectData));
        currentOffset += perObjectDataElementSize;
    }

    // reallocate buffer if needed
    // const auto sceneDataSize = sizeof(PerObjectData) * sceneData.size();
    if (sceneDataSize > allocatedBufferSize) {
        while (sceneDataSize > allocatedBufferSize) {
            allocatedBufferSize *= 2;
        }
        glDeleteBuffers(1, &sceneDataBuffer);
        sceneDataBuffer = allocateBuffer(allocatedBufferSize, "sceneData");
        std::cout << "Reallocated UBO, new size = " << allocatedBufferSize << std::endl;
    }

    // upload new data
    glNamedBufferSubData(sceneDataBuffer, 0, sceneDataSize, sceneData.data());
}

void App::generateRandomObject()
{
    ObjectData object{
        .meshIdx = chooseRandomElement(meshes, rng),
        .textureIdx = chooseRandomElement(textures, rng),
    };
    object.transform.position.x = dist2(rng);
    object.transform.position.y = dist2(rng);
    objects.push_back(object);
}

void App::spawnCube(const glm::vec3& pos, std::size_t textureIdx)
{
    ObjectData object{
        .meshIdx = 0,
        .textureIdx = textureIdx,
    };
    object.transform.position = pos;
    objects.push_back(object);
}
