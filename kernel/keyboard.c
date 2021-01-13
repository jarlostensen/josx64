

#include <jos.h>
#include <interrupts.h>
#include <x86_64.h>
#include <stdint.h>

//ZZZ:
#include <stdio.h>
#include <output_console.h>

// see for example https://wiki.osdev.org/Keyboard

enum _kbd_encoder_ports {
    kKbdEncoder_Port    = 0x60,
};

enum _kbd_controller_ports {
    kKbdController_Port = 0x64,
};

#define KBD_CONTROLLER_WRITE(cmd)   x86_64_outb(kKbdController_Port, cmd)
#define KBD_CONTROLLER_READ()       x86_64_inb(kKbdController_Port)
#define KBD_ENCODER_READ()          x86_64_inb(kKbdEncoder_Port)

#define KBD_NOT_A_KEY (wchar_t)0
static wchar_t _last_key_pressed = KBD_NOT_A_KEY;
static uint8_t _last_scancode = 0;

static void _irq_1_handler(int irqNum) {

    (void)irqNum;

    uint8_t controller_status = KBD_CONTROLLER_READ();
    // check the controller, is there anything to read from the output buffer?
    if ( controller_status & 1 ) {

        // get scan code from the encoder's output buffer
        uint8_t scan_code = KBD_ENCODER_READ();
        bool key_down = (scan_code & 0x80) == 0;

        if (!key_down) {
            //ZZZ: simplistic
            _last_key_pressed = KBD_NOT_A_KEY;
        }
        else {
            // digits
            if ( scan_code > 0x01 && scan_code < 0x0c ) {
                if ( scan_code == 0x0b ) {
                    _last_key_pressed = L'0';
                }
                else {
                    _last_key_pressed = L'0' + scan_code;
                }
            }            

            _last_scancode = scan_code;
        }
    }
}

void keyboard_initialise(void) {
    interrupts_set_irq_handler(0x01, _irq_1_handler);
    interrupts_PIC_enable_irq(0x01);
}

bool        keyboard_has_key(void) {
    return _last_scancode != 0;
}

uint8_t     keyboard_TESTING_get_last_key(void) {
    uint8_t sc = _last_scancode;
    //NOTE: thread safe, we need a spinlock here
    _last_scancode = 0;
    return sc;
}
