#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <net/if.h>
#include <termios.h>
#include <term.h>
#include <ntc.h>
#include <ntc_net.h>
#include <ntc_tools.h>
#include <ntc_reports.h>

void generate_report(hashtable *ht_head, total *traf, u_int32_t ip, int if_idx, char *start) {
struct tm *time_ptr;
time_t the_time;
FILE *report;
u_int8_t ip_s[16], ip_d[16];
char if_name[10], report_name[29], finish[20];
char *cursor;
hashtable *ht;
HashKey *hkey;

    cursor = tigetstr("cup");
    putp(tparm(cursor, 10, 0));
    printf(" %sExit. Please wait, the report is being generated ...%s\n", WHITE_TEXT, RESET);

    (void) time (&the_time);
    time_ptr = localtime (&the_time);
    strftime (report_name, 29, "ntc-%Y-%m-%d__%H-%M-%S.log", time_ptr);
    strftime (finish, 20, "%Y-%m-%d %H-%M-%S", time_ptr);
    if ((report =  fopen (report_name,  "w")) == NULL)
        quit ("\nCan't create the report\n");

    if_indextoname(if_idx, if_name);
    intaddr_to_string(ip, ip_s);

    fprintf(report, "Network interface: %s\n", if_name);
    fprintf(report, "IP address: %s\n", ip_s);
    fprintf(report, "Total traffic:\n");
    fprintf(report, "IN    : %lld\n", traf->in);
    fprintf(report, "OUT   : %lld\n", traf->out);
    fprintf(report, "Start : %s\n", start);
    fprintf(report, "Stop  : %s\n\n", finish);
    fprintf(report, "------------------------------------------------------------------------------------\n");
    fprintf(report, "YEAR MONTH DAY             IPsrc             IPdst                VOL          PACKS\n");
    fprintf(report, "------------------------------------------------------------------------------------\n");
    ht = ht_head;
    while (ht) {
        hkey = ht->ptr_to_hashkey;
        while (hkey) {
            intaddr_to_string(hkey->srcIP, ip_s);
            intaddr_to_string(hkey->dstIP, ip_d);
            fprintf(report, "%4d %2d     %2d   %15s   %15s   %16lld   %12d\n", hkey->year, hkey->month, hkey->day, ip_s, ip_d, hkey->vol, hkey->packs);
            hkey = hkey->next;
        }
        ht = ht->next;
    }
    fprintf(report, "------------------------------------------------------------------------------------\n");
    fclose(report);
    cursor = tigetstr("cup");
    putp(tparm(cursor, 11, 0));
    printf(" The report file: %s%s%s\n", CYAN_TEXT, report_name, RESET);
}

