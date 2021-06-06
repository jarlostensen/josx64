
// =========================================================================================

#define _CRT_SECURE_NO_WARNINGS 1

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
#include "../kernel/include/pe.h"
#include "../kernel/include/video.h"
#include "../kernel/include/output_console.h"
#include "../kernel/programs/scroller.h"
#include "../kernel/include/collections.h"
#include "../libc/include/extensions/slices.h"
#include "../libc/include/extensions/pdb_index.h"
#include "../deps/font8x8/font8x8.h"

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

typedef struct _IO_FILE {

    // a string, for example
    struct {
        uint8_t* _begin;
        const uint8_t* _end;
        const uint8_t* _rp;
        uint8_t* _wp;
        size_t              _size;
    } _buffer;

    size_t(*read)(struct _IO_FILE* file, unsigned char*, size_t);
    size_t(*write)(struct _IO_FILE* file, const unsigned char*, size_t);


} IO_FILE;

#define _jo_fromstring(f, s)\
((f)->_buffer._begin = (f)->_buffer._rp = (const uint8_t*)(s), (f)->_buffer._end = (const uint8_t*)-1, (f)->_buffer._size = 0)

#define _jo_getch(f) ((f)->_buffer._rp!=(f)->_buffer._end ? *(f)->_buffer._rp++ : 0)
#define _jo_ungetch(f) ((f)->_buffer._rp!=(f)->_buffer._begin ? (f)->_buffer._rp-- ; (void)0)

#define _jo_tobuffer(f, buffer, length)\
((f)->_buffer._rp = (void)0, (f)->_buffer._begin = (f)->_buffer._wp = (uint8_t*)(buffer), (f)->_buffer._end = ((f)->_buffer._wp + ((f)->_buffer._size = length)))

int _jos_getc(IO_FILE* stream) {

    if (stream->_buffer._rp == stream->_buffer._end) {
        if (stream->_buffer._size) {
            /*CHECK FOR ERROR*/ stream->read(stream, stream->_buffer._begin, 1);
            stream->_buffer._end = stream->_buffer._begin + 1;
            stream->_buffer._rp = stream->_buffer._begin;
        }
        else {
            // a fixed size buffer, we're underflowing
            return 0;
        }
    }
    return (int)(*stream->_buffer._rp++);
}

int _jos_putc(int c, IO_FILE* stream) {

    if (stream->_buffer._wp == stream->_buffer._end) {
        if (stream->_buffer._size) {

            stream->write(stream, stream->_buffer._begin, 1);
            stream->_buffer._wp = stream->_buffer._begin;
        }
        else {
            // a fixed size buffer, we're overflowing
            return 0;
        }
    }
}

size_t _stdout_write(IO_FILE* file, const unsigned char* buffer, size_t len) {
    return fwrite(buffer, 1, len, stdout);
}

size_t _stdin_read(IO_FILE* file, unsigned char* buffer, size_t len) {
    return fread(buffer, 1, len, stdin);
}

static IO_FILE _jos_stdin = { .read = _stdin_read };
#define jos_stdin &_jos_stdin

void test_io_file(void) {
    _jos_stdin._buffer._size = 32;
    _jos_stdin._buffer._begin = malloc(32);
    _jos_stdin._buffer._end = _jos_stdin._buffer._begin + 1;
    _jos_stdin._buffer._rp = _jos_stdin._buffer._end;

    int c = _jos_getc(jos_stdin);
    printf("%c\n", c);
}

static int _jos_stdin_getch(void) {
    return _jos_getc(jos_stdin);
}

typedef int (*_getch_t)(void);
typedef int (*_putc_t)(int);

#define EXT_KEY 0xe0
#define VK_CR 0x0d
#define VK_TAB '\t'
#define VK_LEFT 0x4b
#define VK_RIGHT 0x4d
#define VK_BS 0x08

int _lab_getch(void) {
    int c = _getch();
    if (c == EXT_KEY) {
        c = _getch();
    }
    return c;
}

typedef struct _console_interface {
    
    size_t  (*cursor_left)(void);
    size_t  (*cursor_right)(void);
    void    (*putc)(int c);
    void    (*clear_from_cursor)(void);
} console_interface_t;

// ==============================================================================
// Windows Console stuff
static HANDLE _std_in, _std_out;
static CONSOLE_SCREEN_BUFFER_INFO _std_out_info;

static void con_cursor_left(void) {
    CONSOLE_SCREEN_BUFFER_INFO con_info;
    GetConsoleScreenBufferInfo(_std_out, &con_info);
    if (con_info.dwCursorPosition.X) {
        --con_info.dwCursorPosition.X;
        SetConsoleCursorPosition(_std_out, con_info.dwCursorPosition);
    }
}

static void con_cursor_right(void) {
    CONSOLE_SCREEN_BUFFER_INFO con_info;
    GetConsoleScreenBufferInfo(_std_out, &con_info);
    if (con_info.dwCursorPosition.X < con_info.srWindow.Right) {
        ++con_info.dwCursorPosition.X;
        SetConsoleCursorPosition(_std_out, con_info.dwCursorPosition);
    }
}

static void con_clear_from_cursor(void) {
    CONSOLE_SCREEN_BUFFER_INFO con_info;
    GetConsoleScreenBufferInfo(_std_out, &con_info);
    DWORD written;
    FillConsoleOutputCharacter(_std_out, ' ', con_info.srWindow.Right - con_info.dwCursorPosition.X, con_info.dwCursorPosition, &written);
}

static void con_putc(int c) {
    putc(c, stdout);
}

jo_status_t basic_line_editor(char* out_buffer, size_t buffer_len, _getch_t _getch, console_interface_t* console);
static void test_line_editor(void) {

    _std_in = GetStdHandle(STD_INPUT_HANDLE);
    _std_out = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(_std_out, &_std_out_info);
    
    _jos_stdin._buffer._size = 32;
    _jos_stdin._buffer._begin = malloc(32);
    _jos_stdin._buffer._end = _jos_stdin._buffer._begin + 1;
    _jos_stdin._buffer._rp = _jos_stdin._buffer._end;

    console_interface_t con;
    con.clear_from_cursor = con_clear_from_cursor;
    con.cursor_left = con_cursor_left;
    con.cursor_right = con_cursor_right;
    con.putc = con_putc;
    char buffer[256];
    basic_line_editor(buffer, sizeof(buffer), _lab_getch, &con);

    printf("\nWe got: \"%s\"", buffer);
}

// ==============================================================================

jo_status_t basic_line_editor(char* out_buffer, size_t buffer_len, _getch_t _getch, console_interface_t* console) {
    if (!out_buffer || buffer_len < 2) {
        return _JO_STATUS_FAILED_PRECONDITION;
    }

    int c;
    size_t wp = 0;
    size_t end = 0;
    while (wp < buffer_len && (c = _getch()) != VK_CR) {

        switch (c) {
        case VK_BS:
        {
            if (wp) {
                --wp;
                end = wp;
                out_buffer[wp] = 0;
                console->cursor_left();
                console->clear_from_cursor();
            }
        }
        break;
        case VK_LEFT:
        {
            if (wp) {
                --wp;
                console->cursor_left();
            }
        }
        break;
        case VK_RIGHT:
        {
            if (wp < end) {
                ++wp;
                console->cursor_right();
            }
        }
        break;
        case VK_TAB:
        {
            //TODO: insert 4 spaces...
        }
        break;
        //TODO: VK_HOME, VK_END...
        default:
        {
            out_buffer[wp++] = (char)(c & 0xff);
            if (wp > end) {
                end = wp;
                out_buffer[wp] = 0;
            }            
            console->putc(c);
        }
        break;
        }
    }

    return _JO_STATUS_SUCCESS;
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
        bm_info_header.biWidth = _info.horisontal_resolution = _window_width;
        bm_info_header.biHeight = -(LONG)(_info.vertical_resolution);
        bm_info_header.biPlanes = 1;
        bm_info_header.biBitCount = 32;
        _info.pixels_per_scan_line = ((_window_width * bm_info_header.biBitCount + 31) / 32);
        DWORD bm_size = 4 * _info.pixels_per_scan_line * _info.vertical_resolution;
        bits = (BYTE*)malloc(bm_size);
        GetDIBits(hdc_mem, bm, 0, _info.vertical_resolution, bits, (BITMAPINFO*)(&bm_info_header), DIB_RGB_COLORS);

        video_initialise(&(jos_allocator_t) {
            .alloc = malloc,
                .free = free
        });
        output_console_initialise();
        output_console_set_colour(0xffffffff);
        output_console_set_bg_colour(0x6495ed);
        output_console_set_font((const uint8_t*)font8x8_basic, 8, 8);
        video_clear_screen(0x6495ed);
        output_console_output_string(L"Press CR...\n");

        srand(1001107);
        scroller_initialise(&(rect_t) {
            .top = 50,
            .left = 8,
            .right = _info.horisontal_resolution - 8,
            .bottom = _info.vertical_resolution - 8,
        });
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        if (bits) {
            video_present();
            SetDIBits(hdc_mem, bm, 0, _info.vertical_resolution, bits, (BITMAPINFO*)(&bm_info_header), DIB_RGB_COLORS);
            BitBlt(hdc, 0, 0, _window_width, _info.vertical_resolution, hdc_mem, 0, 0, SRCCOPY);
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
        cw.right = _window_width;
        cw.top = 0;
        cw.bottom = _info.vertical_resolution;
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

static void ui_test_loop(void) {

    initialise_window();

    region_handle_t handle;
    jo_status_t status = output_console_create_region(&(rect_t) {
        .right = _info.horisontal_resolution,
            .top = 100,
            .bottom = _info.vertical_resolution - 100
    },
        & handle);
    output_console_activate_region(handle);
    output_console_set_colour(0xffffffff);
    output_console_set_bg_colour(0x6495ed);
    output_console_set_font((const uint8_t*)font8x8_basic, 8, 8);
    
    MSG msg = {0};
    uint64_t t0 = GetTickCount64();
    while (msg.message!=WM_QUIT)
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


// ==============================================================================================================

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
*/
    //dump_index(pdb_index_load_from_pdb_yml(), 0);
    pdb_index_load_from_pdb_yml("..\\build\\BOOTX64.YML");
    // RVA: offset from start of .text segment
    // RVA(symbol) = VA(symbol) - VA(.text)
    // VA(.text) = VA(module start) + RVA(.text)

    // EXAMPLE:
    // load address @ 0x81d32000
    // .text offset = 0x1000
    //  efi_main @ 0x81d338e0
    // RVA(efi_main) = 0x81d338e0 - (0x81d32000 + 0x1000) = 2272 (correct)

#define SYMBOL_RVA(symbol_va, text_offset, module_start)\
    (symbol_va) - (module_start + text_offset)
    
    char_array_slice_t slice = pdb_index_symbol_name_for_address(SYMBOL_RVA(0x81d338e0, 0x1000, 0x81d32000));
    slice = pdb_index_symbol_name_for_address(SYMBOL_RVA(0x81d34f70, 0x1000, 0x81d32000));

    //test_io_file();
    //ui_test_loop();
    //test_line_editor();

    return 0;
}