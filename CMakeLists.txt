cmake_minimum_required(VERSION 3.16)

project(InfinityRoom LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Single-config generators put the executable in build/bin; multi-config
# generators (Visual Studio) add their configuration subdirectory, e.g.
# build/bin/Release. $<TARGET_FILE_DIR> keeps the shader copy next to whichever
# binary is produced.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------- dependencies
# GLFW and GLM are fetched with FetchContent. The standard FetchContent override
# variables (FETCHCONTENT_SOURCE_DIR_GLFW / FETCHCONTENT_SOURCE_DIR_GLM) can
# point at an existing checkout or extracted source tree to build offline.
include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# X11 (through XWayland on Wayland desktops) avoids requiring wayland dev packages.
set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw glm)

# GLAD resolves all GL entry points through glfwGetProcAddress at runtime, so
# an OpenGL import library is not strictly required. Link it when the
# platform's SDK provides it (Windows, macOS, Linux with GL dev packages).
find_package(OpenGL QUIET)

# GLAD generated for gl:core=3.3 (glad 0.1.36), committed under third_party.
add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include)
if(MSVC)
    target_compile_options(glad PRIVATE /w)
else()
    target_compile_options(glad PRIVATE -w)
endif()

# stb_image_write (header only), used by --screenshot.
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)

# --------------------------------------------------------------------- target
add_executable(InfinityRoom
    src/main.cpp
    src/Camera.cpp
    src/Renderer.cpp
    src/Room.cpp
    src/Mirror.cpp
    src/Object.cpp
    src/Lattice.cpp
    src/PostProcess.cpp
)

target_include_directories(InfinityRoom PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(InfinityRoom PRIVATE GLFW_INCLUDE_NONE)
if(APPLE)
    # gl.h is deprecated on macOS but still the correct OpenGL 3.3 path.
    target_compile_definitions(InfinityRoom PRIVATE GL_SILENCE_DEPRECATION)
endif()

target_link_libraries(InfinityRoom PRIVATE
    glfw
    glad
    glm::glm
    stb
)
if(TARGET OpenGL::GL)
    target_link_libraries(InfinityRoom PRIVATE OpenGL::GL)
endif()

if(MSVC)
    target_compile_options(InfinityRoom PRIVATE /W4 /permissive-)
else()
    target_compile_options(InfinityRoom PRIVATE -Wall -Wextra)
endif()

# -------------------------------------------------------------- shader deploy
# Shaders are loaded next to the executable, so copy them there for every build
# configuration (including multi-config generators such as Visual Studio).
add_custom_command(TARGET InfinityRoom POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:InfinityRoom>/shaders
    COMMENT "Copying shaders next to the executable"
)
