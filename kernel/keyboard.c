
#include <jos.h>
#include <kernel.h>
#include <interrupts.h>
#include <x86_64.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>

#include <keyboard.h>

enum _kbd_encoder_ports {
    kKbdEncoder_Port    = 0x60,
};

enum _kbd_controller_ports {
    kKbdController_Port = 0x64,
};

enum _kbd_encoder_status_codes {
    kKbdEncoder_Ack     = 0xfa,
    kKbdEncoder_Resend  = 0xfe,
};

#define KBD_CONTROLLER_WRITE(cmd)   x86_64_outb(kKbdController_Port, cmd)
#define KBD_CONTROLLER_READ()       x86_64_inb(kKbdController_Port)
#define KBD_ENCODER_READ()          x86_64_inb(kKbdEncoder_Port)

#define KBD_NOT_A_KEY (wchar_t)0
// mask for scan code without break code
#define SCAN_CODE_MAKE_MASK 0x7f

#define KBD_BUFFER_SIZE 32
static uint8_t _keyboard_buffer[KBD_BUFFER_SIZE];
static size_t _keys_in_buffer = 0;
static keyboard_state_t  _keyboard_state;
static bool _extended_code = false;
static short _keyboard_id_code = 0;

static lock_t _keyboard_buffer_lock;
#define LOCK_BUFFER() lock_spinlock(&_keyboard_buffer_lock)
#define UNLOCK_BUFFER() lock_unlock(&_keyboard_buffer_lock)

// used in the LUT to indicate a key that should be intercepted by the IRQ handler.
#define KEYBOARD_VK_INVALID 0xff


// ======================================================================
// NOTE: THIS CODE USES SET 1 SCAN CODES ONLY
// see for example https://www.win.tue.nl/~aeb/linux/kbd/scancodes-10.html
// or 
//  https://wiki.osdev.org/Keyboard
//
// [scancode][normal,cap]
static char _xt_set1[126][2] = {
    {0,0},                                                                      // 0x00
    {KEYBOARD_VK_ESC,KEYBOARD_VK_ESC},                                          // 0x01
    {'1','!'},                                                                  // 0x02
    {'2','@'},                                                                  // 0x03
    {'3','#'},                                                                  // 0x04
    {'4','$'},                                                                  // 0x05
    {'5','%'},                                                                  // 0x06
    {'6','^'},                                                                  // 0x07
    {'7','&'},                                                                  // 0x08
    {'8','*'},                                                                  // 0x09
    {'9','('},                                                                  // 0x0a
    {'0',')'},                                                                  // 0x0b
    {'-','_'},                                                                  // 0x0c
    {'=','+'},                                                                  // 0x0d
    {KEYBOARD_VK_BACKSPACE,KEYBOARD_VK_BACKSPACE},                              // 0x0e
    {KEYBOARD_VK_TAB,KEYBOARD_VK_TAB},                                          // 0x0f
    {'q','Q'},                                                                  // 0x10
    {'w','W'},                                                                  // 0x11
    {'e','E'},                                                                  // 0x12
    {'r','R'},                                                                  // 0x13
    {'t','T'},                                                                  // 0x14
    {'y','Y'},                                                                  // 0x15
    {'u','U'},                                                                  // 0x16
    {'i','I'},                                                                  // 0x17
    {'o','O'},                                                                  // 0x18
    {'p','P'},                                                                  // 0x19
    {'[','{'},                                                                  // 0x1a
    {']','}'},                                                                  // 0x1b
    {KEYBOARD_VK_CR,KEYBOARD_VK_CR},                                            // 0x1c
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},                                  // 0x1d lctrl
    {'a','A'},                                                                  // 0x1e
    {'s','S'},                                                                  // 0x1f
    {'d','D'},                                                                  // 0x20
    {'f','F'},                                                                  // 0x21
    {'g','G'},                                                                  // 0x22
    {'h','H'},                                                                  // 0x23
    {'j','J'},                                                                  // 0x24
    {'k','K'},                                                                  // 0x25
    {'l','L'},                                                                  // 0x26
    {';',':'},                                                                  // 0x27    
    {'\'','@'},                                                                 // 0x28
    {'`','~'},                                                                  // 0x29
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},                                  // 0x2a lshift
    {'\\','|'},                                                                 // 0x2b
    {'z','Z'},                                                                  // 0x2c
    {'x','X'},                                                                  // 0x2d
    {'c','C'},                                                                  // 0x2e
    {'v','V'},                                                                  // 0x2f
    {'b','B'},                                                                  // 0x30
    {'n','N'},                                                                  // 0x31
    {'m','M'},                                                                  // 0x32
    {',','<'},                                                                  // 0x33
    {'.','>'},                                                                  // 0x34
    {'/','?'},                                                                  // 0x35
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},                                  // 0x36 rshift
    {'*','*'},                                                                  // 0x37
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},                                  // 0x38 lalt
    {' ',' '},                                                                  // 0x39
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},                                  // 0x3a caps
    {KEYBOARD_VK_F1,KEYBOARD_VK_F1},                                            // 0x3b
    {KEYBOARD_VK_F2,KEYBOARD_VK_F2},                                            // 0x3c
    {KEYBOARD_VK_F3,KEYBOARD_VK_F3},                                            // 0x3d
    {KEYBOARD_VK_F4,KEYBOARD_VK_F4},                                            // 0x3e
    {KEYBOARD_VK_F5,KEYBOARD_VK_F5},                                            // 0x3f
    {KEYBOARD_VK_F6,KEYBOARD_VK_F6},                                            // 0x40
    {KEYBOARD_VK_F7,KEYBOARD_VK_F7},                                            // 0x41
    {KEYBOARD_VK_F8,KEYBOARD_VK_F8},                                            // 0x42
    {KEYBOARD_VK_F9,KEYBOARD_VK_F9},                                            // 0x43
    {KEYBOARD_VK_F10,KEYBOARD_VK_F10},                                          // 0x44

    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},                                  // 0x45 NumLock
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},                                  // 0x46 ScrollLock

    // keypad
    {'7','7'},                                                                  // 0x47
    {'8','8'},                                                                  // 0x48
    {'9','9'},                                                                  // 0x49
    {'-','-'},                                                                  // 0x4a
    {'4','4'},                                                                  // 0x4b
    {'5','5'},                                                                  // 0x4c
    {'6','6'},                                                                  // 0x4d
    {'+','+'},                                                                  // 0x4e
    {'1','1'},                                                                  // 0x4f
    {'2','2'},                                                                  // 0x50
    {'3','3'},                                                                  // 0x51
    {'0','0'},                                                                  // 0x52
    {'.','.'},                                                                  // 0x53

    // not used
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},                                  // 0x54
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},                                  // 0x55
    {KEYBOARD_VK_INVALID,KEYBOARD_VK_INVALID},                                  // 0x56

    {KEYBOARD_VK_F11,KEYBOARD_VK_F11},                                          // 0x57
    {KEYBOARD_VK_F12,KEYBOARD_VK_F12},                                          // 0x58

    //THE REST IS RELEASE AND EXTENDED VARIANTS OF THE ABOVE
};

static uint8_t _outb_keyboard_encoder(uint8_t cmd) {
    while(true) {
        // wait for the controller input buffer to be clear
        if((KBD_CONTROLLER_READ() & 0x02)==0) {
            break;
        }
    }
    x86_64_outb(kKbdEncoder_Port, cmd);
    x86_64_io_wait();
    // ack or other
    return KBD_ENCODER_READ();
}

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
        switch(scan_code & SCAN_CODE_MAKE_MASK) {
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

    //ZZZ: this does not appear to work very well

    // // set current scan code set
    // uint8_t ack = _outb_keyboard_encoder(0xf0);
    // if( ack == kKbdEncoder_Ack ) {
    //     ack = _outb_keyboard_encoder(1);
    // }

    // // get current scan code set
    uint8_t ack = _outb_keyboard_encoder(0xf0);
    if(ack==kKbdEncoder_Ack) {
        ack = _outb_keyboard_encoder(0);        
        if(ack==kKbdEncoder_Ack) {
            _keyboard_state.set = KBD_ENCODER_READ();        
        }
    }
    // else ....TODO, error handling

    // get keyboard ID
    ack = _outb_keyboard_encoder(0xf2);
    if( ack == kKbdEncoder_Ack ) {
        _keyboard_id_code = (short)KBD_ENCODER_READ();
        _keyboard_id_code |= ((short)KBD_ENCODER_READ() << 16);
    }

    interrupts_set_irq_handler(0x01, _irq_1_handler);
    interrupts_PIC_enable_irq(0x01);    
}

void        keyboard_get_state(keyboard_state_t* state) {
    memcpy(state, &_keyboard_state, sizeof(_keyboard_state));
}

short       keyboard_get_id(void) {
    return _keyboard_id_code;
}

bool        keyboard_has_key(void) {
    LOCK_BUFFER();
    bool has_keys = _keys_in_buffer>0;
    UNLOCK_BUFFER();
    return has_keys;
}

#define MAKE_VK(scancode, character) ((((uint32_t)scancode)<<24) | (uint32_t)character)
uint32_t     keyboard_get_last_key(void) {
    uint8_t sc = 0;
    LOCK_BUFFER();    
    if (_keys_in_buffer) {
         sc = _keyboard_buffer[--_keys_in_buffer];
    }
    UNLOCK_BUFFER();

    if (sc) {
        //TODO: assert(_xt_set1[sc][_keyboard_state.caps | _keyboard_state.lshift | _keyboard_state.rshift]]!=KEYBOARD_VK_INVALID)
        return MAKE_VK(sc, _xt_set1[sc & SCAN_CODE_MAKE_MASK][_keyboard_state.caps | _keyboard_state.lshift | _keyboard_state.rshift]);
    }

    return 0;
}

