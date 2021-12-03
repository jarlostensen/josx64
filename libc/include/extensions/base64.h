/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 * 
 * With some modifications by Jarl Ostensen to integrate into josx64 kernel project
 */

#ifndef BASE64_H
#define BASE64_H

#include <jos.h>

unsigned char * base64_encode(const unsigned char *src, size_t len, size_t *out_len, generic_allocator_t* allocator);
unsigned char * base64_decode(const unsigned char *src, size_t len, size_t *out_len, generic_allocator_t* allocator);

#endif /* BASE64_H */
