void init_terminal(struct termios *, FILE *);
void restore_terminal(struct termios *, FILE *);
void print_head();
void display_dynamic_info(dyn_struct *, total *);
