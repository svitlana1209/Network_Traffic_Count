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

void destroy_hashtable (hashtable *ht) {
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

