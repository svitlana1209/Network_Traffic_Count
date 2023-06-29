int listen_interface(int);
void receive_from_socket(int, packet *);
u_int32_t get_interface_ip(char *);
u_int32_t addr_to_int(char *);
void intaddr_to_string(u_int32_t, u_int8_t *);

