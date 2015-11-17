
#include "rf24-spi.h"

rf24::SPI::SPI(SPIDriver* driver, SPIConfig config) : driver(driver), config(config) { }

void rf24::SPI::begin() {
    spiStart(driver, &config);
}

uint8_t rf24::SPI::transfer(uint8_t tx) {
    uint8_t rx;
    spiExchange(driver, 1, &tx, &rx);
    return rx;
}

void rf24::SPI::transfernb(char* tbuf, char* rbuf, uint32_t len) {
    spiExchange(driver, len, tbuf, rbuf);
}

void rf24::SPI::transfern(char* buf, uint32_t len) {
    spiSend(driver, len, buf);
}

rf24::SPI::~SPI() {
    spiStop(driver);
}
