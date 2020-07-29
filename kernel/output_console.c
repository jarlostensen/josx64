#include <kernel/output_console.h>
#include "../arch/i386/vga.h"

output_console_t _stdout;
static console_output_driver_t _vga_driver;
static int _vga_width, _vga_height;

void output_console_init(void)
{
	vga_init();
	_vga_driver._blt = vga_blt;
	_vga_driver._clear = vga_clear_to_val;
	vga_display_size(&_vga_width, &_vga_height);

	_stdout._columns = 100;
	_stdout._rows = 60;
	_stdout._driver = &_vga_driver;
	_stdout._attr = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	output_console_create(&_stdout);
}

void output_console_set_fg(output_console_t* con, uint8_t fgcol)
{
	con->_attr &= 0xf0;
	con->_attr |= fgcol;
}

void output_console_set_bg(output_console_t* con, uint8_t bgcol)
{
	con->_attr &= 0x0f;
	con->_attr |= (bgcol << 4);
}

void output_console_flush(output_console_t* con)
{
	_JOS_ASSERT(con);
	_JOS_ASSERT(con->_buffer);
	const size_t input_stride = (size_t)con->_columns * 2;

	if (con->_start_row <= con->_row)
	{
		int lines_to_flush = min(con->_row + 1, _vga_height);
		const int flush_start_row = con->_row - min(_vga_height - 1, con->_row - con->_start_row);
		//NOTE: always flush from col 0
		con->_driver->_blt(con->_buffer + flush_start_row * con->_columns, 0, input_stride, (size_t)con->_columns, lines_to_flush);
	}
	else
	{
		const int lines_in_buffer = con->_row + (con->_rows - con->_start_row) + 1; //< +1 to convert _row to count
		int flush_row = con->_rows - (lines_in_buffer - con->_row - 1); //< as above
		//NOTE: we always flush from col 0
		uint16_t cc = flush_row * con->_columns;
		const size_t lines = con->_rows - flush_row;
		con->_driver->_blt(con->_buffer + cc, 0, input_stride, (size_t)con->_columns, lines);
		// next batch from the top
		con->_driver->_blt(con->_buffer, lines, input_stride, (size_t)con->_columns, con->_row + 1);
	}
}

void output_console_print(output_console_t* con, const char* line)
{
	_JOS_ASSERT(con);
	_JOS_ASSERT(con->_buffer);
	//TODO: horisontal scroll
	const int output_width = min(strlen(line), con->_columns);
	uint16_t cc = con->_row * con->_columns + con->_col;
	const uint16_t attr_mask = ((uint16_t)con->_attr << 8);
	for (int c = 0; c < output_width; ++c)
	{
		switch (line[c])
		{
		case '\n':
		{
			output_console_println(con);
			cc = con->_row * con->_columns;
		}
		break;
		case '\t':
		{
			//TODO: wrap...?
			int tabs = min(4, con->_columns - con->_col);
			while (tabs--)
			{
				con->_buffer[cc++] = attr_mask | ' ';
			}
		}
		break;
		default:
		{
			con->_buffer[cc++] = attr_mask | line[c];
			++con->_col;
		}
		break;
		}
	}
}