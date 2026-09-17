#include "pch.h"
#include "MainWindow.h"

namespace winrt
{
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::DirectX::Direct3D11;
    using namespace Windows::Graphics::Capture;
}

namespace util
{
    using namespace robmikh::common::desktop;
    using namespace robmikh::common::desktop::controls;
}

const std::wstring MainWindow::ClassName = L"WGCWindowExclusionDemo.MainWindow";
std::once_flag MainWindowClassRegistration;

void MainWindow::RegisterWindowClass()
{
    auto instance = winrt::check_pointer(GetModuleHandleW(nullptr));
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = instance;
    wcex.hIcon = LoadIconW(instance, IDI_APPLICATION);
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = ClassName.c_str();
    wcex.hIconSm = LoadIconW(wcex.hInstance, IDI_APPLICATION);
    winrt::check_bool(RegisterClassExW(&wcex));
}

MainWindow::MainWindow(std::wstring const& titleString, int width, int height, winrt::Compositor const& compositor, winrt::IDirect3DDevice const& device)
{
    auto instance = winrt::check_pointer(GetModuleHandleW(nullptr));

    std::call_once(MainWindowClassRegistration, []() { RegisterWindowClass(); });

    auto exStyle = 0;
    auto style = WS_OVERLAPPEDWINDOW;

    winrt::check_bool(CreateWindowExW(exStyle, ClassName.c_str(), titleString.c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, instance, this));
    WINRT_ASSERT(m_window);

    auto dpi = GetDpiForWindow(m_window);

    RECT rect = { 0, 0, width, height };
    winrt::check_bool(AdjustWindowRectExForDpi(&rect, style, false, exStyle, dpi));
    auto adjustedWidth = rect.right - rect.left;
    auto adjustedHeight = rect.bottom - rect.top;
    winrt::check_bool(SetWindowPos(m_window, nullptr, 0, 0, adjustedWidth, adjustedHeight, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER));

    m_borderWindow = std::make_unique<BorderWindow>(compositor);
    m_borderWindow->BorderThickness(5);

    m_font = util::GetFontForDpi(dpi);
    CreateControls(instance);

    // Hookup the capture -- The exclusion API only works with display capture,
    // so we'll just capture the primary monitor.
    auto monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    auto item = util::CreateCaptureItemForMonitor(monitor);
    m_capture = std::make_unique<SimpleCapture>(device, item, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized);
    m_capture->IsBorderRequired(false);
    m_capture->StartCapture();

    uint32_t marginY = MulDiv(10, dpi, 96);
    uint32_t controlHeight = MulDiv(30, dpi, 96);
    float visualMargin = static_cast<float>((marginY * 2) + controlHeight);

    // Setup our visual tree
    m_root = compositor.CreateContainerVisual();
    m_root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    m_root.Size({ 0.0f, -visualMargin });
    m_root.Offset({ 0.0f, visualMargin, 0.0f });
    m_target = CreateWindowTarget(compositor);
    m_target.Root(m_root);
    m_content = compositor.CreateSpriteVisual();
    m_content.Size({ -50.0f, -50.0f });
    m_content.Offset({ 25.0f, 25.0f, 0.0f });
    m_content.RelativeSizeAdjustment({ 1.0f, 1.0f });
    auto brush = compositor.CreateSurfaceBrush();
    brush.HorizontalAlignmentRatio(0.5f);
    brush.VerticalAlignmentRatio(0.5f);
    brush.Stretch(winrt::CompositionStretch::Uniform);
    brush.Surface(m_capture->CreateSurface(compositor));
    m_content.Brush(brush);
    auto shadow = compositor.CreateDropShadow();
    shadow.Mask(brush);
    m_content.Shadow(shadow);
    m_root.Children().InsertAtTop(m_content);

    // Load cursors
    m_standardCursor.reset(winrt::check_pointer(LoadCursorW(nullptr, IDC_ARROW)));
    m_crosshairCursor.reset(winrt::check_pointer(LoadCursorW(nullptr, IDC_CROSS)));
    m_cursorType = CursorType::Standard;

    ShowWindow(m_window, SW_SHOW);
    UpdateWindow(m_window);
}

LRESULT MainWindow::MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam)
{
    switch (message)
    {
    case WM_SETCURSOR:
        if (OnSetCursor())
        {
            return 1;
        }
        else
        {
            return base_type::MessageHandler(message, wparam, lparam);
        }
    case WM_DPICHANGED:
        OnDpiChanged();
        return base_type::MessageHandler(message, wparam, lparam);
    case WM_CTLCOLORSTATIC:
        return util::StaticControlColorMessageHandler(wparam, lparam);
    default:
        return base_type::MessageHandler(message, wparam, lparam);
    }
}

void MainWindow::CreateControls(HINSTANCE instance)
{
    auto dpi = GetDpiForWindow(m_window);

    uint32_t marginX = MulDiv(10, dpi, 96);
    uint32_t marginY = MulDiv(10, dpi, 96);
    uint32_t stepAmount = MulDiv(40, dpi, 96);
    uint32_t width = MulDiv(350, dpi, 96);
    uint32_t height = MulDiv(30, dpi, 96);

    m_controls = std::make_unique<util::StackPanel>(m_window, instance, m_font, marginX, marginY, stepAmount, width, height);

    m_windowSelectionButton = m_controls->CreateControl(util::ControlType::Button, L"Drag to exclude/include a window");
    SetWindowLongPtrW(m_windowSelectionButton, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_windowSelectionButtonWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(m_windowSelectionButton, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SubClassWndProc)));
}

LRESULT MainWindow::SubClassWndProc(HWND window, UINT const message, WPARAM const wparam, LPARAM const lparam)
{
    auto parentWindow = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (parentWindow != nullptr)
    {
        if (window == parentWindow->m_windowSelectionButton)
        {
            return parentWindow->WindowSelectionButtonMessageHandler(message, wparam, lparam);
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT MainWindow::WindowSelectionButtonMessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
        SetCapture(m_windowSelectionButton);
        m_cursorType = CursorType::Crosshair;
        m_pendingCursorChange = true;
        PostMessageW(m_window, WM_SETCURSOR, 0, 0);
        m_cursorCaptured = true;
        m_borderWindow->Show();
        break;
    case WM_LBUTTONUP:
        if (m_cursorCaptured)
        {
            m_cursorType = CursorType::Standard;
            m_pendingCursorChange = true;
            PostMessageW(m_window, WM_SETCURSOR, 0, 0);
            winrt::check_bool(ReleaseCapture());
            m_cursorCaptured = false;
            m_borderWindow->Hide();
            m_currentPickerCandidateWindow = nullptr;
            if (m_currentPickerWindow != nullptr)
            {
                auto selectedWindow = m_currentPickerWindow;
                m_currentPickerWindow = nullptr;
                OnWindowSelected(selectedWindow);
            }
        }
        break;
    case WM_MOUSEMOVE:
    {
        if (m_cursorCaptured)
        {
            auto xPos = GET_X_LPARAM(lparam);
            auto yPos = GET_Y_LPARAM(lparam);

            POINT point = { xPos, yPos };
            winrt::check_bool(ClientToScreen(m_windowSelectionButton, &point));
            auto handle = WindowFromPoint(point);
            handle = GetAncestor(handle, GA_ROOT);
            if (m_currentPickerCandidateWindow != handle)
            {
                m_currentPickerCandidateWindow = handle;
                if (handle != nullptr && handle != GetShellWindow() && handle != GetDesktopWindow())
                {
                    auto exStyle = GetWindowLongPtrW(handle, GWL_EXSTYLE);
                    if ((exStyle & WS_EX_TOOLWINDOW) == 0) // No tooltips
                    {
                        m_borderWindow->PositionOver(handle);
                        m_currentPickerWindow = handle;
                    }
                }
            }
        }
    }
    break;
    default:
        return CallWindowProcW(m_windowSelectionButtonWndProc, m_windowSelectionButton, message, wparam, lparam);
    }
    return 0;
}

void MainWindow::OnWindowSelected(HWND window)
{
    auto windowId = winrt::WindowId{ static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window)) };
    auto search = std::find(m_excludedWindows.begin(), m_excludedWindows.end(), windowId);
    if (search == m_excludedWindows.end()) 
    {
        m_excludedWindows.push_back(windowId);
    }
    else
    {
        m_excludedWindows.erase(search);
    }
    m_capture->UpdateWindowExclusionList(m_excludedWindows);
}

void MainWindow::OnDpiChanged()
{
    auto dpi = GetDpiForWindow(m_window);
    m_font = util::GetFontForDpi(dpi);
    m_controls->OnDpiChanged(m_font);

    uint32_t marginY = MulDiv(10, dpi, 96);
    uint32_t controlHeight = MulDiv(30, dpi, 96);
    float visualMargin = static_cast<float>((marginY * 2) + controlHeight);
    m_root.Size({ 0.0f, -visualMargin });
    m_root.Offset({ 0.0f, visualMargin, 0.0f });
}

bool MainWindow::OnSetCursor()
{
    auto wasPending = m_pendingCursorChange;
    m_pendingCursorChange = false;
    switch (m_cursorType)
    {
    case CursorType::Standard:
        if (wasPending)
        {
            SetCursor(m_standardCursor.get());
        }
        return false;
    case CursorType::Crosshair:
        SetCursor(m_crosshairCursor.get());
        return true;
    default:
        return false;
    }
}