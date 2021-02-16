
#ifdef _JOS_KERNEL_BUILD
#include <c-efi.h>
#include <c-efi-protocol-graphics-output.h>
#endif 

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "include/jos.h"
#include "include/video.h"

#ifdef _JOS_KERNEL_BUILD
// in efi_main.c
extern CEfiBootServices * g_boot_services;
extern CEfiSystemTable  * g_st;
static CEfiGraphicsOutputProtocol * _gop = 0;
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

#ifndef _JOS_KERNEL_BUILD
// implemented in the LAB code base
extern uint32_t*    framebuffer_base(void);
extern video_mode_info_t _info;
#endif

static uint32_t*    backbuffer_wptr(size_t top, size_t left) {
    return (uint32_t*)(_backbuffer) + top*_info.pixels_per_scan_line + left;
}

jo_status_t video_initialise(jos_allocator_t * allocator)
{
    jo_status_t status = _JO_STATUS_SUCCESS;
    
#ifdef _JOS_KERNEL_BUILD

    memset(&_info, 0, sizeof(_info));

    //ZZZ: may need to be bigger or code needs to handle "buffer too-small" error
    CEfiHandle handle_buffer[3];
    CEfiUSize handle_buffer_size = sizeof(handle_buffer);
    memset(handle_buffer,0,sizeof(handle_buffer));

    wchar_t wbuffer[128];

    CEfiStatus efi_status = g_boot_services->locate_handle(C_EFI_BY_PROTOCOL, &C_EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, 0, &handle_buffer_size, handle_buffer);
    if ( efi_status==C_EFI_SUCCESS ) {

        //TODO: this works but it's not science; what makes one handle a better choice than another? 
        //      we should combine these two tests (handler and resolution) and pick the base from that larger set
        size_t num_handles = handle_buffer_size/sizeof(CEfiHandle);
        for(size_t n = 0; n < num_handles; ++n)
        {
            efi_status = g_boot_services->handle_protocol(handle_buffer[n], &C_EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, (void**)&_gop);
            if ( efi_status == C_EFI_SUCCESS )
            {
                break;
            }            
        }
        
        if ( efi_status == C_EFI_SUCCESS ) {

            CEfiU32 max_horiz_res = 0;
            CEfiU32 mode = 0;
            CEfiI32 found_mode = -1;

            CEfiUSize size_of_info = 0;
            CEfiGraphicsOutputModeInformation* info;
            
            efi_status =_gop->query_mode(_gop, mode, &size_of_info, &info);
            while(efi_status==C_EFI_SUCCESS) {
                switch(info->pixel_format)
                {
                    case PixelRedGreenBlueReserved8BitPerColor:
                    case PixelBlueGreenRedReserved8BitPerColor:
                    {
                        if ( info->horizontal_resolution > max_horiz_res 
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

            if ( found_mode >= 0 ) {
                // switch to the mode we've found 
                efi_status = _gop->set_mode(_gop, found_mode);
                if ( efi_status == C_EFI_SUCCESS ) {
                                 
                    _framebuffer_base = _gop->mode->frame_buffer_base;
                    
                    switch(_info.pixel_format)
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

    if ( efi_status!=C_EFI_SUCCESS ) {
        status = _JO_STATUS_INTERNAL;
    }
#endif

    if (_JO_SUCCEEDED(status)) {        
        _backbuffer = (uint8_t*)allocator->_alloc(_info.pixels_per_scan_line * _info.vertical_resolution * 4);
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
    uint32_t *wptr = backbuffer_wptr(0,0);
    size_t pixels_to_fill = _info.pixels_per_scan_line * _info.vertical_resolution;
    while(pixels_to_fill) {
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
        .pixel_format = (_info.pixel_format == PixelBlueGreenRedReserved8BitPerColor ) ? kVideo_Pixel_Format_BGRx : kVideo_Pixel_Format_RBGx,
    };
    return info;
#else
    return _info;
#endif
}

void video_draw_text_segment(draw_text_segment_args_t* args, const wchar_t* text) {

    if ( !args || !text || wcslen(text)==0 ) {
        return;
    }

    if(args->seg_len==0)
        return;

    uint32_t* wptr = backbuffer_wptr(args->top, args->left);

     // pixel set, or not set
    uint32_t colour_lut[2] = {args->bg_colour, args->colour};
    
    size_t n = args->seg_offs;
    size_t end = args->seg_offs+args->seg_len;
    while(n < end) {

        wchar_t c = text[n];
        size_t gyph_offset = (size_t)((c & 0xff) << 3);
        size_t line = 0;
        uint32_t* line_ptr = wptr;
        while(line < 8) {

            uint8_t pixels = args->font_ptr[gyph_offset + line];
            switch(pixels) {
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
                    while(index<8) {
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

void video_draw_text(draw_text_segment_args_t* args, const wchar_t* text) {
    args->seg_len = wcslen(text);
    if ( !args || !text || args->seg_len==0 ) {
        return;
    }
    video_draw_text_segment(args, text);
}

void video_scroll_up_region_full_width(size_t top, size_t bottom, size_t linesToScroll) {

    size_t strip_pixel_stride = linesToScroll * _info.pixels_per_scan_line;
    uint32_t* wptr = backbuffer_wptr(top,0);
    uint32_t* rptr = wptr + strip_pixel_stride;

    const size_t region_height = bottom - top;
    const size_t strips = (region_height/linesToScroll);
    const size_t rem_lines = region_height % linesToScroll;
    for(size_t strip = 0; strip < strips; ++strip) {
        memcpy(wptr, rptr, strip_pixel_stride<<2);
        wptr += strip_pixel_stride;
        rptr += strip_pixel_stride;
    }

    if (rem_lines) {
        memcpy(wptr, rptr, rem_lines*(_info.pixels_per_scan_line<<2));
    }
}

void video_scale_draw_bitmap(const uint32_t* bitmap, size_t src_width, size_t src_height, size_t dest_top, size_t dest_left, size_t dest_width, size_t dest_height) {

    //TODO: need asserts...
    if ( !bitmap || !src_width || !src_height || !dest_width || !dest_height ) {
        return;
    }

    if ( src_width == dest_width && src_height == dest_height ) {

        // plain copy
        uint32_t* wptr = backbuffer_wptr(dest_top, dest_left);
        while(dest_height) {
            memcpy(wptr, bitmap, dest_width<<2);
            wptr += _info.pixels_per_scan_line;
            bitmap += dest_width;
            --dest_height;
        }
    }
    else
    {
        //TODO:
    }
}

void video_scale_draw_indexed_bitmap(const uint8_t* bitmap, const uint32_t* colourmap, size_t colourmap_size, 
                                    size_t src_width, size_t src_height, 
                                    size_t dest_top, size_t dest_left, size_t dest_width, size_t dest_height) {
    //TODO: need asserts...
    // if ( !bitmap || !colourmap || !colourmap_size || !src_width || !src_height || !dest_width || !dest_height ) {
    //     return;
    // }

    uint32_t* wptr = backbuffer_wptr(dest_top, dest_left);
    
    if ( src_width == dest_width && src_height == dest_height ) {

        // plain copy        
        while(src_height) {
            for(size_t p = 0; p < src_width; ++p)
            {
                //TODO: assert on index range (in DEBUG)
                wptr[p] = colourmap[*bitmap++];
            }
            wptr += _info.pixels_per_scan_line;
            --src_height;
        }
    }
    else if ( src_width == dest_width && src_height < dest_height ) {

        // scale up, height
        while(src_height) {
            for(size_t p = 0; p < src_width; ++p)
            {
                //TODO: assert on index range (in DEBUG)
                wptr[p] = colourmap[*bitmap++];
            }
            wptr += _info.pixels_per_scan_line;
            --src_height;
        }
    }
    else 
    {
        //TODO:
    }
}
