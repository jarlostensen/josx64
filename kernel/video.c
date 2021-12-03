
#ifdef _JOS_KERNEL_BUILD
#include <c-efi.h>
#include <c-efi-protocol-graphics-output.h>
#endif 

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "include/arena_allocator.h"
// working memory arena, used as a scratch area for certain operations
static arena_allocator_t* _video_memory_arena = 0;

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "include/jos.h"
#include "include/video.h"

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wsign-compare"
#endif

#define STBIR_MALLOC(size,context) arena_allocator_alloc(_video_memory_arena, (size))
#define STBIR_FREE(ptr,context) arena_allocator_free(_video_memory_arena, (ptr))
#include "../deps/stb/stb_image_resize.h"

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

#ifdef _JOS_KERNEL_BUILD
// in efi_main.c
extern CEfiBootServices* g_boot_services;
extern CEfiSystemTable* g_st;
static CEfiGraphicsOutputProtocol* _gop = 0;
CEfiGraphicsOutputModeInformation _info;
#endif

#define VIDEO_MAX_HORIZ_RES 1024

// copy of active mode info
static size_t _framebuffer_base = 0;
static size_t _red_shift;
static size_t _green_shift;
static size_t _blue_shift;
// all rendering goes to the backbuffer until video_present is called when its contents 
// are copied to the framebuffer
static uint8_t* _backbuffer = 0;
static size_t   _framebuffer_size = 0;

#define _FONT_HEIGHT    8
#define _FONT_WIDTH     8

#ifndef _JOS_KERNEL_BUILD
// implemented in the LAB code base
extern uint32_t* framebuffer_base(void);
extern video_mode_info_t _info;
#endif

static uint32_t* backbuffer_wptr(size_t top, size_t left) {
    return (uint32_t*)(_backbuffer)+top * _info.pixels_per_scan_line + left;
}

#ifdef _JOS_KERNEL_BUILD
jo_status_t video_initialise(static_allocation_policy_t* static_allocation_policy, CEfiBootServices* boot_services)
#else
jo_status_t video_initialise(static_allocation_policy_t* static_allocation_policy)
#endif
{
    jo_status_t status = _JO_STATUS_SUCCESS;

#ifdef _JOS_KERNEL_BUILD

    memset(&_info, 0, sizeof(_info));

    //ZZZ: may need to be bigger or code needs to handle "buffer too-small" error
    CEfiHandle handle_buffer[3];
    CEfiUSize handle_buffer_size = sizeof(handle_buffer);
    memset(handle_buffer, 0, sizeof(handle_buffer));

    CEfiStatus efi_status = boot_services->locate_handle(C_EFI_BY_PROTOCOL, &C_EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, 0, &handle_buffer_size, handle_buffer);
    if (efi_status == C_EFI_SUCCESS) {

        //TODO: this works but it's not science; what makes one handle a better choice than another? 
        //      we should combine these two tests (handler and resolution) and pick the base from that larger set
        size_t num_handles = handle_buffer_size / sizeof(CEfiHandle);
        for (size_t n = 0; n < num_handles; ++n)
        {
            efi_status = boot_services->handle_protocol(handle_buffer[n], &C_EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, (void**)&_gop);
            if (efi_status == C_EFI_SUCCESS)
            {
                break;
            }
        }

        if (efi_status == C_EFI_SUCCESS) {

            CEfiU32 max_horiz_res = 0;
            CEfiU32 mode = 0;
            CEfiI32 found_mode = -1;

            CEfiUSize size_of_info = 0;
            CEfiGraphicsOutputModeInformation* info;

            efi_status = _gop->query_mode(_gop, mode, &size_of_info, &info);
            while (efi_status == C_EFI_SUCCESS) {
                switch (info->pixel_format)
                {
                case PixelRedGreenBlueReserved8BitPerColor:
                case PixelBlueGreenRedReserved8BitPerColor:
                {
                    if (info->horizontal_resolution > max_horiz_res
                        &&
                        info->horizontal_resolution <= VIDEO_MAX_HORIZ_RES
                        ) {
                        max_horiz_res = info->horizontal_resolution;
                        memcpy(&_info, info, sizeof(_info));
                        found_mode = mode;
                    }
                }
                break;
                default:;
                }

                ++mode;
                efi_status = _gop->query_mode(_gop, mode, &size_of_info, &info);
            }

            if (found_mode >= 0) {
                // switch to the mode we've found 
                efi_status = _gop->set_mode(_gop, found_mode);
                if (efi_status == C_EFI_SUCCESS) {

                    _framebuffer_base = _gop->mode->frame_buffer_base;

                    switch (_info.pixel_format)
                    {
                    case PixelRedGreenBlueReserved8BitPerColor:
                    {
                        _red_shift = 0;
                        _green_shift = 8;
                        _blue_shift = 16;
                    }
                    break;
                    case PixelBlueGreenRedReserved8BitPerColor:
                    {
                        _red_shift = 16;
                        _green_shift = 8;
                        _blue_shift = 0;
                    }
                    break;
                    default:;
                    }
                }
            }
        }
    }

    if (efi_status != C_EFI_SUCCESS) {
        status = _JO_STATUS_INTERNAL;
    }
#endif

    if (_JO_SUCCEEDED(status)) {
        _framebuffer_size = (_info.pixels_per_scan_line * _info.vertical_resolution * 4);
         _backbuffer = (uint8_t*)static_allocation_policy->allocator->alloc(static_allocation_policy->allocator, 
                _framebuffer_size);
         // we set aside an arena with some room for scaling operations         
         _video_memory_arena = arena_allocator_create(static_allocation_policy->allocator->alloc(static_allocation_policy->allocator,
             2*_framebuffer_size), 2*_framebuffer_size);
    }

    return status;
}

void video_present(void) {
#ifdef _JOS_KERNEL_BUILD
    uint32_t* framebuffer = (uint32_t*)_framebuffer_base;
#else
    uint32_t* framebuffer = framebuffer_base();
#endif
    memcpy(framebuffer, _backbuffer, _info.pixels_per_scan_line * _info.vertical_resolution * 4);
}

uint32_t video_make_color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << _red_shift) | ((uint32_t)g << _green_shift) | ((uint32_t)b << _blue_shift);
}

void video_clear_screen(uint32_t colour) {
    //TODO: assert(_framebuffer_base!=0)
    uint32_t* wptr = backbuffer_wptr(0, 0);
    size_t pixels_to_fill = _info.pixels_per_scan_line * _info.vertical_resolution;
    while (pixels_to_fill) {
        *wptr++ = colour;
        *wptr++ = colour;
        *wptr++ = colour;
        *wptr++ = colour;
        *wptr++ = colour;
        *wptr++ = colour;
        *wptr++ = colour;
        *wptr++ = colour;
        pixels_to_fill -= 8;
    }
}

video_mode_info_t video_get_video_mode_info() {
#ifdef _JOS_KERNEL_BUILD
    video_mode_info_t info = {
        .horisontal_resolution = _info.horizontal_resolution,
        .vertical_resolution = _info.vertical_resolution,
        .pixel_format = (_info.pixel_format == PixelBlueGreenRedReserved8BitPerColor) ? kVideo_Pixel_Format_BGRx : kVideo_Pixel_Format_RBGx,
    };
    return info;
#else
    return _info;
#endif
}

void video_create_draw_glyph_context(draw_text_segment_args_t* args, _video_glyp_draw_context_t* out_ctx) {
    out_ctx->_wptr = backbuffer_wptr(args->top, args->left);
    out_ctx->_lut[0] = args->bg_colour;
    out_ctx->_lut[1] = args->colour;
    out_ctx->_font_ptr = args->font_ptr;
}

void video_draw_glyph(_video_glyp_draw_context_t* ctx, const wchar_t c) {

    if (!ctx || !ctx->_wptr) {
        return;
    }

    //ZZZ: since we're just throwing away the unicode bit anyway we might as well not use it..?
    size_t glyph_offset = (((size_t)c & 0xff) << 3);
    size_t line = 0;
    uint32_t* line_ptr = ctx->_wptr;
    const uint32_t bg_colour = ctx->_lut[0];
    const uint32_t fg_colour = ctx->_lut[1];
    while (line < _FONT_HEIGHT) {

        uint8_t pixels = ctx->_font_ptr[glyph_offset + line];
        switch (pixels) {
        case 0:
        {
            line_ptr[0] = bg_colour; line_ptr[1] = bg_colour;
            line_ptr[2] = bg_colour; line_ptr[3] = bg_colour;
            line_ptr[4] = bg_colour; line_ptr[5] = bg_colour;
            line_ptr[6] = bg_colour; line_ptr[7] = bg_colour;
        }
        break;
        case 0xff:
        {
            line_ptr[0] = fg_colour; line_ptr[1] = fg_colour;
            line_ptr[2] = fg_colour; line_ptr[3] = fg_colour;
            line_ptr[4] = fg_colour; line_ptr[5] = fg_colour;
            line_ptr[6] = fg_colour; line_ptr[7] = fg_colour;
        }
        break;
        default:
        {
            uint8_t index = 0;
            while (index < _FONT_WIDTH) {
                uint8_t set = pixels & 1;
                line_ptr[index] = ctx->_lut[set];
                pixels >>= 1;
                ++index;
            }
        }
        break;
        }
        ++line;
        line_ptr += _info.pixels_per_scan_line;
    }
    // advance to the next glyph position
    ctx->_wptr += ctx->_kerning;
}

void video_draw_text_segment_w(draw_text_segment_args_t* args, const wchar_t* text) {

    if (!args || !text || wcslen(text) == 0) {
        return;
    }

    if (args->seg_len == 0)
        return;

    uint32_t* wptr = backbuffer_wptr(args->top, args->left);

    // pixel set, or not set
    uint32_t colour_lut[2] = { args->bg_colour, args->colour };

    size_t n = args->seg_offs;
    size_t end = args->seg_offs + args->seg_len;
    while (n < end) {

        wchar_t c = text[n];
        size_t glyph_offset = (size_t)((c & 0xff) << 3);
        size_t line = 0;
        uint32_t* line_ptr = wptr;
        while (line < 8) {

            uint8_t pixels = args->font_ptr[glyph_offset + line];
            switch (pixels) {
            case 0:
            {
                line_ptr[0] = args->bg_colour; line_ptr[1] = args->bg_colour;
                line_ptr[2] = args->bg_colour; line_ptr[3] = args->bg_colour;
                line_ptr[4] = args->bg_colour; line_ptr[5] = args->bg_colour;
                line_ptr[6] = args->bg_colour; line_ptr[7] = args->bg_colour;
            }
            break;
            case 0xff:
            {
                line_ptr[0] = args->colour; line_ptr[1] = args->colour;
                line_ptr[2] = args->colour; line_ptr[3] = args->colour;
                line_ptr[4] = args->colour; line_ptr[5] = args->colour;
                line_ptr[6] = args->colour; line_ptr[7] = args->colour;
            }
            break;
            default:
            {
                uint8_t index = 0;
                while (index < 8) {
                    uint8_t set = pixels & 1;
                    line_ptr[index] = colour_lut[set];
                    pixels >>= 1;
                    ++index;
                }
            }
            break;
            }

            ++line;
            line_ptr += _info.pixels_per_scan_line;
        }
        //NOTE: based on font being 8x8
        wptr += 8;
        ++n;
    }
}

//TODO: combine the _w and _a methods, perhaps always use _w under the hood
void video_draw_text_segment_a(draw_text_segment_args_t* args, const char* text) {

    if (!args || !text || strlen(text) == 0) {
        return;
    }

    if (args->seg_len == 0)
        return;

    uint32_t* wptr = backbuffer_wptr(args->top, args->left);

    // pixel set, or not set
    uint32_t colour_lut[2] = { args->bg_colour, args->colour };

    size_t n = args->seg_offs;
    size_t end = args->seg_offs + args->seg_len;
    while (n < end) {

        char c = text[n];
        size_t glyph_offset = (size_t)((c & 0xff) << 3);
        size_t line = 0;
        uint32_t* line_ptr = wptr;
        while (line < 8) {

            uint8_t pixels = args->font_ptr[glyph_offset + line];
            switch (pixels) {
            case 0:
            {
                line_ptr[0] = args->bg_colour; line_ptr[1] = args->bg_colour;
                line_ptr[2] = args->bg_colour; line_ptr[3] = args->bg_colour;
                line_ptr[4] = args->bg_colour; line_ptr[5] = args->bg_colour;
                line_ptr[6] = args->bg_colour; line_ptr[7] = args->bg_colour;
            }
            break;
            case 0xff:
            {
                line_ptr[0] = args->colour; line_ptr[1] = args->colour;
                line_ptr[2] = args->colour; line_ptr[3] = args->colour;
                line_ptr[4] = args->colour; line_ptr[5] = args->colour;
                line_ptr[6] = args->colour; line_ptr[7] = args->colour;
            }
            break;
            default:
            {
                uint8_t index = 0;
                while (index < 8) {
                    uint8_t set = pixels & 1;
                    line_ptr[index] = colour_lut[set];
                    pixels >>= 1;
                    ++index;
                }
            }
            break;
            }

            ++line;
            line_ptr += _info.pixels_per_scan_line;
        }
        //NOTE: based on font being 8x8
        wptr += 8;
        ++n;
    }
}

void video_put_pixel(size_t x, size_t y, uint32_t rgba) {
    *backbuffer_wptr(y, x) = rgba;
}

uint32_t video_get_pixel(size_t x, size_t y) {
    return *backbuffer_wptr(y, x);
}

void video_draw_text(draw_text_segment_args_t* args, const wchar_t* text) {
    args->seg_len = wcslen(text);
    if (!args || !text || args->seg_len == 0) {
        return;
    }
    video_draw_text_segment_w(args, text);
}

void video_scroll_up_region_full_width(size_t top, size_t bottom, size_t linesToScroll) {

    size_t strip_pixel_stride = linesToScroll * _info.pixels_per_scan_line;
    uint32_t* wptr = backbuffer_wptr(top, 0);
    uint32_t* rptr = wptr + strip_pixel_stride;

    const size_t region_height = bottom - top;
    const size_t strips = (region_height / linesToScroll);
    const size_t rem_lines = region_height % linesToScroll;
    for (size_t strip = 0; strip < strips; ++strip) {
        memcpy(wptr, rptr, strip_pixel_stride << 2);
        wptr += strip_pixel_stride;
        rptr += strip_pixel_stride;
    }

    if (rem_lines) {
        memcpy(wptr, rptr, rem_lines * (_info.pixels_per_scan_line << 2));
    }
}

void video_scale_draw_bitmap(const uint32_t* bitmap, size_t src_width, size_t src_height, size_t src_stride, size_t dest_top, size_t dest_left, size_t dest_width, size_t dest_height, video_filter_mode_t filter_mode) {

    //TODO: need asserts...
    if (!bitmap || !src_width || !src_height || !dest_width || !dest_height) {
        return;
    }

    if (src_width == dest_width && src_height == dest_height) {

        // plain copy
        uint32_t* wptr = backbuffer_wptr(dest_top, dest_left);
        while (dest_height) {
            memcpy(wptr, bitmap, dest_width << 2);
            wptr += _info.pixels_per_scan_line;
            bitmap += dest_width;
            --dest_height;
        }
    }
    else {
        if (filter_mode == kVideo_Filter_None) {

            // straight up/down scaling, no filtering
            if (src_width < dest_width && src_height < dest_height) {

                const uint32_t fp_width = (const uint32_t)(src_width << 16);
                const uint32_t fp_height = (const uint32_t)(src_height << 16);
                const uint32_t fp_x_add = fp_width / (const uint32_t)dest_width;
                const uint32_t fp_y_add = fp_height / (const uint32_t)dest_height;

                size_t y = 0;
                size_t src_y = 0;

                uint32_t* wptr = backbuffer_wptr(dest_top, dest_left);
                const uint32_t* src_row = bitmap;

                while (y < dest_height) {

                    size_t x = 0;
                    size_t src_x = 0;
                    while (x < dest_width) {

                        wptr[x] = src_row[src_x >> 16];
                        src_x += fp_x_add;
                        ++x;
                    }

                    wptr += _info.pixels_per_scan_line;
                    src_y += fp_y_add;
                    src_row = bitmap + (src_y >> 16) * src_stride;
                    ++y;
                }
            }
            // ELSE TODO:
        }
        else {
            // generic re-size and filtering
            stbir_resize_uint8_srgb((const unsigned char*)bitmap, (int)src_width, (int)src_height, (int)src_stride<<2, (unsigned char*)backbuffer_wptr(dest_top, dest_left),
                (int)dest_width, (int)dest_height, (int)_info.pixels_per_scan_line << 2, 4, STBIR_ALPHA_CHANNEL_NONE, 0);
        }
    }
}

void video_scale_draw_indexed_bitmap(const uint8_t* bitmap, const uint32_t* colourmap, size_t colourmap_size,
    size_t src_width, size_t src_height,
    size_t dest_top, size_t dest_left, size_t dest_width, size_t dest_height) {
    _JOS_ASSERT(bitmap && colourmap && colourmap_size && src_width && src_height && dest_width && dest_height);

    uint32_t* wptr = backbuffer_wptr(dest_top, dest_left);

    if (src_width == dest_width && src_height == dest_height) {

        // plain copy        
        while (src_height) {
            for (size_t p = 0; p < src_width; ++p)
            {
                //TODO: assert on index range (in DEBUG)
                wptr[p] = colourmap[*bitmap++];
            }
            wptr += _info.pixels_per_scan_line;
            --src_height;
        }
    }
    else if (src_width == dest_width && src_height < dest_height) {

        // scale up, height
        while (src_height) {
            for (size_t p = 0; p < src_width; ++p)
            {
                //TODO: assert on index range (in DEBUG)
                wptr[p] = colourmap[*bitmap++];
            }
            wptr += _info.pixels_per_scan_line;
            --src_height;
        }
    }
    else {
        //TODO: 
    }
}
