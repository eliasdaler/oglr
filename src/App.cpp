#include "App.h"

#include <chrono>
#include <iostream>

#include <glad/gl.h>

#include <fstream>
#include <glm/gtc/type_ptr.hpp>

#include "ImageLoader.h"

namespace
{
constexpr auto WINDOW_WIDTH = 1280;
constexpr auto WINDOW_HEIGHT = 960;

constexpr auto VP_UNIFORM_LOC = 0;
constexpr auto MODEL_UNIFORM_LOC = 1;
constexpr auto FRAG_TEXTURE_UNIFORM_LOC = 2;

std::string loadFileToString(const std::filesystem::path& path)
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
        printf("Unable to load GL.\n");
        std::exit(1);
    }

    glEnable(GL_FRAMEBUFFER_SRGB);

    { // build shaders
        constexpr int MAX_SHADER_LOG_LENGTH = 512;
        // vertex shader
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        const auto vertShaderStr = loadFileToString("assets/shaders/basic.vert");
        const char* vertSource = vertShaderStr.c_str();
        glShaderSource(vertexShader, 1, &vertSource, NULL);
        glCompileShader(vertexShader);
        // check for shader compile errors
        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertexShader, MAX_SHADER_LOG_LENGTH, NULL, infoLog);
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        }
        // fragment shader
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        const auto fragShaderStr = loadFileToString("assets/shaders/basic.frag");
        const char* fragSource = fragShaderStr.c_str();
        glShaderSource(fragmentShader, 1, &fragSource, NULL);
        glCompileShader(fragmentShader);
        // check for shader compile errors
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragmentShader, MAX_SHADER_LOG_LENGTH, NULL, infoLog);
            std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        }
        // link shaders
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        // check for linking errors
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shaderProgram, MAX_SHADER_LOG_LENGTH, NULL, infoLog);
            std::cout << "linking failed: " << infoLog << std::endl;
        }
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    { // make cube
        // clang-format off
        float vertices[] = {
            -0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
             0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
             0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
            -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
            -0.5f, -0.5f, -0.5f,  1.0f, 0.0f,

            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

            -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
            -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
            -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

             0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
             0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
             0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
             0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
             0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
             0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

            -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
             0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
            -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
        };
        // clang-format on

        unsigned int indices[] = {0, 1, 3, 1, 2, 3};
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(
            1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    { // init camera
        const auto fovX = 45.f;
        const auto zNear = 0.1f;
        const auto zFar = 1000.f;
        camera.init(fovX, zNear, zFar, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT);
        camera.setPosition(glm::vec3{0.f, 1.f, -3.f});
        camera.lookAt(glm::vec3{0.f, 0.f, 0.f});
    }

    { // load texture
        auto imageData = util::loadImage("assets/images/test_texture.png");
        if (!imageData.pixels) {
            std::cout << "Failed to load image\n";
            std::exit(1);
        }
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

        // glGenerateTextureMipmap(texture);

        // assume that we'll bind texture to texture slot 0 for now
        glUseProgram(shaderProgram);
        glUniform1i(FRAG_TEXTURE_UNIFORM_LOC, 0);
    }

    // initial state
    glEnable(GL_DEPTH_TEST);
}

void App::cleanup()
{
    glDeleteTextures(1, &texture);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
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
}

void App::render()
{
    glClearColor(97.f / 255.f, 120.f / 255.f, 159.f / 255.f, 1.0f);
    glClearDepth(1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    {
        glUseProgram(shaderProgram);

        // update camera
        const auto vp = camera.getViewProj();
        glUniformMatrix4fv(VP_UNIFORM_LOC, 1, GL_FALSE, glm::value_ptr(vp));

        // set cube transform
        const auto tm = cubeTransform.asMatrix();
        glUniformMatrix4fv(MODEL_UNIFORM_LOC, 1, GL_FALSE, glm::value_ptr(tm));

        glBindTextureUnit(0, texture);

        glBindVertexArray(vao);
        // draw cube
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    SDL_GL_SwapWindow(window);
}
