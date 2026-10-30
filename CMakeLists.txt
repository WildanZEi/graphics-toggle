cmake_minimum_required(VERSION 3.22)
project(graphics_toggle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# --- 1. Ambil preloader SDK (Tanpa SOURCE_SUBDIR) ---
FetchContent_Declare(
  preloader_android
  GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
  GIT_TAG 0.2.2
)

# --- 2. Dependensi pendukung ---
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_Declare(
  boost_pfr
  GIT_REPOSITORY https://github.com/boostorg/pfr.git
  GIT_TAG boost-1.88.0
)
FetchContent_Declare(
  magic_enum
  GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
  GIT_TAG v0.9.7
)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 11.2.0
)

set(FMT_DOC OFF CACHE BOOL "" FORCE)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(
  preloader_android
  nlohmann_json
  boost_pfr
  magic_enum
  fmt
)

# --- 3. Definisikan target mod kita ---
add_library(graphics_toggle SHARED src/main.cpp)

# Tambahkan include directory untuk boost_pfr secara eksplisit
FetchContent_GetProperties(boost_pfr)
target_include_directories(graphics_toggle PRIVATE "${boost_pfr_SOURCE_DIR}/include")

# --- 4. Link library ---
target_link_libraries(graphics_toggle PRIVATE
  preloader
  fmt::fmt
  nlohmann_json::nlohmann_json
  magic_enum::magic_enum
  boost_pfr
  EGL
  dl
  log
)
