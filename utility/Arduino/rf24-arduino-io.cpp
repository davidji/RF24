
#include "rf24-arduino-spi.h"

RF24ArduinoSpi::RF24ArduinoSpi(int csn) : csn(csn) {
}

void RF24ArduinoSpi::begin() {

}

uint8_t RF24ArduinoSpi::transfer(uint8_t tx_) {

}

void RF24ArduinoSpi::transfernb(char* tbuf, char* rbuf, uint32_t len) {
}

void RF24ArduinoSpi::transfern(char* buf, uint32_t len) {
}

void RF24ArduinoSpi::select() {
    digitalWrite(csn, LOW);
    delayMicroseconds(5);
}

void RF24ArduinoSpi::unselect() {
    digitalWrite(csn, HIGH);
    delayMicroseconds(5);
}

RF24ArduinoSpi::~RF24ArduinoSpi() {
}
