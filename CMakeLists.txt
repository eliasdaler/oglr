cmake_minimum_required(VERSION 3.21)

project(
  OGLR
  VERSION 0.0.1
  LANGUAGES CXX C
)

if (WIN32)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "Link to libs dynamically")
else()
  set(BUILD_SHARED_LIBS ON CACHE BOOL "Link to libs dynamically")
endif()

add_subdirectory(third_party)

add_executable(app
  src/GLDebugCallback.cpp
  src/GraphicsUtil.cpp
  src/FrustumCulling.cpp
  src/ImageLoader.cpp
  src/AABB.cpp
  src/DebugRenderer.cpp
  src/Camera.cpp
  src/App.cpp
  src/main.cpp
)
set_property(TARGET app PROPERTY CXX_STANDARD 20)

# SDL
if(BUILD_SHARED_LIBS)
  target_link_libraries(app PUBLIC
    SDL2::SDL2
  )
else()
  target_link_libraries(app PUBLIC
    SDL2::SDL2-static
  )
endif()


if(WIN32)
  target_link_libraries(app PRIVATE
    SDL2::SDL2main
  )
endif()

# other libs
target_link_libraries(app PUBLIC
  glad::glad
  glm::glm
  stb::image
  imgui::imgui
)

# glm
target_compile_definitions(app
  PUBLIC
    GLM_FORCE_CTOR_INIT
    GLM_FORCE_XYZW_ONLY
    GLM_FORCE_EXPLICIT_CTOR
    GLM_ENABLE_EXPERIMENTAL
)

set(GAME_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets")
add_custom_command(TARGET app POST_BUILD
    COMMENT "Symlink assets to $<TARGET_FILE_DIR:app>/assets"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "${GAME_ASSETS_PATH}" "$<TARGET_FILE_DIR:app>/assets"
  )

