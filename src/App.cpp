#include "App.h"

#include <chrono>

#include <glad/gl.h>

const char *vertexShader = R"(
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
}

void App::cleanup() {
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

  SDL_GL_SwapWindow(window);
}
