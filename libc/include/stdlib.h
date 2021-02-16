#ifndef _JOS_STDLIB_H
#define _JOS_STDLIB_H

#include <sys/cdefs.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((__noreturn__))  void abort(void);

#ifdef NULL
#undef NULL
#define NULL (0)
#endif

//NOTE: 
// implemented in kernel::memory.c for now
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);


void srand(unsigned seed);
int rand(void);

#ifdef __cplusplus
}
#endif

#endif // _JOS_STDLIB_H
