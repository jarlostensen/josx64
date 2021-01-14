#ifndef _JOS_KERNEL_KEYBOARD_H
#define _JOS_KERNEL_KEYBOARD_H

#include <stdint.h>

#define _JOS_K_VK_ESCAPE        0x01
// 0..9 are mapped to '0'..'9' ASCII
// similar for a..z and A..Z

typedef struct _keyboard_state {
    uint8_t     lshift:1;
    uint8_t     rshift:1;
    uint8_t     lctrl:1;
    uint8_t     rctrl:1;
    uint8_t     lalt:1;
    uint8_t     ralt:1;
    uint8_t     caps:1;
} keyboard_state_t;

#define KEYBOARD_VK_BACKSPACE       0x10
#define KEYBOARD_VK_TAB             0x11
#define KEYBOARD_VK_LF              0x12
#define KEYBOARD_VK_CR              0x15
#define KEYBOARD_VK_ESC             0x1b
#define KEYBOARD_VK_INS             0x81
#define KEYBOARD_VK_DEL             0x82
#define KEYBOARD_VK_HOME            0x83
#define KEYBOARD_VK_END             0x84
#define KEYBOARD_VK_PGUP            0x85
#define KEYBOARD_VK_PGDN            0x86
#define KEYBOARD_VK_UP              0x87
#define KEYBOARD_VK_DOWN            0x88
#define KEYBOARD_VK_LEFT            0x89
#define KEYBOARD_VK_RIGHT           0x90
#define KEYBOARD_VK_F1              0x91
#define KEYBOARD_VK_F2              0x92
#define KEYBOARD_VK_F3              0x93
#define KEYBOARD_VK_F4              0x94
#define KEYBOARD_VK_F5              0x95
#define KEYBOARD_VK_F6              0x96
#define KEYBOARD_VK_F7              0x97
#define KEYBOARD_VK_F8              0x98
#define KEYBOARD_VK_F9              0x99
#define KEYBOARD_VK_F10             0xa0
#define KEYBOARD_VK_F11             0xa1
#define KEYBOARD_VK_F12             0xa2

#define KEYBOARD_VK_PRESSED(vk)   ((vk) & 0x80000000)
#define KEYBOARD_VK_CHAR(vk)      ((vk)&0xffff)

void        keyboard_initialise(void);
bool        keyboard_has_key(void);
void        keyboard_get_state(keyboard_state_t* state);
uint32_t    keyboard_get_last_key(void);

#endif // _JOS_KERNEL_KEYBOARD_H

