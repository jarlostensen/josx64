
#include <kernel/io.h>
#include "serial.h"

static void init_port(short port)
{
    // https://wiki.osdev.org/Serial_Ports
    io_outb(port + 1, 0x00);    // Disable all interrupts
    io_outb(port + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    io_outb(port + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    io_outb(port + 1, 0x00);    //                  (hi byte)
    io_outb(port + 3, 0x03);    // 8 bits, no parity, one stop bit
    io_outb(port + 2, 0xc7);    // Enable FIFO, clear them, with 14-byte threshold
    io_outb(port + 4, 0x0b);    // IRQs enabled, RTS/DSR set
}

void serial_initialise()
{
    init_port(kCom1);
    // leave this for now init_port(kCom2);
}

int serial_data_ready(short port)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);
    // line status register, DR bit
    return io_inb(port + 5) & 1;
}

char serial_getch(short port, int wait)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);
    while(wait && !serial_data_ready(port))
    {
        asm volatile ("pause");
    }
    return io_inb(port);
}

int serial_transmit_empty(short port)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);    
    // line status register, transmitter empty bit
    return io_inb(port + 5) & 0x20;
}

void serial_putch(short port, char data, int wait)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);    
    while(wait && !serial_transmit_empty(port))
    {
        asm volatile ("pause");
    }
    io_outb(port, data);
}

void serial_flush(short port)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);    
    while(!serial_transmit_empty(port))
    {
        asm volatile ("pause");
    }
}

void serial_write(short port, const char* data, size_t len)
{
    _JOS_ASSERT(port==kCom1 || port==kCom2);
    while(len)
    {
        serial_putch(port, *data++, 1);
        len--;
    }
}
