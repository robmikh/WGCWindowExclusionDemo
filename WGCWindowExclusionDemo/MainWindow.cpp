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

    RECT rect = { 0, 0, width, height};
    winrt::check_bool(AdjustWindowRectEx(&rect, style, false, exStyle));
    auto adjustedWidth = rect.right - rect.left;
    auto adjustedHeight = rect.bottom - rect.top;

    winrt::check_bool(CreateWindowExW(exStyle, ClassName.c_str(), titleString.c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT, adjustedWidth, adjustedHeight, nullptr, nullptr, instance, this));
    WINRT_ASSERT(m_window);

    m_borderWindow = std::make_unique<BorderWindow>(compositor);
    m_borderWindow->BorderThickness(5);

    CreateControls(instance);

    // Hookup the capture -- The exclusion API only works with display capture,
    // so we'll just capture the primary monitor.
    auto monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    auto item = util::CreateCaptureItemForMonitor(monitor);
    m_capture = std::make_unique<SimpleCapture>(device, item, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized);
    m_capture->IsBorderRequired(false);
    m_capture->StartCapture();

    // Setup our visual tree
    m_root = compositor.CreateContainerVisual();
    m_root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    m_root.Size({ 0.0f, -60.0f });
    m_root.Offset({ 0.0f, 60.0f, 0.0f });
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

    ShowWindow(m_window, SW_SHOW);
    UpdateWindow(m_window);
}

LRESULT MainWindow::MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam)
{
    switch (message)
    {
    case WM_CTLCOLORSTATIC:
        return util::StaticControlColorMessageHandler(wparam, lparam);
    default:
        return base_type::MessageHandler(message, wparam, lparam);
    }

    return 0;
}

void MainWindow::CreateControls(HINSTANCE instance)
{
    auto controls = util::StackPanel(m_window, instance, 10, 10, 40, 350, 30);

    m_windowSelectionButton = controls.CreateControl(util::ControlType::Button, L"Drag to exclude/include a window");
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
        m_cursorCaptured = true;
        m_borderWindow->Show();
        break;
    case WM_LBUTTONUP:
        if (m_cursorCaptured)
        {
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
    auto windowId = winrt::WindowId{ static_cast<uint64_t>(reinterpret_cast<uint32_t>(window)) };
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