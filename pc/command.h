int RDID(int fd, char *buf);
int RDSR(int fd, char *status);
int WREN(int fd);
int WRDI(int fd);
int RD(int fd, char *buf, int addr, int size);
int CE(int fd);
int PP(int fd, char *data, int addr, int size);
int BE(int fd, int addr);
int SE(int fd, int addr);
