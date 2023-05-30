#define RESET        "\033[0m"
#define WHITE_BLACK  "\033[47m\033[30m"
#define WHITE_TEXT   "\033[1;37m"
#define RED_TEXT     "\033[1;31m"
#define CYAN_TEXT    "\033[36m"
#define GREEN_TEXT   "\033[32m"

void quit(char *);
int get_from_packet(packet *, u_int8_t, u_int8_t);
u_int8_t detect_direction(u_int32_t, u_int32_t, u_int32_t);

