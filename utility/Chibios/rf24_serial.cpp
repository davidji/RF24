#include <algorithm>
#include "ch.hpp"
#include "hal.h"
#include "chprintf.h"
#include "rf24_serial.h"

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

void RF24Serial::reset() {
}

void RF24Serial::print(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chvprintf(stream(), fmt, ap);
    va_end(ap);
}

RF24Serial::RF24Serial(RF24& rf24) : BaseStaticThread<512>(), vmt(&VMT), radio(rf24) {
    transmit_packet = transmit_buffer[0];

    transmit_pos = 0;
    for(int i = 1; i < PACKET_COUNT; ++i) {
        transmit_free.post(transmit_buffer[i], TIME_IMMEDIATE);
    }

    receive_pos = PACKET_SIZE;
    for(int i = 0; i < PACKET_COUNT; ++i) {
        receive_free.post(receive_buffer[i], TIME_IMMEDIATE);
    }
}

void RF24Serial::main() {
    packet_t packet;
    while(true) {
        while (radio.available()) {
            if(receive_free.fetch(&packet, TIME_IMMEDIATE) == MSG_OK) {
                radio.read(packet, PACKET_SIZE);
                receive_queue.post(packet, TIME_IMMEDIATE);
            } else {
                break;
            }
        }

        if(transmit_queue.fetch(&packet, MS2ST(4)) == MSG_OK) {
            radio.stopListening();
            radio.writeFast(packet, PACKET_SIZE);
            radio.startListening();
            transmit_free.post(packet, TIME_IMMEDIATE);
        }
    }
}


msg_t RF24Serial::get() {
    if(pull_if_needed() != MSG_OK)
        return Q_RESET;
    msg_t c = receive_packet[receive_pos++];
    if(free_receive_packet() != MSG_OK)
        return Q_RESET;
    return c;
}

size_t RF24Serial::read(uint8_t* bp, size_t n) {
    size_t read = 0;
    while(read < n) {
        if(pull_if_needed() != MSG_OK) break;
        while(!receive_empty()) bp[read++] = receive_packet[receive_pos++];
        if(free_receive_packet() != MSG_OK)
            break;
    }
    return read;
}


msg_t RF24Serial::pull_if_needed() {
    msg_t result = MSG_OK;
    if(receive_empty()) {
        result = receive_queue.fetch(&receive_packet, TIME_INFINITE);
        if(result == MSG_OK) {
            receive_pos = 0;
        }
    }

    return result;
}

msg_t RF24Serial::free_receive_packet() {
    msg_t result = MSG_OK;
    if(receive_pos == PACKET_SIZE || receive_packet[receive_pos] == '\0') {
        result = receive_free.post(receive_packet, TIME_IMMEDIATE);
        if(result == MSG_OK) {
            receive_pos = PACKET_SIZE;
            receive_packet = NULL;
        }
    }

    return result;
}

msg_t RF24Serial::flush(void) {
    msg_t result = MSG_OK;
    if(transmit_packet != NULL) {
        result = transmit_queue.post(transmit_packet, TIME_IMMEDIATE);
        if(result == MSG_OK)
            transmit_packet = NULL;
    }

    return result;
}

msg_t RF24Serial::put(uint8_t b) {
    if(free_transmit_packet() == MSG_OK) {
        transmit_packet[transmit_pos++] = b;
        push_if_needed();
        return MSG_OK;
    } else {
        return MSG_RESET;
    }
}

size_t RF24Serial::write(const uint8_t *bp, size_t n) {
    size_t written = 0;
    while(written < n) {
        if(free_transmit_packet() != MSG_OK) break;
        written += append(&bp[written], n - written);
    }
    return written;
}

msg_t RF24Serial::free_transmit_packet() {
    if(transmit_pos == PACKET_SIZE) {
        msg_t result = transmit_free.fetch(&transmit_packet, TIME_INFINITE);
        if(result  == MSG_OK) {
            transmit_pos = 0;
        }
        return result;
    } else {
        return MSG_OK;
    }
}

msg_t RF24Serial::push_if_needed() {
    // If there's a thread waiting for a packet, or the packet is full
    // then queue the packet.
    if(transmit_pos == PACKET_SIZE || transmit_queue.getFreeCountI() < 0) {
        for(; transmit_pos != PACKET_SIZE; ++transmit_pos) transmit_packet[transmit_pos] = 0;
        return flush();
    } else {
        return MSG_OK;
    }
}

size_t RF24Serial::append(const uint8_t *bp, size_t n) {
    size_t write = std::min(PACKET_SIZE - transmit_pos, n);
    memcpy(&transmit_packet[transmit_pos], bp, write);
    transmit_pos += write;
    push_if_needed();
    return write;
}

