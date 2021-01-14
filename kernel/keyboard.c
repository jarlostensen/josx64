
#include <jos.h>
#include <kernel.h>
#include <interrupts.h>
#include <x86_64.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>

static lock_t _keyboard_buffer_lock;
#define LOCK_BUFFER() lock_spinlock(&_keyboard_buffer_lock)
#define UNLOCK_BUFFER() lock_unlock(&_keyboard_buffer_lock)

#include <keyboard.h>

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

#define KBD_BUFFER_SIZE 32
static uint8_t _keyboard_buffer[KBD_BUFFER_SIZE];
static size_t _keys_in_buffer = 0;
static keyboard_state_t  _keyboard_state;
static bool _extended_code = false;

// used in the LUT to indicate a key that should be intercepted by the IRQ handler.
#define KEYBOARD_VK_INVALID 0xff

// ======================================================================
// NOTE: THIS CODE USES SET 1 SCAN CODES ONLY
// see for example https://www.win.tue.nl/~aeb/linux/kbd/scancodes-10.html
//
// [scancode][normal,cap]
static char _xt_set1[126][2] = {
    {0,0},
    {KEYBOARD_VK_ESC,KEYBOARD_VK_ESC},    
    {'1','!'},
    {'2','@'},
    {'3','#'},
    {'4','$'},
    {'5','%'},
    {'6','^'},
    {'7','&'},
    {'8','*'},
    {'9','('},
    {'0',')'},
    {'-','_'},
    {'=','+'},
    {KEYBOARD_VK_BACKSPACE,KEYBOARD_VK_BACKSPACE},
    {KEYBOARD_VK_TAB,KEYBOARD_VK_TAB},
    {'q','Q'},
    {'w','W'},
    {'e','E'},
    {'r','R'},
    {'t','T'},
    {'y','Y'},
    {'u','U'},
    {'i','I'},
    {'o','O'},
    {'p','P'},
    {'[','{'},
    {']','}'},
    {'\\','|'},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {'a','A'},
    {'s','S'},
    {'d','D'},
    {'f','F'},
    {'g','G'},
    {'h','H'},
    {'j','J'},
    {'k','K'},
    {'l','L'},
    {';',':'},
    {'`','~'},
    {'\'','"'},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_CR,KEYBOARD_VK_CR},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {'z','Z'},
    {'x','X'},
    {'c','C'},
    {'v','V'},
    {'b','B'},
    {'n','N'},
    {'m','M'},
    {',','<'},
    {'.','>'},
    {'/','?'},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {' ',' '},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INS,KEYBOARD_VK_INS},
    {KEYBOARD_VK_DEL,KEYBOARD_VK_DEL},
    {KEYBOARD_VK_HOME,KEYBOARD_VK_HOME},
    {KEYBOARD_VK_END,KEYBOARD_VK_END},
    {KEYBOARD_VK_PGUP,KEYBOARD_VK_PGUP},
    {KEYBOARD_VK_PGDN,KEYBOARD_VK_PGDN},
    {KEYBOARD_VK_LEFT,KEYBOARD_VK_LEFT},
    {KEYBOARD_VK_UP,KEYBOARD_VK_UP},
    {KEYBOARD_VK_DOWN,KEYBOARD_VK_DOWN},
    {KEYBOARD_VK_RIGHT,KEYBOARD_VK_RIGHT},

//TODO:
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},    
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    
    {KEYBOARD_VK_ESC, KEYBOARD_VK_ESC},
    {KEYBOARD_VK_F1,KEYBOARD_VK_F1},
    {KEYBOARD_VK_F2,KEYBOARD_VK_F2},
    {KEYBOARD_VK_F3,KEYBOARD_VK_F3},
    {KEYBOARD_VK_F4,KEYBOARD_VK_F4},
    {KEYBOARD_VK_F5,KEYBOARD_VK_F5},
    {KEYBOARD_VK_F6,KEYBOARD_VK_F6},
    {KEYBOARD_VK_F7,KEYBOARD_VK_F7},
    {KEYBOARD_VK_F8,KEYBOARD_VK_F8},
    {KEYBOARD_VK_F9,KEYBOARD_VK_F9},
    {KEYBOARD_VK_F10,KEYBOARD_VK_F10},
    {KEYBOARD_VK_F11,KEYBOARD_VK_F11},
    {KEYBOARD_VK_F12,KEYBOARD_VK_F12},

//TODO:
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},
};

// this handler doesn't do much scan code translation except for alt,ctrl,shift
static void _irq_1_handler(int irqNum) {

    (void)irqNum;

    if(_keys_in_buffer == KBD_BUFFER_SIZE) {
        //TODO: "beep"..?
        return;
    }

    uint8_t controller_status = KBD_CONTROLLER_READ();
    // check the controller, is there anything to read from the output buffer?
    if ( controller_status & 1 ) {

        // get scan code from the encoder's output buffer and just buffer it
        uint8_t scan_code = KBD_ENCODER_READ();
        if ( scan_code == 0x0e ) {
            // extended scan code, next scan code will be the actual key
            _extended_code = true;
            return;
        }

        uint8_t pressed = 1 ^ ((scan_code & 0x80)>>7);
        switch(scan_code & 0x3f) {
            case 0x1d:
            if ( _extended_code )
                _keyboard_state.rctrl = pressed;
            else
                _keyboard_state.lctrl = pressed;
            break;
        case 0x6a:
            if ( _extended_code )
                _keyboard_state.ralt = pressed;
            else
                _keyboard_state.lalt = pressed;
            break;
        case 0x2a:
            if ( _extended_code )
                _keyboard_state.rshift = pressed;
            else
                _keyboard_state.lshift = pressed;
            break;
        case 0x3a:
            _keyboard_state.caps ^= 1;
            break;
        //TODO: process more special keys
        //TODO: including more extended keys
        default:
        {
            // everything else we just store in the buffer
            LOCK_BUFFER();
            _keyboard_buffer[_keys_in_buffer++] = scan_code;
            UNLOCK_BUFFER();
        }
        break;
        }

        _extended_code = false;
    }
}

void keyboard_initialise(void) {
    memset(&_keyboard_state , 0, sizeof(_keyboard_state));
    lock_initialise(&_keyboard_buffer_lock);
    interrupts_set_irq_handler(0x01, _irq_1_handler);
    interrupts_PIC_enable_irq(0x01);
}

void        keyboard_get_state(keyboard_state_t* state) {
    memcpy(state, &_keyboard_state, sizeof(_keyboard_state));
}

bool        keyboard_has_key(void) {
    LOCK_BUFFER();
    bool has_keys = _keys_in_buffer>0;
    UNLOCK_BUFFER();
    return has_keys;
}

#define MAKE_VK(pressed, character) ((((uint32_t)pressed)<<31) | (uint32_t)character)
uint32_t     keyboard_get_last_key(void) {
    uint8_t sc = 0;
    LOCK_BUFFER();    
    if (_keys_in_buffer) {
         sc = _keyboard_buffer[--_keys_in_buffer];
    }
    UNLOCK_BUFFER();

    if (sc) {
        uint8_t pressed = 1 ^ ((sc & 0x80)>>7);
        sc &= 0x3f;
        //TODO: assert(_xt_set1[sc][_keyboard_state.caps | _keyboard_state.lshift | _keyboard_state.rshift]]!=KEYBOARD_VK_INVALID)
        return MAKE_VK(pressed, _xt_set1[sc][_keyboard_state.caps | _keyboard_state.lshift | _keyboard_state.rshift]);
    }

    return 0;
}

