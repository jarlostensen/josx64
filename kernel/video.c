#include <c-efi.h>
#include <stdbool.h>
#include <stdint.h>

// in efi_main.c
extern CEfiBootServices * g_boot_services;

#define VIDEO_MAX_HORIZ_RES 1024

// active mode info
static uint32_t mode = 0;


bool initialise_video() 
{

}


