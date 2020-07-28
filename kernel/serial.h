#ifndef _JOS_SERIAL_H
#define _JOS_SERIAL_H

enum serial_port_enum
{
    kCom1 = 0x3f8,
    kCom2 = 0x2f8
};

void k_serial_init();
// returns non-0 if data is ready on port
int k_serial_data_ready(short port);
// read one 8 bit character from the port, if wait == 1 wait until ready
char k_serial_getch(short port, int wait);
// returns non-0 if port has transmitted all outstanding data
int k_serial_transmit_empty(short port);
// output one character to serial port, if wait == 1 block until ready
void k_serial_putch(short port, char data, int wait);
// (blocking) wait until the serial port has finished writing
void k_serial_flush(short port);
// write len bytes to port
void k_serial_write(short port, const char* data, size_t len);

#endif // _JOS_SERIAL_H