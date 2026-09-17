#include "pch.h"
#include "MainWindow.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Numerics;
    using namespace Windows::Foundation::Metadata;
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
    using namespace Windows::Graphics::Capture;
}

namespace util
{
    using namespace robmikh::common::desktop;
    using namespace robmikh::common::uwp;
}

int __stdcall WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
    // We use non-virtualized APIs, so let's keep things consistent
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize COM
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // Check to see if we're on a supported build
    auto supported =
        winrt::GraphicsCaptureSession::IsSupported() &&
        winrt::ApiInformation::IsMethodPresent(
            winrt::name_of<winrt::IDisplayGraphicsCaptureSession>(), L"SetWindowExclusionList");
    if (!supported)
    {
        MessageBoxW(nullptr,
            L"This build of Windows doesn't support the required features for this demo!",
            L"WGCWindowExclusionDemo",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    // Create the DispatcherQueue that the compositor needs to run
    auto controller = util::CreateDispatcherQueueControllerForCurrentThread();

    // Init D3D11
    auto d3dDevice = util::CreateD3D11Device();
    auto device = CreateDirect3DDevice(d3dDevice.as<IDXGIDevice>().get());

    // Create our window
    auto compositor = winrt::Compositor();
    auto window = MainWindow(L"WGCWindowExclusionDemo", 800, 600, compositor, device);

    // Message pump
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return util::ShutdownDispatcherQueueControllerAndWait(controller, static_cast<int>(msg.wParam));
}
