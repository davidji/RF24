/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

#include "nRF24L01.h"
#include "RF24.h"



/******************************************************************/

//Similar to the previous write, clears the interrupt flags
template<typename IO>
bool RF24<IO>::write(const void* buf, uint8_t len, const bool multicast)
{
	//Start Writing
	startFastWrite(buf,len,multicast);

	//Wait until complete or failed
	#if defined (FAILURE_HANDLING) || defined (RF24_LINUX)
		uint32_t timer = millis();
	#endif 
	
	for(Status s = status();
    ! (s.dataSent() || s.maxRetries());
     s = status()) { 
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

  if(status(true).maxRetries()){
  	flush_tx(); //Only going to be 1 packet int the FIFO at a time using this method, so just flush
  	return false;
  }

  return true;
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

	while(status().txFifoFull()) {		  //Blocking only if FIFO is full. This will loop and block until TX is successful or timeout

		if(status().maxRetries()){					  //If MAX Retries have been reached
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
bool RF24<IO>::writeFast( const void* buf, uint8_t len, const bool multicast )
{
	//Block until the FIFO is NOT full.
	//Keep track of the MAX retries and set auto-retry if seeing failures
	//Return 0 so the user can control the retrys and set a timer or failure counter if required
	//The radio will auto-clear everything in the FIFO as long as CE remains high

	#if defined (FAILURE_HANDLING) || defined (RF24_LINUX)
    uint32_t timer = millis();
	#endif
	
	while(status().txFifoFull()) {			  //Blocking only if FIFO is full. This will loop and block until TX is successful or fail

		if(status().maxRetries()) {

			write_register(NRF_STATUS,_BV(MAX_RT));			  //Clear max retry flag
			return false;										  //Return 0. The previous payload has been retransmitted
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
	return writeFast(buf,len,false);
}

/****************************************************************************/

//Added the original startWrite back in so users can still use interrupts, ack payloads, etc
//Allows the library to pass all tests
template<typename IO>
void RF24<IO>::startWrite( const void* buf, uint8_t len, const bool multicast ) {
  write_payload( buf, len,multicast? W_TX_PAYLOAD_NO_ACK : W_TX_PAYLOAD ) ;
  io.ce(HIGH);
  #if defined(CORE_TEENSY) || !defined(ARDUINO) || defined (RF24_BBB) || defined (RF24_DUE)
	delayMicroseconds(10);
  #endif
  io.ce(LOW);


}

/****************************************************************************/

template<typename IO>
void RF24<IO>::txReset() {
    io.ce(LOW); // Enter STANDBY-I mode
    flush_tx(); // Flush the transmit buffer
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::txStandBy(uint32_t timeout, bool startTx){

    if(startTx){
	  stopListening();
	  io.ce(HIGH);
	}
	uint32_t start = millis();

	while(!fifoStatus().txEmpty())) {
		if(status().maxRetries()){
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
	return true;

}

/****************************************************************************/

template<typename IO>
void RF24<IO>::maskIRQ(bool tx, bool fail, bool rx){
	write_register(CONFIG, ( read_register(CONFIG) ) | fail << MASK_MAX_RT | tx << MASK_TX_DS | rx << MASK_RX_DR  );
}

/****************************************************************************/

template<typename IO>
void RF24<IO>::read( void* buf, uint8_t len )
/****************************************************************************/

/****************************************************************************/

template<typename IO>
void RF24<IO>::toggle_features(void)
{
    io.beginTransaction();
    // This seems like a command - but it's not a command mentioned
    // in the data sheet. Perhaps it's a command for a compatible chip?
    io.transfer( ACTIVATE );
    io.transfer( 0x73 );
    io.endTransaction();
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::testCarrier(void) {
  return (read_register(CD) & 1);
}

/****************************************************************************/

template<typename IO>
bool RF24<IO>::testRPD(void) {
  return ( read_register(RPD) & 1 ) ;
}

/****************************************************************************/

template<typename IO>
RF24<IO>::RF24(IO io) : io(io) {
}
