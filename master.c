#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#define BAUD B9600
#define MAX_ATTEMPT 0x100000

#define ACK 0x06
#define NAK 0x15
#define SOH 0x01
#define STX 0x02
#define ETX 0x03

void print_array(char *data, int n)
{
	int i;
	unsigned char c;
	for(i = 0; i < n; i++){
		c = (unsigned char) data[i];
		printf("%02X ", c);
	}
	printf("\n");
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

/* set port to 8N1 mode, given baud */
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

ssize_t serial_write(int fd, char *buf, size_t count)
{
	size_t left = count, attempt = 0;
	ssize_t n;
	while(left > 0 && attempt < (count + MAX_ATTEMPT)){
		n = write(fd, buf + count - left, left);
		if(n > 0)
			left -= n;
		attempt++;
	}
	return count - left;
}

ssize_t serial_read(int fd, char *buf, size_t count)
{
	size_t left = count, attempt = 0;
	ssize_t n;
	while(left > 0 && attempt < (count + MAX_ATTEMPT)){
		n = read(fd, buf + count - left, left);
		if(n > 0)
			left -= n;
		attempt++;
	}
	return count - left;
}

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
int isACK(int fd)
{
	char c;
	int n;
	if((n = serial_read(fd, &c, 1)) < 1 || c != ACK){
		printf("%d",n);
		print_array(&c, 1);
		printf("\n");
		return -1;
	}
	return 0;
}


int main(int argc, char **argv)
{
	if(argc != 2){
		printf("usage: %s port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	int fd = serial_open(argv[1]);
	serial_set(fd, BAUD);
	char buf[0x10000];
	char cmd = 0x9F;
	for(;;){
		sleep(1);
		tcflush(fd, TCIOFLUSH);
		printf("sending header:");
		if(send_header(fd, 1, 3) < 0){
			printf("header fail\n");
			continue;
		}
		if(isACK(fd) < 0){
			printf("No header ACK!\n");
			continue;
		}
		printf("ACK!\n");
		printf("sending data:");
		if(send_data(fd, &cmd, 1) < 0){
			printf("data fail\n");
			continue;
		}
		if(isACK(fd) < 0){
			printf("No data ACK\n");
			continue;
		}
		printf("ACK!\n");
		printf("reading data");
		if(read_data(fd, buf, 3) < 0){
			printf("reading fail\n");
			continue;
		}
		print_array(buf,3);
	}
	return 0;
}

