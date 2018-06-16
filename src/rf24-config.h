
/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */
 
#ifndef __RF24_CONFIG_H__
#define __RF24_CONFIG_H__

#define rf24_max(a,b) (a>b?a:b)
#define rf24_min(a,b) (a<b?a:b)

#include "rf24-chibios-config.h"
#include "rf24-arduino-config.h"

#ifndef RF24_250KBPS_TX_RX_DELAY
#define RF24_250KBPS_TX_RX_DELAY 450
#endif
#ifndef RF24_1MBPS_TX_RX_DELAY
#define RF24_1MBPS_TX_RX_DELAY 250
#endif
#ifndef RF24_2MBPS_TX_RX_DELAY
#define RF24_2MBPS_TX_RX_DELAY 190
#endif

#endif // __RF24_CONFIG_H__

