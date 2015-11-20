
#include <stdint.h>

class RF24AtTiny3WireSpi {
public:

    /**
    * SPI constructor
    */
    RF24AtTiny3WireSpi();

    /**
    * Start SPI
    */
    void begin();

    /**
    * Transfer a single byte
    * @param tx_ Byte to send
    * @return Data returned via spi
    */
    uint8_t transfer(uint8_t tx_);

    /**
    * Transfer a buffer of data
    * @param tbuf Transmit buffer
    * @param rbuf Receive buffer
    * @param len Length of the data
    */
    void transfernb(char* tbuf, char* rbuf, uint32_t len);

    /**
    * Transfer a buffer of data without an rx buffer
    * @param buf Pointer to a buffer of data
    * @param len Length of the data
    */
    void transfern(char* buf, uint32_t len);

    /**
     * Select the device, ready for transfers. I.e. CSN goes low.
     */
    void select();

    /**
     * un-select the device. I.e. CSN goes high.
     */
    void unselect();

    virtual ~RF24AtTiny3WireSpi();
};

