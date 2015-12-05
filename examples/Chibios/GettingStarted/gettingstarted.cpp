
#include <ctype.h>
#include "ch.hpp"
#include "hal.h"
#include "chprintf.h"
#include <RF24.h>

using namespace chibios_rt;

/****************** User Config ***************************/
/***      Set this radio as radio number 0 or 1         ***/
bool radioNumber = 0;

#define GPIOB_CS_SPI                4
#define GPIOB_RF24_CE               5
#define GPIOA_SPC                   5
#define GPIOA_SDO                   6
#define GPIOA_SDI                   7

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
  println("RF24/examples/GettingStarted");
  println("*** PRESS 'T' to begin transmitting to the other node");

  print("radio begin...");
  if(!radio.begin()) {

      // Set the PA Level low to prevent power supply related issues since this is a
      // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
      radio.setPALevel(RF24_PA_LOW);

      // Open a writing and reading pipe on each radio, with opposite addresses
      if(radioNumber){
          radio.openWritingPipe(addresses[1]);
          radio.openReadingPipe(1,addresses[0]);
      }else{
          radio.openWritingPipe(addresses[0]);
          radio.openReadingPipe(1,addresses[1]);
      }

      // radio.setChannel(CHANNEL);
  } else {
      println("failed");
  }
}

void ping() {

    radio.stopListening();                                    // First, stop listening so we can talk.


    println("Now sending");

    unsigned long time = micros();                             // Take the time, and send it.  This will block until complete
    if (!radio.write( &time, sizeof(time) )){
        println("failed");
    }

    println("sent");
    radio.startListening();                                    // Now, continue listening

    unsigned long started_waiting_at = micros();               // Set up a timeout period, get the current microseconds
    bool timeout = false;                                   // Set up a variable to indicate if a response was received or not

    while ( ! radio.available() ){                             // While nothing is received
      if (micros() - started_waiting_at > 200000){            // If waited longer than 200ms, indicate timeout and exit while loop
          timeout = true;
          break;
      }
    }

    if ( timeout ) {                                             // Describe the results
        println("Failed, response timed out.");
    } else {
        unsigned long got_time;                                 // Grab the response, compare, and send to debugging spew
        radio.read( &got_time, sizeof(got_time) );
        unsigned long time = micros();

        // Spew it
        print("Sent ");
        print(time);
        print(", Got response ");
        print(got_time);
        print(", Round-trip delay ");
        print(time-got_time);
        println(" microseconds");
    }

    // Try again 1s later
    chThdSleepMilliseconds(1000);

}

void pong() {
    unsigned long got_time;

    if (radio.available()) {
        // Variable for the received timestamp
        while (radio.available()) {                 // While there is data ready
            radio.read(&got_time, sizeof(got_time));     // Get the payload
        }

        radio.stopListening();           // First, stop listening so we can talk
        radio.write(&got_time, sizeof(got_time)); // Send the final one back.
        radio.startListening(); // Now, resume listening so we catch the next packets.
        print("Sent response ");
        print(got_time);
        println("");
    }

    chThdSleepMicroseconds(100);
}

class PingPongThread: public BaseStaticThread<1024> {
protected:
    virtual void main(void) {
        radio.startListening();
        while (true) {
            while(role == 0) {
                pong();
            }

            println("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK");

            while(role == 1) {
                ping();
            }

            println("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK");
        }
    }

public:
    PingPongThread(void) :
            BaseStaticThread<1024>() {
    }
};

void mode() {
    while (true) {
        char c = toupper(chSequentialStreamGet(console));
        if (c == 'T') {
            role = 1;
        } else if (c == 'R') {
            role = 0;
        }
    }
}

static PingPongThread pingPong;

void setupSpiPins() {
    palSetPadMode(GPIOB, GPIOB_CS_SPI, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_SPC, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_SDO, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_SDI, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPad(GPIOB, GPIOB_CS_SPI);
}

int main(void) {
    halInit();
    chSysInit();

    palSetPadMode(GPIOB, GPIOB_RF24_CE, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    sdStart(&SD2, &consoleConfig);

    // Set up SPI pins
    setupSpiPins();
    setup();
    radio.printDetails();
    pingPong.start(NORMALPRIO);
    mode();
}
