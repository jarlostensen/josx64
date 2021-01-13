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
} keyboard_state_t;

void        keyboard_initialise(void);
bool        keyboard_has_key(void);
void        keyboard_get_state(keyboard_state_t* state);

uint8_t     keyboard_TESTING_get_last_key(void);
wchar_t     keyboard_get_last_key(void);

#endif // _JOS_KERNEL_KEYBOARD_H