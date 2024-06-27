#pragma once

#include <Windows.h>

namespace NppDarkMode
{
	bool IsEnabled();

	COLORREF InvertLightness(COLORREF c);
	COLORREF InvertLightnessSofter(COLORREF c);

	COLORREF GetBackgroundColor();
	COLORREF GetSofterBackgroundColor();
	COLORREF GetTextColor();
	COLORREF GetDarkerTextColor();
	COLORREF GetEdgeColor();

	HBRUSH GetBackgroundBrush();
	HBRUSH GetSofterBackgroundBrush();

	// handle events
	bool OnSettingChange(HWND hwnd, LPARAM lParam); // true if dark mode toggled

	// processes messages related to UAH / custom menubar drawing.
	// return true if handled, false to continue with normal processing in your wndproc
	bool UAHWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr);

	// from DarkMode.h
	void InitDarkMode();
	bool IsDarkMode();
	void SetDarkMode(bool set_dark_mode);
	void AllowDarkModeForApp(bool allow);
	bool AllowDarkModeForWindow(HWND hWnd, bool allow);
	void RefreshTitleBarThemeColor(HWND hWnd);

	// enhancements to DarkMode.h
	void EnableDarkScrollBarForWindowAndChildren(HWND hwnd);
}

