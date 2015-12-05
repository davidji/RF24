
#include <ctype.h>
#include "ch.hpp"
#include "hal.h"
#include "chprintf.h"
#include <RF24.h>
#include <rf24_serial.h>

using namespace chibios_rt;

/****************** User Config ***************************/
/***      Set this radio as radio number 0 or 1         ***/

#define GPIOA_CS_SPI                2
#define GPIOA_RF24_CE               3
#define GPIOB_SPC                   3
#define GPIOB_SDO                   5
#define GPIOB_SDI                   4

#define CHANNEL 0x55

static const SerialConfig consoleConfig = {
        230400, 0, 0, 0
};

static const SPIConfig rf24SpiConfig = {
        NULL,
        GPIOA,
        GPIOA_CS_SPI,
        SPI_CR1_BR_1 | SPI_CR1_BR_0,
};

Rf24ChibiosIo radioIo(&SPID1, &rf24SpiConfig, GPIOB, GPIOA_RF24_CE);
RF24 radio(radioIo);
RF24Serial remote(radio);

/**********************************************************/

uint8_t addresses[][6] = {"1Node","2Node"};

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
    palSetPadMode(GPIOB, GPIOA_CS_SPI, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOB, GPIOB_SPC, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOB, GPIOB_SDO, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOB, GPIOB_SDI, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPad(GPIOB, GPIOA_CS_SPI);
}

class RelayThread: public BaseStaticThread<512> {
private:
    BaseSequentialStream *in, *out;
protected:
    virtual void main(void) {
        uint8_t buffer[256];
        while(true) {
            size_t read = chSequentialStreamRead(in, buffer, sizeof(buffer));
            chSequentialStreamWrite(out, buffer, read);
        }
    }

public:
    RelayThread(BaseSequentialStream *in, BaseSequentialStream *out) :
            BaseStaticThread<512>(), in(in), out(out) {
    }
};

RelayThread send(console, remote.stream());
RelayThread recv(remote.stream(), console);

void blink() {
    while (true) {
      palClearPad(GPIOA, GPIOA_LED_GREEN);
      chThdSleepMilliseconds(500);
      palSetPad(GPIOA, GPIOA_LED_GREEN);
      chThdSleepMilliseconds(500);
    }
}

int main(void) {
    halInit();
    chSysInit();

    palSetPadMode(GPIOA, GPIOA_RF24_CE, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_LED_GREEN, PAL_MODE_OUTPUT_PUSHPULL);
    sdStart(&SD2, &consoleConfig);

    // Set up SPI pins
    setupSpiPins();
    setup();
    radio.printDetails();
    remote.start(NORMALPRIO);
    send.start(NORMALPRIO);
    recv.start(NORMALPRIO);
    blink();
}
