#ifndef _JOS_KERNEL_VIDEO_H
#define _JOS_KERNEL_VIDEO_H

CEfiStatus video_initialise();
uint32_t video_make_color(uint8_t r, uint8_t g, uint8_t b);
void video_clear_screen(uint32_t colour);

#endif // _JOS_KERNEL_VIDEO_H