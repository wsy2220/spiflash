int serial_open(char *port);
int serial_set(int fd, int baud);
ssize_t serial_write(int fd, char *buf, size_t count);
ssize_t serial_read(int fd, char *buf, size_t count);
int send_data(int fd, char *data, size_t size);
int send_header(int fd, int Inum, int Onum);
int read_data(int fd, char *buf, int Onum);
int isACK(int fd);
void append_addr(char *data, int addr);
