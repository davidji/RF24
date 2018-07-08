


#include "rf24-chibios-io.h"

#ifdef CHIBIOS

#include <alloca.h>
#include <string.h>

Rf24ChibiosIo::Rf24ChibiosIo(
        SPIDriver* driver,
        const SPIConfig* config,
        ioline_t ce)
    : driver(driver), config(config), ceLine(ce) {
}

void Rf24ChibiosIo::begin() {
    // In Chibios we leave it to the library user to set the correct modes for all pins,
    // rather than having divided responsibility.
    spiStart(driver, config);
}

void Rf24ChibiosIo::beginTransaction() {
#ifdef SPI_USE_MUTUAL_EXCLUSION
    spiAcquireBus(driver);
#endif
    spiSelect(driver);
    chThdSleepMicroseconds(5);
}

void Rf24ChibiosIo::endTransaction() {
    spiUnselect(driver);
#ifdef SPI_USE_MUTUAL_EXCLUSION
    spiReleaseBus(driver);
#endif
}

uint8_t Rf24ChibiosIo::transfer(uint8_t tx) {
    uint8_t rx;
    spiExchange(driver, 1, &tx, &rx);
    return rx;
}

void Rf24ChibiosIo::transfern(const uint8_t* buf, uint32_t len) {
    // The STM32 SPI driver is DMA, and DMA from flash isn't possible:
    // it will result in a halt. I don't know how to work out if the
    // source buffer is safe for the driver so for now, I'm going to assume I
    // have to copy the buffer. The buffer will either be an address, or
    // a packet, so the maximum size is 32 bytes.
    uint8_t *copy;
    copy = (uint8_t *)alloca(len);
    memcpy(copy, buf, len);
    spiSend(driver, len, buf);
}

void Rf24ChibiosIo::ce(bool level) {
    palWriteLine(ceLine, level ? PAL_HIGH : PAL_LOW);
}
#endif
