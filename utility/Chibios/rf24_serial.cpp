#include "ch.hpp"
#include "hal.h"
#include "chprintf.h"
#include "rf24_serial.h"

namespace rf24 {
namespace serial {

using namespace std;

const eventmask_t STOP_EVENT = 0x1;
const eventmask_t IRQ_EVENT = 0x2;
const eventmask_t TX_EVENT = 0x4;
const eventmask_t RX_EVENT = 0x8;

static inline RF24Serial *rf24(void *instance) {
    return (RF24Serial *)instance;
}

static size_t write(void *instance, const uint8_t *bp, size_t n) {
    return rf24(instance)->write(bp, n);
}

/* Stream read buffer method.*/
static size_t read(void *instance, uint8_t *bp, size_t n) {
    return rf24(instance)->read(bp, n);
}

/* Channel put method, blocking.*/
static msg_t put(void *instance, uint8_t b) {
    return rf24(instance)->put(b);
}

/* Channel get method, blocking.*/
static msg_t get(void *instance) {
    return rf24(instance)->get();
}

const struct PacketTransmitStreamVMT VMT = {
        write, read, put, get
};

RF24Serial::RF24Serial(RF24& rf24) : vmt(&VMT), radio(rf24), radioThread(NULL) {
    state = STOP;
}

static void radio_thread_start(void *instance) {
    chRegSetThreadName("rf24serial");
    rf24(instance)->eventMain();
}

void RF24Serial::start(Mode m) {
    stateMutex.lock();
    mode = m;
    if(state == State::STOP) {
        state = State::STARTING;
        chPoolObjectInit(&packets, sizeof(struct packet), NULL);
        chPoolLoadArray(&packets, buffer, PACKET_POOL_COUNT);
        stateMutex.unlock();
        transmit_packet = allocPacket();
        transmit_pos = 0;
        receive_pos = PACKET_SIZE;
        radioThread.thread_ref = chThdCreateStatic(wa, sizeof(wa), NORMALPRIO, radio_thread_start, this);
    } else {
        stateMutex.unlock();
    }
}

void RF24Serial::stop() {
    stateMutex.lock();
    if(ready()) {
        state = State::STOPPING;
        radioThread.signalEvents(STOP_EVENT);
        stateMutex.unlock();
        radioThread.wait();
        transmit_queue.reset();
        receive_queue.reset();
        setState(State::STOP);
    } else {
        stateMutex.unlock();
    }
}

void RF24Serial::print(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chvprintf(stream(), fmt, ap);
    va_end(ap);
}

inline bool RF24Serial::receiveReady() {
    cnt_t free;
    chSysLock();
    free = receive_queue.getFreeCountI();
    chSysUnlock();

    return (free > 0) && radio.available();
}


void RF24Serial::irq() {
    chSysLockFromISR();
    if(ready()) {
        radioThread.signalEventsI(IRQ_EVENT);
    }
    chSysUnlockFromISR();
}



inline void RF24Serial::receiveNonBlocking() {
    while(receiveReady()) {
        packet_t packet = allocPacket();
        radio.read(packet->data, PACKET_SIZE);
        packet->length = radio.getDynamicPayloadSize();
        if(packet->length > 0) {
            receive_queue.post(packet, TIME_IMMEDIATE);
        } else {
            freePacket(packet);
            stats.rx_empty++;
        }
    }
}

inline void RF24Serial::transmitNonBlocking() {
    while(!radio.txFifoFull() && transmitNext()) {};
}

inline bool RF24Serial::transmitNext() {
    packet_t packet;
    if(transmit_queue.fetch(&packet, TIME_IMMEDIATE) == MSG_OK) {
        radio.startFastWrite(packet->data, packet->length, false);
        freePacket(packet);
        return true;
    } else {
        return false;
    }
}


void RF24Serial::eventMain() {

    stateMutex.lock();
    if(state == STARTING) {
        switch(mode) {
        case AUTO: case PRX_ONLY:
            state = PRX;
            radio.startListening();
            break;
        case PTX_ONLY:
            state = PTX;
            transmitNonBlocking();
            break;
        }
    }
    stateMutex.unlock();

    // And wait for it to drain, accepting new messages in the meantime.
    while(ready()) {
        switch(chEvtWaitOne(STOP_EVENT | IRQ_EVENT | TX_EVENT | RX_EVENT)) {
        case IRQ_EVENT:
            stats.irq++;
            bool tx_ok, tx_fail, rx_ready;
            radio.whatHappened(tx_ok, tx_fail, rx_ready);
            if(rx_ready) {
                stats.rx_dr++;
                receiveNonBlocking();
            }

            if(tx_fail) {
                /* There are two conflicting requirements here: this is a serial emulation
                 * so gaps are not expected, but when a receiver joins it expects the data
                 * to be current. The real solution is to implement a TTL to discard packets.
                 */
                stats.max_rt++;
                radio.txFlushFailure();
            }

            if(tx_ok) {
                stats.tx_ok++;
                transmitNonBlocking();
                if(mode == AUTO) {
                    stateMutex.lock();
                    if(state == PTX && radio.txFifoEmpty()) {
                        radio.startListening();
                        state = PRX;
                    }
                    stateMutex.unlock();
                }
            }

            break;
        case TX_EVENT:
            stats.tx++;
            if(mode == AUTO) {
                stateMutex.lock();
                if(state == PRX) {
                    radio.stopListening();
                    state = PTX;
                }
                stateMutex.unlock();
            }
            transmitNonBlocking();
            break;
        case RX_EVENT:
            stats.rx++;
            receiveNonBlocking();
            break;
        case STOP_EVENT:
            break;
        }
    }

    radio.txStandBy();
}


msg_t RF24Serial::get() {
    if(receiveEnsureAvailable() != Q_OK) {
        return Q_RESET;
    } else {
        msg_t c = receive_packet->data[receive_pos++];
        receiveFreeBufferIfEmpty();
        return c;
    }
}

size_t RF24Serial::read(uint8_t* bp, size_t n) {
    size_t read = 0;
    while(read < n) {
        if(receiveEnsureAvailable() != MSG_OK) break;
        while(!receiveBufferEmpty() && read < n) bp[read++] = receive_packet->data[receive_pos++];
        receiveFreeBufferIfEmpty();
    }
    return read;
}

size_t RF24Serial::readPacket(uint8_t* bp) {
    size_t read = 0;
    if(receiveEnsureAvailable() == MSG_OK) {
        while(!receiveBufferEmpty()) bp[read++] = receive_packet->data[receive_pos++];
        receiveFreeBufferIfEmpty();
    }
    return read;
}

inline msg_t RF24Serial::receiveEnsureAvailable() {
    msg_t result = MSG_OK;
    if(!ready()) {
        receive_status = RECEIVE_ENSURE_AVAILABLE_NOT_READY;
        result = MSG_RESET;
    } else if(receive_packet == NULL) {
        if(receive_queue.fetch(&receive_packet, TIME_INFINITE) == MSG_OK) {
            // signal the radio thread that there's a free slot in the receive queue
            radioThread.signalEvents(RX_EVENT);
            receive_pos = 0;
            if(receiveBufferEmpty()) {
                receive_status = RECEIVE_ENSURE_AVAILABLE_EMPTY;
                result = MSG_RESET;
            }
        } else {
            receive_status = RECEIVE_ENSURE_AVAILABLE_RESET;
            result = MSG_RESET;
        }
    }

    return result;
}

inline void RF24Serial::receiveFreeBufferIfEmpty() {
    if(receiveBufferEmpty()) {
        freePacket(receive_packet);
        receive_packet = NULL;
    }
}

msg_t RF24Serial::flush(void) {
    if(!ready()) {
        return MSG_RESET;
    } else if(transmit_pos > 0) {
        transmit_packet->length = transmit_pos;
        if(transmit_queue.post(transmit_packet, TIME_INFINITE) == MSG_OK) {
            radioThread.signalEvents(TX_EVENT);
            transmit_packet = allocPacket();
            transmit_pos = 0;
        } else {
            return MSG_RESET;
        }
    }

    return MSG_OK;
}

msg_t RF24Serial::put(uint8_t b) {
    if(!ready()) {
        return Q_RESET;
    } else {
        transmit_packet->data[transmit_pos++] = b;
        return flushIfFull();
    }
}

size_t RF24Serial::write(const uint8_t *bp, size_t n) {
    if(!ready()) {
        return 0;
    }

    size_t written = 0;
    written += append(&bp[written], n - written);
    while(written < n) {
        if(flush() != MSG_OK) break;
        written += append(&bp[written], n - written);
    }
    flushIfFull();
    return written;
}

static inline size_t min(size_t a, size_t b) {
    return a > b ? b : a;
}

size_t RF24Serial::append(const uint8_t *bp, size_t n) {
    size_t write = min(PACKET_SIZE - transmit_pos, n);
    memcpy(&transmit_packet->data[transmit_pos], bp, write);
    transmit_pos += write;
    return write;
}

void RF24Serial::setError(Error _error) {
    stateMutex.lock();
    error = _error;
    state = State::ERROR;
    stateMutex.unlock();
}


}
}
