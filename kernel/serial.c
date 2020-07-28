
#include "kernel_detail.h"
#include "serial.h"

static void init_port(short port)
{
    // https://wiki.osdev.org/Serial_Ports
    k_outb(port + 1, 0x00);    // Disable all interrupts
    k_outb(port + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    k_outb(port + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    k_outb(port + 1, 0x00);    //                  (hi byte)
    k_outb(port + 3, 0x03);    // 8 bits, no parity, one stop bit
    k_outb(port + 2, 0xc7);    // Enable FIFO, clear them, with 14-byte threshold
    k_outb(port + 4, 0x0b);    // IRQs enabled, RTS/DSR set
}

void k_serial_init()
{
    init_port(kCom1);
    // leave this for now init_port(kCom2);
}

int k_serial_data_ready(short port)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);
    // line status register, DR bit
    return k_inb(port + 5) & 1;
}

char k_serial_getch(short port, int wait)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);
    while(wait && !k_serial_data_ready(port))
    {
        k_pause();
    }
    return k_inb(port);
}

int k_serial_transmit_empty(short port)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);    
    // line status register, transmitter empty bit
    return k_inb(port + 5) & 0x20;
}

void k_serial_putch(short port, char data, int wait)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);    
    while(wait && !k_serial_transmit_empty(port))
    {
        k_pause();
    }
    k_outb(port, data);
}

void k_serial_flush(short port)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);    
    while(!k_serial_transmit_empty(port))
    {
        k_pause();
    }
}

void k_serial_write(short port, const char* data, size_t len)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);
    while(len)
    {
        k_serial_putch(port, *data++, 1);
        len--;
    }
}
