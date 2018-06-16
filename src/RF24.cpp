/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

#include "nRF24L01.h"
#include "RF24.h"

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::read_register(uint8_t reg, uint8_t* buf, uint8_t len)
{
  uint8_t status;

  io.beginTransaction();
  status = io.transfer( R_REGISTER | ( REGISTER_MASK & reg ) );
  while ( len-- ){
    *buf++ = io.transfer(0xff);
  }
  io.endTransaction();

  return status;
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::read_register(uint8_t reg)
{
  uint8_t result;

  io.beginTransaction();
  io.transfer( R_REGISTER | ( REGISTER_MASK & reg ) );
  result = io.transfer(0xff);
  io.endTransaction();

  return result;
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::write_register(uint8_t reg, const uint8_t* buf, uint8_t len)
{
  uint8_t status;

  io.beginTransaction();
  status = io.transfer( W_REGISTER | ( REGISTER_MASK & reg ) );
  io.transfern(buf, len);
  io.endTransaction();

  return status;
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::write_register(uint8_t reg, uint8_t value)
{
  uint8_t status;

  io.beginTransaction();
  status = io.transfer( W_REGISTER | ( REGISTER_MASK & reg ) );
  io.transfer(value);
  io.endTransaction();

  return status;
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::write_payload(const void* buf, uint8_t data_len, const uint8_t writeType)
{
  uint8_t status;
  const uint8_t* current = reinterpret_cast<const uint8_t*>(buf);

  data_len = rf24_min(data_len, payload_size);
  uint8_t blank_len = dynamic_payloads_enabled ? 0 : payload_size - data_len;

  io.beginTransaction();
  status = io.transfer( writeType );
  io.transfern(current, data_len);
  while ( blank_len-- ) {
    io.transfer(0);
  }  
  io.endTransaction();

  return status;
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::read_payload(void* buf, uint8_t data_len)
{
  uint8_t status;
  uint8_t* current = reinterpret_cast<uint8_t*>(buf);

  if(data_len > payload_size) data_len = payload_size;
  uint8_t blank_len = dynamic_payloads_enabled ? 0 : payload_size - data_len;

  io.beginTransaction();
  status = io.transfer( R_RX_PAYLOAD );
  while ( data_len-- ) {
    *current++ = io.transfer(0xFF);
  }
  while ( blank_len-- ) {
    io.transfer(0xff);
  }
  io.endTransaction();

  return status;
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::flush_rx(void)
{
  return spiTrans( FLUSH_RX );
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::flush_tx(void)
{
  return spiTrans( FLUSH_TX );
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::spiTrans(uint8_t cmd)
{
  uint8_t status;

  io.beginTransaction();
  status = io.transfer( cmd );
  io.endTransaction();

  return status;
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::get_status(void)
{
  return spiTrans(NOP);
}

/****************************************************************************/

/****************************************************************************/

template<typename IO>
void RF24<IO>::setChannel(uint8_t channel)
{
  const uint8_t max_channel = 127;
  write_register(RF_CH,rf24_min(channel,max_channel));
}

template<typename IO>
uint8_t RF24<IO>::getChannel()
{
  return read_register(RF_CH);
}
/****************************************************************************/

template<typename IO>
void RF24<IO>::setPayloadSize(uint8_t size)
{
  payload_size = rf24_min(size,32);
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::getPayloadSize(void)
{
  return payload_size;
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::begin(void)
{

  uint8_t setup=0;

  io.begin();
  io.ce(LOW);

  // Must allow the radio time to settle else configuration bits will not necessarily stick.
  // This is actually only required following power up but some settling time also appears to
  // be required after resets too. For full coverage, we'll always assume the worst.
  // Enabling 16b CRC is by far the most obvious case if the wrong timing is used - or skipped.
  // Technically we require 4.5ms + 14us as a worst case. We'll just call it 5ms for good measure.
  // WARNING: Delay is based on P-variant whereby non-P *may* require different timing.
  delay( 5 ) ;

  // Reset CONFIG and enable 16-bit CRC.
  write_register( CONFIG, 0b00001100 ) ;

  // Set 1500uS (minimum for 32B payload in ESB@250KBPS) timeouts, to make testing a little easier
  // WARNING: If this is ever lowered, either 250KBS mode with AA is broken or maximum packet
  // sizes must never be used. See documentation for a more complete explanation.
  setRetries(5,15);

  // Reset value is MAX
  //setPALevel( RF24_PA_MAX ) ;

  // check for connected module and if this is a p nRF24l01 variant
  //
  if( setDataRate( RF24_250KBPS ) )
  {
    p_variant = true ;
  }
  /*setup = read_register(RF_SETUP);
  if( setup == 0b00001110 )     // register default for nRF24L01P
  {
    p_variant = true ;
  }*/
  
  // Then set the data rate to the slowest (and most reliable) speed supported by all
  // hardware.
  setDataRate(RF24_1MBPS) ;

  // Initialize CRC and request 2-byte (16bit) CRC
  //setCRCLength( RF24_CRC_16 ) ;

  // Disable dynamic payloads, to match dynamic_payloads_enabled setting - Reset value is 0
  toggle_features();
  write_register(FEATURE,0 );
  write_register(DYNPD,0);

  // Reset current status
  // Notice reset and flush is the last thing we do
  write_register(NRF_STATUS,_BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  // Set up default configuration.  Callers can always change it later.
  // This channel should be universally safe and not bleed over into adjacent
  // spectrum.
  setChannel(76);

  // Flush buffers
  flush_rx();
  flush_tx();

  powerUp(); //Power up by default when begin() is called

  // Enable PTX, do not write CE high so radio will remain in standby I mode ( 130us max to transition to RX or TX instead of 1500us from powerUp )
  // PTX should use only 22uA of power
  write_register(CONFIG, ( read_register(CONFIG) ) & ~_BV(PRIM_RX) );

  // if setup is 0 or ff then there was no response from module
  return ( setup != 0 && setup != 0xff );
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::startListening(void)
{
#if !defined (RF24_TINY) && ! defined(LITTLEWIRE)
    powerUp();
#endif
    write_register(CONFIG, read_register(CONFIG) | _BV(PRIM_RX));
    write_register(NRF_STATUS, _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );
    io.ce(HIGH);
    // Restore the pipe0 adddress, if exists
    if (pipe0_reading_address[0] > 0){
        write_register(RX_ADDR_P0, pipe0_reading_address, addr_width);
    }else{
        closeReadingPipe(0);
    }

    // Flush buffers
    //flush_rx();
    if(read_register(FEATURE) & _BV(EN_ACK_PAY)){
        flush_tx();
    }

    // Go!
    //delayMicroseconds(100);
}

/****************************************************************************/
static const uint8_t child_pipe_enable[] PROGMEM =
{
  ERX_P0, ERX_P1, ERX_P2, ERX_P3, ERX_P4, ERX_P5
};

template<typename IO>
void RF24<IO>::stopListening(void)
{
    io.ce(LOW);

    delayMicroseconds(txRxDelay);

    if(read_register(FEATURE) & _BV(EN_ACK_PAY)){
        delayMicroseconds(txRxDelay); //200
        flush_tx();
    }

    //flush_rx();
    write_register(CONFIG, ( read_register(CONFIG) ) & ~_BV(PRIM_RX) );

    write_register(EN_RXADDR,read_register(EN_RXADDR) | _BV(pgm_read_byte(&child_pipe_enable[0]))); // Enable RX on pipe0
  //delayMicroseconds(100);

}

/****************************************************************************/

template<typename IO>
void RF24<IO>::powerDown(void)
{
  io.ce(LOW); // Guarantee CE is low on powerDown
  write_register(CONFIG,read_register(CONFIG) & ~_BV(PWR_UP));
}

/****************************************************************************/

//Power up now. Radio will not power down unless instructed by MCU for config changes etc.
template<typename IO>
void RF24<IO>::powerUp(void)
{
   uint8_t cfg = read_register(CONFIG);

   // if not powered up then power up and wait for the radio to initialize
   if (!(cfg & _BV(PWR_UP))){
      write_register(CONFIG,read_register(CONFIG) | _BV(PWR_UP));

      // For nRF24L01+ to go from power down mode to TX or RX mode it must first pass through stand-by mode.
	  // There must be a delay of Tpd2stby (see Table 16.) after the nRF24L01+ leaves power down mode before
	  // the CEis set high. - Tpd2stby can be up to 5ms per the 1.0 datasheet
      delay(5);
   }
}


/******************************************************************/

//Similar to the previous write, clears the interrupt flags
template<typename IO>
bool RF24<IO>::write( const void* buf, uint8_t len, const bool multicast)
{
	//Start Writing
	startFastWrite(buf,len,multicast);

	//Wait until complete or failed
	#if defined (FAILURE_HANDLING) || defined (RF24_LINUX)
		uint32_t timer = millis();
	#endif 
	
	while( ! ( get_status()  & ( _BV(TX_DS) | _BV(MAX_RT) ))) { 
		#if defined (FAILURE_HANDLING) || defined (RF24_LINUX)
			if(millis() - timer > 85){			
				errNotify();
				#if defined (FAILURE_HANDLING)
				  return 0;		
				#else
				  delay(100);
				#endif
			}
		#endif
	}
    
	io.ce(LOW);

	uint8_t status = write_register(NRF_STATUS,_BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  //Max retries exceeded
  if( status & _BV(MAX_RT)){
  	flush_tx(); //Only going to be 1 packet int the FIFO at a time using this method, so just flush
  	return 0;
  }
	//TX OK 1 or 0
  return 1;
}

template<typename IO>
bool RF24<IO>::write( const void* buf, uint8_t len ){
	return write(buf,len,false);
}
/****************************************************************************/

//For general use, the interrupt flags are not important to clear
template<typename IO>
bool RF24<IO>::writeBlocking( const void* buf, uint8_t len, uint32_t timeout )
{
	//Block until the FIFO is NOT full.
	//Keep track of the MAX retries and set auto-retry if seeing failures
	//This way the FIFO will fill up and allow blocking until packets go through
	//The radio will auto-clear everything in the FIFO as long as CE remains high

	uint32_t timer = millis();							  //Get the time that the payload transmission started

	while( ( get_status()  & ( _BV(TX_FULL) ))) {		  //Blocking only if FIFO is full. This will loop and block until TX is successful or timeout

		if( get_status() & _BV(MAX_RT)){					  //If MAX Retries have been reached
			reUseTX();										  //Set re-transmit and clear the MAX_RT interrupt flag
			if(millis() - timer > timeout){ return 0; }		  //If this payload has exceeded the user-defined timeout, exit and return 0
		}
		#if defined (FAILURE_HANDLING) || defined (RF24_LINUX)
			if(millis() - timer > (timeout+85) ){			
				errNotify();
				#if defined (FAILURE_HANDLING)
				return 0;			
                #endif				
			}
		#endif

  	}

  	//Start Writing
	startFastWrite(buf,len,true);								  //Write the payload if a buffer is clear

	return 1;												  //Return 1 to indicate successful transmission
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::reUseTX(){
		write_register(NRF_STATUS,_BV(MAX_RT) );			  //Clear max retry flag
		spiTrans( REUSE_TX_PL );
		io.ce(LOW);										  //Re-Transfer packet
		io.ce(HIGH);
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::txFifoFull(void) {
    return get_status()  & _BV(TX_FULL);
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::writeFast( const void* buf, uint8_t len, const bool multicast )
{
	//Block until the FIFO is NOT full.
	//Keep track of the MAX retries and set auto-retry if seeing failures
	//Return 0 so the user can control the retrys and set a timer or failure counter if required
	//The radio will auto-clear everything in the FIFO as long as CE remains high

	#if defined (FAILURE_HANDLING) || defined (RF24_LINUX)
		uint32_t timer = millis();
	#endif
	
	while(txFifoFull()) {			  //Blocking only if FIFO is full. This will loop and block until TX is successful or fail

		if( get_status() & _BV(MAX_RT)){
			//reUseTX();										  //Set re-transmit
			write_register(NRF_STATUS,_BV(MAX_RT) );			  //Clear max retry flag
			return 0;										  //Return 0. The previous payload has been retransmitted
															  //From the user perspective, if you get a 0, just keep trying to send the same payload
		}
		#if defined (FAILURE_HANDLING) || defined (RF24_LINUX)
			if(millis() - timer > 85 ){			
				errNotify();
				#if defined (FAILURE_HANDLING)
				return 0;
				#endif
			}
		#endif
  	}
		     //Start Writing
	startFastWrite(buf,len,multicast);

	return 1;
}

template<typename IO>
bool RF24<IO>::writeFast( const void* buf, uint8_t len ){
	return writeFast(buf,len,0);
}

/****************************************************************************/

//Per the documentation, we want to set PTX Mode when not listening. Then all we do is write data and set CE high
//In this mode, if we can keep the FIFO buffers loaded, packets will transmit immediately (no 130us delay)
//Otherwise we enter Standby-II mode, which is still faster than standby mode
//Also, we remove the need to keep writing the config register over and over and delaying for 150 us each time if sending a stream of data

template<typename IO>
void RF24<IO>::startFastWrite(const void* buf, uint8_t len, const bool multicast, bool startTx){ //TMRh20

	//write_payload( buf,len);
	write_payload( buf, len,multicast ? W_TX_PAYLOAD_NO_ACK : W_TX_PAYLOAD ) ;
	if(startTx){
		io.ce(HIGH);
	}

}

/****************************************************************************/

//Added the original startWrite back in so users can still use interrupts, ack payloads, etc
//Allows the library to pass all tests
template<typename IO>
void RF24<IO>::startWrite( const void* buf, uint8_t len, const bool multicast ){

  // Send the payload

  //write_payload( buf, len );
  write_payload( buf, len,multicast? W_TX_PAYLOAD_NO_ACK : W_TX_PAYLOAD ) ;
  io.ce(HIGH);
  #if defined(CORE_TEENSY) || !defined(ARDUINO) || defined (RF24_BBB) || defined (RF24_DUE)
	delayMicroseconds(10);
  #endif
  io.ce(LOW);


}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::rxFifoFull(){
	return read_register(FIFO_STATUS) & _BV(RX_FULL);
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::txFifoEmpty(){
    return read_register(FIFO_STATUS) & _BV(TX_EMPTY);
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::txReset() {
    io.ce(LOW); // Enter STANDBY-I mode
    flush_tx(); // Flush the transmit buffer
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::txStandBy(){

    #if defined (FAILURE_HANDLING) || defined (RF24_LINUX)
		uint32_t timeout = millis();
	#endif
	// The following roughtly translates to:
	// while the TX FIFO is not empty, check
	// if the latest message is a failure and if it is, flush it.
	while(!txFifoEmpty()) {
		if( get_status() & _BV(MAX_RT)) {
		    flush_tx();
			return false;
		}
		#if defined (FAILURE_HANDLING) || defined (RF24_LINUX)
			if( millis() - timeout > 85){
				errNotify();
				#if defined (FAILURE_HANDLING)
				return 0;	
				#endif
			}
		#endif
	}

	io.ce(LOW);			   //Set STANDBY-I mode
	return true;
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::txStandBy(uint32_t timeout, bool startTx){

    if(startTx){
	  stopListening();
	  io.ce(HIGH);
	}
	uint32_t start = millis();

	while( ! (read_register(FIFO_STATUS) & _BV(TX_EMPTY)) ){
		if( get_status() & _BV(MAX_RT)){
			write_register(NRF_STATUS,_BV(MAX_RT) );
				io.ce(LOW);										  //Set re-transmit
				io.ce(HIGH);
				if(millis() - start >= timeout){
					io.ce(LOW); flush_tx(); return 0;
				}
		}
		#if defined (FAILURE_HANDLING) || defined (RF24_LINUX)
			if( millis() - start > (timeout+85)){
				errNotify();
				#if defined (FAILURE_HANDLING)
				return 0;	
				#endif
			}
		#endif
	}

	
	io.ce(LOW);				   //Set STANDBY-I mode
	return 1;

}

/****************************************************************************/

template<typename IO>
void RF24<IO>::maskIRQ(bool tx, bool fail, bool rx){
	write_register(CONFIG, ( read_register(CONFIG) ) | fail << MASK_MAX_RT | tx << MASK_TX_DS | rx << MASK_RX_DR  );
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::getDynamicPayloadSize(void)
{
  uint8_t result = 0;

  io.beginTransaction();
  io.transfer( R_RX_PL_WID );
  result = io.transfer(0xff);
  io.endTransaction();

  return result;
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::available(void)
{
  return available(NULL);
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::available(uint8_t* pipe_num)
{
  if (!( read_register(FIFO_STATUS) & _BV(RX_EMPTY) )){

    // If the caller wants the pipe number, include that
    if ( pipe_num ){
	  uint8_t status = get_status();
      *pipe_num = ( status >> RX_P_NO ) & 0b111;
  	}
  	return 1;
  }


  return 0;


}

/****************************************************************************/

template<typename IO>
void RF24<IO>::read( void* buf, uint8_t len ){

  // Fetch the payload
  read_payload( buf, len );

  //Clear the two possible interrupt flags with one command
  write_register(NRF_STATUS,_BV(RX_DR) | _BV(MAX_RT) | _BV(TX_DS) );

}

/****************************************************************************/

template<typename IO>
void RF24<IO>::whatHappened(bool &tx_ok,bool &tx_fail,bool &rx_ready)
{
  // Read the status & reset the status in one easy call
  // Or is that such a good idea?
  uint8_t status = write_register(NRF_STATUS, _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  // Report to the user what happened
  tx_ok = status & _BV(TX_DS);
  tx_fail = status & _BV(MAX_RT);
  rx_ready = status & _BV(RX_DR);
}

/****************************************************************************/

template<typename IO>
Status RF24<IO>::status(bool reset)
{
  Status sstruct;
  // Either read and reset the interrupts in a single operation, or just read
  uint8_t status = reset ? write_register(NRF_STATUS, _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) ) : read_register(NRF_STATUS);

  sstruct.max_rt = status & _BV(MAX_RT);
  sstruct.rx_dr = status & _BV(RX_DR);
  sstruct.tx_ds = status & _BV(TX_DS);
  sstruct.rx_p_no = (status >> RX_P_NO) & 0b111;
  sstruct.tx_full = status & _BV(TX_FULL);

  return sstruct;
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::openWritingPipe(uint64_t value)
{
  // Note that AVR 8-bit uC's store this LSB first, and the NRF24L01(+)
  // expects it LSB first too, so we're good.

  write_register(RX_ADDR_P0, reinterpret_cast<uint8_t*>(&value), addr_width);
  write_register(TX_ADDR, reinterpret_cast<uint8_t*>(&value), addr_width);
  
  
  //const uint8_t max_payload_size = 32;
  //write_register(RX_PW_P0,rf24_min(payload_size,max_payload_size));
  write_register(RX_PW_P0,payload_size);
}

/****************************************************************************/
template<typename IO>
void RF24<IO>::openWritingPipe(const uint8_t *address)
{
  // Note that AVR 8-bit uC's store this LSB first, and the NRF24L01(+)
  // expects it LSB first too, so we're good.

  write_register(RX_ADDR_P0,address, addr_width);
  write_register(TX_ADDR, address, addr_width);

  //const uint8_t max_payload_size = 32;
  //write_register(RX_PW_P0,rf24_min(payload_size,max_payload_size));
  write_register(RX_PW_P0,payload_size);
}

/****************************************************************************/
static const uint8_t child_pipe[] PROGMEM =
{
  RX_ADDR_P0, RX_ADDR_P1, RX_ADDR_P2, RX_ADDR_P3, RX_ADDR_P4, RX_ADDR_P5
};
static const uint8_t child_payload_size[] PROGMEM =
{
  RX_PW_P0, RX_PW_P1, RX_PW_P2, RX_PW_P3, RX_PW_P4, RX_PW_P5
};


template<typename IO>
void RF24<IO>::openReadingPipe(uint8_t child, uint64_t address)
{
  // If this is pipe 0, cache the address.  This is needed because
  // openWritingPipe() will overwrite the pipe 0 address, so
  // startListening() will have to restore it.
  if (child == 0){
    memcpy(pipe0_reading_address,&address,addr_width);
  }

  if (child <= 6)
  {
    // For pipes 2-5, only write the LSB
    if ( child < 2 )
      write_register(pgm_read_byte(&child_pipe[child]), reinterpret_cast<const uint8_t*>(&address), addr_width);
    else
      write_register(pgm_read_byte(&child_pipe[child]), reinterpret_cast<const uint8_t*>(&address), 1);

    write_register(pgm_read_byte(&child_payload_size[child]),payload_size);

    // Note it would be more efficient to set all of the bits for all open
    // pipes at once.  However, I thought it would make the calling code
    // more simple to do it this way.
    write_register(EN_RXADDR,read_register(EN_RXADDR) | _BV(pgm_read_byte(&child_pipe_enable[child])));
  }
}

/****************************************************************************/
template<typename IO>
void RF24<IO>::setAddressWidth(uint8_t a_width){

	if(a_width -= 2){
		write_register(SETUP_AW,a_width%4);
		addr_width = (a_width%4) + 2;
	}

}

/****************************************************************************/

template<typename IO>
void RF24<IO>::openReadingPipe(uint8_t child, const uint8_t *address)
{
  // If this is pipe 0, cache the address.  This is needed because
  // openWritingPipe() will overwrite the pipe 0 address, so
  // startListening() will have to restore it.
  if (child == 0){
    memcpy(pipe0_reading_address,address,addr_width);
  }
  if (child <= 6)
  {
    // For pipes 2-5, only write the LSB
    if ( child < 2 ){
      write_register(pgm_read_byte(&child_pipe[child]), address, addr_width);
    }else{
      write_register(pgm_read_byte(&child_pipe[child]), address, 1);
	}
    write_register(pgm_read_byte(&child_payload_size[child]),payload_size);

    // Note it would be more efficient to set all of the bits for all open
    // pipes at once.  However, I thought it would make the calling code
    // more simple to do it this way.
    write_register(EN_RXADDR,read_register(EN_RXADDR) | _BV(pgm_read_byte(&child_pipe_enable[child])));

  }
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::closeReadingPipe( uint8_t pipe )
{
  write_register(EN_RXADDR,read_register(EN_RXADDR) & ~_BV(pgm_read_byte(&child_pipe_enable[pipe])));
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::toggle_features(void)
{
    io.beginTransaction();
    io.transfer( ACTIVATE );
    io.transfer( 0x73 );
    io.endTransaction();
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::enableDynamicPayloads(void)
{
  // Enable dynamic payload throughout the system

    //toggle_features();
    write_register(FEATURE,read_register(FEATURE) | _BV(EN_DPL) );

  // Enable dynamic payload on all pipes
  //
  // Not sure the use case of only having dynamic payload on certain
  // pipes, so the library does not support it.
  write_register(DYNPD,read_register(DYNPD) | _BV(DPL_P5) | _BV(DPL_P4) | _BV(DPL_P3) | _BV(DPL_P2) | _BV(DPL_P1) | _BV(DPL_P0));

  dynamic_payloads_enabled = true;
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::enableAckPayload(void)
{
  //
  // enable ack payload and dynamic payload features
  //

  write_register(FEATURE,read_register(FEATURE) | _BV(EN_ACK_PAY) | _BV(EN_DPL) );
  //
  // Enable dynamic payload on pipes 0 & 1
  //

  write_register(DYNPD,read_register(DYNPD) | _BV(DPL_P1) | _BV(DPL_P0));
  dynamic_payloads_enabled = true;
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::enableDynamicAck(void){
  //
  // enable dynamic ack features
  //
    //toggle_features();
    write_register(FEATURE,read_register(FEATURE) | _BV(EN_DYN_ACK) );
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::writeAckPayload(uint8_t pipe, const void* buf, uint8_t len)
{
  const uint8_t* current = reinterpret_cast<const uint8_t*>(buf);
  uint8_t data_len = rf24_min(len,32);

  io.beginTransaction();
  io.transfer(W_ACK_PAYLOAD | ( pipe & 0b111 ) );
  io.transfern(current, data_len);
  io.endTransaction();

}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::isAckPayloadAvailable(void)
{
  return ! (read_register(FIFO_STATUS) & _BV(RX_EMPTY));
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::isPVariant(void)
{
  return p_variant ;
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::setAutoAck(bool enable)
{
    write_register(EN_AA, enable ? 0b111111 : 0);
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::setAutoAck(uint8_t pipe, bool enable)
{
  if ( pipe <= 6 )
  {
    uint8_t en_aa = read_register( EN_AA ) ;
    if( enable )
    {
      en_aa |= _BV(pipe) ;
    }
    else
    {
      en_aa &= ~_BV(pipe) ;
    }
    write_register( EN_AA, en_aa ) ;
  }
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::testCarrier(void)
{
  return ( read_register(CD) & 1 );
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::testRPD(void)
{
  return ( read_register(RPD) & 1 ) ;
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::setPALevel(uint8_t level)
{

  uint8_t setup = read_register(RF_SETUP) & 0b11111000;

  if(level > 3){  						// If invalid level, go to max PA
	  level = (RF24_PA_MAX << 1) + 1;		// +1 to support the SI24R1 chip extra bit
  }else{
	  level = (level << 1) + 1;	 		// Else set level as requested
  }


  write_register( RF_SETUP, setup |= level ) ;	// Write it to the chip
}

/****************************************************************************/

template<typename IO>
uint8_t RF24<IO>::getPALevel(void)
{

  return (read_register(RF_SETUP) & (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH))) >> 1 ;
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::setDataRate(rf24_datarate_e speed)
{
  bool result = false;
  uint8_t setup = read_register(RF_SETUP) ;

  // HIGH and LOW '00' is 1Mbs - our default
  setup &= ~(_BV(RF_DR_LOW) | _BV(RF_DR_HIGH)) ;

  if( speed == RF24_250KBPS )
  {
    // Must set the RF_DR_LOW to 1; RF_DR_HIGH (used to be RF_DR) is already 0
    // Making it '10'.
    setup |= _BV( RF_DR_LOW ) ;
    txRxDelay=RF24_250KBPS_TX_RX_DELAY;
  }
  else if ( speed == RF24_2MBPS )
  {
     // Set 2Mbs, RF_DR (RF_DR_HIGH) is set 1
     // Making it '01'
     setup |= _BV(RF_DR_HIGH);
     txRxDelay=RF24_2MBPS_TX_RX_DELAY;
  }
  else {
      txRxDelay=RF24_1MBPS_TX_RX_DELAY;
  }
  write_register(RF_SETUP,setup);

  // Verify our result
  if ( read_register(RF_SETUP) == setup )
  {
    result = true;
  }
  return result;
}

/****************************************************************************/

template<typename IO>
rf24_datarate_e RF24<IO>::getDataRate( void )
{
  rf24_datarate_e result ;
  uint8_t dr = read_register(RF_SETUP) & (_BV(RF_DR_LOW) | _BV(RF_DR_HIGH));

  // switch uses RAM (evil!)
  // Order matters in our case below
  if ( dr == _BV(RF_DR_LOW) )
  {
    // '10' = 250KBPS
    result = RF24_250KBPS ;
  }
  else if ( dr == _BV(RF_DR_HIGH) )
  {
    // '01' = 2MBPS
    result = RF24_2MBPS ;
  }
  else
  {
    // '00' = 1MBPS
    result = RF24_1MBPS ;
  }
  return result ;
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::setCRCLength(rf24_crclength_e length)
{
  uint8_t config = read_register(CONFIG) & ~( _BV(CRCO) | _BV(EN_CRC)) ;

  // switch uses RAM (evil!)
  if ( length == RF24_CRC_DISABLED )
  {
    // Do nothing, we turned it off above.
  }
  else if ( length == RF24_CRC_8 )
  {
    config |= _BV(EN_CRC);
  }
  else
  {
    config |= _BV(EN_CRC);
    config |= _BV( CRCO );
  }
  write_register( CONFIG, config ) ;
}

/****************************************************************************/

template<typename IO>
rf24_crclength_e RF24<IO>::getCRCLength(void)
{
  rf24_crclength_e result = RF24_CRC_DISABLED;
  
  uint8_t config = read_register(CONFIG) & ( _BV(CRCO) | _BV(EN_CRC)) ;
  uint8_t AA = read_register(EN_AA);
  
  if ( config & _BV(EN_CRC ) || AA)
  {
    if ( config & _BV(CRCO) )
      result = RF24_CRC_16;
    else
      result = RF24_CRC_8;
  }

  return result;
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::disableCRC( void )
{
  uint8_t disable = read_register(CONFIG) & ~_BV(EN_CRC) ;
  write_register( CONFIG, disable ) ;
}

/****************************************************************************/
template<typename IO>
void RF24<IO>::setRetries(uint8_t delay, uint8_t count)
{
 write_register(SETUP_RETR,(delay&0xf)<<ARD | (count&0xf)<<ARC);
}

/****************************************************************************/

template<typename IO>
RF24<IO>::RF24(IO io) : io(io), p_variant(false), payload_size(32), dynamic_payloads_enabled(false), addr_width(5) {
}
