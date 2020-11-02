
#include <c-efi.h>
#include <c-efi-protocol-graphics-output.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "include/kernel/video.h"

// in efi_main.c
extern CEfiBootServices * g_boot_services;
extern CEfiSystemTable  * g_st;

static CEfiGraphicsOutputProtocol * _gop = 0;

#define VIDEO_MAX_HORIZ_RES 1024

// copy of active mode info
CEfiGraphicsOutputModeInformation _info;
static size_t _framebuffer_base = 0;
static size_t _red_shift;
static size_t _green_shift;
static size_t _blue_shift;

CEfiStatus video_initialise() 
{
    memset(&_info, 0, sizeof(_info));

    //ZZZ: may need to be bigger or code needs to handle "buffer too-small" error
    CEfiHandle handle_buffer[3];
    CEfiUSize handle_buffer_size = sizeof(handle_buffer);
    memset(handle_buffer,0,sizeof(handle_buffer));

    wchar_t wbuffer[128];

    CEfiStatus status = g_boot_services->locate_handle(C_EFI_BY_PROTOCOL, &C_EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, 0, &handle_buffer_size, handle_buffer);
    if ( status==C_EFI_SUCCESS ) {

        size_t num_handles = handle_buffer_size/sizeof(CEfiHandle);
        for(size_t n = 0; n < num_handles; ++n)
        {
            status = g_boot_services->handle_protocol(handle_buffer[n], &C_EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, (void**)&_gop);
            if ( status == C_EFI_SUCCESS )
            {
                break;
            }            
        }
        
        if ( status == C_EFI_SUCCESS ) {

            CEfiU32 max_horiz_res = 0;
            CEfiU32 mode = 0;
            CEfiI32 found_mode = -1;

            CEfiUSize size_of_info = 0;
            CEfiGraphicsOutputModeInformation* info;
            
            CEfiStatus status =_gop->query_mode(_gop, mode, &size_of_info, &info);
            while(status==C_EFI_SUCCESS) {
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
                status = _gop->query_mode(_gop, mode, &size_of_info, &info);
            }

            if ( found_mode >= 0 ) {
                // switch to the mode we've found 
                status = _gop->set_mode(_gop, found_mode);
                if ( status == C_EFI_SUCCESS ) {
                                 
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

    return status;
}

uint32_t video_make_color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << _red_shift) | ((uint32_t)g << _green_shift) | ((uint32_t)b << _blue_shift);
}

void video_clear_screen(uint32_t colour) {
    //TODO: assert(_framebuffer_base!=0)
    uint32_t *wptr = (uint32_t*)_framebuffer_base;
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