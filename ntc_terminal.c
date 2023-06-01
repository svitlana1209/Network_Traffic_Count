#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <ntc_tools.h>
#include <ntc_terminal.h>

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
