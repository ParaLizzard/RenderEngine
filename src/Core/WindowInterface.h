#pragma once
#include <memory>
#include <string>


struct WindowHandle;
class WindowInterface
{
public:
    virtual ~WindowInterface() = default;

    //virtual std::unique_ptr<WindowInterface> createWindow(int width, int height, std::string title) = 0;

    virtual void setWindowUserPointer(void*/*window handle*/, void* pointer);
    virtual void setWindowTitle(std::string title);

    virtual void pollEvents() = 0;

    virtual void setResizable(bool bResizable) = 0;

    virtual void destroyWindow(WindowHandle) = 0;
    virtual void terminate() = 0;
};

struct WindowHandle
{
    void* hWnd;
    WindowInterface* window;
};