#pragma once

struct SDL_Window;

class App {
public:
  void start();

private:
  void init();
  void cleanup();
  void run();
  void update(float dt);
  void render();

  SDL_Window *window{nullptr};
  bool isRunning{false};
  bool frameLimit{true};
  float frameTime{0.f};
  float avgFPS{0.f};
};
