#pragma once

#ifndef _JOS_KERNEL_OUTPUT_CONSOLE_H
#define _JOS_KERNEL_OUTPUT_CONSOLE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../jos.h"

typedef struct _console_output_driver
{
	// assumes stride in bytes
	void (*_blt)(void* src, size_t start_line, size_t stride, size_t width, size_t lines);
	void (*_clear)(uint16_t value);	
} console_output_driver_t;

typedef struct _output_console
{
	uint16_t		_rows;
	uint16_t		_columns;
	int16_t			_start_row;
	int16_t			_col;
	int16_t			_row;
	uint16_t*		_buffer;
	uint8_t			_attr;
	console_output_driver_t* _driver;

} output_console_t;

extern output_console_t _stdout;

//ZZZ: naming convention, these are not using the k_ prefix...should they?

void output_console_init(void);

void output_console_set_fg(output_console_t* con, uint8_t fgcol);
void output_console_set_bg(output_console_t* con, uint8_t bgcol);

inline void output_console_create(output_console_t* con)
{
	_JOS_ASSERT(con);
	_JOS_ASSERT(con->_rows);
	_JOS_ASSERT(con->_columns);
	const int buffer_byte_size = con->_rows*con->_columns*sizeof(uint16_t);
	con->_buffer = (uint16_t*)malloc(buffer_byte_size);
	_JOS_ASSERT(con->_buffer);
	con->_start_row = 0;
	con->_col = con->_row = 0;
	memset(con->_buffer, 0, buffer_byte_size);
}

inline void output_console_destroy(output_console_t* con)
{
	if(!con || !con->_rows || !con->_columns || !con->_buffer)
		return;

	free(con->_buffer);
	memset(con,0,sizeof(output_console_t));
}

void output_console_flush(output_console_t* con);

inline void output_console_println(output_console_t* con)
{
	_JOS_ASSERT(con);
	_JOS_ASSERT(con->_buffer);
	con->_row = (con->_row+1) % con->_rows;
	if(con->_row == con->_start_row)
	{
		con->_start_row = (con->_start_row+1) % con->_rows;
	}
	con->_col = 0;	
}

void output_console_print(output_console_t* con, const char* line);

#endif // _JOS_KERNEL_OUTPUT_CONSOLE_H