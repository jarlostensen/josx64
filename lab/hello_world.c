
#include <josx64.h>
#include <tasks.h>
#include <output_console.h>
#include <clock.h>
#include <keyboard.h>
#include <video.h>
#include <memory.h>

#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <stddef.h>
#include <stdio.h>

#include "programs/scroller.h"

static bool _read_input(void) {
    wchar_t buf[128];
    if ( keyboard_has_key() ) {
        uint32_t key = keyboard_get_last_key();
        if ( KEYBOARD_VK_PRESSED(key) ) {
            short c = (short)KEYBOARD_VK_CHAR(key);
            
                switch(c) {
                case KEYBOARD_VK_ESC:
                    output_console_output_string_w(L"\ngot ESC\n");
                    return true;
                case KEYBOARD_VK_RIGHT:
                    output_console_output_string_w(L" -> ");
                    break;
                case KEYBOARD_VK_LEFT:
                    output_console_output_string_w(L" <- ");
                    break;
                case KEYBOARD_VK_UP:
                    output_console_output_string_w(L" ^ ");
                    break;
                case KEYBOARD_VK_DOWN:
                    output_console_output_string_w(L" v ");
                    break;
                case KEYBOARD_VK_BACKSPACE:
                    output_console_output_string_w(L"bs");
                    break;
                case KEYBOARD_VK_CR:
                    output_console_line_break();
                    break;
                case KEYBOARD_VK_F1:
                    output_console_output_string_w(L" F1 ");
                    break;
                case KEYBOARD_VK_F2:
                    output_console_output_string_w(L" F2 ");
                    break;
                case KEYBOARD_VK_F3:
                    output_console_output_string_w(L" F3 ");
                    break;
                case KEYBOARD_VK_F4:
                    output_console_output_string_w(L" F4 ");
                    break;
                case KEYBOARD_VK_F5:
                    output_console_output_string_w(L" F5 ");
                    break;
                case KEYBOARD_VK_F6:
                    output_console_output_string_w(L" F6 ");
                    break;
                case KEYBOARD_VK_F7:
                    output_console_output_string_w(L" F7 ");
                    break;
                case KEYBOARD_VK_F8:
                    output_console_output_string_w(L" F8 ");
                    break;
                case KEYBOARD_VK_F9:
                    output_console_output_string_w(L" F9 ");
                    break;
                case KEYBOARD_VK_F10:
                    output_console_output_string_w(L" F10 ");
                    break;
                case KEYBOARD_VK_F11:
                    output_console_output_string_w(L" F11 ");
                    break;
                case KEYBOARD_VK_F12:
                    output_console_output_string_w(L" F12 ");
                    break;
                default: {
                    swprintf(buf, 128, L"0x%x ", key);
                    output_console_output_string_w(buf);                
                }
                break;
            }                
        }
    }
    return false;
}

static jo_status_t scroller_task(void* ptr) {
    (void)ptr;
    _JOS_KTRACE_CHANNEL("scroller_task", "starting");

    scroller_initialise(&(rect_t){
        .top = 250,
        .left = 32,
        .bottom = 400,
        .right = 632
    });

    uint64_t t0 = clock_ms_since_boot();     
    while (true) {
        uint64_t t1 = clock_ms_since_boot();
        if (t1 - t0 >= 33) {
            t0 = t1;
            scroller_render_field();            
        }
        tasks_yield();
    }
}

static jo_status_t main_task(void* ptr) {
    
    _JOS_KTRACE_CHANNEL("main_task", "starting");
    output_console_output_string_w(L"any key or ESC...\n");
    
    tasks_create(&(task_create_args_t) {
        .func = scroller_task,
        .pri = kTaskPri_Normal,
        .name = "scroller_task"
    });
    
    do {
        
        tasks_yield();

    } while(!_read_input());

    output_console_line_break();
    output_console_set_colour(video_make_color(0xff,0,0));
    wchar_t buf[128];
    swprintf(buf,128,L"\nmain task is done @ %dms\n", clock_ms_since_boot());
    output_console_output_string_w(buf);
    
    _JOS_KTRACE_CHANNEL("main_task", "terminating %llu ms after boot", clock_ms_since_boot());    
    
    return _JO_STATUS_SUCCESS;
}

int main(int argc, char* argv[]) {

    // tasks_create(&(task_create_args_t){
    //      .func = main_task,
    //         .pri = kTaskPri_Normal,
    //         .name = "main_task"
    // });

    printf("**************THIS IS MAIN!!!\n");
    _memory_debugger_dump_map();

    return 0;
}
