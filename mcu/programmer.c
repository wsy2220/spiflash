#include "avr.h"
#include "spi.h"
#include "serial.h"

#define DAT_SIZE 2048 /* max data length, buffer size is 2 more */
#define HDR_SIZE  5


/* convert a single byte to 2 ascii codes, only for debugging */
void byte2hex(uint8_t c, uint8_t *hex)
{
	uint8_t h = c;
	h = h >> 4;
	if(h < 10)
		*hex = h + 0x30;
	else
		*hex = h + 0x41 - 10;
	h = c & 0x0F;
	hex++;
	if(h < 10)
		*hex = h + 0x30;
	else
		*hex = h + 0x41 - 10;
}
	



/* write wn bytes, read rn bytes and write to serial port */
void spi2serial(uint8_t *wbuf, uint16_t wn, uint16_t rn)
{
	CS_LOW;
	spi_rw(wbuf, wn, NULL, 0, 0);
	uint16_t i;
	uint8_t temp;
	for(i = 0; i < rn; i++){
		spi_rw(NULL, 0, &temp, 1, 0);
		serial_write(&temp, 1);
	}
	CS_HIGH;
}

int main()
{
	serial_init();
	spi_init();
	uint8_t header[HDR_SIZE];
	uint8_t buffer[DAT_SIZE + 2];
	uint16_t n, Inum, Onum;
	uint8_t ACK = 0x06, NAK = 0x15;
	uint8_t SOH = 0x01, STX = 0x02, ETX = 0x03;
	for(;;){
		rx_flush();
		/* read header, no timeout */
	n = serial_read(header, HDR_SIZE, 1);
	if(n < HDR_SIZE || header[0] != SOH){
		serial_write(&NAK, 1);
		continue;
	}
	Inum = header[1] + (header[2] << 8);
	Onum = header[3] + (header[4] << 8);
	if(Inum > DAT_SIZE){
		serial_write(&NAK, 1);
		continue;
	}
	serial_write(&ACK, 1);

	/* read data */
	n = serial_read(buffer, Inum + 2, 0);
	if(n < (Inum + 2) || buffer[0] != STX || buffer[Inum+1] != ETX){
		serial_write(&NAK, 1);
		continue;
	}
	serial_write(&ACK, 1);
	/* spi rw, directly send data to serial port, so size limit is 64k */
	serial_write(&STX, 1);
	spi2serial(buffer+1, Inum, Onum);
	serial_write(&ETX, 1);
	}
	return 0;
}
