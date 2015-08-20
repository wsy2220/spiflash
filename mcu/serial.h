
void serial_init();
void serial_write(uint8_t *c, uint16_t n);
uint16_t serial_read(uint8_t *c, uint16_t n, uint8_t no_timeout);
void rx_flush();
