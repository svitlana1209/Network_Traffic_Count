#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <ntc.h>
#include <ntc_tools.h>

bool primality_test(int p) {
int rest;
    rest = mod(p,6);
    if (rest==1 || rest==5)
        return true;
    return false;
}

int mod(int p, int m) {
    do
        p = p - m;
    while (p>m);
    return p;
}

void quit(char *s) {
    printf("\n%s\n",s);
    exit(EXIT_FAILURE);
}

/*
    Select 'num' bytes from the packet 'p' from position 'poz'.
    If size of packet is too small and 'poz' exceeds packet size then returns -1
*/
int get_from_packet(packet *p, u_int8_t poz, u_int8_t num) {
int number;
u_int8_t l, i;

    number = 0;
    if (p->size < (poz+num))
        number = -1;
    else {
        l = 8 * (num-1);
        for (i=poz; i<(poz+num); i++) {
            number = number + (0xFFFFFFFF & (p->buff[i]<<l));
            l = l - 8;
        }
    }
    return number;
}

/*
    Detects the traffic direction.
    Returns 0 (incoming) or 1 (outgoing).
*/
u_int8_t detect_direction(u_int32_t IPs, u_int32_t IPd, u_int32_t ip_if) {
u_int8_t d;

    if ((IPd != ip_if) && (IPs != ip_if))
        d = 0;
    else {
        if (IPd == ip_if)
            d = 0;
        else
            d = 1;
    }
    return d;
}

int compare_keys(u_int32_t current_srcIP, u_int32_t current_dstIP, u_int32_t new_srcIP, u_int32_t new_dstIP) {
int result;

    if (new_srcIP > current_srcIP) result = 1;
    else {
        if (new_srcIP < current_srcIP) result = -1;
        else {
            if (new_dstIP > current_dstIP) result = 1;
            else {
                if (new_dstIP < current_dstIP) result = -1;
                else
                    result = 0;
            }
        }
    }
    return result;
}

