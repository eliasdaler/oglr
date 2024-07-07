#include "App.h"

#include <chrono>
#include <iostream>

#include <glad/gl.h>

#include <fstream>
#include <glm/gtc/type_ptr.hpp>

#include "GLDebugCallback.h"
#include "ImageLoader.h"

namespace
{
constexpr auto WINDOW_WIDTH = 1280;
constexpr auto WINDOW_HEIGHT = 960;

constexpr auto VP_UNIFORM_LOC = 0;
constexpr auto MODEL_UNIFORM_LOC = 1;
constexpr auto FRAG_TEXTURE_UNIFORM_LOC = 2;

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

    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    // glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTextureStorage2D(texture, 1, GL_SRGB8_ALPHA8, imageData.width, imageData.height);
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

    return texture;
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

    // we still need an empty VAO even for vertex pulling
    glGenVertexArrays(1, &vao);

    { // make cube
        struct Vertex {
            glm::vec4 position;
            glm::vec2 uv;
            glm::vec2 _padding;
        };
        std::vector<Vertex> vertices2{
            {glm::vec4{-0.5f, -0.5f, -0.5f, 0.f}, glm::vec2{1.0f, 0.0f}},
            {glm::vec4{0.5f, -0.5f, -0.5f, 0.f}, glm::vec2{0.0f, 0.0f}},
            {glm::vec4{0.5f, 0.5f, -0.5f, 0.f}, glm::vec2{0.0f, 1.0f}},
            {glm::vec4{0.5f, 0.5f, -0.5f, 0.f}, glm::vec2{0.0f, 1.0f}},
            {glm::vec4{-0.5f, 0.5f, -0.5f, 0.f}, glm::vec2{1.0f, 1.0f}},
            {glm::vec4{-0.5f, -0.5f, -0.5f, 0.f}, glm::vec2{1.0f, 0.0f}},

            {glm::vec4{-0.5f, -0.5f, 0.5f, 0.f}, glm::vec2{0.0f, 0.0f}},
            {glm::vec4{0.5f, -0.5f, 0.5f, 0.f}, glm::vec2{1.0f, 0.0f}},
            {glm::vec4{0.5f, 0.5f, 0.5f, 0.f}, glm::vec2{1.0f, 1.0f}},
            {glm::vec4{0.5f, 0.5f, 0.5f, 0.f}, glm::vec2{1.0f, 1.0f}},
            {glm::vec4{-0.5f, 0.5f, 0.5f, 0.f}, glm::vec2{0.0f, 1.0f}},
            {glm::vec4{-0.5f, -0.5f, 0.5f, 0.f}, glm::vec2{0.0f, 0.0f}},

            {glm::vec4{-0.5f, 0.5f, 0.5f, 0.f}, glm::vec2{1.0f, 0.0f}},
            {glm::vec4{-0.5f, 0.5f, -0.5f, 0.f}, glm::vec2{1.0f, 1.0f}},
            {glm::vec4{-0.5f, -0.5f, -0.5f, 0.f}, glm::vec2{0.0f, 1.0f}},
            {glm::vec4{-0.5f, -0.5f, -0.5f, 0.f}, glm::vec2{0.0f, 1.0f}},
            {glm::vec4{-0.5f, -0.5f, 0.5f, 0.f}, glm::vec2{0.0f, 0.0f}},
            {glm::vec4{-0.5f, 0.5f, 0.5f, 0.f}, glm::vec2{1.0f, 0.0f}},

            {glm::vec4{0.5f, 0.5f, 0.5f, 0.f}, glm::vec2{1.0f, 0.0f}},
            {glm::vec4{0.5f, 0.5f, -0.5f, 0.f}, glm::vec2{1.0f, 1.0f}},
            {glm::vec4{0.5f, -0.5f, -0.5f, 0.f}, glm::vec2{0.0f, 1.0f}},
            {glm::vec4{0.5f, -0.5f, -0.5f, 0.f}, glm::vec2{0.0f, 1.0f}},
            {glm::vec4{0.5f, -0.5f, 0.5f, 0.f}, glm::vec2{0.0f, 0.0f}},
            {glm::vec4{0.5f, 0.5f, 0.5f, 0.f}, glm::vec2{1.0f, 0.0f}},

            {glm::vec4{-0.5f, -0.5f, -0.5f, 0.f}, glm::vec2{0.0f, 1.0f}},
            {glm::vec4{0.5f, -0.5f, -0.5f, 0.f}, glm::vec2{1.0f, 1.0f}},
            {glm::vec4{0.5f, -0.5f, 0.5f, 0.f}, glm::vec2{1.0f, 0.0f}},
            {glm::vec4{0.5f, -0.5f, 0.5f, 0.f}, glm::vec2{1.0f, 0.0f}},
            {glm::vec4{-0.5f, -0.5f, 0.5f, 0.f}, glm::vec2{0.0f, 0.0f}},
            {glm::vec4{-0.5f, -0.5f, -0.5f, 0.f}, glm::vec2{0.0f, 1.0f}},

            {glm::vec4{-0.5f, 0.5f, -0.5f, 0.f}, glm::vec2{0.0f, 1.0f}},
            {glm::vec4{0.5f, 0.5f, -0.5f, 0.f}, glm::vec2{1.0f, 1.0f}},
            {glm::vec4{0.5f, 0.5f, 0.5f, 0.f}, glm::vec2{1.0f, 0.0f}},
            {glm::vec4{0.5f, 0.5f, 0.5f, 0.f}, glm::vec2{1.0f, 0.0f}},
            {glm::vec4{-0.5f, 0.5f, 0.5f, 0.f}, glm::vec2{0.0f, 0.0f}},
            {glm::vec4{-0.5f, 0.5f, -0.5f, 0.f}, glm::vec2{0.0f, 1.0f}},
        };

        glCreateBuffers(1, &verticesBuffer);
        setDebugLabel(GL_BUFFER, verticesBuffer, "vertices");
        glNamedBufferStorage(
            verticesBuffer,
            sizeof(Vertex) * vertices2.size(),
            vertices2.data(),
            GL_DYNAMIC_STORAGE_BIT);
    }

    texture = loadTextureFromFile("assets/images/test_texture.png");
    if (texture == 0) {
        std::exit(1);
    }

    // initial state
    glEnable(GL_DEPTH_TEST);

    { // init camera
        const auto fovX = 45.f;
        const auto zNear = 0.1f;
        const auto zFar = 1000.f;
        camera.init(fovX, zNear, zFar, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT);
        camera.setPosition(glm::vec3{0.f, 1.f, -3.5f});
        camera.lookAt(glm::vec3{0.f, 0.f, 0.f});
    }

    cubeTransform2.position.x += 1.f;
    cubeTransform2.position.y += 1.f;
    cubeTransform2.scale = glm::vec3{0.25f};
}

void App::cleanup()
{
    glDeleteBuffers(1, &verticesBuffer);
    glDeleteTextures(1, &texture);
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
    // rotate cube
    static const auto rotationSpeed = glm::radians(45.f);
    cubeTransform.heading *= glm::angleAxis(rotationSpeed * dt, glm::vec3{0.f, 1.f, 0.f});

    cubeTransform2.heading *=
        glm::angleAxis(rotationSpeed * dt, glm::normalize(glm::vec3{-1.f, 1.f, 0.f}));
}

void App::render()
{
    glClearColor(97.f / 255.f, 120.f / 255.f, 159.f / 255.f, 1.0f);
    glClearDepth(1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(vao);
    {
        // vertices
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, verticesBuffer);

        // update camera
        const auto vp = camera.getViewProj();
        glProgramUniformMatrix4fv(shaderProgram, VP_UNIFORM_LOC, 1, GL_FALSE, glm::value_ptr(vp));

        // set texture
        glBindTextureUnit(0, texture);
        glProgramUniform1i(shaderProgram, FRAG_TEXTURE_UNIFORM_LOC, 0);

        glUseProgram(shaderProgram);

        // draw first cube
        auto tm = cubeTransform.asMatrix();
        glProgramUniformMatrix4fv(
            shaderProgram, MODEL_UNIFORM_LOC, 1, GL_FALSE, glm::value_ptr(tm));
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // draw second cube
        tm = cubeTransform2.asMatrix();
        glProgramUniformMatrix4fv(
            shaderProgram, MODEL_UNIFORM_LOC, 1, GL_FALSE, glm::value_ptr(tm));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    SDL_GL_SwapWindow(window);
}
