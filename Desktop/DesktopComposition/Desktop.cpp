#include "pch.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Composition::Desktop;
using namespace Windows::UI::Xaml::Hosting;
using namespace Windows::Foundation::Numerics;

template <typename T>
struct DesktopWindow
{
	static T* GetThisFromHandle(HWND const window) noexcept
	{
		return reinterpret_cast<T *>(GetWindowLongPtr(window, GWLP_USERDATA));
	}

	static LRESULT __stdcall WndProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
	{
		WINRT_ASSERT(window);

		if (WM_NCCREATE == message)
		{
			auto cs = reinterpret_cast<CREATESTRUCT *>(lparam);
			T* that = static_cast<T*>(cs->lpCreateParams);
			WINRT_ASSERT(that);
			WINRT_ASSERT(!that->m_window);
			that->m_window = window;
			SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));

			EnableNonClientDpiScaling(window);
			m_currentDpi = GetDpiForWindow(window);
		}
		else if (T* that = GetThisFromHandle(window))
		{
			return that->MessageHandler(message, wparam, lparam);
		}

		return DefWindowProc(window, message, wparam, lparam);
	}

	LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
	{
		switch (message) {
			case WM_DPICHANGED:
			{
				return HandleDpiChange(m_window, wparam, lparam);
			}

			case WM_DESTROY:
			{
				PostQuitMessage(0);
				return 0;
			}

			case WM_SIZE:
			{
				UINT width = LOWORD(lparam);
				UINT height = HIWORD(lparam);

				if (T* that = GetThisFromHandle(m_window))
				{
					that->DoResize(width, height);
				}
			}
		}

		return DefWindowProc(m_window, message, wparam, lparam);
	}

	// DPI Change handler. on WM_DPICHANGE resize the window
	LRESULT HandleDpiChange(HWND hWnd, WPARAM wParam, LPARAM lParam)
	{
		HWND hWndStatic = GetWindow(hWnd, GW_CHILD);
		if (hWndStatic != nullptr)
		{
			UINT uDpi = HIWORD(wParam);

			// Resize the window
			auto lprcNewScale = reinterpret_cast<RECT*>(lParam);

			SetWindowPos(hWnd, nullptr, lprcNewScale->left, lprcNewScale->top,
				lprcNewScale->right - lprcNewScale->left, lprcNewScale->bottom - lprcNewScale->top,
				SWP_NOZORDER | SWP_NOACTIVATE);

			if (T* that = GetThisFromHandle(hWnd))
			{
				that->NewScale(uDpi);
			}
		}
		return 0;
	}

	void NewScale(UINT dpi) {

	}

	void DoResize(UINT width, UINT height) {

	}

protected:

	using base_type = DesktopWindow<T>;
	HWND m_window = nullptr;
	inline static UINT m_currentDpi = 0;
};

struct Window : DesktopWindow<Window>
{
	Window() noexcept
	{
		WNDCLASS wc{};
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hInstance = reinterpret_cast<HINSTANCE>(&__ImageBase);
		wc.lpszClassName = L"XAML island in Win32";
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WndProc;
		RegisterClass(&wc);
		WINRT_ASSERT(!m_window);

		WINRT_VERIFY(CreateWindow(wc.lpszClassName,
			L"XAML island in Win32",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			nullptr, nullptr, wc.hInstance, this));

		WINRT_ASSERT(m_window);

		InitXaml(m_window, m_rootGrid, m_scale);
		NewScale(m_currentDpi);

		
	}

	void InitXaml(
		HWND wind,
		Windows::UI::Xaml::Controls::Grid & root,
		Windows::UI::Xaml::Media::ScaleTransform & dpiScale) {

		Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();

		static DesktopWindowXamlSource source;
		auto interop = source.as<IDesktopWindowXamlSourceNative>();
		check_hresult(interop->AttachToWindow(wind));

		HWND h = nullptr;
		interop->get_WindowHandle(&h);
		m_interopWindowHandle = h;

		//TODO: get correct DPI here
		Windows::UI::Xaml::Media::ScaleTransform st;
		dpiScale = st;
		
		dpiScale.ScaleX(3.0);
		dpiScale.ScaleY(3.0);

		Windows::UI::Xaml::Controls::Grid g;
		g.RenderTransform(st);

		Windows::UI::Xaml::Media::SolidColorBrush background{ Windows::UI::Colors::White() };
		

		root = g;
		source.Content(g);
		OnSize(h, root, m_currentWidth, m_currentHeight);
	}

	void OnSize(HWND interopHandle,
		winrt::Windows::UI::Xaml::Controls::Grid& rootGrid,
		UINT width,
		UINT height) {
		SetWindowPos(interopHandle, 0, 0, 0, width, height, SWP_SHOWWINDOW);
		rootGrid.Width(width);
		rootGrid.Height(height);
	}

	LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
	{
		// TODO: handle messages here...
		return base_type::MessageHandler(message, wparam, lparam);
	}

	void NewScale(UINT dpi) {

		auto scaleFactor = (float)dpi / 100;

		if (m_scale != nullptr) {
			m_scale.ScaleX(scaleFactor);
			m_scale.ScaleY(scaleFactor);
		}
	}

	void DoResize(UINT width, UINT height) {
		m_currentWidth = width;
		m_currentHeight = height;
		if (nullptr != m_rootGrid) {
			OnSize(m_interopWindowHandle, m_rootGrid, m_currentWidth, m_currentHeight);
		}
	}

	void SetRootContent(Windows::UI::Xaml::UIElement content) {
		m_rootGrid.Children().Clear();
		m_rootGrid.Children().Append(content);
	}

private:
	UINT m_currentWidth = 600;
	UINT m_currentHeight = 600;
	HWND m_interopWindowHandle = nullptr;
	DesktopWindowTarget m_target{ nullptr };
	Windows::UI::Xaml::Media::ScaleTransform m_scale{ nullptr };
	Windows::UI::Xaml::Controls::Grid m_rootGrid{ nullptr };
};

Windows::UI::Xaml::UIElement CreateDefaultContent() {
	/*Windows::UI::Xaml::Media::AcrylicBrush acrylicBrush;
	acrylicBrush.BackgroundSource(Windows::UI::Xaml::Media::AcrylicBackgroundSource::HostBackdrop);
	acrylicBrush.TintOpacity(0.5);
	acrylicBrush.TintColor(Windows::UI::Colors::Red());*/

	//g.Background(acrylicBrush);

	//TODO: extract this into a createcontent function
	//TODO: hook up resizing

	Windows::UI::Xaml::Controls::Grid container;

	Windows::UI::Xaml::Controls::Button b;
	b.Width(200);
	b.Height(30);
	b.SetValue(Windows::UI::Xaml::FrameworkElement::VerticalAlignmentProperty(),
		box_value(Windows::UI::Xaml::VerticalAlignment::Top));

	Windows::UI::Xaml::Controls::TextBlock tb;
	tb.Text(L"Hello Win32 love XAML xx");
	b.Content(tb);
	container.Children().Append(b);

	Windows::UI::Xaml::Controls::Button b2;
	b2.Width(200);
	b2.Height(30);
	b2.SetValue(Windows::UI::Xaml::FrameworkElement::VerticalAlignmentProperty(),
		box_value(Windows::UI::Xaml::VerticalAlignment::Bottom));

	b2.SetValue(Windows::UI::Xaml::FrameworkElement::HorizontalAlignmentProperty(),
		box_value(Windows::UI::Xaml::HorizontalAlignment::Right));

	Windows::UI::Xaml::Controls::TextBlock tb2;
	tb2.Text(L"Hello Win32 love XAML xx");
	b2.Content(tb2);
	container.Children().Append(b2);

	return container;
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	init_apartment(apartment_type::single_threaded);

	Window window;

	auto defaultContent = CreateDefaultContent();
	window.SetRootContent(defaultContent);
	
	MSG message;

	while (GetMessage(&message, nullptr, 0, 0))
	{
		DispatchMessage(&message);
	}
}
