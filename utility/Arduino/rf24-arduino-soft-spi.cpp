
#include "rf24-arduino-soft-spi.h"

RF24ArduinoSoftSpi::RF24ArduinoSoftSpi(int csn) {
}

void RF24ArduinoSoftSpi::begin() {
}

uint8_t RF24ArduinoSoftSpi::transfer(uint8_t tx_) {
}

void RF24ArduinoSoftSpi::transfernb(char* tbuf, char* rbuf, uint32_t len) {
}

void RF24ArduinoSoftSpi::transfern(char* buf, uint32_t len) {
}

void RF24ArduinoSoftSpi::select() {
    digitalWrite(csn, LOW);
    delayMicroseconds(5);
    spi.setBitOrder(MSBFIRST);
    spi.setDataMode(SPI_MODE0);
    spi.setClockDivider(SPI_CLOCK_DIV2);
}

void RF24ArduinoSoftSpi::unselect() {
    digitalWrite(csn, HIGH);
    delayMicroseconds(5);
}

RF24ArduinoSoftSpi::~RF24ArduinoSoftSpi() {
}
