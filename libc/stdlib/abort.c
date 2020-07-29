#include <stdio.h>
#include <stdlib.h>

#include <kernel/kernel.h>

__attribute__((__noreturn__)) void abort(void) {
	//TODO: to stderr
	printf("abort()\n");
	k_panic();
}