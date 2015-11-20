
/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.

 */
 
 /**
 * @file RF24_arch_config.h
 * General defines and includes for RF24/Linux
 */

 /**
 * Example of RF24_arch_config.h for RF24 portability
 *
 * @defgroup Porting_General Porting: General
 *
 * 
 * @{
 */
 
 
#ifndef __ARCH_CONFIG_H__
#define __ARCH_CONFIG_H__

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ch.h"
#include "hal.h"
#include "rf24-chibios-io.h"

#define RF24_IO Rf24ChibiosIo

#define _BV(x) (1<<(x))
#define RF24_SPI spi

#undef SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define IF_SERIAL_DEBUG(x) ({x;})
#else
#define IF_SERIAL_DEBUG(x)
#endif

// Avoid spurious warnings
#if 1
#if ! defined( NATIVE ) && defined( ARDUINO )
#undef PROGMEM
#define PROGMEM __attribute__(( section(".progmem.data") ))
#undef PSTR
#define PSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))
#endif
#endif

typedef uint16_t prog_uint16_t;
#define PSTR(x) (x)
#define printf_P printf
#define strlen_P strlen
#define PROGMEM
#define pgm_read_word(p) (*(p))
#define PRIPSTR "%s"

static inline uint8_t pgm_read_byte(const uint8_t *p) {
    return *p;
}

static inline void delay(uint32_t milisec) {
	chThdSleepMilliseconds(milisec);
}

static inline void delayMicroseconds(uint32_t usec) {
	chThdSleepMicroseconds(usec);
}

static inline long millis() {
	return ST2MS(chVTGetSystemTime());
}

static inline void printf(const char *fmt, ...) { (void)fmt; }

#define LOW PAL_LOW
#define HIGH PAL_HIGH
#define INPUT PAL_MODE_INPUT
#define OUTPUT PAL_MODE_OUTPUT_PUSHPULL

#endif // __ARCH_CONFIG_H__


/*@}*/	
