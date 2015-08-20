void spi_init();
void spi_rw(uint8_t *wbuf, uint16_t wn, uint8_t *rbuf, uint16_t rn, uint8_t chcs);

/* Modify definitions below to port to other avr mcus */
#define DDR_SPI   DDRB
#define PORT_SPI  PORTB
#define SS   PB0 
#define SCK  PB1
#define MOSI PB2

#define CS_LOW PORT_SPI &= ~_BV(SS)
#define CS_HIGH PORT_SPI |= _BV(SS)
