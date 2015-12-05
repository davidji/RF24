
#include "rf24_chibios_io.h"

Rf24ChibiosIo::Rf24ChibiosIo(
        SPIDriver* driver,
        const SPIConfig* config,
        ioportid_t ce_port,
        uint8_t ce_pad)
    : driver(driver), config(config),
      ce_port(ce_port), ce_pad(ce_pad) {
}

void Rf24ChibiosIo::begin() {
    // In Chibios we leave it to the library user to set the correct modes for all pins,
    // rather than having divided responsibility.
    spiStart(driver, config);
}

void Rf24ChibiosIo::beginTransaction() {
    // spiStart(driver, config);
}

void Rf24ChibiosIo::endTransaction() {
    // spiStop(driver);
}

void Rf24ChibiosIo::select() {
    spiSelect(driver);
    chThdSleepMicroseconds(5);
}

void Rf24ChibiosIo::unselect() {
    spiUnselect(driver);
}

uint8_t Rf24ChibiosIo::transfer(uint8_t tx) {
    uint8_t rx;
    spiExchange(driver, 1, &tx, &rx);
    return rx;
}

void Rf24ChibiosIo::transfernb(const uint8_t* tx, uint8_t* rx, uint32_t len) {
    spiExchange(driver, len, tx, rx);
}

void Rf24ChibiosIo::transfern(const uint8_t* buf, uint32_t len) {
    spiSend(driver, len, buf);
}

void Rf24ChibiosIo::ce(bool level) {
    palWritePad(ce_port, ce_pad, level);
}

