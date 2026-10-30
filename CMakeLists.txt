cmake_minimum_required(VERSION 3.20)
project(graphics_toggle CXX)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
  preloader_android
  GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
  GIT_TAG 0.2.2
)
FetchContent_MakeAvailable(preloader_android)

add_library(graphics_toggle SHARED src/main.cpp)
target_link_libraries(graphics_toggle PRIVATE preloader EGL dl log)