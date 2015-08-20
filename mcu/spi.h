void spi_init();
void spi_rw(uint8_t *wbuf, uint16_t wn, uint8_t *rbuf, uint16_t rn, uint8_t chcs);

#define CS_LOW PORTB &= ~_BV(PB0)
#define CS_HIGH PORTB |= _BV(PB0)
