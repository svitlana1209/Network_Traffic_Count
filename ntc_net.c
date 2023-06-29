#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <unistd.h>
#include <string.h>
#include <ifaddrs.h>
#include <stdbool.h>
#include <ntc.h>
#include <ntc_net.h>
#include <ntc_tools.h>

int listen_interface(int if_idx) {
    int id_socket, sock_len;
    struct sockaddr_ll packet_raw_socket;

    if ((id_socket = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) < 0)
        quit("Socket access error");

    memset(&packet_raw_socket, 0, sizeof(packet_raw_socket));

    packet_raw_socket.sll_family   = AF_PACKET;
    packet_raw_socket.sll_ifindex  = if_idx;
    packet_raw_socket.sll_protocol = htons(ETH_P_ALL);
    sock_len = sizeof(packet_raw_socket);

    if (bind(id_socket, (struct sockaddr *) &packet_raw_socket, sock_len) < 0)
        quit("Socket bind error");

    return id_socket;
}

void receive_from_socket(int id_socket, packet *p) {
size_t fromlen;
struct sockaddr_in src_addr;

    memset(&(*p), 0, sizeof(*p));
    memset(&src_addr, 0, sizeof(src_addr));

    src_addr.sin_family = AF_INET;
    src_addr.sin_addr.s_addr = inet_addr("1.2.3.4"); /* :) */
    src_addr.sin_port = htons(0);
    fromlen = sizeof(src_addr);

    p->size = recvfrom(id_socket, p->buff, sizeof(p->buff), 0, (struct sockaddr *)&src_addr, (socklen_t *)&fromlen);
    if (p->size < 0) {
        close(id_socket);
        quit("Recieve socket error");
    }
}

/*
    Returns IP address by network interface name
*/
u_int32_t get_interface_ip(char *if_name) {
struct ifaddrs *addrs, *tmp;
u_int32_t ip;

    getifaddrs(&addrs);
    tmp = addrs;
    ip = 0;

    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
            if (strcmp(tmp->ifa_name, if_name) == 0) {
                ip = addr_to_int(inet_ntoa(pAddr->sin_addr));
            }
        }
        if (ip == 0)
            tmp = tmp->ifa_next;
        else
            break;
    }
    freeifaddrs(addrs);
    return ip;
}

/*
   Converts the IP address "aaa.bbb.ccc.ddd" to the format little-endian (host byte order)
*/
u_int32_t addr_to_int(char *str) {
u_int32_t ip;
u_int8_t i, j, y;
char s[4];

    y=24; ip=0; j=0;
    memset (s,0,sizeof(s));

    for (i=0; str[i]!='\0'; i++) {
        if (str[i]!='.') {
            s[j]=str[i];
            j++;
        }
        else {
            s[j]='\0';
            ip=ip+(0xFFFFFFFF & (atoi(s)<<y));
            j=0;
            y=y-8;
        }
    }
    s[j]='\0';
    ip=ip+(0xFFFFFFFF & (atoi(s)<<y));

    return ip;
}

/*
   Converts the IP address from integer (little-endian) to string "aaa.bbb.ccc.ddd"
*/
void intaddr_to_string(u_int32_t d, u_int8_t *addr) {
int y, p, length, octet, rest, z;
char m[4];

    z=length = 0;
    y = 24;
    while (y >= 0) {
        memset (m,0,3);
        p = octet = 0x000000FF & (d >> y);
        y=y-8;
        length = 0;
        if (octet == 0) {
            addr[z] = '0';
            z++;
        }
        else {
            while (p != 0) {
                length++;
                p = p / 10;
            }
            for (p = 0; p < length; p++) {
                rest = octet % 10;
                octet = octet / 10;
                m[p] = rest + '0';
            }
            m[p]='\0';
            p--;
            while (p >= 0) {
                addr[z] = m[p];
                p--;
                z++;
            }
        }
        if (y >= 0) {
            addr[z] = '.'; 
            z++;
        }
    }
    addr[z] = '\0';
}

