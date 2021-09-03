#include <windows.h>
#include <stdio.h>
#include "chip8.h"
#include "chip8.c"

b32 running;
b32 pause;

typedef struct
{
	BITMAPINFO bitmap_info;
	void* memory;
	i32 width;
	i32 height;
	i32 bytes_per_pixel;
	i32 pitch;
	i32 size;
} Win32ScreenBuffer;

Win32ScreenBuffer screen_buffer;

static void Win32ResizeDisplayBuffer(Win32ScreenBuffer* sb, i32 width, i32 height)
{
	if (sb->memory) {
		VirtualFree(sb->memory, 0, MEM_RELEASE);
	}

	sb->width = width;
	sb->height = height;
	sb->bytes_per_pixel = 4;
	sb->pitch = sb->width * sb->bytes_per_pixel;
	sb->size = sb->width * sb->height * sb->bytes_per_pixel;

	sb->bitmap_info.bmiHeader.biSize = sizeof(sb->bitmap_info.bmiHeader);
	sb->bitmap_info.bmiHeader.biWidth = sb->width;
	sb->bitmap_info.bmiHeader.biHeight = -sb->height;
	sb->bitmap_info.bmiHeader.biPlanes = 1;
	sb->bitmap_info.bmiHeader.biBitCount = 32;
	sb->bitmap_info.bmiHeader.biCompression = BI_RGB;

	sb->memory = VirtualAlloc(0, sb->size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void Win32ShowDisplayBufferInWindow(HDC device_context, Win32ScreenBuffer* sb, i32 window_width, i32 window_height)
{
	StretchDIBits(device_context,
				  0, 0, window_width, window_height,
				  0, 0, sb->width, sb->height,
				  sb->memory,
				  &sb->bitmap_info,
				  DIB_RGB_COLORS,
				  SRCCOPY);
}

static void Win32DrawScreen()
{
	for (u8 y = 0; y < DISPLAY_Y; ++y) {
		for (u8 x = 0; x < DISPLAY_X; ++x) {
			u32 color = display_buffer[x + DISPLAY_X * y] ?
				0xFFFFFFFF : 0x00000000;

			u32* pixel = (u32*)((u8*)screen_buffer.memory +
						x * 4 +
						screen_buffer.pitch * y);
			*pixel = color;
		}
	}
}

static u8 Win32Keymap(u32 key)
{
	switch (key) {
		case '1': return 0x0;
		case '2': return 0x1;
		case '3': return 0x2;
		case '4': return 0x3;
		case 'Q': return 0x4;
		case 'W': return 0x5;
		case 'E': return 0x6;
		case 'R': return 0x7;
		case 'A': return 0x8;
		case 'S': return 0x9;
		case 'D': return 0xA;
		case 'F': return 0xB;
		case 'Z': return 0xC;
		case 'X': return 0xD;
		case 'C': return 0xE;
		case 'V': return 0xF;

		default: return 0x10;
	}
}

static void Win32ProcessKey(u32 key, b32 is_down)
{
	u8 mapped_key = Win32Keymap(key);
	keys[mapped_key] = is_down ? 0x1 : 0x0;
}

static LRESULT CALLBACK Win32WindowProc(HWND window_handle,
										UINT message,
										WPARAM w_param,
										LPARAM l_param)
{
	LRESULT result = 0;

	switch (message) {
		case WM_CREATE: {
			SetWindowPos(window_handle, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
		} break;

		case WM_PAINT: {
			PAINTSTRUCT paint_struct;
			HDC device_context = BeginPaint(window_handle, &paint_struct);

			RECT window_size;
			GetClientRect(window_handle, &window_size);

			i32 window_width = window_size.right;
			i32 window_height = window_size.bottom;

			Win32ShowDisplayBufferInWindow(device_context, &screen_buffer,
										   window_width, window_height);

			EndPaint(window_handle, &paint_struct);
		} break;

		case WM_SYSCHAR: {
			// NOTE: This WM_SYSCHAR message is handled to prevent
			// *beep* sound that Windows produces when ALT key is pressed.
		} break;

		case WM_DESTROY:
		case WM_CLOSE:
		case WM_QUIT: {
			running = 0;
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP: {
			u32 key_code = (u32)w_param;
			b32 was_down = (l_param & (1 << 30));
			b32 is_down = !(l_param & (1 << 31));
			b32 alt_is_down = (l_param & (1 << 29));

			switch (key_code) {
				case VK_F4: {
					if (is_down && alt_is_down) {
						running = 0;
					}
				} break;
				case VK_ESCAPE: {
					if (is_down) {
						running = 0;
					}
				} break;
				case '1':
				case '2':
				case '3':
				case '4':
				case 'Q':
				case 'W':
				case 'E':
				case 'R':
				case 'A':
				case 'S':
				case 'D':
				case 'F':
				case 'Z':
				case 'X':
				case 'C':
				case 'V': {
					Win32ProcessKey(key_code, is_down);
				} break;

				case 'P': {
					if (is_down) {
						pause = !pause;
					}
				} break;

			}

		} break;

		case WM_NCHITTEST: {
			WORD x = (WORD)l_param;
			WORD y = (WORD)(l_param >> 16);

			LONG bx = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
			LONG by = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

			RECT rect;
			GetWindowRect(window_handle, &rect);

			if (x < rect.left + bx   && y < rect.top + by)     return HTTOPLEFT;
			if (x >= rect.right - bx && y < rect.top + by)     return HTTOPRIGHT;
			if (x < rect.left + bx   && y >= rect.bottom - by) return HTBOTTOMLEFT;
			if (x >= rect.right - bx && y >= rect.bottom - by) return HTBOTTOMRIGHT;

			if (x < rect.left + bx)    return HTLEFT;
			if (x >= rect.right - bx)  return HTRIGHT;
			if (y < rect.top + by)     return HTTOP;
			if (y >= rect.bottom - by) return HTBOTTOM;

			return HTCAPTION;
		} break;

		case WM_NCCALCSIZE: {
			if ((BOOL)w_param) {
				if (IsZoomed(window_handle)) {
					LONG bx = GetSystemMetrics(SM_CXSIZEFRAME);
					LONG by = GetSystemMetrics(SM_CYSIZEFRAME);

					RECT* rect = ((NCCALCSIZE_PARAMS*)l_param)->rgrc;
					rect->top += by;
					rect->left += bx;
					rect->right -= bx;
					rect->bottom -= by;
				}
			}
			break;
		}

		default: {
			result = DefWindowProcA(window_handle, message, w_param, l_param);
		} break;
	};

	return result;
 }

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance,
				   LPSTR lp_cmd_line, int n_cmd_show)
{
#if CHIP8_DEBUG
	AllocConsole();

	FILE* console_file;
	freopen_s(&console_file, "CONOUT$", "w", stdout);
#endif
	
	LPWSTR* args;
	i32 argc;
	args = CommandLineToArgvW(GetCommandLineW(), &argc);

	if (argc != 2) {
		return 0;
	}
	
	i32 len = WideCharToMultiByte(CP_UTF8, 0, args[1], -1, 0, 0, 0, 0);
	
	char file_buffer[256];
	WideCharToMultiByte(CP_UTF8, 0, args[1], -1, file_buffer, len, 0, 0);
	
	Win32ResizeDisplayBuffer(&screen_buffer, 64, 32);

	WNDCLASSA window_class = {0};

	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = Win32WindowProc;
	window_class.hInstance = instance;
	window_class.lpszClassName = "Chip8";

	if (!RegisterClassA(&window_class)) {
		// TODO: Logging and assert?
		return 0;
	}

	HWND window_handle =
		CreateWindowExA(WS_EX_APPWINDOW,
						window_class.lpszClassName,
						"Chip8",
						WS_OVERLAPPEDWINDOW | WS_VISIBLE,
						CW_USEDEFAULT,
						CW_USEDEFAULT,
						CW_USEDEFAULT,
						CW_USEDEFAULT,
						0,
						0,
						instance,
						0);

	if (!window_handle) {
		// TODO: Logging and assert?
		return 0;
	}

	char* file_name = (char*)file_buffer;
	
	Chip8Initialize();
	Chip8LoadProgram(file_name);

	LARGE_INTEGER counter_frequency;
	LARGE_INTEGER begin_counts;
	LARGE_INTEGER end_counts;

	QueryPerformanceFrequency(&counter_frequency);

	f32 hz = 600.0f;
	f32 timer_hz = 60.0f;
	f32 target_seconds_per_frame = 1.0f / hz;
	f32 timer_period = hz / timer_hz;
	i32 timer_count = 0;
	b32 is_sleep_granular = timeBeginPeriod(1) == TIMERR_NOERROR;
	running = 1;
	while (running) {
		QueryPerformanceCounter(&begin_counts);

		MSG message;
		while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {

			TranslateMessage(&message);
			DispatchMessageA(&message);
		}

		// NOTE: Chip8 things.
		{
			if (!pause) {
				Chip8FetchExecuteCycle();

				if (is_drawing) {
					Win32DrawScreen();
					is_drawing = 0;
				}

				++timer_count;
				if (timer_count >= timer_period) {
					timer_count = 0;
					if (delay_timer > 0) {
						--delay_timer;
					}

					if (sound_timer > 0) {
						--sound_timer;
					}
				}
			}
		}

		// NOTE: Display screen_buffer on the screen.
		{
			HDC device_context = GetDC(window_handle);
			RECT window_size;
			GetClientRect(window_handle, &window_size);

			i32 window_width = window_size.right;
			i32 window_height = window_size.bottom;
			Win32ShowDisplayBufferInWindow(device_context, &screen_buffer,
										   window_width, window_height);
			ReleaseDC(window_handle, device_context);
		}

		// NOTE: Frame timing.
		{
			QueryPerformanceCounter(&end_counts);

			i32 elapsed_counts = (i32)end_counts.QuadPart - (i32)begin_counts.QuadPart;
			i32 target_counts = (i32)(target_seconds_per_frame * (i32)counter_frequency.QuadPart);
			i32 counts_to_wait = target_counts - elapsed_counts;

			LARGE_INTEGER end_wait_counts;
			LARGE_INTEGER start_wait_counts;
			QueryPerformanceCounter(&start_wait_counts);

			while (counts_to_wait > 0) {
				if (is_sleep_granular) {
					DWORD sleep_in_milliseconds = (DWORD)(1000.0f * ((f32)counts_to_wait / (f32)counter_frequency.QuadPart));
					if (sleep_in_milliseconds > 0) {
						Sleep(sleep_in_milliseconds);
					}
				}

				QueryPerformanceCounter(&end_wait_counts);
				counts_to_wait -= ((i32)end_wait_counts.QuadPart - (i32)start_wait_counts.QuadPart);
				start_wait_counts = end_wait_counts;
			}
		}
	}

#if CHIP8_DEBUG
	fclose(console_file);
#endif
	
	LocalFree(args);
	return 0;
}
