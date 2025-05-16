#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include "linux/spi/spidev.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <linux_spi.h>

#define SPI_FREQUENCY   (500000)
#define SPI_MODE        SPI_MODE_0

class SPIClass
{
public:
    SPIClass(char *spidev);
    int send_byte_data(uint8_t *txByteData, uint8_t *rxByteData, uint8_t len);
    uint8_t tx[1024] = {0, };
    uint8_t rx[1024] = {0, };
private :
  uint8_t _u8_Ch2Gain, _u8_Ch1Gain;
    void init(void);
    void spi_init();
    void clear_rx();
    int mode_spi = SPI_MODE;
    int ret_spi=0;
    int fd_spi;
    uint8_t bits = 8;
    uint32_t speed = SPI_FREQUENCY;
    uint16_t delay = 10;

    char *spiDev;
    void delay_mSec(int mSec);
    Linux_SPI* spi_dev;
};

#endif // SPI_H
