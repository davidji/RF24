/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * @file RF24.h
 *
 * Class declaration for RF24 and helper enums
 */

#ifndef __RF24_H__
#define __RF24_H__

#include "rf24-config.h"
#include "rf24-settings.h"
#include <array>
#include <assert.h>

typedef struct {
  uint32_t tx_rx_delay;
} ClientSettings;


static const unsigned int RX_P_NO_EMPTY = 0b111;

struct Status {
  uint8_t status;

  bool txFifoFull() { return _BV(TX_FULL) & status; }
  uint8_t rxPipeNo() { return (status >> RX_P_NO) & 0b111; }
  bool maxRetries() { return status & _BV(MAX_RT); }
  bool dataSent() { return status & _BV(TX_DS); }
  bool dataReceived() { return status & _BV(RX_DR); }
};

struct FifoStatus {
  uint8_t status;
  bool rxEmpty() { return status & _BV(RX_EMPTY); }
  bool rxFull() { return status & _BV(RX_FULL); }
  bool txEmpty() { return status & _BV(TX_EMPTY); }
  bool txFull() { return status & _BV(TX_FULL); }
};

class RF24InternalSettings {
  class PrimaryRx {
  public:
     static constexpr Setting setting = { CONFIG, _BV(PRIM_RX) };
     static constexpr SettingValue ENABLE = { setting, _BV(PRIM_RX) };
     static constexpr SettingValue DISABLE = { setting, 0 };
  };

  class Power {
  public:
     static constexpr Setting setting = { CONFIG, _BV(PWR_UP) };
     static constexpr SettingValue UP = { setting, _BV(PWR_UP) };
     static constexpr SettingValue DOWN = { setting, 0 };
  };

  class ConfigRegister {
  public:
    static constexpr Setting setting = { CONFIG, 0xff };
    // Initial setting enable 16-bit CRC.
    static constexpr SettingValue INIT = { setting, 0b00001100 };
  };

  class FeatureRegister {
  public:
    static constexpr Setting setting = { FEATURE, 0xff };
    static constexpr SettingValue INIT = { setting, 0x00 };
  };

  class DynamicPayloadsRegister {
  public:
    static constexpr Setting setting = { DYNPD, 0xff };
    static constexpr SettingValue INIT = { setting, 0x00 };
  };

  static const std::array<SettingValue, 4> INIT;

  class AckPayloads {
  public:
    static constexpr BooleanSetting feature = { { FEATURE, _BV(EN_ACK_PAY) } };
    static const std::array<SettingValue, 4> ENABLE;
  };

  class Receive {
  public:
    static constexpr BooleanSetting pipe(uint8_t pipe) {
      return {{ EN_RXADDR, _BV(rf24_min(5, pipe))}};
    }
    static constexpr uint8_t addressRegister(uint8_t pipe) {
      return RX_ADDR_P0 + rf24_min(5, pipe);
    }
    static constexpr uint8_t payloadLengthRegister(uint8_t pipe) {
      return RX_ADDR_P0 + rf24_min(5, pipe);
    }
  };
};

/**
 * Driver for nRF24L01(+) 2.4GHz Wireless Transceiver
 */

template<typename IO, uint8_t addressWidth>
class RF24 : RF24InternalSettings
{
private:
  IO io;
  uint8_t pipe0_reading_address[5]; /**< Last address set on pipe 0 for reading. */
  uint32_t txRxDelay; /**< Var for adjusting delays depending on datarate */
  bool ackPayloads;

  static constexpr uint8_t child_pipe_enable[] PROGMEM =
  {
    ERX_P0, ERX_P1, ERX_P2, ERX_P3, ERX_P4, ERX_P5
  };

public:

  /**
   * @name Primary public interface
   *
   *  These are the main methods you need to operate the chip
   */
  /**@{*/

  RF24(IO io) : io(io) {}

  typedef enum {
    ERROR = 0,
    UPDATED = 1,
    UNCHANGED = 2
  } SetResult;

  /**
   * Apply a single setting.
   * Return true if the setting was successfully changed.
   * This is done by reading back the register(s)
   */
  SetResult set(SettingValue value) {
      uint8_t regvalue = read_register(value.setting.reg);
      regvalue &= ~value.setting.mask;
      uint8_t newvalue = regvalue | (value.value & value.setting.mask);
      if(newvalue != regvalue) {
          write_register(value.setting.reg, regvalue);
          return (read_register(value.setting.reg) == regvalue) ? UPDATED : ERROR;
      } else {
          return UNCHANGED;
      }
  }

  /**
   * Apply several settings.
   * Call set on each setting in turn, and return ERROR
   * if any of the individual settings return ERROR,
   * UNCHANGED if all the settings return UNCHANGED and
   * UPDATED otherwise.
   * If ERROR is returned, which settings have been applied is undefined.
   */
  template<unsigned int N>
  SetResult set(const std::array<SettingValue, N> &settings) {
    SetResult result = UNCHANGED;
    for(SettingValue setting: settings) {
      switch(set(setting)) {
      case ERROR:
        return ERROR;
      case UPDATED:
        result = UPDATED;
        break;
      default:
        break;
      }
    }

    return result;
  }

  /**
   * Begin operation of the chip
   * 
   * Call this in setup(), before calling any other methods.
   * @code radio.begin() @endcode
   */
  bool begin(void)
  {
    io.begin();
    io.ce(LOW);

    // Must allow the radio time to settle else configuration bits will not necessarily stick.
    // This is actually only required following power up but some settling time also appears to
    // be required after resets too. For full coverage, we'll always assume the worst.
    // Enabling 16b CRC is by far the most obvious case if the wrong timing is used - or skipped.
    // Technically we require 4.5ms + 14us as a worst case. We'll just call it 5ms for good measure.
    // WARNING: Delay is based on P-variant whereby non-P *may* require different timing.
    delay(5);

    if(set(INIT) == ERROR) {
      return false;
    }

    if(set(DataRate::_1MBPS) == ERROR) {
      return false;
    }

    // Reset current status
    // Notice reset and flush is the last thing we do
    resetStatus();

    // Set up default configuration.  Callers can always change it later.
    // This channel should be universally safe and not bleed over into adjacent
    // spectrum.
    if(set(Channel::channel(76)) == ERROR) {
      return false;
    }

    // Flush buffers
    flush_rx();
    flush_tx();

    powerUp();

    // Enable PTX, do not write CE high so radio will remain in standby I mode ( 130us max to transition to RX or TX instead of 1500us from powerUp )
    // PTX should use only 22uA of power
    if(set(PrimaryRx::ENABLE) == ERROR) {
      return false;
    }

    return true;
  }

  /**
   * Start listening on the pipes opened for reading.
   *
   * 1. Be sure to call openReadingPipe() first.  
   * 2. Do not call write() while in this mode, without first calling stopListening().
   * 3. Call available() to check for incoming traffic, and read() to get it. 
   *  
   * @code
   * Open reading pipe 1 using address CCCECCCECC
   *  
   * byte address[] = { 0xCC,0xCE,0xCC,0xCE,0xCC };
   * radio.openReadingPipe(1,address);
   * radio.startListening();
   * @endcode
   */
  void startListening(void)
  {
    powerUp();
    set(PrimaryRx::ENABLE);
    resetStatus();
    io.ce(HIGH);
    // Restore the pipe0 adddress, if exists
    if (pipe0_reading_address[0] > 0) {
      write_register(Receive::addressRegister(0), pipe0_reading_address, addressWidth);
    } else {
      closeReadingPipe(0);
    }

    if (ackPayloads) {
      flush_tx();
    }
  }

  /**
   * Stop listening for incoming messages, and switch to transmit mode.
   *
   * Do this before calling write().
   * @code
   * radio.stopListening();
   * radio.write(&data,sizeof(data));
   * @endcode
   */
  void stopListening(void) {
    io.ce(LOW);

    delayMicroseconds(txRxDelay);

    if(ackPayloads){
        delayMicroseconds(txRxDelay); //200
        flush_tx();
    }

    set(PrimaryRx::DISABLE);
    set(Receive::pipe(0).enable());
  }


  /**
   * Read the available payload
   *
   * The size of data read is the fixed payload size, see getPayloadSize()
   *
   * @note I specifically chose 'void*' as a data type to make it easier
   * for beginners to use.  No casting needed.
   *
   * @note No longer boolean. Use available to determine if packets are
   * available. Interrupt flags are now cleared during reads instead of
   * when calling available().
   *
   * @param buf Pointer to a buffer where the data should be written
   * @param len Maximum number of bytes to read into the buffer
   *
   * @code
   * if(radio.available()){
   *   radio.read(&data,sizeof(data));
   * }
   * @endcode
   * @return No return value. Use available().
   */
  void read(void* buf, uint8_t len)  {
    read_payload(buf, len);
    resetStatus();
  }

  /**
   * Be sure to call openWritingPipe() first to set the destination
   * of where to write to.
   *
   * This blocks until the message is successfully acknowledged by
   * the receiver or the timeout/retransmit maxima are reached.  In
   * the current configuration, the max delay here is 60-70ms.
   *
   * The maximum size of data written is the fixed payload size, see
   * getPayloadSize().  However, you can write less, and the remainder
   * will just be filled with zeroes.
   *
   * TX/RX/RT interrupt flags will be cleared every time write is called
   *
   * @param buf Pointer to the data to be sent
   * @param len Number of bytes to be sent
   *
   * @code
   * radio.stopListening();
   * radio.write(&data,sizeof(data));
   * @endcode
   * @return True if the payload was delivered successfully false if not
   */
  bool write( const void* buf, uint8_t len );

  /**
   * New: Open a pipe for writing via byte array. Old addressing format retained
   * for compatibility.
   *
   * Only one writing pipe can be open at once, but you can change the address
   * you'll write to. Call stopListening() first.
   *
   * Addresses are assigned via a byte array, default is 5 byte address lengths
   *
   * @code
   *   uint8_t addresses[][6] = {"1Node","2Node"};
   *   radio.openWritingPipe(addresses[0]);
   * @endcode
   * @code
   *  uint8_t address[] = { 0xCC,0xCE,0xCC,0xCE,0xCC };
   *  radio.openWritingPipe(address);
   *  address[0] = 0x33;
   *  radio.openReadingPipe(1,address);
   * @endcode
   * @see setAddressWidth
   *
   * @param address The address of the pipe to open. Coordinate these pipe
   * addresses amongst nodes on the network.
   */
  void openWritingPipe(const uint8_t *address) {
    write_register(RX_ADDR_P0, address, addressWidth);
    write_register(TX_ADDR, address, addressWidth);
    write_register(RX_PW_P0, RF24_MAX_PAYLOAD);
  }

  /**
   * Open a pipe for reading
   *
   * Up to 6 pipes can be open for reading at once.  Open all the required
   * reading pipes, and then call startListening().
   *
   * @see openWritingPipe
   * @see setAddressWidth
   *
   * @note Pipes 0 and 1 will store a full 5-byte address. Pipes 2-5 will technically 
   * only store a single byte, borrowing up to 4 additional bytes from pipe #1 per the
   * assigned address width.
   * @warning Pipes 1-5 should share the same address, except the first byte.
   * Only the first byte in the array should be unique, e.g.
   * @code
   *   uint8_t addresses[][6] = {"1Node","2Node"};
   *   openReadingPipe(1,addresses[0]);
   *   openReadingPipe(2,addresses[1]);
   * @endcode
   *
   * @warning Pipe 0 is also used by the writing pipe.  So if you open
   * pipe 0 for reading, and then startListening(), it will overwrite the
   * writing pipe.  Ergo, do an openWritingPipe() again before write().
   *
   * @param number Which pipe# to open, 0-5.
   * @param address The 24, 32 or 40 bit address of the pipe to open.
   */
  void openReadingPipe(uint8_t pipe, const uint8_t *address) {
      switch(pipe) {
      case 0: 
        // If this is pipe 0, cache the address.  This is needed because
        // openWritingPipe() will overwrite the pipe 0 address, so
        // startListening() will have to restore it.
        memcpy(pipe0_reading_address,address,addressWidth);
        __attribute__ ((fallthrough));
      case 1:
        write_register(Receive::addressRegister(pipe), address, addressWidth);
        break;
      case 2: case 3: case 4: case 5:
        write_register(Receive::addressRegister(pipe), address, 1);
        break;
      default:
        assert(false);
	  }

    write_register(Receive::payloadLengthRegister(pipe),RF24_MAX_PAYLOAD);
    set(Receive::pipe(pipe).enable());
  }


  /**
   * Flush the transmit queue. If whatHappened returns tx_fail
   * call this to discard the transmit queue and be ready to start
   * transmitting again, by writing new packets.
   */
  void txReset();

  /**
   * Try and send the message at the head of the queue again.
   * If whatHappened returns tx_fail
   * call this to start transmission again, retrying the last packet
   */
  void txRetry();

  /**
   * Enter low-power mode
   *
   * To return to normal power mode, call powerUp().
   *
   * @note After calling startListening(), a basic radio will consume about 13.5mA
   * at max PA level.
   * During active transmission, the radio will consume about 11.5mA, but this will
   * be reduced to 26uA (.026mA) between sending.
   * In full powerDown mode, the radio will consume approximately 900nA (.0009mA)   
   *
   * @code
   * radio.powerDown();
   * avr_enter_sleep_mode(); // Custom function to sleep the device
   * radio.powerUp();
   * @endcode
   */
  void powerDown(void) {
    io.ce(LOW);
    return set(Power::DOWN);
  }


  /**
   * Leave low-power mode - required for normal radio operation after calling powerDown()
   * 
   * To return to low power mode, call powerDown().
   * @note This will take up to 5ms for maximum compatibility 
   */
  SetResult powerUp(void) {
   SetResult result = set(Power::UP);
   if(result == UPDATED) {
     delay(5);
   }
   return result;
  }

  /**
  * Write for single NOACK writes. Optionally disables acknowledgements/autoretries for a single write.
  *
  * @note enableDynamicAck() must be called to enable this feature
  *
  * Can be used with enableAckPayload() to request a response
  * @see enableDynamicAck()
  * @see setAutoAck()
  * @see write()
  *
  * @param buf Pointer to the data to be sent
  * @param len Number of bytes to be sent
  * @param multicast Request ACK (0), NOACK (1)
  */
  bool write( const void* buf, uint8_t len, const bool multicast );

  /**
   * This will not block until the 3 FIFO buffers are filled with data.
   * Once the FIFOs are full, writeFast will simply wait for success or
   * timeout, and return 1 or 0 respectively. From a user perspective, just
   * keep trying to send the same data. The library will keep auto retrying
   * the current payload using the built in functionality.
   * @warning It is important to never keep the nRF24L01 in TX mode and FIFO full for more than 4ms at a time. If the auto
   * retransmit is enabled, the nRF24L01 is never in TX mode long enough to disobey this rule. Allow the FIFO
   * to clear by issuing txStandBy() or ensure appropriate time between transmissions.
   *
   * @code
   * Example (Partial blocking):
   *
   *			radio.writeFast(&buf,32);  // Writes 1 payload to the buffers
   *			txStandBy();     		   // Returns 0 if failed. 1 if success. Blocks only until MAX_RT timeout or success. Data flushed on fail.
   *
   *			radio.writeFast(&buf,32);  // Writes 1 payload to the buffers
   *			txStandBy(1000);		   // Using extended timeouts, returns 1 if success. Retries failed payloads for 1 seconds before returning 0.
   * @endcode
   *
   * @see txStandBy()
   * @see write()
   * @see writeBlocking()
   *
   * @param buf Pointer to the data to be sent
   * @param len Number of bytes to be sent
   * @return True if the payload was delivered successfully false if not
   */
  bool writeFast( const void* buf, uint8_t len );

  /**
  * WriteFast for single NOACK writes. Disables acknowledgements/autoretries for a single write.
  *
  * @note enableDynamicAck() must be called to enable this feature
  * @see enableDynamicAck()
  * @see setAutoAck()
  *
  * @param buf Pointer to the data to be sent
  * @param len Number of bytes to be sent
  * @param multicast Request ACK (0) or NOACK (1)
  */
  bool writeFast( const void* buf, uint8_t len, const bool multicast );

  /**
   * This function extends the auto-retry mechanism to any specified duration.
   * It will not block until the 3 FIFO buffers are filled with data.
   * If so the library will auto retry until a new payload is written
   * or the user specified timeout period is reached.
   * @warning It is important to never keep the nRF24L01 in TX mode and FIFO full for more than 4ms at a time. If the auto
   * retransmit is enabled, the nRF24L01 is never in TX mode long enough to disobey this rule. Allow the FIFO
   * to clear by issuing txStandBy() or ensure appropriate time between transmissions.
   *
   * @code
   * Example (Full blocking):
   *
   *			radio.writeBlocking(&buf,32,1000); //Wait up to 1 second to write 1 payload to the buffers
   *			txStandBy(1000);     			   //Wait up to 1 second for the payload to send. Return 1 if ok, 0 if failed.
   *					  				   		   //Blocks only until user timeout or success. Data flushed on fail.
   * @endcode
   * @note If used from within an interrupt, the interrupt should be disabled until completion, and sei(); called to enable millis().
   * @see txStandBy()
   * @see write()
   * @see writeFast()
   *
   * @param buf Pointer to the data to be sent
   * @param len Number of bytes to be sent
   * @param timeout User defined timeout in milliseconds.
   * @return True if the payload was loaded into the buffer successfully false if not
   */
  bool writeBlocking( const void* buf, uint8_t len, uint32_t timeout );

  /**
   * This function should be called as soon as transmission is finished to
   * drop the radio back to STANDBY-I mode. If not issued, the radio will
   * remain in STANDBY-II mode which, per the data sheet, is not a recommended
   * operating mode.
   *
   * @note When transmitting data in rapid succession, it is still recommended by
   * the manufacturer to drop the radio out of TX or STANDBY-II mode if there is
   * time enough between sends for the FIFOs to empty. This is not required if auto-ack
   * is enabled.
   *
   * Relies on built-in auto retry functionality.
   *
   * @code
   * Example (Partial blocking):
   *
   *			radio.writeFast(&buf,32);
   *			radio.writeFast(&buf,32);
   *			radio.writeFast(&buf,32);  //Fills the FIFO buffers up
   *			bool ok = txStandBy();     //Returns 0 if failed. 1 if success.
   *					  				   //Blocks only until MAX_RT timeout or success. Data flushed on fail.
   * @endcode
   * @see txStandBy(unsigned long timeout)
   * @return True if transmission is successful
   *
   */
  bool txStandBy() {
    int32_t timeout = millis();
    // The following roughtly translates to:
	  // while the TX FIFO is not empty, check
	  // if the latest message is a failure and if it is, flush it.
	  while(!fifoStatus().txEmpty()) {
		  if(status().maxRetries()) {
		    flush_tx();
			  return false;
		  }
  
      if(millis() - timeout > 85) {
        return false;
      }
	  }

    io.ce(LOW);			   //Set STANDBY-I mode
	  return true;
  }

  /**
   * This function allows extended blocking and auto-retries per a user defined timeout
   * @code
   *	Fully Blocking Example:
   *
   *			radio.writeFast(&buf,32);
   *			radio.writeFast(&buf,32);
   *			radio.writeFast(&buf,32);   //Fills the FIFO buffers up
   *			bool ok = txStandBy(1000);  //Returns 0 if failed after 1 second of retries. 1 if success.
   *					  				    //Blocks only until user defined timeout or success. Data flushed on fail.
   * @endcode
   * @note If used from within an interrupt, the interrupt should be disabled until completion, and sei(); called to enable millis().
   * @param timeout Number of milliseconds to retry failed payloads
   * @return True if transmission is successful
   *
   */
   bool txStandBy(uint32_t timeout, bool startTx = 0);

  /**
   * Write an ack payload for the specified pipe
   *
   * The next time a message is received on @p pipe, the data in @p buf will
   * be sent back in the acknowledgement.
   * @see enableAckPayload()
   * @see enableDynamicPayloads()
   * @warning Only three of these can be pending at any time as there are only 3 FIFO buffers.<br> Dynamic payloads must be enabled.
   * @note Ack payloads are handled automatically by the radio chip when a payload is received. Users should generally
   * write an ack payload as soon as startListening() is called, so one is available when a regular payload is received.
   * @note Ack payloads are dynamic payloads. This only works on pipes 0&1 by default. Call 
   * enableDynamicPayloads() to enable on all pipes.
   *
   * @param pipe Which pipe# (typically 1-5) will get this response.
   * @param buf Pointer to data that is sent
   * @param len Length of the data to send, up to 32 bytes max.  Not affected
   * by the static payload set by setPayloadSize().
   */
  void writeAckPayload(uint8_t pipe, const void* buf, uint8_t len) {
    write_payload(buf, len, W_ACK_PAYLOAD | ( pipe & 0b111 ));
  }

  /**
   * Returns the status register, de-structured into bit fields.
   * @param reset if true, the interrupts will be cleared, making this an alternative to whatHappenned
   * @return the current status
   */
  Status status() {
      return command(NOP);
  }

  Status resetStatus() {
      return write_register(NRF_STATUS, _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT));
  }

  /**
   * Get the fifo status.
   * You can find out here if the device is ready to accept more
   * packets from the microcontroller or the air.
   */
  FifoStatus fifoStatus() {
    uint8_t status = read_register(FIFO_STATUS);
    return { status };
  }

  /**
   * Non-blocking write to the open writing pipe used for buffered writes
   *
   * @note Optimization: This function now leaves the CE pin high, so the radio
   * will remain in TX or STANDBY-II Mode until a txStandBy() command is issued. Can be used as an alternative to startWrite()
   * if writing multiple payloads at once.
   * @warning It is important to never keep the nRF24L01 in TX mode with FIFO full for more than 4ms at a time. If the auto
   * retransmit/autoAck is enabled, the nRF24L01 is never in TX mode long enough to disobey this rule. Allow the FIFO
   * to clear by issuing txStandBy() or ensure appropriate time between transmissions.
   *
   * @see write()
   * @see writeFast()
   * @see startWrite()
   * @see writeBlocking()
   *
   * For single noAck writes see:
   * @see enableDynamicAck()
   * @see setAutoAck()
   *
   * @param buf Pointer to the data to be sent
   * @param len Number of bytes to be sent
   * @param multicast Request ACK (0) or NOACK (1)
   * @return True if the payload was delivered successfully false if not
   */
  void startFastWrite(
    const void* buf, 
    uint8_t len, 
    const bool multicast, 
    bool startTx = true ) {
  	write_payload( buf, len,multicast ? W_TX_PAYLOAD_NO_ACK : W_TX_PAYLOAD ) ;
	  if(startTx){
		  io.ce(HIGH);
	  }
  }


  /**
   * Non-blocking write to the open writing pipe
   *
   * Just like write(), but it returns immediately. To find out what happened
   * to the send, catch the IRQ and then call whatHappened().
   *
   * @see write()
   * @see writeFast()
   * @see startFastWrite()
   * @see whatHappened()
   *
   * For single noAck writes see:
   * @see enableDynamicAck()
   * @see setAutoAck()
   *
   * @param buf Pointer to the data to be sent
   * @param len Number of bytes to be sent
   * @param multicast Request ACK (0) or NOACK (1)
   *
   */
  void startWrite( const void* buf, uint8_t len, const bool multicast );

  /**
   * This function is mainly used internally to take advantage of the auto payload
   * re-use functionality of the chip, but can be beneficial to users as well.
   *
   * The function will instruct the radio to re-use the data in the FIFO buffers,
   * and instructs the radio to re-send once the timeout limit has been reached.
   * Used by writeFast and writeBlocking to initiate retries when a TX failure
   * occurs. Retries are automatically initiated except with the standard write().
   * This way, data is not flushed from the buffer until switching between modes.
   *
   * @note This is to be used AFTER auto-retry fails if wanting to resend
   * using the built-in payload reuse features.
   * After issuing reUseTX(), it will keep reending the same payload forever or until
   * a payload is written to the FIFO, or a flush_tx command is given.
   */
   void reUseTX() {
		write_register(NRF_STATUS,_BV(MAX_RT) );			  //Clear max retry flag
		command( REUSE_TX_PL );
		io.ce(LOW);										  //Re-Transfer packet
		io.ce(HIGH);
  }

  /**
   * Empty the transmit buffer. This is generally not required in standard operation.
   * May be required in specific cases after stopListening() , if operating at 250KBPS data rate.
   *
   * @return Current value of status register
   */
  Status flush_tx(void) {
    return command(FLUSH_TX);
  }


  /**
   * Test whether there was a carrier on the line for the
   * previous listening period.
   *
   * Useful to check for interference on the current channel.
   *
   * @return true if was carrier, false if not
   */
  bool testCarrier(void);

  /**
   * Test whether a signal (carrier or otherwise) greater than
   * or equal to -64dBm is present on the channel. Valid only
   * on nRF24L01P (+) hardware. On nRF24L01, use testCarrier().
   *
   * Useful to check for interference on the current channel and
   * channel hopping strategies.
   *
   * @code
   * bool goodSignal = radio.testRPD();
   * if(radio.available()){
   *    Serial.println(goodSignal ? "Strong signal > 64dBm" : "Weak signal < 64dBm" );
   *    radio.read(0,0);
   * }
   * @endcode
   * @return true if signal => -64dBm, false if not
   */
  bool testRPD(void) ;

  /**
   * Test whether this is a real radio, or a mock shim for
   * debugging.  Setting either pin to 0xff is the way to
   * indicate that this is not a real radio.
   *
   * @return true if this is a legitimate radio
   */
  bool isValid() {
      return true;
  }
  
   /**
   * Close a pipe after it has been previously opened.
   * Can be safely called without having previously opened a pipe.
   * @param pipe Which pipe # to close, 0-5.
   */
  SetResult closeReadingPipe( uint8_t pipe ) {
    return set(Receive::pipe(pipe).disable());
  }

   /**
   * Enable error detection by un-commenting #define FAILURE_HANDLING in RF24_config.h
   * If a failure has been detected, it usually indicates a hardware issue. By default the library
   * will cease operation when a failure is detected.  
   * This should allow advanced users to detect and resolve intermittent hardware issues.  
   *   
   * In most cases, the radio must be re-enabled via radio.begin(); and the appropriate settings
   * applied after a failure occurs, if wanting to re-enable the device immediately.
   * 
   * Usage: (Failure handling must be enabled per above)
   *  @code
   *  if(radio.failureDetected){ 
   *    radio.begin();                       // Attempt to re-configure the radio with defaults
   *    radio.failureDetected = 0;           // Reset the detection value
   *	radio.openWritingPipe(addresses[1]); // Re-configure pipe addresses
   *    radio.openReadingPipe(1,addresses[0]);
   *    report_failure();                    // Blink leds, send a message, etc. to indicate failure
   *  }
   * @endcode
  */
  //#if defined (FAILURE_HANDLING)
    bool failureDetected; 
  //#endif
    
  /**@}*/

  /**@}*/
  /**
   * @name Optional Configurators
   *
   *  Methods you can use to get or set the configuration of the chip.
   *  None are required.  Calling begin() sets up a reasonable set of
   *  defaults.
   */
  /**@{*/

  /**
   * Get Dynamic Payload Size
   *
   * For dynamic payloads, this pulls the size of the payload off
   * the chip
   *
   * @note Corrupt packets are now detected and flushed per the
   * manufacturer.
   * @code
   * if(radio.available()){
   *   if(radio.getDynamicPayloadSize() < 1){
   *     // Corrupt payload has been flushed
   *     return; 
   *   }
   *   radio.read(&data,sizeof(data));
   * }
   * @endcode
   *
   * @return Payload length of last-received dynamic payload
   */
  uint8_t getDynamicPayloadSize(void) {
    uint8_t result = 0;

    io.beginTransaction();
    io.transfer( R_RX_PL_WID );
    result = io.transfer(0xff);
    io.endTransaction();

    return result;
  }

  /**
   * Enable dynamic ACKs (single write multicast or unicast) for chosen messages
   *
   * @note To enable full multicast or per-pipe multicast, use setAutoAck()
   *
   * @warning This MUST be called prior to attempting single write NOACK calls
   * @code
   * radio.enableDynamicAck();
   * radio.write(&data,32,1);  // Sends a payload with no acknowledgement requested
   * radio.write(&data,32,0);  // Sends a payload using auto-retry/autoACK
   * @endcode
   */
  void enableDynamicAck();

   /**
    * Enable custom payloads on the acknowledge packets
    *
    * Ack payloads are a handy way to return data back to senders without
    * manually changing the radio modes on both units.
    *
    * @note Ack payloads are dynamic payloads. This only works on pipes 0&1 by default. Call 
    * enableDynamicPayloads() to enable on all pipes.
    */
  SetResult enableAckPayload(void) {
    SetResult result = set(AckPayloads::ENABLE);
    if(result != ERROR) ackPayloads = true;
    return result;
  }

  
  /**
   * Set the transmission data rate
   *
   * @warning setting RF24_250KBPS will fail for non-plus units
   *
   * @param speed RF24_250KBPS for 250kbs, RF24_1MBPS for 1Mbps, or RF24_2MBPS for 2Mbps
   * @return true if the change was successful
   */
  SetResult set(DataRateOption rate) {
    SetResult result = set((SettingValue)rate);
    if(result != ERROR) txRxDelay = rate.txRxDelay;
    return result;
  }

  /**
  * The radio will generate interrupt signals when a transmission is complete,
  * a transmission fails, or a payload is received. This allows users to mask
  * those interrupts to prevent them from generating a signal on the interrupt
  * pin. Interrupts are enabled on the radio chip by default.
  *
  * @code
  * 	Mask all interrupts except the receive interrupt:
  *
  *		radio.maskIRQ(1,1,0);
  * @endcode
  *
  * @param tx_ok  Mask transmission complete interrupts
  * @param tx_fail  Mask transmit failure interrupts
  * @param rx_ready Mask payload received interrupts
  */
  void maskIRQ(bool tx_ok,bool tx_fail,bool rx_ready);
  
  /**@}*/
  /**
   * @name Deprecated
   *
   *  Methods provided for backwards compabibility.
   */
  /**@{*/


private:

  /**
   * @name Low-level internal interface.
   *
   *  Protected methods that address the chip directly.  Regular users cannot
   *  ever call these.  They are documented for completeness and for developers who
   *  may want to extend this class.
   */
  /**@{*/

  /**
   * Read single byte from a register
   *
   * @param reg Which register. Use constants from nRF24L01.h
   * @return Current value of register @p reg
   */
  uint8_t read_register(uint8_t reg) {
    uint8_t result;

    io.beginTransaction();
    io.transfer(R_REGISTER | (REGISTER_MASK & reg));
    result = io.transfer(0xff);
    io.endTransaction();

    return result;
  }

  /**
   * Write a single byte to a register
   *
   * @param reg Which register. Use constants from nRF24L01.h
   * @param value The new value to write
   * @return Current value of status register
   */
  Status write_register(uint8_t reg, uint8_t value) {
    io.beginTransaction();
    uint8_t status = io.transfer(W_REGISTER | (REGISTER_MASK & reg));
    io.transfer(value);
    io.endTransaction();

    return { status };
  }

  Status write_register(uint8_t reg, const uint8_t values[], uint8_t len) {
    io.beginTransaction();
    uint8_t status = io.transfer(W_REGISTER | (REGISTER_MASK & reg));
    io.transfern(values, len);
    io.endTransaction();
    return { status };
  }

  /**
   * Write the transmit payload
   *
   * The size of data written is the fixed payload size, see getPayloadSize()
   *
   * @param buf Where to get the data
   * @param len Number of bytes to be sent
   * @return Current value of status register
   */
  uint8_t write_payload(const void *buf, uint8_t len, const uint8_t writeType) {
    uint8_t status;
    const uint8_t *current = reinterpret_cast<const uint8_t *>(buf);
    len = rf24_min(len, RF24_MAX_PAYLOAD);

    io.beginTransaction();
    status = io.transfer(writeType);
    io.transfern(current, len);
    io.endTransaction();

    return status;
  }

  /**
   * Read the receive payload
   * @param buf Where to put the data
   * @param len Maximum number of bytes to read
   * @return Current value of status register
   */
  uint8_t read_payload(void *buf, uint8_t len) {
    uint8_t status;
    uint8_t *current = reinterpret_cast<uint8_t *>(buf);

    len = rf24_min(len, RF24_MAX_PAYLOAD);

    io.beginTransaction();
    status = io.transfer(R_RX_PAYLOAD);
    while (len--) {
      *current++ = io.transfer(0xFF);
    }

    io.endTransaction();

    return status;
  }

  /**
   * Empty the receive buffer
   *
   * @return Current value of status register
   */
  Status flush_rx(void) {
    return command(FLUSH_RX);
  }

    /**
   * Turn on or off the special features of the chip
   *
   * The chip has certain 'features' which are only available when the 'features'
   * are enabled.  See the datasheet for details.
   */
  void toggle_features(void) {

  }

  /**
   * Built in spi transfer function to simplify repeating code repeating code
   */
  Status command(uint8_t cmd) {
    uint8_t status;

    io.beginTransaction();
    status = io.transfer( cmd );
    io.endTransaction();

    return { status };
  }
  
  /**@}*/

};


#endif // __RF24_H__


