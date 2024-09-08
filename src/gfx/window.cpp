#include "window.hpp"
#include "util/misc.hpp"
#include <GLFW/glfw3.h>
#include <util/log.hpp>

static constexpr int DefaultWindowWidth  = 1280;
static constexpr int DefaultWindowHeight = 720;

namespace gfx
{
    Window::Window()
        : window {nullptr}
    {
        static auto glfwErrorCallback = [](int ec, const char* description)
        {
            util::logFatal(
                "glfw error raised | Error: {} | Description: {}",
                ec,
                description);
        };

        glfwSetErrorCallback(glfwErrorCallback);

        if (glfwInit() != GLFW_TRUE)
        {
            const char* outErrorDescription = nullptr;
            const int   glfwErrorCode = glfwGetError(&outErrorDescription);

            outErrorDescription =
                outErrorDescription == nullptr ? "" : outErrorDescription;

            util::logFatal(
                "Failed to initialize glfw | Error: {} | Message: {}",
                glfwErrorCode,
                outErrorDescription);
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        this->window = glfwCreateWindow(
            DefaultWindowWidth,
            DefaultWindowHeight,
            "lavender",
            nullptr,
            nullptr);

        util::assertFatal(
            this->window != nullptr, "Glfw initialization failed");

        if constexpr (util::isDebugBuild())
        {
            // Move the window so that debug logs can actually be seen
            glfwSetWindowPos(this->window, 100, 100);
        }

        glfwSetWindowUserPointer(this->window, this);
    }

    Window::~Window() noexcept
    {
        glfwDestroyWindow(this->window);
        glfwTerminate();
    }

    void Window::beginFrame()
    {
        if (glfwGetKey(this->window, GLFW_KEY_ESCAPE) != GLFW_RELEASE)
        {
            glfwSetWindowShouldClose(this->window, GLFW_TRUE);
        }

        glfwPollEvents();
    }

    bool Window::shouldWindowClose()
    {
        return glfwWindowShouldClose(this->window) != 0;
    }
} // namespace gfx
