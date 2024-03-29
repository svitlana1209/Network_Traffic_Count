#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <net/if.h>
#include <stdbool.h>
#include <termios.h>
#include <math.h>
#include <time.h>
#include <ntc.h>
#include <ntc_net.h>
#include <ntc_tools.h>
#include <ntc_queue.h>
#include <ntc_ht.h>
#include <ntc_dyn.h>
#include <ntc_terminal.h>
#include <ntc_reports.h>
#include <ntc_db.h>

FILE *t_tty;
struct termios init_term;
sem_t sem_get_pack, sem_ht;
int key_exit, db, id_socket, network_interface_idx, network_interface_type, htsize;
pthread_t thread_wait_key, thread_queue, thread_display_dyn;
Queue *queue_head, *queue_tail;
hashtable *ht_head;
total all_traf;
dyn_struct dyn;
u_int32_t ip;
struct tm *time_ptr;
time_t the_time;
char start_time[20];

int main(int argc, char *argv[]) {
double fill_factor;
Queue *new_queue_head;
hashtable *ht_found;
u_int32_t hash;

    key_exit = 0;
    db = 0;
    network_interface_type = -1;

    if (argc < 2) {
        printf("Usage example: ./ntc enp3s0\n");
        printf("           or: ./ntc enp3s0 db\n");
        printf("        where: 'enp3s0' - network interface name\n");
        printf("               'db' - save data to database\n");
        exit(0);
    }

    (void) time (&the_time);
    time_ptr = localtime (&the_time);
    strftime(start_time, 20, "%Y-%m-%d %H-%M-%S", time_ptr);

    network_interface_idx = if_nametoindex(argv[1]);
    if (network_interface_idx == 0)
        quit("Network interface name not found");
    if (strstr(argv[1], "enp")!=NULL || strstr(argv[1], "eth")!=NULL)
        network_interface_type = 0;
    if (strstr(argv[1], "ppp")!=NULL)
        network_interface_type = 1;
    if (network_interface_type < 0)
        quit("Set the name of network interface. For example: enp3s0 or eth1 or ppp0.");
    ip = get_interface_ip(argv[1]);

    if (argc == 3) {
        if (strcmp(argv[2],"db") == 0)
            db = 1;
        else
            quit("Specify either 'db' or nothing as the second argument");
    }
    call_init();
    while (key_exit == 0) {
        /* Waiting for a packet */
        sem_wait(&sem_get_pack);
        /* Pulls an element from the head of the queue and puts it onto a hash table */
        /* Queue head address will change */

        hash = get_hash(queue_head->srcIP, queue_head->dstIP, htsize);
        ht_found = locate_hash(ht_head, hash);
        if (ht_found)
            update_hashtable(ht_found, queue_head);
        else {
            fill_factor = htsize * 0.75;
            if (count_records_hashtable(ht_head) > fill_factor) {
                htsize  = set_htsize(htsize*2); /* 6 (!) */
                ht_head = rehash(ht_head, htsize);
            }
            ht_head = append_to_hashtable(ht_head, hash, queue_head, 1);
        }
        new_queue_head = queue_head->next;
        new_queue_head->prev = NULL;
        free(queue_head);
        queue_head = new_queue_head;
        sem_post(&sem_ht);
    }
    call_exit();
    return 0;
}

void call_init(){

    if (!(t_tty  = fopen ("/dev/tty","r")))
        quit("tty access error");
    init_terminal(&init_term, t_tty);

    queue_head = queue_tail = create_queue();
    ht_head = NULL;
    htsize = set_htsize(pow(2,16));

    all_traf.in = 0;
    all_traf.out = 0;

    if (sem_init(&sem_get_pack, 0, 0) != 0)
        quit("Semaphore initialization failed (get_pack)");
    if (sem_init(&sem_ht, 0, 0) != 0)
        quit("Semaphore initialization failed (hashtable)");
    if ((pthread_create(&thread_wait_key, NULL, waiting_for_key, (void *)(t_tty))) != 0)
        quit("Thread creation failed (wait key)");
    if ((pthread_create(&thread_queue, NULL, process_queue, (void *)(queue_tail))) != 0)
        quit("Thread creation failed (queue)");
    if ((pthread_create(&thread_display_dyn, NULL, display_info, NULL ) ) != 0)
        quit("Thread creation failed (display_dyn)");
    print_head();
}

void call_exit() {

    generate_report(ht_head, &all_traf, ip, network_interface_idx, start_time);
    restore_terminal(&init_term, t_tty);
    if (db == 1) {
        upload_to_database(ht_head);
        printf("\n %sDone%s\n", GREEN_TEXT, RESET);
    }

    if ((pthread_join(thread_wait_key, NULL)) != 0)
        quit("Thread join failed (wait key)");

    destroy_queue(queue_head);
    destroy_hashtable(ht_head);
    sem_destroy(&sem_get_pack);
    sem_destroy(&sem_ht);
    close(id_socket);
}

void * waiting_for_key (void *ptr_tty) {
FILE *terminal;

    terminal = (FILE *)(ptr_tty);
    while (1) {
        if (fgetc(terminal) == 'q') {
            key_exit = 1;
            break;
        }
    }
    pthread_exit(NULL);
}

void * process_queue(void *tail) {
Queue *tmp;
packet pack;

    id_socket = listen_interface(network_interface_idx);
    queue_tail = (Queue *)(tail);
    while(1) {
        receive_from_socket(id_socket, &pack);
        tmp = add_to_queue(queue_tail, &pack, network_interface_type, &all_traf, ip);
        if (tmp != queue_tail)        /* if pack was added to queue */
            sem_post(&sem_get_pack);  /* then set the semaphore */
        queue_tail = tmp;
    }
    pthread_exit(NULL);
}

void * display_info() {

    if (pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL) != 0)
        quit ("Thread setcancelstate failed.");
    if (pthread_setcanceltype  (PTHREAD_CANCEL_DEFERRED, NULL) != 0)
        quit ("Thread setcanceltype failed.");

    while(1) {
        sem_wait(&sem_ht);
        get_dynamic_info(get_ht_addr(), &dyn);
        display_dynamic_info(&dyn, &all_traf);
    }
    pthread_exit(NULL);
}

hashtable * get_ht_addr() {
    return ht_head;
}
