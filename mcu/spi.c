#include "avr.h"
#include "spi.h"
/* initialize SPI with CPOL=CPHA=0, MSB first, double speed */
void spi_init()
{
	/* set output, PB0 as CS# */
	DDRB |= _BV(PB2) | _BV(PB1) | _BV(PB0);
	SPSR = _BV(SPI2X);
	SPCR = _BV(MSTR) | _BV(SPE);
}

/* write wn bytes, then read rn bytes back, half-duplex 
 * chcs = 0: do not change cs status*/
void spi_rw(uint8_t *wbuf, uint16_t wn, 
		uint8_t *rbuf, uint16_t rn, uint8_t chcs)
{
	if(chcs)
		CS_LOW;

	uint16_t i;
	for(i = 0; i < wn + rn; i++){
		if(i < wn)
			SPDR = *(wbuf + i);
		else
			SPDR = 0;
		while( !(SPSR & _BV(SPIF)) )
			;
		if(i >= wn)
			*(rbuf + i - wn) = SPDR;
	}
	if(chcs)
		CS_HIGH;
}
