
#ifdef __CHIBIOS__

#include <algorithm>

#include "ch.hpp"
#include "hal.h"
#include "chprintf.h"
#include "rf24_serial.h"

namespace rf24 {
namespace serial {

using namespace std;

const eventmask_t STOP_EVENT = 0x1;
const eventmask_t IRQ_EVENT = 0x2;
const eventmask_t POST_EVENT = 0x4;
const eventmask_t FETCH_EVENT = 0x8;
const eventmask_t TX_FAIL_EVENT = 0x10;
const eventmask_t TX_OK_EVENT = 0x20;
const eventmask_t RX_RD_EVENT = 0x40;

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

void RF24Serial::start() {
    stateMutex.lock();
    if(state == State::STOP) {
        radio.setAutoAck(true);
        radio.setRetries(5,15);

        switch(mode) {
        case PRX_ONLY:
            radio.enableAckPayload();
            radio.openReadingPipe(readPipe, readAddress);
            break;
        case PTX_ONLY:
            radio.enableAckPayload();
            radio.openWritingPipe(writeAddress);
            break;
        case ADHOC:
            radio.openWritingPipe(writeAddress);
            radio.openReadingPipe(readPipe, readAddress);
            break;
        }

        state = State::STARTING;
        chPoolObjectInit(&packets, sizeof(struct packet), NULL);
        chPoolLoadArray(&packets, buffer, PACKET_POOL_COUNT);
        stateMutex.unlock();
        transmit_packet = allocPacket();
        transmit_pos = 0;
        receive_pos = PACKET_SIZE;
        radioThread.thread_ref = chThdCreateStatic(wa, sizeof(wa), NORMALPRIO, radio_thread_start, this);
        while(state == State::STARTING) {
            chThdYield();
        }
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

void RF24Serial::irq() {
    chSysLockFromISR();
    if(ready()) {
        radioThread.signalEventsI(IRQ_EVENT);
    }
    chSysUnlockFromISR();
}

int RF24Serial::receiveFreeCount() {
    cnt_t free;
    System::lock();
    free = receive_queue.getFreeCountI();
    System::unlock();
    return free;
}

void RF24Serial::receive() {
    uint8_t length = min(PACKET_SIZE, radio.getDynamicPayloadSize());
    packet_t packet = allocPacket();
    packet->length = length;
    radio.read(packet->data, length);
    System::lock();
    receive_queue.postI(packet);
    receive_queue_available += length;
    System::unlock();
    status = radio.status();
}

void RF24Serial::receiveNonBlocking() {
    for(int free = receiveFreeCount(); (free > 0) && (status.rx_p_no != RX_P_NO_EMPTY) && radio.available(); --free) {
        receive();
    }
}

inline void RF24Serial::transmitNonBlocking(bool ack) {
    while(!status.tx_full && transmitNext(ack)) {};
}

inline bool RF24Serial::transmitNext(bool ack) {
    packet_t packet;
    if(transmit_queue.fetch(&packet, TIME_IMMEDIATE) == MSG_OK) {
        // broadcastFlags(TX_EVENT);
        if(ack) {
            radio.writeAckPayload(readPipe, packet->data, packet->length);
        } else {
            radio.startFastWrite(packet->data, packet->length, false);
        }
        freePacket(packet);
        status = radio.status();
        return true;
    } else {
        return false;
    }
}

Status RF24Serial::whatHappened() {
    stats.irq++;
    Status status = radio.status(true);
    if(status.rx_dr) {
        stats.rx_dr++;
        stats.rx_pipe[status.rx_p_no]++;
    }
    if(status.max_rt) stats.max_rt++;
    if(status.tx_ds) stats.tx_ds++;
    stats.tx_full = status.tx_full;
    return status;
}

void RF24Serial::ptxMain() {
    if(transition(STARTING, PTX)) {
        while (true) {
            eventmask_t events = chEvtWaitAny(STOP_EVENT | IRQ_EVENT | POST_EVENT | FETCH_EVENT);

            if(events & STOP_EVENT) {
                break;
            }

            if(events & IRQ_EVENT) {
                status = whatHappened();
                if (status.max_rt) {
                    radio.reUseTX();
                }
            }

            receiveNonBlocking();
            transmitNonBlocking();
        }
    }
}

void RF24Serial::prxMain() {
    if(transition(STARTING, PRX)) {
        radio.startListening();
        while (true) {
            eventmask_t events = chEvtWaitAnyTimeout(STOP_EVENT | IRQ_EVENT | POST_EVENT | FETCH_EVENT, MS2ST(4));
            if(events & STOP_EVENT) {
                break;
            } else {
                if(events & IRQ_EVENT || events == 0) {
                    status = whatHappened();
                }

                receiveNonBlocking();
                transmitNonBlocking(true);
            }
        }
    }
}

void RF24Serial::adhocMain() {
    if(transition(STARTING, PRX)) {
        radio.startListening();
        while(ready()) {
            switch(chEvtWaitOne(STOP_EVENT | IRQ_EVENT | POST_EVENT | FETCH_EVENT)) {
            case IRQ_EVENT:
                {
                    Status status = whatHappened();
                    if(status.rx_dr) {
                        receiveNonBlocking();
                    }

                    if(status.max_rt) {
                        if(transition(PTX, PRX)) {
                            radio.startListening();
                        }
                    }

                    if(status.tx_ds) {
                        transmitNonBlocking();
                        stateMutex.lock();
                        if(state == PTX && radio.txFifoEmpty()) {
                            radio.startListening();
                            state = PRX;
                        }
                        stateMutex.unlock();
                    }
                }
                break;
            case POST_EVENT:
                stateMutex.lock();
                if(transition(PRX, PTX)) {
                    radio.stopListening();
                }

                transmitNonBlocking();
                break;
            case FETCH_EVENT:
                receiveNonBlocking();
                break;
            case STOP_EVENT:
                break;
            }
        }
    }
}

void RF24Serial::eventMain() {

    switch(mode) {
    case ADHOC:
        adhocMain();
        break;
    case PRX_ONLY:
        prxMain();
        break;
    case PTX_ONLY:
        ptxMain();
        break;
    }

    // And wait for it to drain, accepting new messages in the meantime.

    radio.txStandBy();
}


msg_t RF24Serial::get() {
    if(receiveEnsureAvailable() != Q_OK) {
        return Q_RESET;
    } else {
        preReadCheck();
        msg_t c = receive_packet->data[receive_pos++];
        receiveFreeBufferIfEmpty();
        return c;
    }
}

size_t RF24Serial::read(uint8_t* bp, size_t n) {
    size_t read = 0;
    while(read < n) {
        if(receiveEnsureAvailable() != MSG_OK) break;
        preReadCheck();
        while(!receiveBufferEmpty() && read < n) bp[read++] = receive_packet->data[receive_pos++];
        receiveFreeBufferIfEmpty();
    }
    return read;
}

size_t RF24Serial::readPacket(uint8_t* bp) {
    size_t read = 0;
    if(receiveEnsureAvailable() == MSG_OK) {
        preReadCheck();
        while(!receiveBufferEmpty()) bp[read++] = receive_packet->data[receive_pos++];
        receiveFreeBufferIfEmpty();
    }
    return read;
}

size_t RF24Serial::available() {
    return receive_queue_available + (receive_packet == NULL ? 0 : receive_packet->length - receive_pos);
}

inline msg_t RF24Serial::receiveEnsureAvailable() {
    msg_t result = MSG_OK;
    if(!ready()) {
        receive_status = RECEIVE_ENSURE_AVAILABLE_NOT_READY;
        result = MSG_RESET;
    } else if(receive_packet == NULL) {
        packet_t packet;
        if(receive_queue.fetch(&packet, TIME_INFINITE) == MSG_OK) {
            System::lock();
            receive_queue_available -= packet->length;
            receive_packet = packet;
            receive_pos = 0;
            System::unlock();
            // signal the radio thread that there's a free slot in the receive queue
            radioThread.signalEvents(FETCH_EVENT);
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
            radioThread.signalEvents(POST_EVENT);
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
        preWriteCheck();
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
    preWriteCheck();
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

#endif // __CHIBIOS__