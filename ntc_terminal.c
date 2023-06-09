#include <stdio.h>
#include <sys/types.h>
#include <stdbool.h>
#include <term.h>
#include <ntc.h>
#include <ntc_tools.h>
#include <ntc_terminal.h>
#include <ntc_net.h>

void init_terminal(struct termios *init_t, FILE *cons) {
struct termios new_term;

    setupterm (NULL, fileno(cons), (int *)0);
    printf("\033[0d\033[2J");
    if (!((cons)  = fopen ("/dev/tty","r")))
        quit ("Can't open tty");
    tcgetattr (fileno(cons), &(*init_t));
    new_term = *init_t;
    new_term.c_lflag &= ~ICANON;
    new_term.c_lflag &= ~ECHO;
    new_term.c_cc[VMIN]  = 1;
    new_term.c_cc[VTIME] = 0;
    new_term.c_lflag &= ~ISIG;
    if (tcsetattr (fileno(cons), TCSANOW, &new_term) !=0 )
        quit("Can't set up attributes of terminal");
}

void restore_terminal(struct termios *init_t, FILE *cons) {

    tcsetattr (fileno(cons), TCSANOW, &(*init_t));
}

void print_head() {

    printf (" Total traffic:\n");
    printf (" IN  : \n");
    printf (" OUT : \n");
    printf (" ---------------------------------------------------------------------------------\n");
    printf (" Max activity:             src              dst                  vol        packs\n\n\n");
    printf (" ---------------------------------------------------------------------------------\n");
    printf (" Press 'q' for quit ...\n");
}

void display_dynamic_info(dyn_struct *dyn, total *trf) {
char *cursor;
u_int8_t ip_s[16], ip_d[16];

    cursor = tigetstr("cup");
    putp (tparm (cursor,1,8));
    printf ("%s%lld%s\n", CYAN_TEXT, trf->in, RESET);

    putp (tparm (cursor,2,8));
    printf ("%s%lld%s\n", CYAN_TEXT, trf->out, RESET);

    intaddr_to_string(dyn->src_x, ip_s);
    intaddr_to_string(dyn->dst_y, ip_d);
    putp (tparm (cursor,5,14));
    printf ("%s%16s %16s %20d %12d %s\n", GREEN_TEXT, ip_s, ip_d, dyn->vol1, dyn->packs1, RESET);

    intaddr_to_string(dyn->src_y, ip_s);
    intaddr_to_string(dyn->dst_x, ip_d);
    putp (tparm (cursor,6,14));
    printf ("%s%16s %16s %20d %12d %s\n", GREEN_TEXT, ip_s, ip_d, dyn->vol2, dyn->packs2, RESET);

}

