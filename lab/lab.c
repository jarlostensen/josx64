
// =========================================================================================

#define _CRT_SECURE_NO_WARNINGS 1

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <windows.h>

#include "../libc/internal/include/libc_internal.h"
#include "../kernel/include/hex_dump.h"
#include "../kernel/include/pe.h"
#include "../kernel/include/video.h"
#include "../kernel/include/output_console.h"
#include "../kernel/include/collections.h"
#include "../libc/include/extensions/slices.h"
#include "../libc/include/extensions/pdb_index.h"
#include "../kernel/font8x8/font8x8.h"

void slice_printf(char_array_slice_t* slice) {
    for (unsigned n = 0; n < slice->_length; ++n) {
        printf("%c", slice->_ptr[n]);
    }
}


void dump_index(pdb_index_node_t* root, int level) {

    // this is a beauty!
    printf("%*s", level << 1, "");

    if (!slice_is_empty(&root->_prefix)) {
        slice_printf(&root->_prefix);
    }
    else {
        printf("root");
    }
    if (!symbol_is_empty(&root->_symbol)) {
        printf("\trva = 0x%x, section = %d", root->_symbol._rva, root->_symbol._section);
    }
    if (!vector_is_empty(&root->_children)) {
        printf("\n");
        const unsigned child_count = vector_size(&root->_children);
        for (unsigned c = 0; c < child_count; ++c) {
            pdb_index_node_t* child = (pdb_index_node_t*)vector_at(&root->_children, c);
            dump_index(child, level + 1);
        }
    }
    else {
        printf("\n");
    }
}

void test_search(const char* str) {

    pdb_index_node_t root, * leaf;
    memset(&root, 0, sizeof(root));

    char_array_slice_t body;
    char_array_slice_create(&body, str, 0, 0);
    char_array_slice_t prefix = pdb_index_next_token(&body);
    pdb_index_match_result m = pdb_index_match_search(&root, prefix, body, &leaf);

}


// ======================================================================================

extern int _JOS_LIBC_FUNC_NAME(swprintf)(wchar_t* __restrict buffer, size_t sizeOfBuffer, const wchar_t* __restrict format, ...);
extern int _JOS_LIBC_FUNC_NAME(vswprintf)(wchar_t* __restrict buffer, size_t bufsz, const wchar_t* __restrict format, va_list vlist);
extern int _JOS_LIBC_FUNC_NAME(snprintf)(char* __restrict buffer, size_t sizeOfBuffer, const char* __restrict format, ...);
extern int _JOS_LIBC_FUNC_NAME(vsnprintf)(char* __restrict buffer, size_t bufsz, const char* __restrict format, va_list vlist);

void test_a(char* buffer, const char* format, ...)
{
    va_list parameters;
    va_start(parameters, format);
    _JOS_LIBC_FUNC_NAME(vsnprintf)(buffer, 512, format, parameters);
    //vsnprintf(buffer, 512, format, parameters);
    va_end(parameters);
}

void test_w(wchar_t* buffer, const wchar_t* format, ...)
{
    va_list parameters;
    va_start(parameters, format);
    _JOS_LIBC_FUNC_NAME(vswprintf)(buffer, 512, format, parameters);
    //vswprintf(buffer, 512, format, parameters);
    va_end(parameters);
}

void trace(const char* __restrict channel, const char* __restrict format, ...) {

    if (!format || !format[0])
        return;

    static unsigned long long _ticks = 0;

    //ZZZ:
    char buffer[256];
    va_list parameters;
    va_start(parameters, format);
    int written;
    if (channel)
        written = snprintf(buffer, sizeof(buffer), "[%lld:%s] ", _ticks++, channel);
    else
        written = snprintf(buffer, sizeof(buffer), "[%lld] ", _ticks++);
    written += vsnprintf(buffer + written, sizeof(buffer) - written, format, parameters);
    va_end(parameters);

    buffer[written + 0] = '\r';
    buffer[written + 1] = '\n';
    buffer[written + 2] = 0;

    printf(buffer);
}

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
        bm = CreateCompatibleBitmap(source, _window_width, _info.vertical_resolution);
        SelectObject(hdc_mem, bm);
        ZeroMemory(&bm_info_header, sizeof bm_info_header);
        bm_info_header.biSize = sizeof bm_info_header;
        bm_info_header.biWidth = _window_width;
        bm_info_header.biHeight = -(LONG)(_info.vertical_resolution);
        bm_info_header.biPlanes = 1;
        bm_info_header.biBitCount = 32;
        _info.pixels_per_scan_line = ((_window_width * bm_info_header.biBitCount + 31) / 32);
        DWORD bm_size = 4*_info.pixels_per_scan_line * _info.vertical_resolution;
        bits = (BYTE*)malloc(bm_size);
        GetDIBits(hdc_mem, bm, 0, _info.vertical_resolution, bits, (BITMAPINFO*)(&bm_info_header), DIB_RGB_COLORS);        
        video_clear_screen(0x6495ed);
        output_console_output_string(L"Press CR...\n");
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        if (bits) {            
            SetDIBits(hdc_mem, bm, 0, _info.vertical_resolution, bits, (BITMAPINFO*)(&bm_info_header), DIB_RGB_COLORS);
            BitBlt(hdc, 0, 0, _window_width, _info.vertical_resolution, hdc_mem, 0, 0, SRCCOPY);
        }
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_KEYDOWN:
    {
        static size_t returns = 1;
        if (wParam == VK_RETURN) {
            wchar_t buffer[128];
            swprintf_s(buffer,sizeof(buffer)/sizeof(wchar_t), L"line %d...\n", returns++);
            output_console_output_string(buffer);
            InvalidateRect(hWnd, 0, TRUE);
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
        cw.right = _window_width;
        cw.top = 0;
        cw.bottom = _info.vertical_resolution;
        AdjustWindowRect(&cw, WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME, FALSE);

        HWND hwnd = CreateWindowEx(
            0,
            _class_name,
            TEXT("josx64_lab"),
            WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
            CW_USEDEFAULT, CW_USEDEFAULT, cw.right-cw.left, cw.bottom-cw.top,
            NULL,
            NULL,
            wc.hInstance,
            NULL
        );

        ShowWindow(hwnd, SW_SHOW);
    }
}

uint32_t* framebuffer_wptr(size_t top, size_t left) {
    return (uint32_t*)(bits + top*(_info.pixels_per_scan_line<<2) + (left<<2));
}

static void ui_test_loop(void) {

    output_console_initialise();
    output_console_set_colour(0xffffffff);
    output_console_set_bg_colour(0x6495ed);
    output_console_set_font((const uint8_t*)font8x8_basic, 8,8);

    initialise_window();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

int main(void)
{
    /*char buffer[512];
    wchar_t wbuffer[512];

    trace("lab", "this is a message from the lab");
    trace("lab", "and this is also a message from the lab");

    test_a(buffer, "%s %S", "this is a test", L"and this is wide");
    test_w(wbuffer, L"%s %S", L"this is w test ", "and this is narrow");

    _JOS_LIBC_FUNC_NAME(snprintf)(buffer, sizeof(buffer), "\tid %d, status 0x%x, package %d, core %d, thread %d, TSC is %S\n",
        1,
        42,
        80,
        1,
        0,
        L"enabled"
        );

    _JOS_LIBC_FUNC_NAME(swprintf)(wbuffer, sizeof(wbuffer) / sizeof(wchar_t), L"\tid %d, status 0x%x, package %d, core %d, thread %d, TSC is %S\n",
        1,
        42,
        80,
        1,
        0,
        "enabled"
        );

    HMODULE this_module = GetModuleHandle(0);
    hex_dump_mem((void*)this_module, 64, k8bitInt);
    peutil_pe_context_t pe_ctx;
    peutil_bind(&pe_ctx, (const void*)this_module, kPe_Relocated);
    uintptr_t entry = peutil_entry_point(&pe_ctx);

    dump_index(pdb_index_load_from_pdb_yml(), 0);

    char_array_slice_t slice = pdb_index_symbol_name_for_address(71904);
    slice = pdb_index_symbol_name_for_address(137950);
*/

    ui_test_loop();

    return 0;
}