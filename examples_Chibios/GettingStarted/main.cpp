
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

static const SPIConfig rf24SpiConfig = {
        NULL,
        GPIOB,
        GPIOB_CS_SPI,
        SPI_CR1_BR_0,
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

unsigned long micros() {
	return ST2US(chVTGetSystemTime());
}

void setup() {
  println("RF24/examples/GettingStarted");
  println("*** PRESS 'T' to begin transmitting to the other node");

  radio.begin();

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

  radio.setChannel(0x60);

  // Start the radio listening for data
  radio.startListening();
}

void ping() {

    radio.stopListening();                                    // First, stop listening so we can talk.


    println("Now sending");

    unsigned long time = micros();                             // Take the time, and send it.  This will block until complete
     if (!radio.write( &time, sizeof(unsigned long) )){
       println("failed");
     }

    radio.startListening();                                    // Now, continue listening

    unsigned long started_waiting_at = micros();               // Set up a timeout period, get the current microseconds
    bool timeout = false;                                   // Set up a variable to indicate if a response was received or not

    while ( ! radio.available() ){                             // While nothing is received
      if (micros() - started_waiting_at > 200000 ){            // If waited longer than 200ms, indicate timeout and exit while loop
          timeout = true;
          break;
      }
    }

    if ( timeout ) {                                             // Describe the results
        println("Failed, response timed out.");
    } else {
        unsigned long got_time;                                 // Grab the response, compare, and send to debugging spew
        radio.read( &got_time, sizeof(unsigned long) );
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
            radio.read(&got_time, sizeof(unsigned long));     // Get the payload
        }

        radio.stopListening();           // First, stop listening so we can talk
        radio.write(&got_time, sizeof(unsigned long)); // Send the final one back.
        radio.startListening(); // Now, resume listening so we catch the next packets.
        print("Sent response ");
        print(got_time);
        println("");
    }

    chThdSleepMilliseconds(1);
}

class PingPongThread: public BaseStaticThread<1024> {
protected:
    virtual void main(void) {
        while (true) {
            if (role == 1) {
                ping();
            } else if (role == 0) {
                pong();
            }
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
        if (c == 'T' && role == 0) {
            println(
                    "*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK");
            role = 1;               // Become the primary transmitter (ping out)

        } else if (c == 'R' && role == 1) {
            println("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK");
            role = 0;                // Become the primary receiver (pong back)
            radio.startListening();
        }
    }
}

static PingPongThread pingPong;

void setupSpiPins() {
    palSetPadMode(GPIOB, GPIOB_CS_SPI,
            PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_SPC,
            PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_SDO,
            PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, GPIOA_SDI,
            PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_HIGHEST);
    palSetPad(GPIOB, GPIOB_CS_SPI);
}

int main(void) {
    halInit();
    chSysInit();

    sdStart(&SD2, NULL);

    // Set up SPI pins
    setupSpiPins();
    setup();
    pingPong.start(NORMALPRIO);
    mode();
}
