
#include <ctype.h>
#include "ch.hpp"
#include "hal.h"
#include "chprintf.h"
#include <RF24.h>
#include <rf24_serial.h>

using namespace chibios_rt;
using namespace rf24::serial;

static const uint16_t GPIOB_RF24_CS = 2;
static const uint16_t GPIOB_RF24_CE  = 1;
static const uint16_t GPIOB_RF24_SPC = 13;
static const uint16_t GPIOB_RF24_SDI = 14;
static const uint16_t GPIOB_RF24_SDO = 15;
static const uint16_t GPIOC_RF24_IRQ = 4;

void setup_rf24_spi_pins() {
    palSetPadMode(GPIOB, GPIOB_RF24_CS, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOB, GPIOB_RF24_SPC, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOB, GPIOB_RF24_SDO, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOB, GPIOB_RF24_SDI, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPad(GPIOB, GPIOB_RF24_CS);
}


#define CHANNEL 0x55

static const SerialConfig consoleConfig = {
        230400, 0, 0, 0
};

static const SPIConfig rf24SpiConfig = {
        NULL,
        GPIOB,
        GPIOB_RF24_CS,
        SPI_CR1_BR_1,
};

Rf24ChibiosIo radioIo(&SPID2, &rf24SpiConfig, GPIOB, GPIOB_RF24_CE);
RF24 radio(radioIo);
RF24Serial remote(radio);

void remoteIrq(EXTDriver *extp, expchannel_t channel) {
    (void)extp;
    (void)channel;
    remote.irq();
}

static const EXTConfig extcfg = {
  {
    {EXT_CH_MODE_DISABLED, NULL}, // 0
    {EXT_CH_MODE_DISABLED, NULL}, // 1
    {EXT_CH_MODE_DISABLED, NULL}, // 2
    {EXT_CH_MODE_DISABLED, NULL}, // 3
    {EXT_CH_MODE_FALLING_EDGE |
     EXT_CH_MODE_AUTOSTART |
     EXT_MODE_GPIOC, remoteIrq},  // IRQ line connected to PC4
    {EXT_CH_MODE_DISABLED, NULL}, // 5
    {EXT_CH_MODE_DISABLED, NULL}, // 6
    {EXT_CH_MODE_DISABLED, NULL}, // 7
    {EXT_CH_MODE_DISABLED, NULL}, // 8
    {EXT_CH_MODE_DISABLED, NULL}, // 9
    {EXT_CH_MODE_DISABLED, NULL}, // 10
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL}
  }
};


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
      radio.setPALevel(RF24_PA_HIGH);
      radio.setDataRate(RF24_2MBPS);
      radio.setAutoAck(true);
      radio.setRetries(5,15);
      radio.setChannel(CHANNEL);

      radio.openWritingPipe(addresses[0]);
      radio.openReadingPipe(1,addresses[1]);

  } else {
      println("failed");
  }
}


class RelayThread: public BaseStaticThread<1024> {
private:
    BaseSequentialStream *in, *out;
protected:
    virtual void main(void) {
        uint8_t buffer[256];
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
        palClearPad(GPIOA, GPIOA_LED_GREEN);
        chThdSleepMilliseconds(500);
        palSetPad(GPIOA, GPIOA_LED_GREEN);
        chThdSleepMilliseconds(500);
    }
}

class SendThread : public BaseStaticThread<1024> {
protected:
    virtual void main(void) {
        int counter = 0;
        while(true) {
            remote.print("ABCDEFGHIJKLMNOPQRSTUVWXYZ%5d\n", counter);
            print("sent "); print(counter++); println("");
        }
    }

public:
    SendThread() : BaseStaticThread<1024>() { }
};

// RelayThread send(console, remote.stream());
SendThread send;
RelayThread recv(remote.stream(), console);

int main(void) {
    halInit();
    System::init();
    extInit();

    palSetPadMode(GPIOB, GPIOB_RF24_CE, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOC, GPIOC_RF24_IRQ, PAL_MODE_INPUT | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_LED_GREEN, PAL_MODE_OUTPUT_PUSHPULL);
    sdStart(&SD2, &consoleConfig);

    // Set up SPI pins
    setup_rf24_spi_pins();
    setup();
    radio.printDetails();
    extStart(&EXTD1, &extcfg);
    remote.start();
    send.start(NORMALPRIO);
    recv.start(NORMALPRIO);
    blink();
}
