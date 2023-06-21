#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <ntc.h>
#include <ntc_dyn.h>
#include <ntc_tools.h>

void get_dynamic_info(hashtable *ht, dyn_struct *dyn) {
HashKey *hkey, *hkey_pair;

    hkey = get_max_vol(ht);
    dyn.src_x  = hkey->srcIP;
    dyn.dst_y  = hkey->dstIP;
    dyn.vol1   = hkey->vol;
    dyn.packs1 = hkey->packs;

    hkey_pair = get_pair_for_max(ht, hkey);
    if (hkey_pair) {
        dyn.src_y  = hkey_pair->srcIP;
        dyn.dst_x  = hkey_pair->dstIP;
        dyn.vol2   = hkey_pair->vol;
        dyn.packs2 = hkey_pair->packs;
    }
    else {
        dyn.src_y  = 0;
        dyn.dst_x  = 0;
        dyn.vol2   = 0;
        dyn.packs2 = 0;
    }
}

HashKey * get_max_vol(hashtable *ht_head) {
u_int32_t max_vol;
HashKey *hkey, *hkey_max;
hashtable *ht;

    max_vol = 0;
    ht = ht_head;
    while (ht) {
        hkey = ht->ptr_to_hashkey;
        while (hkey) {
            if (hkey->vol > max_vol) {
                max_vol = hkey->vol;
                hkey_max = hkey;
            }
            hkey = hkey->next;
        }
        ht = ht->next;
    }
    return hkey_max;
}

HashKey * get_pair_for_max(hashtable *ht_head, HashKey *hkey_max) {
HashKey *hkey;
hashtable *ht;

    ht = ht_head;
    while (ht) {
        hkey = ht->ptr_to_hashkey;
        while (hkey) {
            if ((hkey-srcIP == hkey_max->dstIP) && (hkey-dstIP == hkey_max->srcIP)) {
                return hkey;
            }
            hkey = hkey->next;
        }
        ht = ht->next;
    }
    return NULL;
}
