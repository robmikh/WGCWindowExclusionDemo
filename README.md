# WGCWindowExclusionDemo
A demo of Windows.Graphics.Capture's window exclusion API.

## Calling the API
The window exclusion API is exposed via the [`IDisplayGraphicsCapture`](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.idisplaygraphicscapturesession?view=winrt-26100) interface. Only `GraphicsCaptureSession`s created via a `GraphicsCaptureItem` that represents a display will implement this interface. If you're using C++/WinRT, you can QI for the interface like this:

```cpp
namespace winrt
{
    using namespace Windows::UI;
    using namespace Windows::Graphics::Capture;
}

void SetWindowExclusionList(
    winrt::GraphicsCaptureSession const& session, 
    std::vector<winrt::WindowId> const& windowsToExclude)
{
    if (auto displaySession = session.try_as<winrt::IDisplayGraphicsCaptureSession>())
    {
        displaySession.SetWindowExclusionList(windowsToExclude);
    }
}
```

## Handling ConfigurationIterations
The Windows.Graphics.Capture API is fundementally async, as you interact with the API the DWM is likely rendering frames. In order to know when your exclusion list is now being used when DWM renders, you can check the [`ConfigurationIteration`](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframe.configurationiteration?view=winrt-28000#windows-graphics-capture-direct3d11captureframe-configurationiteration) property on the `Direct3D11CaptureFrame`. 

First, when you call `SetWindowExclusionList`, it will return a configuration iteration value:

```cpp
std::atomic<uint64_t> m_lastConfigurationIteration = 0;

m_lastConfigurationIteration.store(displaySession.SetWindowExclusionList(windowsToExclude), std::memory_order_release);
```

Then, when you receive a frame from the `Direct3D11CaptureFramePool`, you can check the `ConfigurationIteration` property:

```cpp
namespace winrt
{
    using namespace Windows::Graphics::Capture;
}

void Sample::OnFrameArrived(winrt::Direct3D11CaptureFramePool const& sender, winrt::IInspectable const&)
{
    winrt::Direct3D11CaptureFrame frame = sender.TryGetNextFrame();
    if (frame.ConfigurationIteration() < m_lastConfigurationIteration.load(std::memory_order_acquire))
    {
        // Skip the frame
        return;
    }
    // Handle the frame like you normally would
}
```