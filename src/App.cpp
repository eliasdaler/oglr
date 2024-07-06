#include "App.h"

#include <chrono>
#include <iostream>

#include <glad/gl.h>

const char *vertexShaderSource = R"(
#version 460 core
layout (location = 0) in vec3 aPos;
void main()
{
   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
)";

const char *fragmentShaderSource = R"(
#version 460 core
out vec4 FragColor;
void main()
{
   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
}
)";

void App::start() {
  init();
  run();
  cleanup();
}

void App::init() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) <
      0) {
    printf("SDL could not initialize. SDL Error: %s\n", SDL_GetError());
    std::exit(1);
  }

  window = SDL_CreateWindow("App",
                            // pos
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            // size
                            1280, 960, SDL_WINDOW_OPENGL);

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
    // vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
      std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                << infoLog << std::endl;
    }
    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
      std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                << infoLog << std::endl;
    }
    // link shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
      std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
  }

  {
    float vertices[] = {
        0.5f,  0.5f,  0.0f, // top right
        0.5f,  -0.5f, 0.0f, // bottom right
        -0.5f, -0.5f, 0.0f, // bottom left
        -0.5f, 0.5f,  0.0f  // top left
    };
    unsigned int indices[] = {0, 1, 3, 1, 2, 3};
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                          (void *)0);
    glEnableVertexAttribArray(0);
  }
}

void App::cleanup() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteProgram(shaderProgram);

  SDL_GL_DeleteContext(glContext);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void App::run() {
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
      const auto frameTime =
          std::chrono::duration<float>(now - prevTime).count();
      if (dt > frameTime) {
        SDL_Delay(static_cast<std::uint32_t>(dt - frameTime));
      }
    }
  }
}

void App::update(float dt) {}

void App::render() {
  glClearColor(97.f / 255.f, 120.f / 255.f, 159.f / 255.f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // draw rect
  glUseProgram(shaderProgram);
  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  SDL_GL_SwapWindow(window);
}
