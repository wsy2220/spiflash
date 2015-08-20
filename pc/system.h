#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define ACK 0x06
#define NAK 0x15
#define SOH 0x01
#define STX 0x02
#define ETX 0x03

#define BAUD B115200
