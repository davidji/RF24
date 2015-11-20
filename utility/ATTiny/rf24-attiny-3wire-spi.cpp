
#include "rf24-attiny-3wire-spi.h"
//ATTiny support code pulled in from https://github.com/jscrane/RF24

#if defined(RF24_TINY)

void SPIClass::begin() {

  pinMode(USCK, OUTPUT);
  pinMode(DO, OUTPUT);
  pinMode(DI, INPUT);
  USICR = _BV(USIWM0);

}

byte SPIClass::transfer(byte b) {

  USIDR = b;
  USISR = _BV(USIOIF);
  do
    USICR = _BV(USIWM0) | _BV(USICS1) | _BV(USICLK) | _BV(USITC);
  while ((USISR & _BV(USIOIF)) == 0);
  return USIDR;

}

void SPIClass::end() {}
void SPIClass::setDataMode(uint8_t mode){}
void SPIClass::setBitOrder(uint8_t bitOrder){}
void SPIClass::setClockDivider(uint8_t rate){}


#endif

RF24AtTiny3WireSpi::RF24AtTiny3WireSpi() {
}

void RF24AtTiny3WireSpi::begin() {
}

uint8_t RF24AtTiny3WireSpi::transfer(uint8_t tx_) {

}

void RF24AtTiny3WireSpi::transfernb(char* tbuf, char* rbuf, uint32_t len) {
}

void RF24AtTiny3WireSpi::transfern(char* buf, uint32_t len) {
}

void RF24AtTiny3WireSpi::select() {
    PORTB |= (1<<PINB2);    // SCK->CSN HIGH
    delayMicroseconds(100); // allow csn to settle.
}

void RF24AtTiny3WireSpi::unselect() {
    PORTB &= ~(1<<PINB2);   // SCK->CSN LOW
    delayMicroseconds(11);  // allow csn to settle
}

RF24AtTiny3WireSpi::~RF24AtTiny3WireSpi() {
}
