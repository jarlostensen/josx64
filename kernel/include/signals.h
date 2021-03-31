#pragma once

#include <jos.h>

typedef void* signal_t;

signal_t    signal_create();
void        signal_signal(signal_t signal);
bool        signal_is_signalled(signal_t signal);
void        signal_clear(signal_t signal);

