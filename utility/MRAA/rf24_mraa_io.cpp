

#include "spi.h"
#include "mraa.h"


void SPI::end() {
	// Prophet: we should check for existence of mspi before deleting it
	if (mspi != NULL)
		delete mspi;
}

Rf24MraaIo::Rf24MraaIo(mraa::Spi& spi, mraa::Gpio& ce_pin) {
}

void Rf24MraaIo::begin() {
    spi.mode(mraa::SPI_MODE0);
    spi.bitPerWord(8);
    spi.frequency(8000000);
}

void Rf24MraaIo::beginTransaction() {
}

void Rf24MraaIo::endTransaction() {
}

void Rf24MraaIo::select() {
}

void Rf24MraaIo::unselect() {
}

void Rf24MraaIo::ce(bool level) {
}
