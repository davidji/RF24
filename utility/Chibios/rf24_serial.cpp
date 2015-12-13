#include "ch.hpp"
#include "hal.h"
#include "chprintf.h"
#include "rf24_serial.h"

namespace rf24 {
namespace serial {

using namespace std;

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
        transmit_queue.reset();
        receive_queue.reset();
        chPoolObjectInit(&packets, PACKET_SIZE, NULL);
        chPoolLoadArray(&packets, buffer, PACKET_POOL_COUNT);
        radioThread.thread_ref = chThdCreateStatic(wa, sizeof(wa), NORMALPRIO, radio_thread_start, this);
        transmit_packet = get_packet();
        transmit_pos = 0;
        receive_pos = PACKET_SIZE;
        state = State::READY;
    }
    stateMutex.unlock();
}

void RF24Serial::stop() {
    stateMutex.lock();
    if(state == State::READY) {
        radioThread.requestTerminate();
        radioThread.wait();
        state = STOP;
    }
    stateMutex.unlock();
}

void RF24Serial::print(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chvprintf(stream(), fmt, ap);
    va_end(ap);
}

bool RF24Serial::receiveReady() {
    bool result = false;
    if(radio.available()) {
        chSysLock();
        result = receive_queue.getFreeCountI() > 0;
        chSysUnlock();
    }

    return result;
}

const eventmask_t IRQ_EVENT = 0x1;
const eventmask_t TX_EVENT = 0x2;
const eventmask_t RX_EVENT = 0x3;

void RF24Serial::irq() {
    chSysLockFromISR();
    radioThread.signalEventsI(IRQ_EVENT);
    chSysUnlockFromISR();
}

void RF24Serial::receiveNonBlocking() {
    while(receiveReady()) {
        packet_t packet = get_packet();
        radio.read(packet, PACKET_SIZE);
        receive_queue.post(packet, TIME_IMMEDIATE);
    }
}

void RF24Serial::transmitNonBlocking() {
    packet_t packet;
    while(!radio.txFifoFull()) {
        if(transmit_queue.fetch(&packet, TIME_IMMEDIATE) == MSG_OK) {
            radio.writeFast(packet, PACKET_SIZE, false);
            free_packet(packet);
        } else {
            break;
        }
    }
}

void RF24Serial::transmitEventLoop() {
    radio.stopListening();

    // Fill up the fifo
    transmitNonBlocking();

    // And wait for it to drain, accepting new messages in the meantime.
    while(!chThdShouldTerminateX() && !radio.txFifoEmpty()) {
        switch(chEvtWaitOne(IRQ_EVENT | TX_EVENT)) {
        case IRQ_EVENT:
            bool tx_ok, tx_fail, rx_ready;
            radio.whatHappened(tx_ok, tx_fail, rx_ready);
            if(tx_ok) {
                transmitNonBlocking();
            }

            if(tx_fail) {
                transmit_failures++;
                radio.txStandBy();
                radio.startListening();
                return;
            }

            if(rx_ready) {
                receiveNonBlocking();
            }
            break;
        case TX_EVENT:
            transmitNonBlocking();
            break;
        }
    }

    radio.txStandBy();
    radio.startListening();
}

void RF24Serial::eventMain() {
    radio.startListening();

    while(!chThdShouldTerminateX()) {
        switch(chEvtWaitOne(IRQ_EVENT | TX_EVENT | RX_EVENT)) {
        case IRQ_EVENT:
            bool tx_ok, tx_fail, rx_ready;
            radio.whatHappened(tx_ok, tx_fail, rx_ready);
            if(rx_ready) {
                receiveNonBlocking();
            }
            break;
        case RX_EVENT:
            receiveNonBlocking();
            break;
        case TX_EVENT:
            transmitEventLoop();
            break;
        }
    }
}

void RF24Serial::main() {
    packet_t packet;

    radio.startListening();

    while(!chThdShouldTerminateX()) {

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


msg_t RF24Serial::receiveEnsureAvailable() {
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

void RF24Serial::receiveFreeBufferIfEmpty() {
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
    if(transmit_pos == PACKET_SIZE || transmit_idle()) {
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
