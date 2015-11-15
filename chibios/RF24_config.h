
/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

#ifndef __RF24_CONFIG_H__
#define __RF24_CONFIG_H__

#include <stddef.h>

// Stuff that is normally provided by Arduino
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "ch.h"

#undef SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define IF_SERIAL_DEBUG(x) ({x;})
#else
#define IF_SERIAL_DEBUG(x)
#endif

#define HIGH PAL_HIGH
#define LOW PAL_LOW
#define _BV(x) (1<<(x))

#endif // __RF24_CONFIG_H__
