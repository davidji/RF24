
#include <rf24_linux_io.h>

Rf24LinuxIo::Rf24LinuxIo(int spifd) {
}

static const uint32_t mode = SPI_CPOL;
static const uint8_t bits = 8;
static const uint32_t speed = 10000000;

void Rf24LinuxIo::begin() {
    uint32_t mode = SPI_CPOL;
    fd = open(device, O_RDWR);
    ioctl(fd, SPI_IOC_WR_MODE32, &mode);
    ioctl(fd, SPI_IOC_RD_MODE32, &mode);
    ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
}

void Rf24LinuxIo::beginTransaction() {
}

void Rf24LinuxIo::endTransaction() {
}

uint8_t Rf24LinuxIo::transfer(uint8_t tx) {
    uint8_t rx;
    struct spi_ioc_transfer xfer = { (sizeof_t)&tx, (sizeof_t)&rx, 1, 0, speed, bits };
    ioctl(fd, SPI_IOC_MESSAGE(1), &xfer );
    return rx;
}

void Rf24LinuxIo::transfernb(const uint8_t* tx, uint8_t* rx, uint32_t len) {
    struct spi_ioc_transfer xfer  = { (sizeof_t)tx, (sizeof_t)rx, len, 0, speed, bits };
    ioctl(fd, SPI_IOC_MESSAGE(1), &xfer);
}

void Rf24LinuxIo::transfern(const uint8_t* buf, uint32_t len) {
    write(fd, rbuf, len);
}

void Rf24LinuxIo::select() {
}

void Rf24LinuxIo::unselect() {
}

void Rf24LinuxIo::ce(bool level) {
}
