/*
MIT License

Copyright (c) 2024 MSc Games Engineering Team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#define NOMINMAX
#include <Windows.h>
#include <string>

#define WINDOW_GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define WINDOW_GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#pragma warning( disable : 26495)

class Window
{
public:
	HWND hwnd;
	HINSTANCE hinstance;
	int width;
	int height;
	float invZoom;
	std::string name;
	bool keys[256];
	int mousedx;
	int mousedy;
	bool mouseButtons[3];
	int mouseWheel;
	bool useMouseClip;
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		Window* window = NULL;
		if (msg == WM_CREATE)
		{
			window = reinterpret_cast<Window*>(((LPCREATESTRUCT)lParam)->lpCreateParams);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
		} else
		{
			window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		}
		if (window) return window->realWndProc(hwnd, msg, wParam, lParam);
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	LRESULT CALLBACK realWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			exit(0);
			return 0;
		}
		case WM_CLOSE:
		{
			PostQuitMessage(0);
			exit(0);
			return 0;
		}
		case WM_INPUT: // Needs to update to be buffered
		{
			unsigned int inputSize = 0;
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &inputSize, sizeof(RAWINPUTHEADER));
			unsigned char* rawInputData = new unsigned char[inputSize];
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, rawInputData, &inputSize, sizeof(RAWINPUTHEADER));
			RAWINPUT* input = (RAWINPUT*)rawInputData;
			if (input->header.dwType == RIM_TYPEKEYBOARD)
			{
				keys[input->data.keyboard.VKey] = input->data.keyboard.Flags == 0 ? 1 : 0;
			} else if (input->header.dwType == RIM_TYPEMOUSE)
			{
				mousedx = input->data.mouse.lLastX;
				mousedy = input->data.mouse.lLastY;
				if (input->data.mouse.usButtonFlags == RI_MOUSE_BUTTON_1_DOWN)
				{
					mouseButtons[0] = true;
				}
				if (input->data.mouse.usButtonFlags == RI_MOUSE_BUTTON_1_UP)
				{
					mouseButtons[0] = false;
				}
				if (input->data.mouse.usButtonFlags == RI_MOUSE_BUTTON_2_DOWN)
				{
					mouseButtons[2] = true;
				}
				if (input->data.mouse.usButtonFlags == RI_MOUSE_BUTTON_2_UP)
				{
					mouseButtons[2] = false;
				}
				if (input->data.mouse.usButtonFlags == RI_MOUSE_BUTTON_3_DOWN)
				{
					mouseButtons[1] = true;
				}
				if (input->data.mouse.usButtonFlags == RI_MOUSE_BUTTON_3_UP)
				{
					mouseButtons[1] = false;
				}
				if (input->data.mouse.usButtonFlags == RI_MOUSE_WHEEL)
				{
					mouseWheel = mouseWheel + (int)input->data.mouse.usButtonData;
				}
			}

			delete[] rawInputData;
			return 0;
		}
		default:
		{
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
		}
	}
	void pumpLoop()
	{
		MSG msg;
		ZeroMemory(&msg, sizeof(MSG));
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	void create(int window_width, int window_height, const std::string window_name, float zoom = 1.0f, bool window_fullscreen = false, int window_x = 0, int window_y = 0)
	{
		WNDCLASSEX wc;
		hinstance = GetModuleHandle(NULL);
		name = window_name;
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		wc.lpfnWndProc = WndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hinstance;
		wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
		wc.hIconSm = wc.hIcon;
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wc.lpszMenuName = NULL;
		std::wstring wname = std::wstring(name.begin(), name.end());
		wc.lpszClassName = wname.c_str();
		wc.cbSize = sizeof(WNDCLASSEX);
		RegisterClassEx(&wc);
		DWORD style;
		if (0)//window_fullscreen)
		{
			width = GetSystemMetrics(SM_CXSCREEN);
			height = GetSystemMetrics(SM_CYSCREEN);
			DEVMODE fs;
			memset(&fs, 0, sizeof(DEVMODE));
			fs.dmSize = sizeof(DEVMODE);
			fs.dmPelsWidth = (unsigned long)width;
			fs.dmPelsHeight = (unsigned long)height;
			fs.dmBitsPerPel = 32;
			fs.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
			ChangeDisplaySettings(&fs, CDS_FULLSCREEN);
			style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP;
		} else
		{
			width = window_width;
			height = window_height;
			style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
		}
		//SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
		RECT wr = { 0, 0, (long)(width * zoom), (long)(height * zoom) };
		AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
		hwnd = CreateWindowEx(WS_EX_APPWINDOW, wname.c_str(), wname.c_str(), style, window_x, window_y, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hinstance, this);
		invZoom = 1.0f / (float)zoom;
		ShowWindow(hwnd, SW_SHOW);
		SetForegroundWindow(hwnd);
		SetFocus(hwnd);
		RAWINPUTDEVICE rid[2];
		rid[0].usUsagePage = 0x01;
		rid[0].usUsage = 0x06; // Keyboard
		rid[0].dwFlags = RIDEV_NOLEGACY;
		rid[0].hwndTarget = 0;
		rid[1].usUsagePage = 0x01;
		rid[1].usUsage = 0x02; // Mouse
		rid[1].dwFlags = 0;// RIDEV_NOLEGACY;
		rid[1].hwndTarget = 0;
		RegisterRawInputDevices(rid, 2, sizeof(rid[0]));
		useMouseClip = false;
		ShowCursor(true);
	}
	void checkInput()
	{
		mousedx = 0;
		mousedy = 0;
		if (useMouseClip)
		{
			clipMouseToWindow();
		}
		pumpLoop();
	}
	bool keyPressed(int key)
	{
		return keys[key];
	}
	int getMouseInWindowX()
	{
		POINT p;
		GetCursorPos(&p);
		ScreenToClient(hwnd, &p);
		RECT rect;
		GetClientRect(hwnd, &rect);
		p.x = p.x - rect.left;
		p.x = (long)(p.x * invZoom);
		return p.x;
	}
	int getMouseInWindowY()
	{
		POINT p;
		GetCursorPos(&p);
		ScreenToClient(hwnd, &p);
		RECT rect;
		GetClientRect(hwnd, &rect);
		p.y = p.y - rect.top;
		p.y = (long)(p.y * invZoom);
		return p.y;
	}
	void clipMouseToWindow()
	{
		RECT rect;
		GetClientRect(hwnd, &rect);
		POINT ul;
		ul.x = rect.left;
		ul.y = rect.top;
		POINT lr;
		lr.x = rect.right;
		lr.y = rect.bottom;
		MapWindowPoints(hwnd, nullptr, &ul, 1);
		MapWindowPoints(hwnd, nullptr, &lr, 1);
		rect.left = ul.x;
		rect.top = ul.y;
		rect.right = lr.x;
		rect.bottom = lr.y;
		ClipCursor(&rect);
	}
	~Window()
	{
		ShowCursor(true);
		ClipCursor(NULL);
	}
};