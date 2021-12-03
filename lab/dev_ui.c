#define _CRT_SECURE_NO_WARNINGS 1
#pragma warning(disable:5105)

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <conio.h>


#include <windows.h>

#include "../libc/internal/include/libc_internal.h"
#include "../kernel/include/hex_dump.h"
#include "../kernel/include/video.h"
#include "../kernel/include/output_console.h"
#include "../kernel/include/collections.h"

#include "../kernel/include/linear_allocator.h"
#include "../kernel/include/arena_allocator.h"


#include "programs/scroller.h"

// =================================================================================================

video_mode_info_t _info = { .vertical_resolution = 768, .pixel_format = kVideo_Pixel_Format_RBGx };
static size_t _window_width = 1024;

HDC hdc_mem;
HBITMAP bm;
BITMAPINFOHEADER bm_info_header;
BYTE* bits = 0;

static LRESULT CALLBACK labWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	switch (uMsg) {
	case WM_SHOWWINDOW:
	{
		free(bits);
		HDC source = GetDC(hWnd);
		hdc_mem = CreateCompatibleDC(source);
		bm = CreateCompatibleBitmap(source, (int)_window_width, (int)_info.vertical_resolution);
		SelectObject(hdc_mem, bm);
		ZeroMemory(&bm_info_header, sizeof bm_info_header);
		bm_info_header.biSize = sizeof bm_info_header;
		bm_info_header.biWidth = (LONG)(_info.horisontal_resolution = _window_width);
		bm_info_header.biHeight = -(LONG)(_info.vertical_resolution);
		bm_info_header.biPlanes = 1;
		bm_info_header.biBitCount = 32;
		_info.horisontal_resolution = _window_width;
		_info.pixels_per_scan_line = ((_window_width * bm_info_header.biBitCount + 31) / 32);
		DWORD bm_size = (DWORD)(4 * _info.pixels_per_scan_line * _info.vertical_resolution);
		bits = (BYTE*)malloc(bm_size);
		GetDIBits(hdc_mem, bm, 0, (UINT)_info.vertical_resolution, bits, (BITMAPINFO*)(&bm_info_header), DIB_RGB_COLORS);		
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		if (bits) {
			video_present();
			SetDIBits(hdc_mem, bm, 0, (UINT)_info.vertical_resolution, bits, (BITMAPINFO*)(&bm_info_header), DIB_RGB_COLORS);
			BitBlt(hdc, 0, 0, (int)_window_width, (int)_info.vertical_resolution, hdc_mem, 0, 0, SRCCOPY);
		}
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_KEYDOWN:
	{
		switch (wParam)
		{
		case VK_ESCAPE:
		{
			output_console_clear_screen();
			InvalidateRect(hWnd, 0, TRUE);
		}
		break;
		default:;
		}
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	default: return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return 0;
}

static CHAR _class_name[] = TEXT("josx64_lab");
static HWND ui_hwnd = 0;
static void initialise_window(void) {

	WNDCLASS wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = labWndProc;
	wc.hInstance = GetModuleHandle(0);
	wc.lpszClassName = _class_name;
	ATOM wnd = RegisterClass(&wc);
	if (wnd) {
		RECT cw;
		cw.left = 0;
		cw.right = (LONG)_window_width;
		cw.top = 0;
		cw.bottom = (LONG)_info.vertical_resolution;
		AdjustWindowRect(&cw, WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME, FALSE);

		ui_hwnd = CreateWindowEx(
			0,
			_class_name,
			TEXT("josx64_lab"),
			WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
			CW_USEDEFAULT, CW_USEDEFAULT, cw.right - cw.left, cw.bottom - cw.top,
			NULL,
			NULL,
			wc.hInstance,
			NULL
		);

		ShowWindow(ui_hwnd, SW_SHOW);
	}
}

uint32_t* framebuffer_base(void) {
	return (uint32_t*)bits;
}

// ===============================================================================
// some ideas here:
// http://www.osdever.net/tutorials/view/gui-development
// 

typedef uint16_t window_coord_t;

typedef struct _window {

	window_coord_t		_top;
	window_coord_t		_left;
	window_coord_t		_width;
	window_coord_t		_height;

	void* _bm;
	size_t _bm_size;

	struct _window* _parent;
	struct _window* _children;
	struct _window* _next;

} window_t;

typedef window_t* window_handle_t;

static generic_allocator_t* _window_allocator = 0;
static generic_allocator_t* _bitmap_allocator = 0;

jo_status_t window_create(window_handle_t* out_handle, 
	window_handle_t parent, 
	window_coord_t top, window_coord_t left, window_coord_t width, window_coord_t height) {

	if (!out_handle || !width || !height) {
		return _JO_STATUS_INVALID_INPUT;
	}
	// ensure we allocate with room for 8 byte alignment of the bitmap pointer as well
	static const size_t aligned_winstruct_size = _JOS_ALIGN(sizeof(window_t), 8);	
	*out_handle = (window_t*)_window_allocator->alloc(_window_allocator, aligned_winstruct_size);
	if (!out_handle[0]) {
		return _JO_STATUS_RESOURCE_EXHAUSTED;
	}	

	// the window's bitmap is allocated from a different heap
	out_handle[0]->_bm_size = ((size_t)width * height * VIDEO_PIXEL_STRIDE);
	out_handle[0]->_bm = _bitmap_allocator->alloc(_bitmap_allocator, out_handle[0]->_bm_size);
	if (!out_handle[0]->_bm) {
		return _JO_STATUS_RESOURCE_EXHAUSTED;
	}

	out_handle[0]->_top = top;
	out_handle[0]->_left = left;
	out_handle[0]->_width = width;
	out_handle[0]->_height = height;

	out_handle[0]->_parent = parent;
	out_handle[0]->_children = 0;	
	out_handle[0]->_next = 0;
	if (parent) {
		window_handle_t leftmost_child = 0;
		window_handle_t child = parent->_children;
		while (child) {
			leftmost_child = child;
			child = child->_next;
		}
		if (leftmost_child) {
			// add this window to the list of children for the parent
			leftmost_child->_next = out_handle[0];
		}
		else {
			// first child
			parent->_children = out_handle[0];
		}
	}
	return _JO_STATUS_SUCCESS;
}

void window_clear(window_handle_t wnd) {
	if (!wnd)
		return;
	memset(wnd->_bm, 0, wnd->_bm_size);
}

void test_ui_loop(generic_allocator_t* allocator, const uint8_t* font8x8) {
	
	initialise_window();

	video_initialise(&(static_allocation_policy_t) { .allocator = allocator });
	output_console_initialise();
	output_console_set_colour(0xffffffff);
	output_console_set_bg_colour(0x6495ed);
	output_console_set_font(font8x8, 8, 8);
	
	srand(1001107);
	scroller_initialise(&(rect_t) {
		.top = 50,
			.left = 8,
			.right = _info.horisontal_resolution - 8,
			.bottom = _info.vertical_resolution - 8,
	});

	region_handle_t handle;
	jo_status_t status = output_console_create_region(&(rect_t) {
		.right = _info.horisontal_resolution,
			.top = 100,
			.bottom = _info.vertical_resolution - 100
	},
		& handle);
	(void)status;
	
	output_console_activate_region(handle);
	//ZZZ: need to set this whenever a region is activated, that's not great (inherit from previous?)
	output_console_set_colour(0xffffffff);
	output_console_set_bg_colour(0x6495ed);
	output_console_set_font(font8x8, 8, 8);

	video_clear_screen(0x6495ed);
	output_console_output_string_w(L"Press CR...\n");
	
	MSG msg = { 0 };
	uint64_t t0 = GetTickCount64();
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, TRUE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		uint64_t t1 = GetTickCount64();
		if (t1 - t0 > 66) {
			scroller_render_field();
			InvalidateRect(ui_hwnd, 0, FALSE);
			t0 = t1;
		}
	}
}