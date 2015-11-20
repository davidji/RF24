
/**
 * @file spi.h
 * Class declaration for SPI helper files
 */

 /**
 * Example of spi.h class declaration for SPI portability
 *
 * @defgroup Porting_SPI Porting: SPI
 *
 * 
 * @{
 */
#include <stdint.h>
#include "ch.h"
#include "hal.h"

template<int SOFT_SPI_MISO_PIN, int SOFT_SPI_MOSI_PIN, int SOFT_SPI_SCK_PIN> class RF24ArduinoSoftSpi {
public:

	/**
	* SPI constructor
	*/	 
	RF24ArduinoSoftSpi(int csn);
	
	/**
	* Start SPI
	*/
	void begin();
	
	/**
	* Transfer a single byte
	* @param tx_ Byte to send
	* @return Data returned via spi
	*/
	uint8_t transfer(uint8_t tx_);
	
	/**
	* Transfer a buffer of data
	* @param tbuf Transmit buffer
	* @param rbuf Receive buffer
	* @param len Length of the data
	*/
	void transfernb(char* tbuf, char* rbuf, uint32_t len);

	/**
	* Transfer a buffer of data without an rx buffer
	* @param buf Pointer to a buffer of data
	* @param len Length of the data
	*/	
	void transfern(char* buf, uint32_t len);

    /**
     * Select the device, ready for transfers. I.e. CSN goes low.
     */
    void select();

    /**
     * un-select the device. I.e. CSN goes high.
     */
    void unselect();


	
	virtual ~RF24ArduinoSoftSpi();

private:
	int csn;
	SoftSPI<SOFT_SPI_MISO_PIN, SOFT_SPI_MOSI_PIN, SOFT_SPI_SCK_PIN, 0> spi;
};


/*@}*/
