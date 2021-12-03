#pragma once
#include <stdint.h>
#include <jos.h>

typedef struct _IO_FILE {

	// a string, for example
	struct {
		uint8_t* _begin;
		const uint8_t* _end;
		const uint8_t* _rp;
		uint8_t* _wp;
		size_t              _size;
	} _buffer;

	void* _pimpl;

	size_t  _pos;
	
	size_t(*read)(struct _IO_FILE* file, unsigned char*, size_t);
	size_t(*write)(struct _IO_FILE* file, const unsigned char*, size_t);
	void(*flush)(struct _IO_FILE* file);

} IO_FILE;


#define _io_file_fromstring(f, s)\
((f)->_buffer._begin = (f)->_buffer._rp = (const uint8_t*)(s), (f)->_buffer._end = (const uint8_t*)-1, (f)->_buffer._size = 0)

#define _io_file_getch(f) ((f)->_buffer._rp!=(f)->_buffer._end ? *(f)->_buffer._rp++ : 0)
#define _io_file_ungetch(f) ((f)->_buffer._rp!=(f)->_buffer._begin ? (f)->_buffer._rp-- ; (void)0)

#define _io_file_tobuffer(f, buffer, length)\
((f)->_buffer._rp = (void)0, (f)->_buffer._begin = (f)->_buffer._wp = (uint8_t*)(buffer), (f)->_buffer._end = ((f)->_buffer._wp + ((f)->_buffer._size = length)))

#define _io_file_from_buffer(f, b, l)\
((f)->_buffer._begin = (f)->_buffer._wp = (uint8_t*)(b), (f)->_buffer._rp = (const uint8_t*)(b), (f)->_buffer._end = ((const uint8_t*)(b)+l), (f)->_buffer._size = l, (f))

#define _io_file_update_pos(f, l)\
    (f)->_pos += l

_JOS_INLINE_FUNC int _fflush(IO_FILE* stream) {
    if (stream->_buffer._wp > stream->_buffer._begin) {
        stream->write(stream, stream->_buffer._begin, stream->_buffer._wp - stream->_buffer._begin);
        stream->_buffer._wp = stream->_buffer._begin;
    }
    return 0;
}

_JOS_INLINE_FUNC int _fwrite(const char* src, size_t elem_size, size_t elem_count, IO_FILE* stream) {
    size_t transferred = 0;
    size_t transfer_size = elem_size * elem_count;
    while (transfer_size) {
        size_t stream_buffer_avail = stream->_buffer._end - stream->_buffer._wp;
        size_t chunk_len = min(stream_buffer_avail, transfer_size);
        memcpy(stream->_buffer._wp, src, chunk_len);
        stream->_buffer._wp += chunk_len;
        transfer_size -= chunk_len;
        _io_file_update_pos(stream, chunk_len);
        transferred += chunk_len / elem_size;     //< number of items, not bytes
        if (transfer_size) {
            if (stream->flush) {
                stream->flush(stream);
            }
            else {
                // error; we'll return whatever count we've managed to transfer
                //TODO: errno
                break;
            }
        }
    }
    return (int)transferred;
}

_JOS_INLINE_FUNC int _fread(void* buffer, size_t elem_size, size_t elem_count, IO_FILE* stream) {
    char* wp = (char*)buffer;
    size_t transferred = 0;
    size_t transfer_size = elem_size * elem_count;
    size_t stream_buffer_avail = stream->_buffer._end - stream->_buffer._rp;
    size_t chunk_len = min(stream_buffer_avail, transfer_size);
    memcpy(wp, stream->_buffer._rp, chunk_len);
    stream->_buffer._rp += chunk_len;
    transfer_size -= chunk_len;
    transferred += chunk_len / elem_size;
    _io_file_update_pos(stream, chunk_len);
    //TODO: what to do if full?
    //NOTE: we either got what we asked for or we didn't (blocking?)
    return (int)transferred;
}

_JOS_INLINE_FUNC size_t _ftell(IO_FILE* stream) {
    return stream->_pos;
}

#ifdef _JO_BARE_METAL_BUILD
#define FILE IO_FILE
#define fread _fread
#define fwrite _fwrite
#define fflush _fflush
#define ftell _ftell
#endif

