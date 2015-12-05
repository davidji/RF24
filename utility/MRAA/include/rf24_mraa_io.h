/*
 * TMRh20 2015
 * SPI layer for RF24
 */

#ifndef _SPI_H_INCLUDED
#define _SPI_H_INCLUDED
 /**
 * @file spi.h
 * \cond HIDDEN_SYMBOLS
 * Class declaration for SPI helper files
 */
#include <stdio.h>
#include "mraa.hpp"

class Rf24MraaIo {
public:

    /**
    * Constructor
    */
    Rf24MraaIo(int spi_bus, int ce_pin);

    Rf24MraaIo(Rf24MraaIo &io) = default;

    /**
    * Initialise.
    */
    void begin();

    /**
    * Start SPI
    */
    void beginTransaction();

    /**
     * Start SPI
     */
    void endTransaction();

    /**
    * Transfer a single byte
    * @param tx_ Byte to send
    * @return Data returned via spi
    */
    static inline uint8_t transfer(uint8_t tx) {
        return spi->writeByte(tx);
    }

    /**
    * Transfer a buffer of data
    * @param tbuf Transmit buffer
    * @param rbuf Receive buffer
    * @param len Length of the data
    */
    static inline void transfernb(const uint8_t* tbuf, uint8_t* rbuf, uint32_t len) {
        spi->transfer(tbuf, rbuf, len);
    }

    /**
    * Transfer a buffer of data without an rx buffer
    * @param buf Pointer to a buffer of data
    * @param len Length of the data
    */
    static inline void transfern(const uint8_t* buf, uint32_t len) {
        spi->transfer(buf, buf, len);
    }

    /**
     * Select the device, ready for transfers. I.e. CSN goes low.
     */
    void select();

    /**
     * un-select the device. I.e. CSN goes high.
     */
    void unselect();

    /**
     * Set the level of the CE pin
     * @param level
     */
    static inline void ce(bool level) {
        ce->write(level);
    }

private:

    const int spi_bus, ce_pin;
    mraa::Spi *spi;
    mraa::Gpio *ce;
};

/**
 * \endcond
 */
#endif
