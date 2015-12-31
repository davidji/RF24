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

void RF24Serial::start() {
    stateMutex.lock();
    if(state == State::STOP) {
        state = State::STARTING;
        chPoolObjectInit(&packets, PACKET_SIZE, NULL);
        chPoolLoadArray(&packets, buffer, PACKET_POOL_COUNT);
        stateMutex.unlock();
        transmit_packet = get_packet();
        transmit_pos = 0;
        receive_pos = PACKET_SIZE;
        radioThread.thread_ref = chThdCreateStatic(wa, sizeof(wa), NORMALPRIO, radio_thread_start, this);
    } else {
        stateMutex.unlock();
    }
}

void RF24Serial::stop() {
    stateMutex.lock();
    if(state == State::READY) {
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
    bool result = false;
    if(radio.available()) {
        chSysLock();
        result = receive_queue.getFreeCountI() > 0;
        chSysUnlock();
    }

    return result;
}


void RF24Serial::irq() {
    chSysLockFromISR();
    if(state == State::READY) {
        radioThread.signalEventsI(IRQ_EVENT);
    }
    chSysUnlockFromISR();
}

inline void RF24Serial::receiveNonBlocking() {
    while(receiveReady()) {
        packet_t packet = get_packet();
        radio.read(packet, PACKET_SIZE);
        receive_queue.post(packet, TIME_IMMEDIATE);
    }
}

inline bool RF24Serial::transmitNonBlocking() {
    bool full;
    for(full = radio.txFifoFull(); !full && transmitNext(); full = radio.txFifoFull());
    return full;
}

inline bool RF24Serial::transmitNext() {
    packet_t packet;
    if(transmit_queue.fetch(&packet, TIME_IMMEDIATE) == MSG_OK) {
        radio.startFastWrite(packet, PACKET_SIZE, false);
        free_packet(packet);
        return true;
    } else {
        return false;
    }
}


void RF24Serial::transmitEventLoop() {

    radio.stopListening();
    for(int i = 0; i != 3 && transmitNext(); ++i);

    // And wait for it to drain, accepting new messages in the meantime.
    while(state == State::READY) {
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
                stats.max_rt++;
                radio.txStandBy();
                radio.startListening();
                return;
            }

            if(tx_ok) {
                stats.tx_ok++;
                if(transmitNext()) {
                    transmitNonBlocking();
                } else if(radio.txFifoEmpty()) {
                    return;
                }
            }

            break;
        case TX_EVENT:
            stats.tx++;
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
    radio.startListening();
}

void RF24Serial::eventMain() {
    setState(State::READY);
    radio.startListening();

    while(state == State::READY) {
        switch(chEvtWaitOne(STOP_EVENT | IRQ_EVENT | TX_EVENT | RX_EVENT)) {
        case IRQ_EVENT:
            bool tx_ok, tx_fail, rx_ready;
            radio.whatHappened(tx_ok, tx_fail, rx_ready);
            if(rx_ready) {
                receiveNonBlocking();
            }
            break;
        case RX_EVENT:
            stats.rx++;
            receiveNonBlocking();
            break;
        case TX_EVENT:
            stats.tx++;
            transmitEventLoop();
            break;
        case STOP_EVENT:
            break;
        }
    }

}

void RF24Serial::main() {
    packet_t packet;

    setState(State::READY);
    radio.startListening();

    while(state == State::READY) {

        receiveNonBlocking();

        if(transmit_queue.fetch(&packet, MS2ST(4)) == MSG_OK) {
            radio.stopListening();
            do {
                radio.writeFast(packet, PACKET_SIZE);
                free_packet(packet);
            } while(transmit_queue.fetch(&packet, TIME_IMMEDIATE) == MSG_OK);

            radio.txStandBy();
            radio.startListening();
        }
    }
}

msg_t RF24Serial::get() {
    if(receiveEnsureAvailable() != Q_OK) {
        return Q_RESET;
    } else {
        msg_t c = receive_packet[receive_pos++];
        receiveFreeBufferIfEmpty();
        return c;
    }
}

size_t RF24Serial::read(uint8_t* bp, size_t n) {
    size_t read = 0;
    while(read < n) {
        if(receiveEnsureAvailable() != Q_OK) break;
        while(!receiveBufferEmpty() && read < n) bp[read++] = receive_packet[receive_pos++];
        receiveFreeBufferIfEmpty();
    }
    return read;
}

size_t RF24Serial::readPacket(uint8_t* bp) {
    size_t read = 0;
    if(receiveEnsureAvailable() == Q_OK) {
        while(!receiveBufferEmpty()) bp[read++] = receive_packet[receive_pos++];
        receiveFreeBufferIfEmpty();
    }
    return read;
}

inline msg_t RF24Serial::receiveEnsureAvailable() {
    msg_t result = Q_OK;
    if(state != State::READY) {
        result = Q_RESET;
    } else if(receiveBufferEmpty()) {
        if(receive_queue.fetch(&receive_packet, TIME_INFINITE) == MSG_OK) {
            radioThread.signalEvents(RX_EVENT);
            receive_pos = 0;
        } else {
            result = Q_RESET;
        }
    }

    return result;
}

inline void RF24Serial::receiveFreeBufferIfEmpty() {
    if(receiveBufferEmpty()) {
        free_packet(receive_packet);
        receive_pos = PACKET_SIZE;
    }
}

msg_t RF24Serial::flush(void) {
    if(state != State::READY) {
        return Q_RESET;
    } else if(transmit_pos > 0) {
        for(; transmit_pos != PACKET_SIZE; ++transmit_pos) transmit_packet[transmit_pos] = 0;
        if(transmit_queue.post(transmit_packet, TIME_INFINITE) == MSG_OK) {
            radioThread.signalEvents(TX_EVENT);
            transmit_packet = get_packet();
            transmit_pos = 0;
        } else {
            return Q_RESET;
        }
    }

    return Q_OK;
}

msg_t RF24Serial::put(uint8_t b) {
    if(state != State::READY) {
        return Q_RESET;
    } else {
        transmit_packet[transmit_pos++] = b;
        return flush_if_full_or_ready();
    }
}

size_t RF24Serial::write(const uint8_t *bp, size_t n) {
    if(state != READY) {
        return 0;
    }

    size_t written = 0;
    written += append(&bp[written], n - written);
    while(written < n) {
        if(flush() != Q_OK) break;
        written += append(&bp[written], n - written);
    }
    flush_if_full_or_ready();
    return written;
}

bool RF24Serial::transmit_idle() {
    chSysLock();
    bool result = transmit_queue.getFreeCountI() < 0;
    chSysUnlock();
    return result;
}

msg_t RF24Serial::flush_if_full_or_ready() {
    if(transmit_pos == PACKET_SIZE) {
        return flush();
    } else {
        return Q_OK;
    }
}

static inline size_t min(size_t a, size_t b) {
    return a > b ? b : a;
}

size_t RF24Serial::append(const uint8_t *bp, size_t n) {
    size_t write = min(PACKET_SIZE - transmit_pos, n);
    memcpy(&transmit_packet[transmit_pos], bp, write);
    transmit_pos += write;
    return write;
}

void RF24Serial::set_error(Error _error) {
    stateMutex.lock();
    error = _error;
    state = State::ERROR;
    stateMutex.unlock();
}


}
}
