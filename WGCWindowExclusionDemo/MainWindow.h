#pragma once
#include <robmikh.common/DesktopWindow.h>
#include "BorderWindow.h"
#include "SimpleCapture.h"

struct MainWindow : robmikh::common::desktop::DesktopWindow<MainWindow>
{
	static const std::wstring ClassName;
	MainWindow(
		std::wstring const& titleString, 
		int width, 
		int height, 
		winrt::Windows::UI::Composition::Compositor const& compositor,
		winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device);
	LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam);

private:
	static void RegisterWindowClass();
	static LRESULT SubClassWndProc(HWND window, UINT const message, WPARAM const wparam, LPARAM const lparam);
    void CreateControls(HINSTANCE instance);
	LRESULT WindowSelectionButtonMessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam);
	void OnWindowSelected(HWND window);

private:
	std::unique_ptr<SimpleCapture> m_capture;
	winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget m_target{ nullptr };
	winrt::Windows::UI::Composition::ContainerVisual m_root{ nullptr };
	winrt::Windows::UI::Composition::SpriteVisual m_content{ nullptr };
    HWND m_windowSelectionButton = nullptr;
	WNDPROC m_windowSelectionButtonWndProc = nullptr;
	std::vector<winrt::Windows::UI::WindowId> m_excludedWindows;
	std::unique_ptr<BorderWindow> m_borderWindow;
	HWND m_currentPickerWindow = nullptr;
	HWND m_currentPickerCandidateWindow = nullptr;
	bool m_cursorCaptured = false;
};
