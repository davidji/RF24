#ifndef _RF24_SERIAL_H_
#define _RF24_SERIAL_H_

#include <ch.hpp>
#include <RF24.h>

namespace rf24 {
namespace serial {

using namespace chibios_rt;

static const size_t PACKET_SIZE = 32;
static const uint8_t PACKET_COUNT = 5;
static const size_t PACKET_POOL_COUNT = 2 * PACKET_COUNT;
static const size_t QUEUE_COUNT = PACKET_COUNT - 2;

typedef uint8_t *packet_t;

struct PacketTransmitStreamVMT {
    _base_sequential_stream_methods
};

typedef enum {
    STOP, READY, ERROR
} State;

typedef enum {
    NONE, TRANSMIT_FREE_POST, TRANSMIT_FREE_FETCH
} Error;

struct RF24Serial {
    const struct PacketTransmitStreamVMT * const vmt;
private:

    RF24 &radio;
    State state = State::STOP;
    Error error = Error::NONE;
    Mutex stateMutex;

    // This is a blob of memory big enough to hold all the packets we could need
    __attribute__((aligned(sizeof(void *))))
    uint8_t buffer[PACKET_POOL_COUNT * PACKET_SIZE];
    // We want to be able to re-init the pool each time we start, so we don't
    // use the C++ wrapper.
    memory_pool_t packets;

    // The radio IO thread
    THD_WORKING_AREA(wa, 256);
    ThreadReference radioThread;

    // -------------------------------------------------------------
    // Transmit state

    /*
     * We want to have one packet in transmit_packet, one being sent
     * and the remainder in the transmit queue. This guarantees that
     * immediately after posting to the transmit_queue there will be
     * a free packet in transmit_free. This wastes one packet, but
     * simplifies the code to a single helper method: All methods leave
     * transmit_packet ready to accept at least one byte.
     */

    Mailbox<packet_t, QUEUE_COUNT> transmit_queue;
    packet_t transmit_packet;
    uint8_t transmit_pos;
    uint32_t transmit_failures = 0;

    // -------------------------------------------------------------
    // Receive state
    Mailbox<packet_t, QUEUE_COUNT> receive_queue;
    packet_t receive_packet;
    uint8_t receive_pos;

    void set_error(Error error);

    inline packet_t get_packet() {
        return (packet_t)chPoolAlloc(&packets);
    }

    inline void free_packet(packet_t packet) {
        chPoolFree(&packets, packet);
    }

    // -------------------------------------------------------------
    // Transmit private methods
    void transmitEventLoop();
    void transmitNonBlocking();
    bool transmit_idle();
    msg_t flush_if_full_or_ready();
    size_t append(const uint8_t *bp, size_t n);

    // -------------------------------------------------------------
    // Receive private methods
    inline bool receiveBufferEmpty() {
        return receive_pos == PACKET_SIZE ? true : receive_packet[receive_pos] == '\0';
    }

    bool receiveReady();
    void receiveNonBlocking();
    msg_t receiveEnsureAvailable();
    void receiveFreeBufferIfEmpty();

public:
    RF24Serial(RF24 &rf24);
    void reset();
    void start();
    void stop();
    void main();
    void irq();
    void eventMain();
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

}
}
#endif
