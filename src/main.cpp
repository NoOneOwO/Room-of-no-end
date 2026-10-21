// main.cpp
// Infinity Room / Infinity Cube: first-person scenes built with the method of
// images (no stencil, no recursive framebuffers), rendered in HDR with bloom.
#if defined(_WIN32)
// Must precede any header that may pull in <windows.h> (min/max macros would
// otherwise break std::min/std::max).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
// Scope the third-party header's warnings so the project itself stays clean
// under -Wall -Wextra /W4.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996) // stb uses fopen on MSVC
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <stb_image_write.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "Camera.h"
#include "Constants.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace {

namespace ir = ::ir;

struct Options {
    bool screenshot = false;
    std::filesystem::path screenshotPath;

    bool cube = false;
    bool noBloom = false;
    bool demo = false;

    bool record = false;
    std::filesystem::path recordDirectory;
    double seconds = ir::constants::kRecordDefaultSeconds;

    int depth = -1; // -1 keeps the mode's default
};

struct AppState {
    ir::Camera camera;
    ir::Renderer* renderer = nullptr;
    bool captured = false;
    bool paused = false;
    bool mouseInitialized = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    float animTime = 0.0f;
};

void glfwErrorCallback(int code, const char* description) {
    std::cerr << "GLFW error " << code << ": " << description << '\n';
}

void printUsage(const char* executable) {
    std::cout
        << "Usage: " << executable << " [options]\n"
        << "  --screenshot <out.png>  render " << ir::constants::kScreenshotFrames
        << " frames, save a PNG and exit\n"
        << "  --cube                  start in cube / lattice mode\n"
        << "  --no-bloom              start with bloom disabled\n"
        << "  --demo                  start the scripted deterministic demo camera\n"
        << "  --record <dir>          save frame_%05d.png at a fixed 1/60 s timestep\n"
        << "  --seconds <n>           record length in seconds (default "
        << ir::constants::kRecordDefaultSeconds << ")\n"
        << "  --depth <n>             initial reflection depth (cube mode clamps to 5)\n"
        << "  --help                  print this help\n";
}

bool parseOptions(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--screenshot") {
            if (i + 1 >= argc) {
                std::cerr << "--screenshot requires an output path\n";
                return false;
            }
            options.screenshot = true;
            options.screenshotPath = argv[++i];
        } else if (argument == "--cube") {
            options.cube = true;
        } else if (argument == "--no-bloom") {
            options.noBloom = true;
        } else if (argument == "--demo") {
            options.demo = true;
        } else if (argument == "--record") {
            if (i + 1 >= argc) {
                std::cerr << "--record requires an output directory\n";
                return false;
            }
            options.record = true;
            options.recordDirectory = argv[++i];
        } else if (argument == "--seconds") {
            if (i + 1 >= argc) {
                std::cerr << "--seconds requires a value\n";
                return false;
            }
            try {
                options.seconds = std::stod(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "--seconds requires a numeric value\n";
                return false;
            }
            if (!(options.seconds > 0.0) ||
                options.seconds > ir::constants::kRecordMaxSeconds) {
                std::cerr << "--seconds must be in (0, "
                          << ir::constants::kRecordMaxSeconds << "]\n";
                return false;
            }
        } else if (argument == "--depth") {
            if (i + 1 >= argc) {
                std::cerr << "--depth requires a value\n";
                return false;
            }
            try {
                options.depth = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "--depth requires a numeric value\n";
                return false;
            }
        } else if (argument == "--help" || argument == "-h") {
            printUsage(argv[0]);
            std::exit(EXIT_SUCCESS);
        } else {
            std::cerr << "Unknown argument: " << argument << '\n';
            printUsage(argv[0]);
            return false;
        }
    }

    if (options.screenshot && options.record) {
        std::cerr << "--screenshot and --record are mutually exclusive\n";
        return false;
    }
    if (options.record) {
        // A recording has no interactive input, so drive the deterministic
        // demo camera by default; otherwise every frame would be identical.
        options.demo = true;
    }
    return true;
}

std::filesystem::path executablePath(const char* argv0) {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            break;
        }
        if (length < buffer.size() - 1) {
            return std::filesystem::path(std::wstring(buffer.data(), length));
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        std::error_code error;
        const std::filesystem::path resolved =
            std::filesystem::canonical(buffer.data(), error);
        return error ? std::filesystem::path(buffer.data()) : resolved;
    }
#else
    std::vector<char> buffer(4096, '\0');
    for (;;) {
        const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) {
            break;
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            return std::filesystem::path(std::string(buffer.data(),
                                                     static_cast<std::size_t>(length)));
        }
        buffer.resize(buffer.size() * 2);
    }
#endif
    std::error_code error;
    const std::filesystem::path fromArgument = std::filesystem::absolute(argv0, error);
    return error ? std::filesystem::path(argv0) : fromArgument;
}

std::filesystem::path shaderDirectory(const char* argv0) {
    // Shaders are copied next to the executable by CMake and loaded relative to
    // the executable path, never relative to the working directory.
    return executablePath(argv0).parent_path() / "shaders";
}

void captureCursor(GLFWwindow* window, AppState& state) {
    state.captured = true;
    state.mouseInitialized = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(window, &state.lastMouseX, &state.lastMouseY);
}

void releaseCursor(GLFWwindow* window, AppState& state) {
    state.captured = false;
    state.mouseInitialized = false;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void cursorPositionCallback(GLFWwindow* window, double x, double y) {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) {
        return;
    }
    if (!state->captured || !state->mouseInitialized) {
        // Remember the position without applying a delta, so the first frame
        // after capturing does not jolt the camera.
        state->lastMouseX = x;
        state->lastMouseY = y;
        return;
    }

    const double deltaX = x - state->lastMouseX;
    const double deltaY = y - state->lastMouseY;
    state->lastMouseX = x;
    state->lastMouseY = y;
    state->camera.addMouseDelta(deltaX, deltaY);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) {
        return;
    }
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) {
        return;
    }

    if (!state->captured) {
        // A click while the cursor is released re-captures instead of spawning.
        captureCursor(window, *state);
        return;
    }
    if (state->renderer != nullptr) {
        state->renderer->spawnAt(state->camera);
    }
}

void keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action,
                 int /*mods*/) {
    if (action != GLFW_PRESS) {
        return;
    }
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr || state->renderer == nullptr) {
        return;
    }

    if (key == GLFW_KEY_ESCAPE) {
        if (state->captured) {
            releaseCursor(window, *state);
        } else {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        return;
    }

    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        // The renderer clamps to 9 in tunnel mode and to 5 in cube mode.
        state->renderer->setReflectionDepth(key - GLFW_KEY_1 + 1);
        return;
    }

    switch (key) {
    case GLFW_KEY_M:
        state->renderer->toggleMode();
        break;
    case GLFW_KEY_B:
        state->renderer->toggleBloom();
        break;
    case GLFW_KEY_LEFT_BRACKET:
        state->renderer->adjustBloomIntensity(-ir::constants::kBloomIntensityStep);
        break;
    case GLFW_KEY_RIGHT_BRACKET:
        state->renderer->adjustBloomIntensity(ir::constants::kBloomIntensityStep);
        break;
    case GLFW_KEY_T:
        state->renderer->toggleDemo(state->animTime);
        break;
    case GLFW_KEY_R:
        state->renderer->randomizeColors();
        break;
    case GLFW_KEY_SPACE:
        state->paused = !state->paused;
        break;
    case GLFW_KEY_F:
        state->renderer->toggleReflections();
        break;
    case GLFW_KEY_C:
        state->renderer->clearSpawned();
        break;
    default:
        break;
    }
}

void updateTitle(GLFWwindow* window, const AppState& state, double fps) {
    const bool cube = state.renderer->mode() == ir::SceneMode::Cube;
    std::ostringstream title;
    title << ir::constants::kWindowTitle << " | FPS "
          << static_cast<int>(std::lround(fps)) << " | "
          << (cube ? "CUBE" : "TUNNEL") << " N "
          << state.renderer->reflectionDepth() << " | ";
    if (state.renderer->bloomEnabled()) {
        title << "BLOOM " << std::fixed << std::setprecision(2)
              << state.renderer->bloomIntensity();
    } else {
        title << "BLOOM OFF";
    }
    title << " | Objects " << state.renderer->objectCount()
          << (state.renderer->demoEnabled() ? " | DEMO" : "")
          << (state.paused ? " | PAUSED" : " | RUNNING");
    glfwSetWindowTitle(window, title.str().c_str());
}

bool writePng(const std::filesystem::path& path, int width, int height,
              bool announce) {
    if (width <= 0 || height <= 0) {
        std::cerr << "Cannot save image: empty framebuffer\n";
        return false;
    }

    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) *
                                      static_cast<std::size_t>(height) * 4u);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    stbi_flip_vertically_on_write(1);
    const int written = stbi_write_png(path.string().c_str(), width, height, 4,
                                       pixels.data(), width * 4);
    if (written == 0) {
        std::cerr << "Failed to write PNG: " << path.string() << '\n';
        return false;
    }
    if (announce) {
        std::cout << "Saved image " << path.string() << " (" << width << "x"
                  << height << ")\n";
    }
    return true;
}

std::filesystem::path framePath(const std::filesystem::path& directory, int frame) {
    std::ostringstream name;
    name << "frame_" << std::setw(5) << std::setfill('0') << frame << ".png";
    return directory / name.str();
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseOptions(argc, argv, options)) {
        return EXIT_FAILURE;
    }

    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "Failed to initialise GLFW\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // The scene is multisampled in an offscreen HDR target instead of the
    // window, so the default framebuffer does not need MSAA.
    glfwWindowHint(GLFW_SAMPLES, 0);
#if defined(__APPLE__)
    // Required for a core profile context on macOS.
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window =
        glfwCreateWindow(ir::constants::kWindowWidth, ir::constants::kWindowHeight,
                         ir::constants::kWindowTitle, nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create an OpenGL 3.3 core window\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync for interactive use

    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) {
        std::cerr << "Failed to load OpenGL functions with GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::cout << "OpenGL " << glGetString(GL_VERSION) << " | "
              << glGetString(GL_RENDERER) << '\n';

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth > 0 && framebufferHeight > 0) {
        glViewport(0, 0, framebufferWidth, framebufferHeight);
    }

    const std::filesystem::path shaders = shaderDirectory(argv[0]);
    std::cout << "Loading shaders from: " << shaders.string() << '\n';

    {
        std::unique_ptr<ir::Renderer> renderer;
        try {
            renderer = std::make_unique<ir::Renderer>(shaders);
        } catch (const std::exception& error) {
            std::cerr << "Renderer initialisation failed: " << error.what() << '\n';
            glfwDestroyWindow(window);
            glfwTerminate();
            return EXIT_FAILURE;
        }

        renderer->setMode(options.cube ? ir::SceneMode::Cube : ir::SceneMode::Tunnel);
        if (options.depth >= 0) {
            renderer->setReflectionDepth(options.depth);
        }
        renderer->setBloomEnabled(!options.noBloom);
        renderer->setDemoEnabled(options.demo, 0.0f);
        renderer->resize(framebufferWidth, framebufferHeight);

        std::cout << "Mode: " << (options.cube ? "cube" : "tunnel")
                  << " | bloom " << (options.noBloom ? "off" : "on")
                  << " | scene MSAA " << renderer->sceneSamples() << "x\n";

        AppState state;
        state.renderer = renderer.get();
        glfwSetWindowUserPointer(window, &state);
        glfwSetCursorPosCallback(window, cursorPositionCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetKeyCallback(window, keyCallback);

        if (options.record) {
            std::error_code error;
            std::filesystem::create_directories(options.recordDirectory, error);
            if (error) {
                std::cerr << "Cannot create record directory: "
                          << options.recordDirectory.string() << '\n';
                renderer.reset();
                glfwDestroyWindow(window);
                glfwTerminate();
                return EXIT_FAILURE;
            }

            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            glfwSwapInterval(0); // render as fast as possible, fixed sim step

            const int totalFrames = std::max(
                1, static_cast<int>(std::lround(options.seconds *
                                                ir::constants::kRecordFps)));
            int writtenFrames = 0;

            for (int frame = 0; frame < totalFrames; ++frame) {
                glfwPollEvents();
                if (glfwWindowShouldClose(window) != 0) {
                    break;
                }

                const float time = static_cast<float>(frame) /
                                   static_cast<float>(ir::constants::kRecordFps);
                renderer->updateDemo(time, state.camera);

                glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
                if (framebufferWidth <= 0 || framebufferHeight <= 0) {
                    continue;
                }

                renderer->render(state.camera, time, framebufferWidth,
                                 framebufferHeight);
                if (!writePng(framePath(options.recordDirectory, frame),
                              framebufferWidth, framebufferHeight, false)) {
                    break;
                }
                ++writtenFrames;
                glfwSwapBuffers(window);

                if (frame % 60 == 0) {
                    std::cout << "Recording frame " << frame << " / "
                              << totalFrames << '\r' << std::flush;
                }
            }

            std::cout << "\nRecorded " << writtenFrames << " frames to "
                      << options.recordDirectory.string() << '\n';
            renderer.reset();
            glfwDestroyWindow(window);
            glfwTerminate();
            return writtenFrames > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        if (options.screenshot) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            float animTime = ir::constants::kScreenshotStartAnimTime;
            for (int frame = 0; frame < ir::constants::kScreenshotFrames; ++frame) {
                animTime += static_cast<float>(ir::constants::kScreenshotDeltaTime);
                renderer->updateDemo(animTime, state.camera);

                glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
                if (framebufferWidth <= 0 || framebufferHeight <= 0) {
                    framebufferWidth = ir::constants::kWindowWidth;
                    framebufferHeight = ir::constants::kWindowHeight;
                }

                renderer->render(state.camera, animTime, framebufferWidth,
                                 framebufferHeight);
                if (frame + 1 < ir::constants::kScreenshotFrames) {
                    glfwSwapBuffers(window);
                }
                glfwPollEvents();
            }
            glFinish();
            const bool saved =
                writePng(options.screenshotPath, framebufferWidth, framebufferHeight,
                         true);
            renderer.reset();
            glfwDestroyWindow(window);
            glfwTerminate();
            return saved ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        std::cout << "WASD move | mouse look | LMB spawn | R colors | Space pause | "
                     "1-9 depth | M mode | B bloom | [ ] intensity | T demo | "
                     "F reflections | C clear | Esc release/exit\n";

        double lastTime = glfwGetTime();
        double titleTimer = 0.0;
        int framesSinceTitle = 0;
        double fps = 0.0;
        float animTime = 0.0f;
        updateTitle(window, state, fps);

        while (glfwWindowShouldClose(window) == 0) {
            glfwPollEvents();

            const double now = glfwGetTime();
            const double frameDelta = now - lastTime;
            lastTime = now;
            const float deltaTime =
                static_cast<float>(std::min(frameDelta, ir::constants::kMaxFrameDelta));

            if (!state.paused) {
                animTime += deltaTime; // frozen while paused, no jump on resume
            }
            state.animTime = animTime;

            const bool demo = renderer->demoEnabled();
            if (demo) {
                renderer->updateDemo(animTime, state.camera);
            } else if (state.captured) {
                float forward = 0.0f;
                float strafe = 0.0f;
                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                    forward += 1.0f;
                }
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                    forward -= 1.0f;
                }
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                    strafe += 1.0f;
                }
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                    strafe -= 1.0f;
                }
                state.camera.move(glm::vec3(strafe, 0.0f, forward), deltaTime);
            }
            state.camera.clampToRoom();

            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
            if (framebufferWidth > 0 && framebufferHeight > 0) {
                renderer->render(state.camera, animTime, framebufferWidth,
                                 framebufferHeight);
                glfwSwapBuffers(window);

                ++framesSinceTitle;
                titleTimer += frameDelta;
                if (titleTimer >= ir::constants::kTitleUpdateInterval) {
                    fps = static_cast<double>(framesSinceTitle) / titleTimer;
                    updateTitle(window, state, fps);
                    framesSinceTitle = 0;
                    titleTimer = 0.0;
                }
            }
        }

        renderer.reset(); // GL resources must die while the context is current
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
