
The DMA settings Chibios uses for STM32F411 Nucleo board
are different to the STM32F401



colour* | periph | signal | pin  | dev  | alt
--------|--------|--------|------|------|----
white   | RF24   | GND    |      |      |
grey    | RF24   | VCC    |      |      |
brown   | RF24   | CE     | PB1  |      |
black   | RF24   | CSN    | PB2  |      |
orange  | RF24   | SCLK   | PB13 | SPI2 | 5
red     | RF24   | MOSI   | PB15 | SPI2 | 5
green   | RF24   | MISO   | PB14 | SPI2 | 5
yellow  | RF24   | IRQ    | PC4  |      |

* the colours are really for my benefit, obviously it's arbitrary.
