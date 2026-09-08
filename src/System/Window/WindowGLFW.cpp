#include "WindowGLFW.h"

#include <iostream>

#include "Core/Engine.h"
#include "Core/Log.h"
#include "System/Events/EventDispatcher.h"
#include "System/Events/KeyEvents.h"
#include "System/Events/MouseEvents.h"
#include "System/Events/WindowEvents.h"
#include "Core/Types.h"


namespace Engine {
    WindowGLFW::WindowGLFW(const WindowProps &props): properties(props)
    {
        glfwInit();

        if (!glfwVulkanSupported()) {
            LOG_FATAL("WindowGLFW", "Vulkan support not available");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, properties.resizable ? GLFW_TRUE : GLFW_FALSE);

        GLFWmonitor* monitor = properties.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
        hwnd = glfwCreateWindow(
            static_cast<int>(properties.width),
            static_cast<int>(properties.height),
            properties.title.c_str(),
            monitor,
            nullptr
        );

        ENGINE_ASSERT(hwnd != nullptr, "Failed to create GLFW window!");
        glfwSetWindowUserPointer(hwnd, this);

        SetCallbacks();

        ApplyDpiScaling();
        EnableDarkMode();
    }

    WindowGLFW::~WindowGLFW()
    {
        if (hwnd) {
            glfwDestroyWindow(hwnd);
            hwnd = nullptr;
        }
        glfwTerminate();
    }

    void WindowGLFW::PollEvents()
    {
        glfwPollEvents();


    }

    void WindowGLFW::SetCallbacks()
    {
        glfwSetFramebufferSizeCallback(hwnd,
                                      [](GLFWwindow *w, int width, int height) {
                                          auto *self = static_cast<WindowGLFW *>(glfwGetWindowUserPointer(w));
                                          self->properties.width = width;
                                          self->properties.height = height;
                                          WindowResizeEvent event(width, height);
                                          EventDispatcher::Get().PostEvent(event);
                                      });

        glfwSetWindowCloseCallback(hwnd,
                                   [](GLFWwindow *w) {
                                       auto *self = static_cast<WindowGLFW *>(glfwGetWindowUserPointer(w));
                                       self->shouldClose = true;
                                       WindowCloseEvent event;
                                       EventDispatcher::Get().PostEvent(event);
                                   });

        glfwSetKeyCallback(hwnd, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
            if (key < 0) {
                return;
            }
            auto keyCode = static_cast<KeyCode>(key);

            if (keyCode == KeyCode::Unknown) {
                return;
            }

            switch (action) {
                case GLFW_PRESS: {
                    KeyPressedEvent event(keyCode, false);
                    EventDispatcher::Get().PostEvent(event);
                    break;
                }
                case GLFW_REPEAT: {
                    KeyPressedEvent event(keyCode, true);
                    EventDispatcher::Get().PostEvent(event);
                    break;
                }
                case GLFW_RELEASE: {
                    KeyReleasedEvent event(keyCode);
                    EventDispatcher::Get().PostEvent(event);
                    break;
                }
                default:
                    break;
            }
        });

        glfwSetCharCallback(hwnd, [](GLFWwindow* w, unsigned codepoint) {
            auto keyCode = static_cast<uint32_t>(codepoint);

            KeyTypedEvent event(keyCode);
            EventDispatcher::Get().PostEvent(event);
        });

        glfwSetCursorPosCallback(hwnd, [](GLFWwindow* w, double x, double y) {
            MouseMovedEvent event(x, y);
            EventDispatcher::Get().PostEvent(event);
        });

        glfwSetMouseButtonCallback(hwnd, [](GLFWwindow* w, int button, int action, int mods) {
            auto keyCode = static_cast<MouseButton>(button);

            switch (action) {
                case GLFW_PRESS: {
                    MouseButtonPressedEvent event(keyCode);
                    EventDispatcher::Get().PostEvent(event);
                    break;
                }
                case GLFW_RELEASE: {
                    MouseButtonReleasedEvent event(keyCode);
                    EventDispatcher::Get().PostEvent(event);
                    break;
                }
                default:
                    break;
            }
        });

        glfwSetScrollCallback(hwnd, [](GLFWwindow* w, double x, double y) {
            MouseScrolledEvent event(x,y);
            EventDispatcher::Get().PostEvent(event);
        });

        glfwSetDropCallback(hwnd, [](GLFWwindow* w, int count, const char** paths) {
            if (count > 0 && paths != nullptr) {
                WindowDropFilesEvent event(std::vector<std::string>(paths, paths + count));
                EventDispatcher::Get().PostEvent(event);
            }
        });

        glfwSetWindowContentScaleCallback(hwnd, [](GLFWwindow* w, float xScale, float yScale) {
            auto *self = static_cast<WindowGLFW *>(glfwGetWindowUserPointer(w));
            self->dpiScale = Vec2(xScale, yScale);
        });

        glfwSetWindowFocusCallback(hwnd,[](GLFWwindow *w, int focused) {
            WindowFocusEvent event(focused);
            EventDispatcher::Get().PostEvent(event);
        });

        glfwSetWindowIconifyCallback(hwnd, [](GLFWwindow* w, int iconified) {
            WindowIconifyEvent event(iconified);
            EventDispatcher::Get().PostEvent(event);
        });

        glfwSetCursorEnterCallback(hwnd, [](GLFWwindow* w, int entered) {
            MouseEnterEvent event(entered);
            EventDispatcher::Get().PostEvent(event);
        });
    }

    void WindowGLFW::SetTitle(std::string_view title)
    {
        glfwSetWindowTitle(hwnd, title.data());
    }

    void WindowGLFW::SetCursorMode(CursorMode mode)
    {
        cursorMode = mode;
        int glfwMode = GLFW_CURSOR_NORMAL;
        switch (mode) {
        case CursorMode::Normal:   glfwMode = GLFW_CURSOR_NORMAL;   break;
        case CursorMode::Hidden:   glfwMode = GLFW_CURSOR_HIDDEN;   break;
        case CursorMode::Disabled: glfwMode = GLFW_CURSOR_DISABLED; break;
        }
        glfwSetInputMode(hwnd, GLFW_CURSOR, glfwMode);
    }

    bool WindowGLFW::CreateVulkanSurface(VkInstance instance, VkSurfaceKHR*outSurface)
    {
        if (glfwCreateWindowSurface(instance, hwnd,nullptr, outSurface) != VK_SUCCESS) {
            LOG_FATAL("WindowGLFW", "Failed to create window surface");
            Engine::Get().RequestExit();
        }

        return true;
    }

    void WindowGLFW::ApplyDpiScaling()
    {
        float xscale = 1.0f;
        float yscale = 1.0f;
        glfwGetWindowContentScale(hwnd, &xscale, &yscale);
        dpiScale = Vec2(xscale, yscale);
    }

    void WindowGLFW::EnableDarkMode()
    {
#if defined(_WIN32)
    HWND hwnds = glfwGetWin32Window(hwnd);
    if (hwnds) {
        BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(hwnds, 20, &useDarkMode, sizeof(useDarkMode));
    }
#endif
    }
}
