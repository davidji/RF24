

#ifndef _RF24_ARDUINO_IO_H_
#define _RF24_ARDUINO_IO_H_

#ifdef ARDUINO

#include <Arduino.h>
#include <SPI.h>
#include "nRF24L01.h"

class RF24ArduinoSpi {
public:

	/**
	* SPI constructor
	*/	 
	RF24ArduinoSpi(int csn, int ce);
	
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
	* Transfer a buffer of data
	* @param tbuf Transmit buffer
	* @param rbuf Receive buffer
	* @param len Length of the data
	*/
	void transfernb(const uint8_t* tbuf, uint8_t* rbuf, uint32_t len);

	/**
	* Transfer a buffer of data without an rx buffer
	* @param buf Pointer to a buffer of data
	* @param len Length of the data
	*/	
	void transfern(const uint8_t* buf, uint32_t len);

    /**
     * Set the level of the CE pin
     * @param level
     */
    void ce(bool level);


private:
    int csnpin, cepin;

};

RF24ArduinoSpi::RF24ArduinoSpi(int csn, int ce) : csnpin(csn), cepin(ce) {
}

void RF24ArduinoSpi::beginTransaction() {
    SPI.beginTransaction(SPISettings(RF24_SPI_SPEED, MSBFIRST, SPI_MODE0));
    digitalWrite(csnpin, HIGH);
}

void RF24ArduinoSpi::endTransaction() {
    digitalWrite(csnpin, HIGH);
    SPI.endTransaction();
}

uint8_t RF24ArduinoSpi::transfer(uint8_t tx) {
    return SPI.transfer(tx);
}

void RF24ArduinoSpi::transfernb(const uint8_t* tbuf, uint8_t* rbuf, uint32_t len) {
    for(uint32_t i = 0; i != len; ++i) {
        rbuf[i] = transfer(tbuf[i]);
    }
}

void RF24ArduinoSpi::transfern(const uint8_t* buf, uint32_t len) {
    for(uint32_t i = 0; i != len; ++i) {
        transfer(buf[i]);
    }
}

void RF24ArduinoSpi::ce(bool level) {
    digitalWrite(cepin, level ? HIGH : LOW);
    delayMicroseconds(5);
}


#endif // ARDUINO
#endif // _RF24_ARDUINO_H_
