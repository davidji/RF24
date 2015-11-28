
#include <ctype.h>
#include "ch.hpp"
#include "hal.h"
#include "chprintf.h"
#include <RF24.h>
#include <rf24_serial.h>

using namespace chibios_rt;

/****************** User Config ***************************/
/***      Set this radio as radio number 0 or 1         ***/

#define GPIOB_CS_SPI                4
#define GPIOB_RF24_CE               5
#define GPIOA_SPC                   5
#define GPIOA_SDO                   6
#define GPIOA_SDI                   7

#define CHANNEL 0x55

static const SPIConfig rf24SpiConfig = {
        NULL,
        GPIOB,
        GPIOB_CS_SPI,
        SPI_CR1_BR_1 | SPI_CR1_BR_0,
};

Rf24ChibiosIo radioIo(&SPID1, &rf24SpiConfig, GPIOB, GPIOB_RF24_CE);
RF24 radio(radioIo);
RF24Serial remote(radio);

/**********************************************************/

uint8_t addresses[][6] = {"1Node","2Node"};

// Used to control whether this node is sending or receiving
bool role = 0;

BaseSequentialStream *console = (BaseSequentialStream *)&SD2;

void println(const char *msg) {
	chprintf(console, "%s\n", msg);
}

void print(const char *msg) {
	chprintf(console, msg);
}

void print(unsigned long value) {
	chprintf(console, "%d", value);
}

void printf_P(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chvprintf(console, fmt, ap);
    va_end(ap);
}

unsigned long micros() {
	return ST2US(chVTGetSystemTime());
}

void setup() {
  println("RF24/examples_Chibios/Stream");

  print("radio begin...");
  if(!radio.begin()) {

      // Set the PA Level low to prevent power supply related issues since this is a
      // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
      radio.setPALevel(RF24_PA_LOW);

      radio.openWritingPipe(addresses[0]);
      radio.openReadingPipe(1,addresses[1]);

      // radio.setChannel(CHANNEL);
  } else {
      println("failed");
  }
}


void setupSpiPins() {
    palSetPadMode(GPIOB, GPIOB_CS_SPI, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_SPC, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_SDO, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_SDI, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPad(GPIOB, GPIOB_CS_SPI);
}

void echo(void) {
    uint8_t buffer[256];
    while(true) {
        size_t read = chSequentialStreamRead(remote.stream(), buffer, sizeof(buffer));
        chSequentialStreamWrite(console, buffer, read);
        chSequentialStreamWrite(remote.stream(), buffer, read);
    }
}

int main(void) {
    halInit();
    chSysInit();

    palSetPadMode(GPIOB, GPIOB_RF24_CE, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    sdStart(&SD2, NULL);

    // Set up SPI pins
    setupSpiPins();
    setup();
    radio.printDetails();
    echo();
}
