
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

typedef int (*_getch_t)(void);

jo_status_t basic_line_editor(char* out_buffer, size_t buffer_len, size_t* out_characters_read, _getch_t getch) {
    if (!out_buffer || buffer_len < 2) {
        return _JO_STATUS_FAILED_PRECONDITION;
    }

    jo_status_t status = _JO_STATUS_SUCCESS;



    return status;
}

// =================================================================================================

typedef enum _scroller_tile_type {

    kScTile_Ground1,
    kScTile_PipeBody1,
    kScTile_Cloud,
    kScTile_Sky,

    kScTile_NumberOfTiles

} scroller_tile_t;

enum _scroller_constants {

    kScLayer_Height = 8,

    kScLayer_VisibleFieldWidth = 32,
    kScLayer_LayerFieldWidth,   //< NOTE: kScLayer_VisibleLayerWidth+1

    // pixels
    kScTile_Height = 8,
    kScTile_Width = 8,
};

static uint32_t _scroller_palette[] = {

    // black
    0,
    // dark brown
    0x663300,
    // light brown
    0x99c400,
    // dark green
    0x193300,
    // grass green
    0x006600,
    // pipe body silver 
    0xc0c0c0,
    // pipe body dark 
    0x808080,
    // sky blue
    0x00ffff,
    // cloud white
    0x99ffff,
    // white
    0xffffff,
};

enum _scroller_palette_colour {

    kScColour_Black,
    kScColour_DarkBrown,
    kScColour_LightBrown,
    kScColour_DarkGreen,
    kScColour_GrassGreen,
    kScColour_PipeBodySilver,
    kScColour_PipeBodyDark,
    kScColour_SkyBlue,
    kScColour_CloudWhite,
    kScColour_White
};

// top down
static uint8_t _scroller_sprites[kScTile_NumberOfTiles][kScTile_Height][kScTile_Width] = {

    // ground
    {
        { kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen,kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen },
        { kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen,kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_GrassGreen },
        { kScColour_GrassGreen, kScColour_DarkGreen, kScColour_DarkGreen, kScColour_GrassGreen, kScColour_DarkGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_DarkGreen },
        { kScColour_DarkGreen, kScColour_DarkGreen, kScColour_GrassGreen, kScColour_DarkGreen, kScColour_DarkGreen, kScColour_GrassGreen, kScColour_GrassGreen, kScColour_DarkGreen },
        { kScColour_DarkGreen, kScColour_DarkGreen, kScColour_DarkGreen, kScColour_DarkGreen, kScColour_DarkGreen, kScColour_GrassGreen, kScColour_DarkGreen, kScColour_DarkGreen },
        { kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown },
        { kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown },
        { kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown, kScColour_DarkBrown },
    },

    // pipe body
    {
        { kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver,kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodySilver, kScColour_PipeBodyDark },
        { kScColour_CloudWhite, kScColour_PipeBodyDark, kScColour_PipeBodyDark, kScColour_PipeBodyDark, kScColour_PipeBodyDark, kScColour_PipeBodyDark, kScColour_PipeBodyDark, kScColour_PipeBodyDark },
    },

    // cloud 
    {
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue},
        { kScColour_SkyBlue, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_SkyBlue, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_SkyBlue },
        { kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite},
        { kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite},
        { kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite,kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_SkyBlue,kScColour_CloudWhite, kScColour_CloudWhite, kScColour_CloudWhite, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
    },

    // sky
    {
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
        { kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue, kScColour_SkyBlue },
    }
};

//NOTE: a multiple of kScTile_Height and Width
static rect_t _scroller_window;
// the tile field which we'll be rendering
static scroller_tile_t _scroller_layers[kScLayer_Height][kScLayer_LayerFieldWidth];
static uint32_t _scroller_bm[kScLayer_Height * kScTile_Height][kScLayer_LayerFieldWidth * kScTile_Width];

//TEST:
void scroller_generate_field(bool refresh) {

    if (refresh)
    {
        for (size_t row = 0; row < kScLayer_Height; ++row) {
            for (size_t col = kScLayer_VisibleFieldWidth - 1; col < kScLayer_LayerFieldWidth; ++col) {
                _scroller_layers[row][col] = kScTile_Sky;
            }
        }

        // ground
        for (size_t col = kScLayer_VisibleFieldWidth-1; col < kScLayer_LayerFieldWidth; ++col) {
            _scroller_layers[kScLayer_Height - 1][col] = kScTile_Ground1;
        }

        // a few clouds 
        for (size_t col = kScLayer_VisibleFieldWidth-1; col < kScLayer_LayerFieldWidth; ++col) {
            if (!(rand() % 4)) {
                _scroller_layers[rand() % 3][col] = kScTile_Cloud;
            }
        }

        // a few intermittent tiles to step on 
        for (size_t col = kScLayer_VisibleFieldWidth-1; col < kScLayer_LayerFieldWidth; ++col) {
            if (!(rand() % 6)) {
                _scroller_layers[kScLayer_Height - 2][col] = kScTile_PipeBody1;
            }
        }
    }
    else {
        for (size_t row = 0; row < kScLayer_Height; ++row) {
            for (size_t col = 0; col < kScLayer_LayerFieldWidth; ++col) {
                _scroller_layers[row][col] = kScTile_Sky;
            }
        }
        // ground
        for (size_t col = 0; col < kScLayer_LayerFieldWidth; ++col) {
            _scroller_layers[kScLayer_Height - 1][col] = kScTile_Ground1;
        }

        // a few clouds 
        for (size_t col = 0; col < kScLayer_LayerFieldWidth; ++col) {
            if (!(rand() % 4)) {
                _scroller_layers[rand() % 3][col] = kScTile_Cloud;
            }
        }

        // a few intermittent tiles to step on 
        for (size_t col = 0; col < kScLayer_LayerFieldWidth; ++col) {
            if (!(rand() % 6)) {
                _scroller_layers[kScLayer_Height - 2][col] = kScTile_PipeBody1;
            }
        }

        size_t x = 0;
        size_t y = 0;
        for (size_t row = 0; row < kScLayer_Height; ++row) {
            for (size_t col = 0; col < kScLayer_LayerFieldWidth; ++col) {

                const uint8_t* sprite = &_scroller_sprites[_scroller_layers[row][col]];

                for (size_t i = 0; i < kScTile_Height; ++i) {
                    for (size_t j = 0; j < kScTile_Width; ++j) {

                        _scroller_bm[y + i][x + j] = _scroller_palette[*sprite++];
                    }
                }
                x += kScTile_Width;
            }
            x = 0;
            y += kScTile_Height;
        }
    }
}

static size_t _scroll_pos = 0;

void scroller_render_field(void) {

    static const size_t bm_height = (kScLayer_Height * kScTile_Height);
    static const size_t bm_width = (kScLayer_LayerFieldWidth * kScTile_Width);
    static const size_t bm_vis_width = (kScLayer_VisibleFieldWidth * kScTile_Width);

    if (_scroll_pos) {
        // scroll bitmap horisontally, feed in from rightmost column
        uint32_t* bm_row = _scroller_bm;
        for (size_t row = 0; row < bm_height; ++row) {
            for (size_t j = 0; j < (bm_width - 1); ++j) {
                bm_row[j] = bm_row[j + 1];
            }
            bm_row += bm_width;
        }
    }

    video_scale_draw_bitmap(_scroller_bm, bm_vis_width, bm_height, bm_width,
        _scroller_window.top, _scroller_window.left, _scroller_window.right - _scroller_window.left, _scroller_window.bottom - _scroller_window.top, kVideo_Filter_None);
    video_present();

    ++_scroll_pos;
    if ((_scroll_pos % kScTile_Width) == 0) {
        // wrap; generate a new rigthmost column
        scroller_generate_field(true);

        // fill the bitmap rightmost column with fresh pixels
        size_t y = 0;
        for (size_t row = 0; row < kScLayer_Height; ++row) {
            for (size_t col = kScLayer_VisibleFieldWidth-1; col < kScLayer_LayerFieldWidth; ++col) {
                const uint8_t* sprite = &_scroller_sprites[_scroller_layers[row][col]];
                for (size_t i = 0; i < kScTile_Height; ++i) {
                    for (size_t j = 0; j < kScTile_Width; ++j) {
                        _scroller_bm[y + i][bm_vis_width + j] = _scroller_palette[*sprite++];
                    }
                }
            }
            y += kScTile_Height;
        }
    }
}

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
            ._alloc = malloc,
                ._free = free
        });
        output_console_initialise();
        output_console_set_colour(0xffffffff);
        output_console_set_bg_colour(0x6495ed);
        output_console_set_font((const uint8_t*)font8x8_basic, 8, 8);
        video_clear_screen(0x6495ed);
        output_console_output_string(L"Press CR...\n");

        srand(1001107);

        _scroller_window.left = 8;
        _scroller_window.top = 50;
        _scroller_window.right = _info.horisontal_resolution - 8;
        _scroller_window.bottom = _info.vertical_resolution - 8;

        scroller_generate_field(false);
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
        case VK_RETURN:
        {            
            scroller_render_field();
            InvalidateRect(hWnd, 0, TRUE);
        }
        break;
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
            CW_USEDEFAULT, CW_USEDEFAULT, cw.right - cw.left, cw.bottom - cw.top,
            NULL,
            NULL,
            wc.hInstance,
            NULL
        );

        ShowWindow(hwnd, SW_SHOW);
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

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
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

    dump_index(pdb_index_load_from_pdb_yml(), 0);

    char_array_slice_t slice = pdb_index_symbol_name_for_address(71904);
    slice = pdb_index_symbol_name_for_address(137950);
*/

//test_io_file();
    ui_test_loop();

    return 0;
}