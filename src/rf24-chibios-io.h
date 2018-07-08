
/**
 * @file rf24-chibios-io.h
 * IO primitives for Chibios port
 */
#ifndef _RF24_CHIBIOS_IO_H_
#define _RF24_CHIBIOS_IO_H_
 /**
 * @defgroup Porting_SPI Porting: SPI
 *
 * 
 * @{
 */
#ifdef CHIBIOS
#include <stdint.h>
#include "ch.h"
#include "hal.h"

class Rf24ChibiosIo {
public:

	/**
	* SPI constructor
	*/
	Rf24ChibiosIo(SPIDriver *driver, const SPIConfig *config, ioline_t ce);

	Rf24ChibiosIo(Rf24ChibiosIo &io) = default;

    /**
    * Initialise. This involves initialising ce and csn for output and setting them both low.
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
	uint8_t transfer(uint8_t tx_);
	
	/**
	* Transfer a buffer of data, discarding the responses
	* @param buf Pointer to a buffer of data
	* @param len Length of the data
	*/	
	void transfern(const uint8_t* buf, uint32_t len);

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
    void ce(bool level);

private:
	SPIDriver *driver;
	const SPIConfig *config;
	ioline_t ceLine;
};

#endif
#endif
