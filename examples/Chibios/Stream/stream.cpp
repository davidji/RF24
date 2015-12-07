
#include <ctype.h>
#include "ch.hpp"
#include "hal.h"
#include "chprintf.h"
#include <RF24.h>
#include <rf24_serial.h>

using namespace chibios_rt;
using namespace rf24::serial;

/****************** User Config ***************************/
/***      Set this radio as radio number 0 or 1         ***/

#define GPIOB_CS_SPI                13
#define GPIOB_RF24_CE               14
#define GPIOB_SPC                   3
#define GPIOB_SDI                   4
#define GPIOB_SDO                   5

#define CHANNEL 0x55

static const SerialConfig consoleConfig = {
        230400, 0, 0, 0
};

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

BaseSequentialStream *console = (BaseSequentialStream *)&SD2;
Mutex consoleMutex;

void println(const char *msg) {
    printf_P("%s\n", msg);
}

void print(const char *msg) {
    printf_P(msg);
}

void print(unsigned long value) {
	printf_P("%d", value);
}

void printf_P(const char *fmt, ...) {
    consoleMutex.lock();
    va_list ap;
    va_start(ap, fmt);
    chvprintf(console, fmt, ap);
    va_end(ap);
    consoleMutex.unlock();
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
    palSetPadMode(GPIOB, GPIOB_SPC, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOB, GPIOB_SDO, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOB, GPIOB_SDI, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPad(GPIOB, GPIOB_CS_SPI);
}

class RelayThread: public BaseStaticThread<1024> {
private:
    BaseSequentialStream *in, *out;
protected:
    virtual void main(void) {
        uint8_t buffer[256];
        println("Relay");
        while(true) {
            size_t read = chSequentialStreamRead(in, buffer, sizeof(buffer));
            if(read > 0) {
                chSequentialStreamWrite(out, buffer, read);
            } else {
                println("empty read");
            }
        }
    }

public:
    RelayThread(BaseSequentialStream *_in, BaseSequentialStream *_out) :
            BaseStaticThread<1024>(), in(_in), out(_out) {
    }
};

void blink() {
    while (true) {
        println("Blink");
        palClearPad(GPIOA, GPIOA_LED_GREEN);
        chThdSleepMilliseconds(500);
        palSetPad(GPIOA, GPIOA_LED_GREEN);
        chThdSleepMilliseconds(500);
    }
}

RelayThread send(console, remote.stream());
RelayThread recv(remote.stream(), console);

int main(void) {
    halInit();
    System::init();

    palSetPadMode(GPIOB, GPIOB_RF24_CE, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_LED_GREEN, PAL_MODE_OUTPUT_PUSHPULL);
    sdStart(&SD2, &consoleConfig);

    // Set up SPI pins
    setupSpiPins();
    setup();
    radio.printDetails();
    remote.start();
    send.start(NORMALPRIO);
    recv.start(NORMALPRIO);
    blink();
}
