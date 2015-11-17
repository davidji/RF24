
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

using namespace std;

namespace rf24 {
class SPI {
public:

	/**
	* SPI constructor
	*/	 
	SPI(SPIDriver *driver, SPIConfig config);
	
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
	
	virtual ~SPI();

private:

	SPIDriver *driver;
	SPIConfig config;

};

}
/*@}*/
