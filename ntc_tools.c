#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
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

    if ((new_srcIP == current_srcIP) && (new_dstIP == current_dstIP)) return 0;
    if (new_srcIP > current_srcIP) return  1;
    if (new_srcIP < current_srcIP) return -1;
    if (new_dstIP > current_dstIP) return  1;
    if (new_dstIP < current_dstIP) return -1;
    return 0;
}

int ht_count(hashtable *ht) {
int count;

    count = 0;
    while (ht) {
        count++;
        ht =  ht->next;
    }
    return count;
}

void int_date_to_str(u_int32_t ymd, char *str) {
int p;
u_int16_t y;
u_int8_t d,i;
char c, r, m[4];

    y =  ymd >> 16;
    for (i=0; i<5; i++) {
        d = ymd & 0x000000FF;
        c = (d / 10) + '0';
        r = (d % 10) + '0';
        str[i] = c; i++;
        str[i] = r; i++;
        str[i] = '.';
        ymd = ymd >> 8;
    }
    for (p = 0; p < 4; p++) {
        r = y % 10;
        y = y / 10;
        m[p] = r + '0';
    }
    for (p=p-1; p>=0; p--,i++)
        str[i] = m[p];
    str[i] = '\0';
}
