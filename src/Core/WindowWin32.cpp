//
// Created by Jan Varga on 13.07.2026.
//

#include "WindowWin32.h"

#include <memory>
#include <sstream>

#include "Window.h"

/*
LRESULT CALLBACK WindowWin32::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* pThis = reinterpret_cast<WindowWin32*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
        if (wParam == 'F') {
            pThis->setWindowTitle("Random title hmmm....");
        }
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
*/

/*
int CALLBACK WindowWin32::WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const auto pClassName = "WindowClass";

    // Register window class
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = nullptr;
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = pClassName;
    wc.hIconSm = nullptr;

    RegisterClassEx(&wc);

    //Create window
    hWnd = CreateWindowEx(
    0,
    pClassName,
    "RenderEngine",
    WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU,
    800,600,
    800,600,
    nullptr,
    nullptr,
    hInstance,
    nullptr
    );

    ShowWindow(hWnd,SW_SHOW);

    //WindowWin32(800,600,"Dog fart");

    MSG msg;
    BOOL gResult;
    while ((gResult = GetMessage(&msg, nullptr, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (gResult == -1) {
        return -1;
    }else {
        return msg.wParam;
    }
}*/

WindowWin32::WindowWin32(int width, int height, std::string title)
{
    // Calculate window size based on client region size
    RECT wr{};
    wr.left = 100;
    wr.right = width + wr.left;
    wr.top = 100;
    wr.bottom = height + wr.top;
    if (FAILED(AdjustWindowRect(&wr, WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU, FALSE)))
    {
        throw WND_LAST_EXCEPT();
    }

    // Creates window, returns handle
    hWnd = CreateWindow(
        WindowClass::getName(), title.c_str(),
        WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,wr.right - wr.left,wr.bottom - wr.top,
        nullptr,nullptr,WindowClass::getInstance(),this);

    if (hWnd == nullptr) {
        throw WND_LAST_EXCEPT();
    }

    // Show window
    ShowWindow(hWnd,SW_SHOWDEFAULT);
}

WindowWin32::~WindowWin32()
{
    DestroyWindow(hWnd);
}

LRESULT WindowWin32::HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE) {
        // Extract pointer to window class from creation data
        const CREATESTRUCTW* const pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        WindowWin32* const pWnd = static_cast<WindowWin32*>(pCreate->lpCreateParams);
        // set WinAPI-managed user data to store ptr to window class
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
        // Set message proc to normal handler now that setup is finished
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowWin32::HandleMsgThunk));

        return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT WINAPI WindowWin32::HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    //retrieve ptr to window class
    WindowWin32* const pWnd = reinterpret_cast<WindowWin32*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    //forward message to window class handler
    return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
}

LRESULT WindowWin32::HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

std::unique_ptr<WindowInterface> WindowWin32::createWindow(int width, int height, std::string title)
{
    return std::make_unique<WindowWin32>(width,height,title);
}

void WindowWin32::pollEvents()
{
    MSG msg;
    if (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void WindowWin32::setWindowTitle(std::string title)
{
    SetWindowText(hWnd, title.c_str());
}




/*
* Window class
*/

WindowWin32::WindowClass::WindowClass() noexcept : hInst(GetModuleHandle(nullptr))
{
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = HandleMsgSetup;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = getInstance();
    wc.hIcon = nullptr;
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = getName();
    wc.hIconSm = nullptr;
    RegisterClassEx(&wc);
}

WindowWin32::WindowClass::~WindowClass()
{
    UnregisterClass("WindowClass", getInstance());
}

const char * WindowWin32::WindowClass::getName() noexcept
{
    return windowClassName;
}

HINSTANCE WindowWin32::WindowClass::getInstance() noexcept
{
    return wndClass.hInst;
}

/*
* Window exception handling
*/

WindowWin32::WindowException::WindowException(int line, const char *file, HRESULT hr) noexcept:
Exception(line, file),
hr(hr)
{}

const char * WindowWin32::WindowException::what() const noexcept
{
    std::ostringstream oss;
    oss << getType() << std::endl
        << "[Error Code] " << getErrorCode() << std::endl
        << "[Description] " << getErrorMessage() << std::endl
        << getOriginString();
    whatBuffer = oss.str();
    return whatBuffer.c_str();
}

const char * WindowWin32::WindowException::getType() const noexcept
{
    return "WindowWin32::WindowException";
}

std::string WindowWin32::WindowException::translateErrorCode(HRESULT hr)
{
    char* pMsgBuf = nullptr;
    DWORD nMsgLen = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&pMsgBuf), 0, nullptr);

    if (nMsgLen == 0) {
        return "Unidentified error code";
    }
    std::string errorString = pMsgBuf;
    LocalFree(pMsgBuf);
    return errorString;

}

HRESULT WindowWin32::WindowException::getErrorCode() const noexcept
{
    return hr;
}

std::string WindowWin32::WindowException::getErrorMessage() const noexcept
{
    return  translateErrorCode(hr);
}


