#pragma once

/**
 * UEFI Protocol - Graphics Output Protocol
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <c-efi-base.h>
#include <c-efi-system.h>

#define C_EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID C_EFI_GUID(0x9042a9de, 0x23dc, 0x4a38, 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a)

typedef enum CEfiGraphicsPixelFormat {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax,
} CEfiGraphicsPixelFormat;

typedef struct CEfiPixelBitmask {
    CEfiU32 red_mask;
    CEfiU32 blue_mask;
    CEfiU32 green_mask;
    CEfiU32 reserved_mask;
} CEfiPixelBitmask;

typedef struct CEfiGraphicsOutputModeInformation {
    CEfiU32 version;
    CEfiU32 horizontal_resolution;
    CEfiU32 vertical_resolution;
    CEfiGraphicsPixelFormat pixel_format;    
    CEfiPixelBitmask pixel_information;
    CEfiU32 pixels_per_scan_line;
} CEfiGraphicsOutputModeInformation;

typedef struct CEfiGraphicsOutputMode {
        CEfiI32 max_mode;
        CEfiI32 mode;
        CEfiGraphicsOutputModeInformation* info;
        CEfiUSize size_of_info;
        CEfiU64 frame_buffer_base;
        CEfiUSize frame_buffer_size;
} CEfiGraphicsOutputMode;

typedef enum CEfiGraphicsOutputBltOperation {
    BltVideoFill,
    BltVideoToBltBuffer,
    BltBufferToVideo,
    BltVideoToVideo,
    GraphicsOutputBltOperationMax, 
} CEfiGraphicsOutputBltOperation;

typedef struct GraphicsOutputBltPixel {
    CEfiU8  blue;
    CEfiU8  green;
    CEfiU8  red;
    CEfiU8  reserved;
} GraphicsOutputBltPixel;

typedef struct CEfiGraphicsOutputProtocol {

    CEfiStatus (CEFICALL *query_mode) (
        struct CEfiGraphicsOutputProtocol* this_,
        CEfiU32 mode,
        CEfiUSize *size_of_info,
        CEfiGraphicsOutputModeInformation ** info
    );
    CEfiStatus (CEFICALL *set_mode) (
        struct CEfiGraphicsOutputProtocol* this_,
        CEfiU32 mode
    );
    CEfiStatus (CEFICALL *blt) (
        struct CEfiGraphicsOutputProtocol* this_,
        const GraphicsOutputBltPixel * blt_buffer,
        CEfiGraphicsOutputBltOperation blt_operation,
        CEfiUSize source_x,
        CEfiUSize source_y,
        CEfiUSize destination_x,
        CEfiUSize destination_y,
        CEfiUSize width,
        CEfiUSize height,
        CEfiUSize delta
    );

    CEfiGraphicsOutputMode * mode;

} CEfiGraphicsOutputProtocol;

#ifdef __cplusplus
}
#endif
