#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <stdbool.h>
#include <ntc.h>
#include <ntc_ht.h>
#include <ntc_tools.h>

/*
    HASHTABLE                   HashKey
    +-------+     +-+-+-+-----+-----+---+-----+   +---------+   +---------+
  0 | hash  |-----|Y|M|D|srcIP|dstIP|vol|packs|---| HashKey |...| HashKey |
  1 | hash  |     +-+-+-+-----+-----+---+-----+   +---------+   +---------+
 ...| ....  |
    |       |
  n | hash  |
    |       |
  M +-------+

*/

int set_htsize(int n) {
int p;
    /* Looking for the prime numer in the range [n;2*n] */
    for (p=n; p<(2*n); p++) {
        if (primality_test(p))
            break;
    }
    return p;
}

void destroy_hashtable(hashtable *ht) {
void *tmp;
HashKey *hkey;

    while (ht) {
        hkey = ht->ptr_to_hashkey;
        while (hkey) {
            tmp = hkey->next;
            free(hkey);
            hkey = tmp;
        }
        tmp = ht->next;
        free(ht);
        ht = tmp;
    }
}

void update_hashtable(hashtable *ht_found, Queue *queue_head) {
HashKey *hkey_found;

    hkey_found = locate_hkey(ht_found, queue_head);
    if (hkey_found)
        update_hashkey(hkey_found, queue_head);
    else
        append_new_hashkey(ht_found, queue_head, 1);
}

u_int32_t get_hash(u_int32_t s, u_int32_t d, int htsize) {

    return (s%htsize + d%htsize)%htsize;
}

hashtable * locate_hash(hashtable *ht_head, u_int32_t hash) {
hashtable *tmp;

    for (tmp=ht_head; tmp!=NULL; tmp=tmp->next) {
        if (tmp->hash == hash)
            return tmp;
    }
    return NULL;
}

HashKey * locate_hkey(hashtable *ht, Queue *q) {
HashKey *hkey_found, *tmp;

    hkey_found = NULL;
    for (tmp=ht->ptr_to_hashkey; tmp!=NULL; tmp=tmp->next) {
        if (tmp->srcIP==q->srcIP && tmp->dstIP==q->dstIP && tmp->year==q->year && tmp->month==q->month && tmp->day==q->day) {
            hkey_found = tmp;
            break;
        }
    }
    return hkey_found;
}

void update_hashkey(HashKey *hkey, Queue *q) {

    hkey->vol = hkey->vol + q->vol;
    hkey->packs = hkey->packs + 1;
}

void append_new_hashkey(hashtable *ht_found, Queue *q, u_int32_t packs) {
HashKey *new_hkey, *current_hkey, *tmp_hkey;
u_int8_t flag;

    flag = 0;
    current_hkey = ht_found->ptr_to_hashkey;
    new_hkey = (HashKey *)malloc(sizeof(HashKey));
    new_hkey->year  = q->year;
    new_hkey->month = q->month;
    new_hkey->day   = q->day;
    new_hkey->srcIP = q->srcIP;
    new_hkey->dstIP = q->dstIP;
    new_hkey->vol   = q->vol;
    new_hkey->packs = packs;
    do {
        if (compare_keys(current_hkey->srcIP, current_hkey->dstIP, new_hkey->srcIP, new_hkey->dstIP) < 0) {
            /* new_hkey < current_hkey */
            if (current_hkey->prev == NULL) { /* the head of the list */
                current_hkey->prev = new_hkey;
                new_hkey->next = current_hkey;
                new_hkey->prev = NULL;
                ht_found->ptr_to_hashkey = new_hkey;
            }
            else { /* between keys */
                tmp_hkey = current_hkey->prev;
                current_hkey->prev = new_hkey;
                new_hkey->next = current_hkey;
                new_hkey->prev = tmp_hkey;
                tmp_hkey->next = new_hkey;
            }
           flag = 1;
        }
        else {
            if (current_hkey->next != NULL) current_hkey = current_hkey->next;
            else {
                /* new_key > last record */
                current_hkey->next = new_hkey;
                new_hkey->prev = current_hkey;
                new_hkey->next = NULL;
                flag = 1;
            }
        }
    } while (flag != 1);
}

hashtable * append_to_hashtable(hashtable *ht_head, u_int32_t hash, Queue *q, u_int32_t packs) {
hashtable *tmp_ht, *current_ht, *new_ht_row;
HashKey *new_hkey;
u_int8_t flag;

    flag = 0;

    new_ht_row = (hashtable *)malloc(sizeof(hashtable));
    new_ht_row->hash = hash;
    new_ht_row->ptr_to_hashkey = (HashKey *)malloc(sizeof(HashKey));
    new_ht_row->next = NULL;
    new_ht_row->prev = NULL;

    new_hkey = new_ht_row->ptr_to_hashkey;
    new_hkey->year  = q->year;
    new_hkey->month = q->month;
    new_hkey->day   = q->day;
    new_hkey->srcIP = q->srcIP;
    new_hkey->dstIP = q->dstIP;
    new_hkey->vol   = q->vol;
    new_hkey->packs = packs;
    new_hkey->next  = NULL;
    new_hkey->prev  = NULL;

    if (ht_head == NULL)
        ht_head = new_ht_row;
    else {
        current_ht = ht_head;
        do {
            if (hash < current_ht->hash)  {
                /* Inserting a new hash before the current one */
                if (current_ht->prev == NULL) {  /* the head of the list */
                    current_ht->prev = new_ht_row;
                    new_ht_row->next = current_ht;
                    new_ht_row->prev = NULL;
                    ht_head = new_ht_row;
                }
                else { /* between cells */
                    tmp_ht = current_ht->prev;
                    current_ht->prev = new_ht_row;
                    new_ht_row->next = current_ht;
                    new_ht_row->prev = tmp_ht;
                    tmp_ht->next = new_ht_row;
                }
                flag = 1;
            }
            else {
                if (current_ht->next != NULL)
                    current_ht = current_ht->next;
                else {
                    /* Add new hash after the last record */
                    current_ht->next = new_ht_row;
                    new_ht_row->prev = current_ht;
                    new_ht_row->next = NULL;
                    flag = 1;
                }
            }
        } while (flag != 1);
    }
    return ht_head;
}

int count_records_hashtable(hashtable *ht) {
int recs;
hashtable *tmp;

    recs = 0;
    for (tmp=ht; tmp!=NULL; tmp=tmp->next) recs++;
    return recs;
}

hashtable * rehash(hashtable *ht, int htsize) {
u_int32_t hash;
void *tmp;
HashKey *hkey;
hashtable *new_ht, *ht_found;
Queue *q;

    new_ht = NULL;
    q = (Queue *)malloc(sizeof(Queue));
    q->next = NULL;
    q->prev = NULL;

    while (ht) {
        hkey = ht->ptr_to_hashkey;
        while (hkey) {
            q->year  = hkey->year;
            q->month = hkey->month;
            q->day   = hkey->day;
            q->srcIP = hkey->srcIP;
            q->dstIP = hkey->dstIP;
            q->vol   = hkey->vol;
            hash = get_hash(q->srcIP, q->dstIP, htsize);
            ht_found = locate_hash(new_ht, hash);
            if (ht_found) {
                if (locate_hkey(ht_found, q) == NULL)
                    append_new_hashkey(ht_found, q, hkey->packs);
            }
            else
                new_ht = append_to_hashtable(new_ht, hash, q, hkey->packs);

            tmp = hkey->next;
            free(hkey);
            hkey = tmp;
        }
        tmp = ht->next;
        free(ht);
        ht = tmp;
    }
    free(q);
    return new_ht;
}

