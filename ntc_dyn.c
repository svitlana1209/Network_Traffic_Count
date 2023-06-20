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
    dyn->src_x  = hkey->srcIP;
    dyn->dst_y  = hkey->dstIP;
    dyn->vol1   = hkey->vol;
    dyn->packs1 = hkey->packs;

    hkey_pair = get_pair_for_max(ht, hkey);
    dyn->src_y  = hkey_pair->srcIP;
    dyn->dst_x  = hkey_pair->dstIP;
    dyn->vol2   = hkey_pair->vol;
    dyn->packs2 = hkey_pair->packs;
}

