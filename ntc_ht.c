#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <ntc.h>
#include <ntc_ht.h>

/*
    HASHTABLE                   HashKey
    +-------+     +-+-+-+-----+-----+---+-----+   +---------+   +---------+
  0 | hash  |-----|Y|M|D|srcIP|dstIP|vol|packs|---| HashKey |...| HashKey |
  1 | hash  |     +-+-+-+-----+-----+---+-----+   +---------+   +---------+
 ...| ....  |
    |       |
  n | hash  |
    |       |
    |       |
  M +-------+

*/

Queue * fill_hashtable(Queue *queue_head, hashtable *ht_head) {
Queue *new_queue_head;
hashtable *ht_found;
HashKey *hkey_found;
u_int32_t hash;

    hash = get_hash(queue_head->srcIP, queue_head->dstIP);
    ht_found = locate_hash(ht_head, hash);
    if (ht_found) {
        hkey_found = locate_hkey(ht_found, queue_head);
        if (hkey_found)
            update_hashkey(hkey_found, queue_head);
        else
            append_new_hashkey(ht_found, queue_head);
    }
    else
        ht_head = append_to_hashtable(ht_head, hash, queue_head);

    new_queue_head = queue_head->next;
    new_queue_head->prev = NULL;
    free(queue_head);
    return new_queue_head;
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

u_int32_t get_hash(u_int32_t s, u_int32_t d) {

    return (s%HTSIZE + d%HTSIZE)%HTSIZE;
}

hashtable * locate_hash(hashtable *ht_head, u_int32_t hash) {
hashtable *ht_found, *tmp;

    ht_found = NULL;
    for (tmp=ht_head; tmp->next != NULL; tmp=tmp->next) {
        if (tmp->hash == hash) {
            ht_found = tmp;
            break;
        }
    }
    return ht_found;
}

HashKey * locate_hkey(hashtable *ht, Queue *q) {
HashKey *hkey_found, *tmp;

    hkey_found = NULL;
    for (tmp=ht->ptr_to_hashkey; tmp->next != NULL; tmp=tmp->next) {
        if (tmp->srcIP==q->srcIP && tmp->dstIP==q->dstIP && tmp->year==q->year && tpm->month==q->month && tmp->day==q->day) {
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

void append_new_hashkey (hashtable *ht_found, Queue *q) {
HashKey *new_hkey, *current_hkey, *tmp_hkey;
u_int8_t flag;

    current_hkey = ht_found->ptr_to_hashkey;
    new_hkey = (HashKey *)malloc(sizeof(HashKey));
    new_hkey->year  = q->year;
    new_hkey->mont  = q->month;
    new_hkey->day   = q->day;
    new_hkey->srcIP = q->srcIP;
    new_hkey->dstIP = q->dstIP;
    new_hkey->vol   = q->vol;
    new_hkey->packs = 1;
    do {
        if (compare_keys(current_hkey, new_hkey) < 0) {
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

