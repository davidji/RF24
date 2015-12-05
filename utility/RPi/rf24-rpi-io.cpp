
#include "rf24-rpi-io.h"
#include "bcm2835.h"

Rf24RaspberryPiIo::Rf24RaspberryPiIo(int bus_no, int csn_pin, int ce_pin, uint16_t speed) {
}

void Rf24RaspberryPiIo::begin() {
}

void Rf24RaspberryPiIo::beginTransaction() {
}

void Rf24RaspberryPiIo::endTransaction() {
}

uint8_t Rf24RaspberryPiIo::transfer(uint8_t tx_) {
}

void Rf24RaspberryPiIo::transfernb(const uint8_t* tbuf, uint8_t* rbuf,
        uint32_t len) {
}

void Rf24RaspberryPiIo::transfern(const uint8_t* buf, uint32_t len) {
}

void Rf24RaspberryPiIo::select() {
    bcm2835_spi_setBitOrder(RF24_BIT_ORDER);
    bcm2835_spi_setDataMode(RF24_DATA_MODE);
    bcm2835_spi_setClockDivider(spi_speed ? spi_speed : RF24_CLOCK_DIVIDER);
    bcm2835_spi_chipSelect(csn_pin);
    delayMicroseconds(5);
}

void Rf24RaspberryPiIo::unselect() {
}

void Rf24RaspberryPiIo::ce(bool level) {
}
