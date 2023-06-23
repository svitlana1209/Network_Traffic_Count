#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <ntc.h>
#include <ntc_tools.h>
#include <ntc_queue.h>

Queue * create_queue() {
Queue *q;

    q = (Queue *)malloc(sizeof(Queue));
    q->next = NULL;
    q->prev = NULL;
    q->year = 0;
    q->month = 0;
    q->day = 0;
    q->srcIP = 0;
    q->dstIP = 0;
    q->vol = 0;

    return q;
}

void destroy_queue (Queue *queue_head) {
Queue *q, *t;

    q = queue_head;
    if (q != NULL) {
        while (q->next != NULL) {
            t = q->next;
            free (q);
            q = t;
        }
        free (q);
    }
}

/*
    The packet is added to the tail of the queue.
    ip_if: IP addr of the network interface.
    traf: ptr to struct (vol incoming/outgoing traffic).
    If there is no IPsrc/IPdst in the packet, then it is not added to queue,
    it is simply summed up in the total volume.
*/
Queue * add_to_queue(Queue *queue_tail, packet *pack, int if_type, total *traf, u_int32_t ip_if) {
Queue * tail, *new_tail;
struct tm *time_ptr;
time_t the_time;
u_int32_t ip_s, ip_d, protocol;
u_int8_t direction, i, brdcast;

    ip_s = 0;
    ip_d = 0;
    brdcast = 1;
    direction = 1;  /* incoming traffic*/

    switch (if_type) {
        case 0: {
            protocol = get_from_packet(pack, 12, 2);
            for (i=0; i<6; i++) {
                if (pack->buff[i] != 0xff) {
                    brdcast = 0;
                    break;
                }
            }
            switch (protocol) {
                case 0x806: {
                    /* ARP */
                    ip_s = get_from_packet(pack, 28, 4);
                    ip_d = get_from_packet(pack, 38, 4);
                }
                case 0x800: {
                    /* IPv4 */
                    ip_s = get_from_packet(pack, 26, 4);
                    ip_d = get_from_packet(pack, 30, 4);
                    if (brdcast == 0)
                        direction = detect_direction(ip_s, ip_d, ip_if);
                }
            }
        }
        case 1: {
            ip_s = get_from_packet(pack, 12, 4);
            ip_d = get_from_packet(pack, 16, 4);
            direction = detect_direction(ip_s, ip_d, ip_if);
        }
    }
    if (direction == 0)
        traf->out = traf->out + pack->size;
    else
        traf->in = traf->in + pack->size;

    if (ip_s != 0) {
        (void) time(&the_time);
        time_ptr = localtime(&the_time);
        tail = queue_tail;
        tail->year  = (u_int16_t)(1900 + time_ptr->tm_year);
        tail->month = (u_int8_t)(time_ptr->tm_mon);
        tail->day   = (u_int8_t)(time_ptr->tm_mday);
        tail->srcIP = ip_s;
        tail->dstIP = ip_d;
        tail->vol   = pack->size;

        new_tail = (Queue *)malloc(sizeof(Queue));
        new_tail->next = NULL;
        new_tail->prev = tail;
        new_tail->year  = 0;
        new_tail->month = 0;
        new_tail->day   = 0;
        new_tail->srcIP = 0;
        new_tail->dstIP = 0;
        new_tail->vol   = 0;
        tail->next = new_tail;
    }
    else
        new_tail = queue_tail;

    return new_tail;
}

