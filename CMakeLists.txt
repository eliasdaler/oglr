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
)
