
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


#define _JOS_IMPLEMENT_JSON

#include "../libc/internal/include/libc_internal.h"
#include "../kernel/include/hex_dump.h"
#include "../kernel/include/interrupts.h"
#include "../kernel/include/pe.h"
#include "../kernel/include/video.h"
#include "../kernel/include/output_console.h"
#include "../kernel/programs/scroller.h"
#include "../kernel/include/collections.h"
#include "../libc/include/extensions/slices.h"
#include "../libc/include/extensions/pdb_index.h"
#include "../deps/font8x8/font8x8.h"
#include "../libc/internal/include/_file.h"
#include "../libc/include/extensions/json.h"
#include "../libc/include/extensions/base64.h"
#include "../kernel/include/linear_allocator.h"
#include "../kernel/include/arena_allocator.h"
#include "../kernel/include/hive.h"


#include "tests.h"


heap_allocator_t _malloc_allocator;

static void* malloc_alloc(heap_allocator_t* alloc, size_t size) {
	(void)alloc;
	return malloc(size);
}

static void* malloc_realloc(heap_allocator_t* alloc, void* ptr, size_t size) {
	(void)alloc;
	return realloc(ptr, size);
}

static void malloc_free(heap_allocator_t* alloc, void* ptr) {
	(void)alloc;
	free(ptr);
}

static size_t malloc_avail(heap_allocator_t* alloc) {
	(void)alloc;
	return (size_t)~0;
}

static void init_tests(void) {
	_malloc_allocator.alloc = malloc_alloc;
	_malloc_allocator.free = malloc_free;
	_malloc_allocator.available = malloc_avail;
	_malloc_allocator.realloc = malloc_realloc;
}


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



size_t _stdout_write(IO_FILE* file, const unsigned char* buffer, size_t len) {
    (void)file;
    return fwrite(buffer, 1, len, stdout);
}

size_t _stdin_read(IO_FILE* file, unsigned char* buffer, size_t len) {
    (void)file;
    return fread(buffer, 1, len, stdin);
}

static IO_FILE _jos_stdin = { .read = _stdin_read };
#define jos_stdin &_jos_stdin
static IO_FILE _jos_stdout = { .write = _stdout_write };
#define jos_stdout &_jos_stdout

void test_fstream_io(void) {

    char buffer[1024];
    const char* data = "hello world";
    IO_FILE out_file = { ._buffer._begin = buffer };
    out_file._buffer._end = out_file._buffer._begin + sizeof(buffer);
    out_file._buffer._wp = out_file._buffer._begin;
    out_file._buffer._rp = out_file._buffer._begin;
    out_file._buffer._size = sizeof(buffer);
    int written = _fwrite(data, 1, strlen(data) + 1, &out_file);
    
    IO_FILE in_file = { ._buffer._begin = buffer };
    out_file._buffer._end = out_file._buffer._begin + sizeof(buffer);
    out_file._buffer._wp = out_file._buffer._begin;
    out_file._buffer._rp = out_file._buffer._begin;
    out_file._buffer._size = 8;
    char out_buffer[128];
    int read = _fread(out_buffer, 1, strlen(data) + 1, &out_file);
    _stdout_write(0, out_buffer, read);
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

// ==============================================================================

void unordered_map_dump_stats(unordered_map_t* umap) {
    printf("unordered_map:\n");
    for (int i = 0; i < umap->_num_slots; ++i) {
        const size_t count = vector_size(umap->_slots + i);
        if (count) {
            printf("\t");
            for (int j = 0; j < vector_size(umap->_slots + i); ++j) {
                printf("=");
            }
            printf("\n");
        }
        else {
            printf("\t.\n");
        }
    }
}

static uint32_t identity_hash_func(const void* key) {
    return (uint32_t)(*(int*)key);
}

static bool int_cmp_func(const void* a, const void* b) {
    return *(const int*)a == *(const int*)b;
}

typedef struct _test_data {
    int _a;
    char _b;
} test_data_t;

static void test_unordered_map(void) {

    printf("test_unordered_map...");

    unordered_map_t umap;
    unordered_map_create(&umap, &(unordered_map_create_args_t){
            .value_size = sizeof(test_data_t),
            .key_size = sizeof(int),
            .hash_func = identity_hash_func,
            .cmp_func = int_cmp_func
    }, 
    &_malloc_allocator);

	int k = rand();
	const void* value = unordered_map_find(&umap, (map_key_t)&k);
	assert(value == NULL);

    size_t inserted = 0;
    for (int n = 0; n < 1000; ++n) {
        k = rand();
        test_data_t v = (struct _test_data){ ._a = rand(), ._b = n & 0xff };
        inserted += unordered_map_insert(&umap, (map_key_t)&k, (map_value_t)&v) ? 1:0;
        value = unordered_map_find(&umap, (map_key_t)&k);
        assert(value != NULL);
        assert(((const test_data_t*)value)->_a == v._a);
    }

    assert(unordered_map_size(&umap) == inserted);
    //unordered_map_dump_stats(&umap);
    unordered_map_destroy(&umap);
    printf("passed\n");
}

// ==============================================================================
// JSON stuff

static void test_json(void) {

    json_writer_context_t ctx;
    json_initialise_writer(&ctx, stdout);

    char register_buffer[512];
    linear_allocator_t* buffer_allocator = linear_allocator_create(register_buffer, sizeof(register_buffer));
    size_t out_len = 0;
    interrupt_stack_t stack;
    memset(&stack, 0xcd, sizeof(stack));
    unsigned char* encoded = base64_encode((const unsigned char*)&stack, sizeof(interrupt_stack_t), &out_len, (heap_allocator_t*)buffer_allocator);

    json_write_object_start(&ctx);
        json_write_key(&ctx, "version");
        json_write_object_start(&ctx);
            json_write_key(&ctx, "major");
            json_write_number(&ctx, 0);
            json_write_key(&ctx, "minor");
            json_write_number(&ctx, 1);
            json_write_key(&ctx, "patch");
            json_write_number(&ctx, 0);
        json_write_object_end(&ctx);
        json_write_key(&ctx, "image_info");
        json_write_object_start(&ctx);
            json_write_key(&ctx, "base");
            json_write_number(&ctx, 0x12345678abcdef00);
        json_write_object_end(&ctx);
        json_write_key(&ctx, "binary");
        json_write_string(&ctx, encoded);
    json_write_object_end(&ctx);
    
    printf("\n");

    FILE* ifstream;
    fopen_s(&ifstream, "json_data.json", "r");
    char buffer[512];
    memset(buffer, 0, sizeof(buffer));
    fread(buffer, 1, sizeof(buffer), ifstream);
    
    char_array_slice_t json_slice;
    char_array_slice_create(&json_slice, buffer, 0, strlen(buffer));

    char heap[512];
    heap_allocator_t* allocator = (heap_allocator_t*)arena_allocator_create(heap, sizeof(heap));
    vector_t tokens;
    vector_create(&tokens, 12, sizeof(json_token_t), allocator);
    json_token_t* root = json_tokenise(&tokens, json_slice);
    assert(root->_type == kJsonParse_Token_Object);
    json_tokenise(&tokens, root->_slice);
    json_token_t version = json_value(&tokens, "\"version\"");
    assert(version._type == kJsonParse_Token_Object);
    vector_clear(&tokens);
    json_tokenise(&tokens, version._slice);
    json_token_t major = json_value(&tokens, "\"major\"");
    long long major_version = strtoll(major._slice._ptr, NULL, 10);

    printf("found match %d, at %s\n", major_version, major._slice._ptr);
}

// ==============================================================================

typedef struct _console_interface {
    
    size_t  (*cursor_left)(void);
    size_t  (*cursor_right)(void);
    void    (*putc)(int c);
    void    (*clear_from_cursor)(void);
} console_interface_t;


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

        video_initialise(&(heap_allocator_t) {
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

void alloc_tests(void) {

    char register_buffer[1024];
    linear_allocator_t* buffer_allocator = linear_allocator_create(register_buffer+1, sizeof(register_buffer)-1);

    void *base_ptr, *ptr;
    alloc_alignment_t alignment = kAllocAlign_128;
    aligned_alloc((heap_allocator_t*)buffer_allocator, 42, alignment, &base_ptr, &ptr);
    _JOS_ASSERT(((uintptr_t)ptr & ((uintptr_t)alignment) - 1) == 0);
}

#ifndef _JOS_KERNEL_BUILD
void test_load_dll(void) {

    HMODULE dll = LoadLibrary("example.dll");
    peutil_pe_context_t ctx;
    peutil_bind(&ctx, (const void*)dll, kPe_Relocated);

    peutil_get_proc_name_address(&ctx, "eglTerminate");
    peutil_load_dll(&ctx, NULL);
}
#else
void test_load_dll(void) { };
#endif

// ==============================================================================================================

int main(void)
{
    init_tests();
    
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
*/
    test_load_dll();

    test_unordered_map();
    test_vector(&_malloc_allocator);
    test_vector_aligned(&_malloc_allocator);
    test_paged_list(&_malloc_allocator);
    test_hive(&_malloc_allocator);
    test_linear_allocator();
    test_arena_allocator();

    /* alloc_tests();

     char buffer[512];
     uintptr_t total = 0xddd2ce90;
     uintptr_t pages = 35;
     _JOS_LIBC_FUNC_NAME(snprintf)(buffer, sizeof(buffer), "total %lld", total);

     HMODULE this_module = GetModuleHandle(0);
     hex_dump_mem((void*)this_module, 64, k8bitInt);
     peutil_pe_context_t pe_ctx;
     peutil_bind(&pe_ctx, (const void*)this_module, kPe_Relocated);
     uintptr_t entry = peutil_entry_point(&pe_ctx);
     uintptr_t out_rva;
     bool is_executable = peutil_phys_is_executable(&pe_ctx, entry, &out_rva);
     is_executable = peutil_phys_is_executable(&pe_ctx, entry-0x100, 0);
     is_executable = peutil_phys_is_executable(&pe_ctx, (uintptr_t)(this_module) + 0x6c008, 0);

     void* heap = malloc(1024*1024);
     heap_allocator_t* allocator = (heap_allocator_t*)arena_allocator_create(heap, 1024 * 1024);

     vector_t a;
     vector_t b;
     vector_create(&a, 16, sizeof(int), allocator);
     vector_create_like(&b, &a);
     int e = 1;
     vector_push_back(&b, (void*)&e);
     vector_swap(&a, &b);
     vector_destroy(&b);*/
        
    /*test_fixed_allocator();
    test_linear_allocator();
    test_arena_allocator_allocator();

    test_json();*/

#if TODO
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
#endif
    return 0;
}