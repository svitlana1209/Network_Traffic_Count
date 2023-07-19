#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <ntc.h>
#include <ntc_db.h>
#include <ntc_tools.h>
/*
    DB page structure:
    +----------------+-------------+-------------+     +-------------+
    | service_record | data_record | data_record | ... | data_record |
    |     20 bytes   |   24 bytes  |   24 bytes  | ... |   24 bytes  |
    +----------------+-------------+-------------+     +-------------+

    ------+--------------------+---------------+--------------------------------------------------
    Field | Field name         |  Field size,  | Description
    number|                    |  bites        |
    ------+--------------------+---------------+--------------------------------------------------
      SERVICE FIELDS AT THE TOP OF THE PAGE (DB_SERVICE_RECORD):
      1     count                    4           Number of records (db_data_record) per page.
      2     page_number              4           This page number. Numbering starts from one.
      3     page_for_write           4           Work page number to write (FOR CORE PAGE ONLY!).
      4     page_max                 4           Number of pages in db file (FOR CORE PAGE ONLY!).
      5     db_records_number        4           Number of records in DB (FOR CORE PAGE ONLY!).
      -------- TOTAL -------------- 20 bytes
      DATA FIELDS (DB_DATA_RECORD):
      1     YEAR                     2           (u_int16_t)
            MONTH                    1           (u_int8_t)
            DAY                      1           (u_int8_t)
      2     srcIP                    4           (u_int32_t)
      3     dstIP                    4           (u_int32_t)
      4     vol                      8           (long long int)
      5     packs                    4           (u_int32_t)
      -------- TOTAL -------------- 24 bytes
*/

/*
    Structure of records on index file page:
    +----------------+-------------+-------------+     +-------------+
    | service_record | data_record | data_record | ... | data_record |
    |     32 bytes   |   24 bytes  |  24 bytes   | ... |   24 bytes  |
    +----------------+-------------+-------------+     +-------------+

    ------+--------------------+-----------+--------------------------------------------------------------------------------------------------
    Field | Field name         |  Field    | Description
    number|                    |  size,    |
          |                    |  bites    |
    ------+--------------------+-----------+--------------------------------------------------------------------------------------------------
       IDX_SERVICE_RECORD:
       1     count                  4        Number of keys per page.
       2     level_number           4        Level number in the tree (0/1/2)
       3     page_number            4        Page number of idx file. Numbering starts from one.
       4     page_count             4        Number of pages in idx file (FOR CORE PAGE ONLY! If page number != 1 and page_count==0 then this is not the root page).
       5     offset_0               4        Page number "offset_0"
       6     rezerv                 4
       7     rezerv                 4
       8     rezerv                 4
      -------- TOTAL ------------- 32 bytes
       IDX_DATA_RECORD:
       1     YEAR                   2  ---+
             MONTH                  1     |
             DAY                    1     |--- This is the key
       2     srcIP                  4     |
       3     dstIP                  4  ---+
       4     page_lower_level       4        Page number of the lower level (offset_N).
       5     db_page_number         4        The page number of the db file for this key's data.
       6     db_offset_on_page      4        Offset inside the DB page relative to its beginning for the data of this key.
      -------- TOTAL ------------- 24 bytes


                                                     + ----------------------------------------------------------------+
        Level L0                                     |                        page 1 (ROOT)                            |
                                                     +--------+--------------+-------------------+---+-----------------+
                                                     |offset_0| key1;offset_1| ...key_i;offset_i |...| key_n;offset_ n |
                                                     + -------+--------------+-------------------+---+-----------------+
                                                      |                |                                        |
                                                      |                |                                        |
        Level L1                    +-------------------+     +------------------+
                                    |  page 2           |     |  page 3          | ..                     ...page (n+1)
                                    |key...key...n...   |     |key...key...n..   |
                                    +-------------------+     +------------------+
                                     |    ...     |
                                     |            |
        Level L2    +-------------------+    +------------------+
                    |  page             |    |  page            |
                    |key...key...2n     |    |key...key...2n    |
                    +-------------------+    + -----------------+
*/

CFG cfg;

void open_page_registry(CFG *config) {
void *idx_start_page, *db_start_page;
Page_registry *idx_pr, *db_pr;
u_int32_t *ptr;

    /* The first DB page: */
    db_start_page = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, config->db, 0);
    if (db_start_page == MAP_FAILED)
        quit ("mmap failed.\n");
    ptr = (u_int32_t*)db_start_page; /* count (Number of records (db_data_record) per page) */
    if ((*ptr) == 0) {
        /* Empty file */
        *(++ptr) = 1; /* page_number */
        *(++ptr) = 1; /* page_for_write */
        *(++ptr) = 1; /* page_max */
    }
    else {
        config->db_records = *(ptr+4); /* db_records_number */
    }

    /* Root page of IDX tree.
       Keep in memory until the task is completed or until the root of the tree is split.
    */
    idx_start_page = mmap (NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, config->idx, 0);
    if (idx_start_page == MAP_FAILED)
        quit ("mmap failed.\n");
    ptr = (u_int32_t*)idx_start_page; /* count */
    if ((*ptr) == 0) {
        *(++ptr) = 0; /* level number (CORE) */
        *(++ptr) = 1; /* page_number */
        *(++ptr) = 1; /* page_count (Number of pages in idx file) */
    }

    db_pr = (Page_registry *)malloc(sizeof(Page_registry));
    db_pr->next = NULL;
    db_pr->prev = NULL;
    db_pr->page_addr = db_start_page;
    db_pr->page_number = 1;

    idx_pr = (Page_registry *)malloc(sizeof(Page_registry));
    idx_pr->next = NULL;
    idx_pr->prev = NULL;
    idx_pr->page_addr = idx_start_page;
    idx_pr->page_number = 1;

    config->db_page_registry  = db_pr;
    config->idx_page_registry = idx_pr;
}

void close_page_registry(CFG *config) {
Page_registry *idx_ree, *db_ree, *t;
u_int32_t *ptr;
void *db_start_page;

    db_ree  = config->db_page_registry;
    db_start_page = db_ree->page_addr;
    ptr = (u_int32_t*)db_start_page;
    *(ptr+4) = config->db_records;
    idx_ree  = config->idx_page_registry;

    if (db_ree != NULL) {
        while (db_ree->next != NULL) {
            t = db_ree->next;
            msync(db_ree->page_addr, PAGE_SIZE, MS_ASYNC);
            munmap(db_ree->page_addr, PAGE_SIZE);
            free(db_ree);
            db_ree = t;
        }
        msync(db_ree->page_addr, PAGE_SIZE, MS_ASYNC);
        munmap(db_ree->page_addr, PAGE_SIZE);
        free(db_ree);
    }
    if (idx_ree != NULL) {
        while (idx_ree->next != NULL) {
            t = idx_ree->next;
            msync(idx_ree->page_addr, PAGE_SIZE, MS_ASYNC);
            munmap(idx_ree->page_addr, PAGE_SIZE);
            free(idx_ree);
            idx_ree = t;
        }
        msync(idx_ree->page_addr, PAGE_SIZE, MS_ASYNC);
        munmap(idx_ree->page_addr, PAGE_SIZE);
        free(idx_ree);
    }
}

void upload_to_database(hashtable *ht) {
const char *fname_db="ntc.db", *fname_idx="ntc.idx";
struct stat st;
HashKey *hkey;
long long int poz;

    if ((cfg.db  = open (fname_db,  O_CREAT|O_RDWR|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
        quit("\nCan't write ntc.db\n");
    if ((cfg.idx = open (fname_idx, O_CREAT|O_RDWR|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
        quit("\nCan't write ntc.idx\n");

    if (fstat(cfg.db, &st) == -1)
        quit("fstat error");
    open_page_registry(&cfg);

    if (st.st_size < 1)
        ht_to_db(ht, &cfg);
    else {
        while (ht) {
            hkey = ht->ptr_to_hashkey;
            while (hkey) {
                poz = locate_record(hkey, cfg.idx, cfg.idx_page_registry);
                if (poz > 0)
                    update_rec_in_db(&cfg, poz, hkey);
                else
                    add_rec_to_db(&cfg, hkey);
                hkey = hkey->next;
            }
            ht =  ht->next;
        }
    }
    close_page_registry(&cfg);
    close(cfg.db);
    close(cfg.idx);
}

/*
    Looks for a key in the IDX tree.
    Returns the page number and offset from the beginning of the page of the db file, or 0 if there is no key.
*/
long long int locate_record(HashKey *hkey, int idx, Page_registry *idx_registry) {
long long int address;
u_int32_t *ptr, *count;
void *addr_page;
u_int8_t change_page;
u_int16_t i;
u_int32_t srcIP, dstIP, ymd_field, hkey_ymd, offset_previous, offset_i, next_offset, page_db, offset_db;
int rez_compare;

    addr_page = (*idx_registry)->page_addr;   /* core IDX page */
    ptr = count = (u_int32_t *)(addr_page);
    offset_previous = *(ptr + 4);             /* offset_0 */
    ptr = ptr + IDX_SERVICE_RECORD_LEN;       /* field YEAR+MONTH+DAY */
    i = 1;
    while (i <= (*count)) {
        ymd_field = *(ptr++);
        srcIP     = *(ptr++);
        dstIP     = *(ptr++);
        offset_i  = *(ptr++);   /* page_lower_level */
        page_db   = *(ptr++);   /* db_page_number */
        offset_db = *ptr;       /* db_offset_on_page */


        rez_compare = compare_keys (ymd_field, srcIP, dstIP, hkey);
        if (rez_compare == 0) {
            address = page_db;
            address = (address << 32) | offset_db;
            return address;
        }
        else {
            change_page = 1;
            if (rez_compare < 0)
                next_offset = offset_previous;  /* New_key < Key_i, take 'offset' to the left of the Key_i */
            else {
                if (*count == i)
                    next_offset = offset_i;     /* Last key on the page. Take last 'offset', because New_key > Key_last */
                else {
                    offset_previous = offset_i; /* The key is not the last, we take the next key from the page */
                    change_page = 0;
                    i++;
                    ptr++;
                }
            }
            if change_page = 1 {
                if (next_offset == 0)
                    return 0;                   /* Reached the last level. There is no key in the tree. */
                else {
                    addr_page = locate_page_in_registry(&(**idx_registry), next_offset);
                    if (addr_page == NULL)
                        addr_page = map_page_from_hdd_to_registry(&(*idx_registry), next_offset, idx, NULL);
                    ptr = count = (u_int32_t *)(addr_page);
                    offset_previous = *(ptr + 4);        /* offset_0 */
                    ptr = ptr + IDX_SERVICE_RECORD_LEN;  /* field YEAR+MONTH+DAY */
                    i = 1;
                }
            }
        }
    }
}

/*
    The function returns:
     0 if the keys are equal
    -1 if the new key is less than the existing one
     1 if the new key is greater than the existing one
*/
int compare_keys (u_int32_t ymd_db, u_int32_t srcIP_db, u_int32_t dstIP_db, HashKey *hkey) {
u_int32_t ymd_hkey;
int ymd, src, dst;

    ymd_hkey = ((hkey->year) << 16) | (((hkey->month) << 8) | hkey->day);

    ymd = ymd_hkey - ymd_db;
    src = hkey->srcIP - srcIP_db;
    dst = hkey->dstIP - dstIP_db;

    if ((ymd == 0) && (src == 0) && (dst == 0)) return 0;
    if (ymd > 0) return  1;
    if (ymd < 0) return -1;
    if (src > 0) return  1;
    if (src < 0) return -1;
    if (dst > 0) return  1;
    if (dst < 0) return -1;
}

void * locate_page_in_registry (Page_registry *registry, u_int32_t N_page) {
Page_registry *tmp;

    tmp = registry;
    while (tmp) {
        if (tmp->page_number == N_page)
            return tmp->page_addr;
        else
            tmp = tmp->next;
    }
    return NULL;
}

void ht_to_db(hashtable *ht, CFG *config) {

}

void update_rec_in_db(CFG *config, long long int poz, HashKey *hkey) {

}

void add_rec_to_db(CFG *config, HashKey *hkey){

}

