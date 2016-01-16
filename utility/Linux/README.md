
This driver is intended to work on any Linux host that supports
[spidev](https://www.kernel.org/doc/Documentation/spi/spidev).

MRAA uses spidev under the covers any way, and doesn't give any
control over chip select. Raspberry Pi has spidev drivers too,
so you could think of this as a replacement for Linux and MRAA
support.

You can test this builds on a generic Linux, and with appropriate SPI
hardware you can test the result.

You might think the other benefit was that it would be asynchronous.
Sadly it isn't. Fortunately, GPIO in general can be asynchronous, so
some overall approximation of asynchronous use is possible if you
use the IRQ line. I want something that works with Python twisted.
