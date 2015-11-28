#ifndef _RF24_SERIAL_H_
#define _RF24_SERIAL_H_

#include <ch.hpp>
#include <RF24.h>

using namespace chibios_rt;

static const size_t PACKET_SIZE = 32;
static const uint8_t PACKET_COUNT = 4;

typedef uint8_t *packet_t;

struct PacketTransmitStreamVMT {
    _base_sequential_stream_methods
};

struct RF24Serial : public BaseStaticThread<512> {

private:
    const struct PacketTransmitStreamVMT *vmt;
    RF24 &radio;

    // -------------------------------------------------------------
    // Transmit state
    uint8_t transmit_buffer[PACKET_COUNT][PACKET_SIZE];
    Mailbox<packet_t, PACKET_COUNT> transmit_free;
    Mailbox<packet_t, PACKET_COUNT> transmit_queue;
    packet_t transmit_packet = NULL;
    uint8_t transmit_pos = PACKET_SIZE;

    // -------------------------------------------------------------
    // Receive state
    uint8_t receive_buffer[PACKET_COUNT][PACKET_SIZE];
    Mailbox<packet_t, PACKET_COUNT> receive_free;
    Mailbox<packet_t, PACKET_COUNT> receive_queue;
    packet_t receive_packet = NULL;
    uint8_t receive_pos = PACKET_SIZE;

    // -------------------------------------------------------------
    // Transmit private methods
    msg_t free_transmit_packet();
    msg_t push_if_needed();
    size_t append(const uint8_t *bp, size_t n);

    msg_t free_receive_packet();
    msg_t pull_if_needed();
    inline bool receive_empty() {
        return receive_pos == PACKET_SIZE || receive_packet[receive_pos] == '\0';
    }

public:
    RF24Serial(RF24 &rf24);
    void reset();
    void main();
    msg_t get();
    size_t read(uint8_t *bp, size_t n);
    msg_t put(uint8_t b);
    size_t write(const uint8_t *bp, size_t n);
    void print(const char *fmt, ...);
    msg_t flush(void);

    inline BaseSequentialStream *stream() {
        return (BaseSequentialStream *)this;
    }
};

#endif
