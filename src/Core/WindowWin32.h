#pragma once
#include "Exception.h"
#include "WindowInterface.h"
#include "windows.h"

class WindowWin32 : public WindowInterface
{
public:
    class WindowException : public Engine::Exception
    {
    public:
        WindowException(int line, const char* file, HRESULT hr) noexcept;
        const char* what() const noexcept override;
        const char* getType() const noexcept override;
        static std::string translateErrorCode(HRESULT hr);
        HRESULT getErrorCode() const noexcept;
        std::string getErrorMessage() const noexcept;
    private:
        HRESULT hr;
    };

private:
    class WindowClass
    {
    public:
        static const char* getName() noexcept;
        static HINSTANCE getInstance() noexcept;
    private:
        WindowClass() noexcept;
        ~WindowClass();
        WindowClass(const WindowClass&) = delete;
        WindowClass& operator=(const WindowClass&) = delete;
        static constexpr const char* windowClassName = "WindowClass";
        static WindowClass wndClass;
        HINSTANCE hInst;
    };
public:
    WindowWin32(int width, int height, std::string title);
    ~WindowWin32() override;
    WindowWin32(const WindowWin32&) = delete;
    WindowWin32& operator=(const WindowWin32&) = delete;

    static std::unique_ptr<WindowInterface> createWindow(int width, int height, std::string title);

private:
    static LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

    void pollEvents() override;
    void setWindowTitle(std::string title) override;
    void setResizable(bool bResizable) override;
    void destroyWindow(WindowHandle handle) override;
    void terminate() override;


    static LRESULT HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT WINAPI HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
private:
    int width, height;
    HWND hWnd;
};

#define WND_EXCEPT(hr) WindowWin32::WindowException(__FILE__,__LINE__,hr)
#define WND_LAST_EXCEPT() WindowWin32::WindowException(__FILE__,__LINE__,GetLastError())

