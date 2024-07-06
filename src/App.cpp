#include "App.h"

#include <SDL2/SDL.h>

#include <chrono>
#include <numeric> // lerp

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
}

void App::cleanup() {
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

void App::render() {}
