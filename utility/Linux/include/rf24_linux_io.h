

#ifndef _RF24_LINUX_IO_H_
#define _RF24_LINUX_IO_H_
 /**
 * @defgroup Porting_SPI Porting: SPI
 *
 *
 * @{
 */

#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

class Rf24LinuxIo {
public:

    /**
    * SPI constructor
    */
    Rf24LinuxIo(int spifd);

    Rf24LinuxIo(Rf24LinuxIo &io) = default;

    /**
    * Initialise. This involves initialising ce and csn for output and setting them both low.
    */
    void begin();

    /**
    * Start SPI
    */
    void beginTransaction();

    /**
     * Start SPI
     */
    void endTransaction();

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
    void transfernb(const uint8_t* tbuf, uint8_t* rbuf, uint32_t len);

    /**
    * Transfer a buffer of data without an rx buffer
    * @param buf Pointer to a buffer of data
    * @param len Length of the data
    */
    void transfern(const uint8_t* buf, uint32_t len);

    /**
     * Select the device, ready for transfers. I.e. CSN goes low.
     */
    void select();

    /**
     * un-select the device. I.e. CSN goes high.
     */
    void unselect();

    /**
     * Set the level of the CE pin
     * @param level
     */
    void ce(bool level);

private:
    int fd;


};

/*@}*/
#endif
