#include "system.h"


#define ACK_TIMEOUT 10
#define RW_TIMEOUT 20
/*append 24 bit address to data in big endian, 
 *start from the second byte */
void append_addr(char *data, int addr)
{
	data[1] = (addr >> 16) & 0xFF;
	data[2] = (addr >> 8 ) & 0xFF;
	data[3] = addr & 0xFF;
}
int serial_open(char *port)
{
	int fd;
	int flag = O_RDWR | O_NOCTTY | O_NDELAY;
	fd = open(port, flag);
	if(fd < 0) {
		fprintf(stderr, "Failed to open port, %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	return fd;
}

/* set port to 8N1 mode, given baud in B*  */
int serial_set(int fd, int baud)
{
	struct termios option;

	if(tcgetattr(fd, &option) < 0){
		fprintf(stderr, "Failed to get port attr, %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	option.c_iflag = 0;
	option.c_oflag = 0;
	option.c_cflag  = CS8 | CREAD | CLOCAL;
	option.c_lflag = 0;
	cfsetospeed(&option, baud);
	cfsetispeed(&option, baud);

	if(tcsetattr(fd, TCSANOW, &option) < 0){
		fprintf(stderr, "Failed to get port attr, %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	return 0;
}

/* unbuffered reliable write, 
 * return number of bytes actually written */
ssize_t serial_write(int fd, char *buf, size_t count)
{
	size_t left = count;
	ssize_t n;
	time_t t0, t1;
	t0 = t1 = time(NULL);
	while(left > 0 && (t1 - t0) < RW_TIMEOUT){
		n = write(fd, buf + count - left, left);
		if(n > 0)
			left -= n;
		t1 = time(NULL);
	}
	return count - left;
}

/* try to get enough data before timeout
 * return bytes actually read */
ssize_t serial_read(int fd, char *buf, size_t count)
{
	size_t left = count;
	ssize_t n;
	time_t t0, t1;
	t0 = t1 = time(NULL);
	while(left > 0 && (t1 - t0) < RW_TIMEOUT){
		n = read(fd, buf + count - left, left);
		if(n > 0)
			left -= n;
		t1 = time(NULL);
	}
	return count - left;
}

/* unbuffered, append STX and ETX
 * return 0 on sucess, -1 on short or error */
int send_data(int fd, char *data, size_t size)
{
	char stx = STX, etx = ETX;
	if(serial_write(fd, &stx, 1) < 1){
		return -1;
	}
	if(serial_write(fd, data, size) < size){
		return -1;
	}
	if(serial_write(fd, &etx, 1) < 1){
		return -1;
	}
	return 0;
}

/* send header of given Inum and Onum
 * return 0 on sucess, -1 or short or error */
int send_header(int fd, int Inum, int Onum)
{
	short num[2] = {(short)Inum, (short)Onum};
	char soh = SOH;
	if(serial_write(fd, &soh, 1) < 1)
		return -1;
	if(serial_write(fd, (char *)num, 4) < 4)
		return -1;
	return 0;
}

/* read data of given Onum, STX and ETX are checked.
 * return 0 on sucess, -1 or short or error */
int read_data(int fd, char *buf, int Onum)
{
	char c;
	int n;
	if((n = serial_read(fd, &c, 1) < 1) || c != STX){
		return -1;
	}
	if((n = serial_read(fd, buf, Onum)) < Onum){
		return -1;
	}
	if(serial_read(fd, &c, 1) < 1 || c != ETX){
		return -1;
	}
	return 0;
}

/* read one byte from serial, check if it is ACK.
 * return 1 on ACK, 0 on error */
int isACK(int fd)
{
	char c = 0;
	time_t t0, t1;
	t0 = t1 = time(NULL);
	while(serial_read(fd, &c, 1) < 1 && (t1-t0) < ACK_TIMEOUT){
		t1 = time(NULL);
	}
	if(c == ACK)
		return 1;
	else{
		if(c == NAK)
			printf("NAK!!\n");
		return 0;
	}
}
