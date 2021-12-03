#pragma once
#ifndef _JOS_KERNEL_VIDEO_H
#define _JOS_KERNEL_VIDEO_H

#ifdef _JOS_KERNEL_BUILD
#include <c-efi.h>
jo_status_t video_initialise(static_allocation_policy_t* static_allocation_policy, CEfiBootServices* boot_services);
#else
jo_status_t video_initialise(static_allocation_policy_t* static_allocation_policy);
#endif

#include <wchar.h>

#define VIDEO_PIXEL_STRIDE  4

uint32_t video_make_color(uint8_t r, uint8_t g, uint8_t b);
void video_clear_screen(uint32_t colour);

//NOTE: always assumes 32bit RGBx (or BGRx)
typedef enum _video_mode_pixel_format {

        kVideo_Pixel_Format_BGRx,
        kVideo_Pixel_Format_RBGx

} video_mode_pixel_format_t;

typedef struct _video_mode_info {

    size_t                      horisontal_resolution;
    size_t                      vertical_resolution;    
    video_mode_pixel_format_t   pixel_format;
#ifndef _JOS_KERNEL_BUILD
    size_t                      pixels_per_scan_line;
#endif

} video_mode_info_t;

video_mode_info_t video_get_video_mode_info(void);

typedef struct _draw_text_segment_args {
    size_t      left;
    size_t      top;
    uint32_t    colour;
    uint32_t    bg_colour;
    // expected to be 128x8
    const uint8_t*    font_ptr;
    size_t      seg_offs;
    size_t      seg_len;

} draw_text_segment_args_t;

typedef enum _video_filter_mode {

    kVideo_Filter_None,
    kVideo_Filter_Default,

} video_filter_mode_t;

typedef struct _video_glyp_draw_context {
    uint32_t*                   _wptr;
    uint32_t                    _lut[2];
    size_t                      _kerning;
    const uint8_t*              _font_ptr;
} _video_glyp_draw_context_t;

void video_create_draw_glyph_context(draw_text_segment_args_t* args, _video_glyp_draw_context_t* out_ctx);
void video_draw_glyph(_video_glyp_draw_context_t* ctx, const wchar_t c);

void video_draw_text_segment_w(draw_text_segment_args_t* args, const wchar_t* text);
void video_draw_text_segment_a(draw_text_segment_args_t* args, const char* text);
void video_draw_text(draw_text_segment_args_t* args, const wchar_t* text);

void video_scale_draw_bitmap(const uint32_t* bitmap, size_t src_width, size_t src_height, size_t src_stride,
    size_t dest_top, size_t dest_left, size_t dest_width, size_t dest_height, video_filter_mode_t filter_mode);

void video_scale_draw_indexed_bitmap(const uint8_t* bitmap, const uint32_t* colourmap, size_t colourmap_size, 
                                        size_t src_width, size_t src_height, 
                                        size_t dest_top, size_t dest_left, size_t dest_width, size_t dest_height);
void video_scroll_up_region_full_width(size_t top, size_t bottom, size_t linesToScroll);

void video_put_pixel(size_t x, size_t y, uint32_t rgba);
uint32_t video_get_pixel(size_t x, size_t y);

void video_present(void);

#endif // _JOS_KERNEL_VIDEO_H
