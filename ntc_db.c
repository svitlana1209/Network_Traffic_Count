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
        quit("mmap failed.\n");
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
        quit("mmap failed.\n");
    ptr = (u_int32_t*)idx_start_page; /* count */
    if ((*ptr) == 0) {
        *(++ptr) = 0; /* level number (CORE) */
        *(++ptr) = 1; /* page_number */
        *(++ptr) = 1; /* page_count (Number of pages in idx file) */
        *(++ptr) = 0; /* offset_0 */
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
                    update_rec_in_db(&cfg, &poz, hkey);
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
u_int32_t srcIP, dstIP, ymd_field, offset_previous, offset_i, next_offset, page_db, offset_db;
int rez_compare;

    addr_page = idx_registry->page_addr;   /* core IDX page */
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


        rez_compare = compare(ymd_field, srcIP, dstIP, hkey);
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
            if (change_page == 1) {
                if (next_offset == 0)
                    return 0;                   /* Reached the last level. There is no key in the tree. */
                else {
                    addr_page = locate_page_in_registry(idx_registry, next_offset);
                    if (addr_page == NULL)
                        addr_page = map_page_from_hdd_to_registry(idx_registry, next_offset, idx, NULL);
                    ptr = count = (u_int32_t *)(addr_page);
                    offset_previous = *(ptr + 4);        /* offset_0 */
                    ptr = ptr + IDX_SERVICE_RECORD_LEN;  /* field YEAR+MONTH+DAY */
                    i = 1;
                }
            }
        }
    }
    return address;
}

/*
    The function returns:
     0 if the keys are equal
    -1 if the new key is less than the existing one
     1 if the new key is greater than the existing one
*/
int compare(u_int32_t ymd_db, u_int32_t srcIP_db, u_int32_t dstIP_db, HashKey *hkey) {
u_int32_t ymd_hkey;
int ymd, src, dst, rez;

    ymd_hkey = ((hkey->year) << 16) | (((hkey->month) << 8) | hkey->day);

    ymd = ymd_hkey - ymd_db;
    src = hkey->srcIP - srcIP_db;
    dst = hkey->dstIP - dstIP_db;

    if ((ymd == 0) && (src == 0) && (dst == 0)) rez = 0;
    if (ymd > 0) rez =  1;
    if (ymd < 0) rez = -1;
    if (src > 0) rez =  1;
    if (src < 0) rez = -1;
    if (dst > 0) rez =  1;
    if (dst < 0) rez = -1;

    return rez;
}

void * locate_page_in_registry(Page_registry *registry, u_int32_t N_page) {
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

/*
    Map page (target_file) from HDD into memory.
    The address and number of the new page writes into the Registry of open pages if there is a free space in the Registry.
    The number of records in the Registry does not exceed NUMBER_PAGES_OPEN (32).
    The function returns the address of the new mapped page.

    If the Registry already has 32 records, it is necessary to unmap one page before loading a new one.
    The root page is never removed from the Registry.
    The page removal occurs depending on the parameter:
    1) (Chain *) == NULL. To map a new page, the function will unmap any page except the root.
    2) (Chain *) != NULL. A Chain of pages passed by a key is transmitted to the function (the tail of the chain is transmitted).
                          This means that you can unmap any page that does not belong to the Chain (except the root).
*/
void * map_page_from_hdd_to_registry(Page_registry *registry, u_int32_t N_page, int target_file, Chain *cell) {
void *new_addr_page;
Page_registry *tmp, *new_rec;
u_int32_t i;
off_t place;

    place = PAGE_SIZE * (N_page-1);
    tmp = registry;
    new_addr_page = NULL;
    /* Looking for a free space in the Registry of open pages: */
    for (i=1; i<NUMBER_PAGES_OPEN; i++) {
        if (tmp->next == NULL) {
            /* There is a place in the Regisry. Mapping a page to memory (N_page*8192) */
            new_addr_page = mmap (NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, target_file, place);
            if (new_addr_page == MAP_FAILED)
                quit("mmap failed.\n");
            new_rec = (Page_registry *)malloc(sizeof(Page_registry));
            new_rec->next = NULL;
            new_rec->prev = tmp;
            new_rec->page_addr = new_addr_page;
            new_rec->page_number = N_page;
            tmp->next = new_rec;
            return new_addr_page;
        }
        else
            tmp = tmp->next;
    }
    /* No free space in the Registry: */
    tmp = registry;
    /* The Chain is not transmitted. You can unmap any page except the root: */
    if (cell == NULL) {
        tmp = tmp->next;
        /* Mapping a new page to memory (N_page*8192): */
        new_addr_page = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, target_file, place);
        if (new_addr_page == MAP_FAILED)
            quit("mmap failed.\n");
        /* Return a page from memory to disk: */
        msync(tmp->page_addr, PAGE_SIZE, MS_ASYNC);
        munmap(tmp->page_addr, PAGE_SIZE);
        /* Set the address and number of the new page in the Registry instead of the unmapped page: */
        tmp->page_addr = new_addr_page;
        tmp->page_number = N_page;
        return new_addr_page;
    }
    else {
        /* Check the Registry elements for entering the Chain.
           Remove the page not belonging to the Chain from the Registry, write the address of the new page in its place: */
        while (tmp != NULL) {
            if (page_in_chain(tmp->page_addr, cell))
                tmp = tmp->next;
            else {
                new_addr_page = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, target_file, place);
                if (new_addr_page == MAP_FAILED)
                    quit("mmap failed.\n");
                msync(tmp->page_addr, PAGE_SIZE, MS_ASYNC);
                munmap(tmp->page_addr, PAGE_SIZE);
                tmp->page_addr = new_addr_page;
                tmp->page_number = N_page;
                return new_addr_page;
            }
        }
    }
    if (new_addr_page == NULL)
        quit("Can't allocate the record in the Page Registry\n");
    /*
        Attention: if the height of the index tree (i.e. the length of the Chain) strives for NUMBER_PAGES_OPEN,
        (all the pages of the Registry are included in the Chain), then the function cannot delete any page from
        the Registry to add a new page. And will return Null.
        The height of the tree should be less than NUMBER_PAGES_OPEN.
    */
    return new_addr_page;
}

bool page_in_chain(void *addr, Chain *cell_tail) {
Chain *cell_current;

    cell_current = cell_tail;
    while (cell_current != NULL) {
        if (addr == cell_current->addr_page)
            return true;
        else
            cell_current = cell_current->prev;
    }
    return false;
}

void update_rec_in_db(CFG *config, long long int *poz, HashKey *hkey) {
int db;
u_int32_t page_for_write, offset_on_page;
u_int32_t *ptr;
long long int *lli;
void *addr_page;
Page_registry *db_registry;

    page_for_write = *poz >> 32;
    offset_on_page = *poz;
    db = config->db;
    db_registry = config->db_page_registry;

    addr_page = locate_page_in_registry(db_registry, page_for_write);
    if (addr_page == NULL) {
        addr_page = map_page_from_hdd_to_registry(db_registry, page_for_write, db, NULL);
    ptr = (u_int32_t *)addr_page;
    ptr = ptr + offset_on_page + 2;
    lli = (long long int *)ptr;
    *lli = *lli + hkey->vol;
    ptr = ptr + 2;
    *ptr = *ptr + hkey->packs;
}

void add_rec_to_db(CFG *config, HashKey *hkey) {
void *page_addr;
u_int32_t *ptr, *count, *page_number, *page_for_write, *page_max;
long long int *lli;
Page_registry *db_registry;
int db, flag;
u_int32_t begin_cur_record;
u_int8_t new_page;

    db_registry  = config->db_page_registry;
    db  = config->db;
    /* First DB page: */
    ptr = (u_int32_t *)(db_registry->page_addr);
    count = ptr;
    page_number    = ++ptr;
    page_for_write = ++ptr;
    page_max       = ++ptr;
    if (*page_for_write > 1) {
        new_page = 0;
        page_addr = locate_page_in_registry(db_registry, *page_for_write);
        if (page_addr == NULL) {
            if (*page_for_write > *page_max) {
                new_page = 1;                                  /* This is a new page, it is not on disk */
                ftruncate (db, PAGE_SIZE *(*page_for_write));  /* Extended file to page size */
                (*page_max)++;
            }
            page_addr = map_page_from_hdd_to_registry(db_registry, *page_for_write, db, NULL);
        }
        ptr = (u_int32_t *)(page_addr);
        count = ptr;
        page_number = ++ptr;
        if (new_page == 1)
            *page_number = *page_for_write;
    }
    begin_cur_record = DB_SERVICE_RECORD_LEN + (*count * DB_DATA_RECORD_LEN);
    ptr = count + begin_cur_record;
    *(ptr++) = ((hkey->year) << 16) | (((hkey->month) << 8) | hkey->day);
    *(ptr++) = hkey->srcIP;
    *(ptr++) = hkey->dstIP;
    lli = (long long int *)ptr;
    *(lli++) = hkey->vol;
    ptr = ptr + 2;
    *ptr = hkey->packs;
    (*count)++;
    (config->db_records)++;

    if (*count >= N_DB) {
        if (*page_for_write > 1)
            unload_page(db_registry, page_addr);
        (*page_for_write)++; /* There is no free space. Next page to write */
    }
    flag = add_key_to_idx(hkey, *page_for_write, begin_cur_record, config);
    if (flag < 0)
        quit("Split page error. The IDX tree is full.\n");
}

void unload_page(Page_registry *registry, void *addr_page) {
Page_registry *tmp, *head, *tail;

    for (tmp = registry; tmp->page_addr != addr_page; tmp = tmp->next) {
        if (tmp == NULL)
            quit("Page unload error\n");
    }
    if (tmp != registry) {
        head = tmp->prev;
        tail = tmp->next;
        head->next = tail;
        tail->prev = head;
        free(tmp);
        msync(addr_page, PAGE_SIZE, MS_ASYNC);
        munmap(addr_page, PAGE_SIZE);
    }
}

int add_key_to_idx(HashKey *hkey, u_int32_t db_page_number, u_int32_t offset_on_db_page, CFG *config) {
Page_registry *idx_registry;
u_int32_t *count_idx_pages, *top_of_page;
u_int32_t N_page, keys_limit, current_level, count, last_level;
int flag, idx;
void *addr_page;
Chain *cell_head, *cell_tail;
idx_page_content *new_key;

    idx_registry = config->idx_page_registry;
    idx = config->idx;
    last_level = IDX_LEVEL_LIMIT - 1;

    /* Root IDX page */
    top_of_page = (u_int32_t *)((*idx_registry).page_addr);
    count_idx_pages = top_of_page + 3;
    if (*top_of_page < (N_IDX)) {
        /* If the Root contains less than 170 keys: */
        add_key_to_current_idx_page(top_of_page, hkey, db_page_number, offset_on_db_page);
        return 0;
    }
    /* Root page is full. Further, the root will be filled only when splitting the pages of the lower level. */
    cell_head = (Chain *)malloc(sizeof(Chain));
    cell_head->addr_page = idx_registry->page_addr;
    cell_head->next = NULL;
    cell_head->prev = NULL;
    cell_tail = cell_head;
    N_page = choice_offset(top_of_page, hkey);
    while (N_page != 0xFFFFFFFF) {
        addr_page = locate_page_in_registry(idx_registry, N_page);
        if (addr_page == NULL)
            addr_page = map_page_from_hdd_to_registry(idx_registry, N_page, idx, cell_tail);
        top_of_page = (u_int32_t *)addr_page;
        count = *top_of_page;
        current_level = *(top_of_page + 1);
        keys_limit = (current_level == last_level) ? ((2*N_IDX)-1) : N_IDX;
        if (count < (keys_limit)) {
            add_key_to_current_idx_page(top_of_page, hkey, db_page_number, offset_on_db_page);
            destroy_chain(cell_head);
            return 0;
        }
        cell_tail = new_cell(cell_tail, addr_page);
        N_page = choice_offset(top_of_page, hkey);
    }
    /* Last level: */
    new_key = key(hkey, N_page, db_page_number, offset_on_db_page);
    flag = split(addr_page, new_key, config, cell_tail);
    *count_idx_pages = config->idx_page_count;
    destroy_chain(cell_head);
    return flag;
}

Chain * new_cell(Chain *old_tail, void *addr_page) {
Chain *new_tail;

    new_tail = (Chain *)malloc(sizeof(Chain));
    new_tail->addr_page = addr_page;
    new_tail->prev = old_tail;
    new_tail->next = NULL;
    old_tail->next = new_tail;
    return new_tail;
}

void destroy_chain(Chain *cell) {
Chain *tmp;

    while (cell->next != NULL) {
        tmp = cell->next;
        free(cell);
        cell = tmp;
    }
    free (cell);
}

/* choice_offset() looks for which sublevel page the key can be written to.
   Returns page number of the lower level.
   We assume that there is no such key. This is what the search procedure says.
   Algorithm:
    1) x < k1:       the new key will be written on the page with the number 'offset_0';
    2) k_i<x<k_(i+1) the new key will be written on the page with the number 'offset_i';
    3) x > k_n:      the new key will be written on the page with the number 'offset_n'.

  +-----------------------------------------------------------------+
  |                           page N...                             |
  +-----------------------------------------------------------------+
  |offset_0| k1;offset_1  | ...k_i;offset_i   |...| k_n;offset_ n   |
  ------------------------------------------------------------------+
*/
u_int32_t choice_offset(u_int32_t *ptr, HashKey *hkey) {
u_int32_t count, level, i, srcIP, dstIP, ymd_field;
u_int32_t *offset_p, *offset_i;
int rez_compare;
u_int8_t step;

    count = *ptr;
    level = *(ptr + 1);
    offset_i = ptr = ptr + 4;   /* offset_0 */
    step = 4;

    for (i=1; i<=count; i++) {
        ptr = ptr + step;
        ymd_field = *(ptr++);
        srcIP = *(ptr++);
        dstIP = *(ptr++);
        offset_p = offset_i;
        offset_i = ptr;         /* page_lower_level */
        step = 3;
        rez_compare = compare(ymd_field, srcIP, dstIP, hkey);
        if (rez_compare < 0)    /* if the new key (hkey) is less than the current one */
            break;
    }
    if (i > count)
        offset_p = offset_i;
    if (*offset_p == 0)                 /* no page */
        *offset_p = add_page(level+1);  /* lower level page */

    return *offset_p;
}

/*  Adds a page to the target level.
    Page numbers in ascending order.
    Returns the new page number (max+1).
 */
u_int32_t add_page(u_int32_t level) {
off_t offset;
u_int32_t *ptr;
u_int32_t offset_0;
void *page_addr;

    (cfg.idx_page_count)++;
    offset_0 = (level == (IDX_LEVEL_LIMIT - 1)) ? 0xFFFFFFFF : 0;
    /* Add a new page to the IDX file, expand the file to the size of the page: */
    ftruncate (cfg.idx, PAGE_SIZE * (cfg.idx_page_count));
    /* Loading the page into memory and writing new values: */
    offset = PAGE_SIZE * ((cfg.idx_page_count)-1);
    page_addr = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, cfg.idx, offset);
    ptr = (u_int32_t *)page_addr;
    *(++ptr) = level;
    *(++ptr) = cfg.idx_page_count;
    ptr = ptr + 2;
    *ptr = offset_0;
    /* Returning the page to disk: */
    msync (page_addr, PAGE_SIZE, MS_ASYNC);
    munmap (page_addr, PAGE_SIZE);

    return cfg.idx_page_count;
}

void add_key_to_current_idx_page(u_int32_t *ptr, HashKey *hkey, u_int32_t db_page_number, u_int32_t offset_on_db_page) {
u_int32_t offset_lower_level, this_level, ymd_hkey;
idx_page_content *list, *new_key;
u_int32_t *count;

    count = ptr;
    this_level = *(ptr + 1);
    offset_lower_level = (this_level == (IDX_LEVEL_LIMIT - 1)) ? 0xFFFFFFFF : 0;
    ptr = ptr + IDX_SERVICE_RECORD_LEN;

    if (*count == 0) {
        *(ptr++) = ymd_hkey;
        *(ptr++) = hkey->srcIP;
        *(ptr++) = hkey->dstIP;
        *(ptr++) = offset_lower_level;
        *(ptr++) = db_page_number;
        *ptr = offset_on_db_page;
    }
    else {
        list = upload_keys(*count, ptr);
        new_key = key(hkey, offset_lower_level, db_page_number, offset_on_db_page);
        list = add_new_key(list, new_key);
        write_keys(ptr, list);
        destroy_list(list);
    }
    (*count)++;
}

idx_page_content * upload_keys(u_int32_t count, u_int32_t *ptr) {
idx_page_content *list, *l_tmp, *l_prv;
u_int32_t i;

    l_tmp = (idx_page_content *)malloc(sizeof(idx_page_content));
    l_tmp->next = NULL;
    l_tmp->prev = NULL;
    list = l_tmp;
    for (i=1; i <= count; i++) {
        l_tmp->ymd   = *(ptr++);
        l_tmp->srcIP = *(ptr++);
        l_tmp->dstIP = *(ptr++);
        l_tmp->offset_lower_level = *(ptr++);
        l_tmp->db_page_number     = *(ptr++);
        l_tmp->offset_on_db_page  = *ptr;
        if (i != count) {
            l_prv = l_tmp;
            l_tmp = (idx_page_content *)malloc(sizeof(idx_page_content));
            l_prv->next = l_tmp;
            l_tmp->prev = l_prv;
            l_tmp->next = NULL;
            ptr++;
        }
    }
    return list;
}

idx_page_content * key(HashKey *hkey, u_int32_t offset_lower_level, u_int32_t db_page_number, u_int32_t offset_on_db_page) {
idx_page_content *new_key;

    new_key = (idx_page_content *)malloc(sizeof(idx_page_content));
    new_key->ymd   = ((hkey->year) << 16) | (((hkey->month) << 8) | hkey->day);
    new_key->srcIP = hkey->srcIP;
    new_key->dstIP = hkey->dstIP;
    new_key->offset_lower_level = offset_lower_level;
    new_key->db_page_number     = db_page_number;
    new_key->offset_on_db_page  = offset_on_db_page;

    return new_key;
}

idx_page_content * add_new_key(idx_page_content *head, idx_page_content *new_key) {
int rez_compare;
idx_page_content *list, *tmp;

    list = head;
    while (list) {
        rez_compare = compare(list->ymd, list->srcIP, list->dstIP, hkey);
        if (rez_compare < 0)  {
            if (list->prev == NULL) {
                list->prev = new_key;
                new_key->next = list;
                new_key->prev = NULL;
                return new_key;
            }
            else {
                tmp = list->prev;
                list->prev = new_key;
                new_key->next = list;
                new_key->prev = tmp;
                tmp->next = new_key;
                return head;
            }
        }
        if (list->next == NULL) {
            list->next = new_key;
            new_key->prev = list;
            new_key->next = NULL;
            return head;
        }
        list = list->next;
    }
}

void write_keys(u_int32_t *ptr, idx_page_content *list) {
    ptr--;
    while (list){
        *(++ptr) = list->ymd;
        *(++ptr) = list->srcIP;
        *(++ptr) = list->dstIP;
        *(++ptr) = list->offset_lower_level;
        *(++ptr) = list->db_page_number;
        *(++ptr) = list->offset_on_db_page;
        list = list->next;
    }
}

void destroy_list(idx_page_content *list) {
idx_page_content *tmp;

    while (list->next != NULL) {
        tmp = list->next;
        free(list);
        list = tmp;
    }
    free(list);
}

void ht_to_db(hashtable *ht, CFG *config) {
HashKey *hkey;

    while (ht) {
        hkey = ht->ptr_to_hashkey;
        while (hkey) {
            add_rec_to_db(config, hkey);
            hkey = hkey->next;
        }
        ht =  ht->next;
    }
}

/*
    Expanding the index tree. The expansion starts from the bottom level of the tree - from the leaves.
    A page of this level will be divided if a new key (k) cannot be added to it, because the number of keys on it = 2n.
    At the current level, a blank page is created, but in fact it is added to the end of the file.
    The content of the page and the new key k (i.e. 2n+1) are sorted, halved,
    the first half is written back onto the page, and the second half onto a new blank page of the same level.
    The key that is in the middle (median) goes up one page level.
    If the "count" of this page does not exceed 2n, then add the median key to the page.
    If the "count" exceeds 2n and does not allow insertion of a median key, then this page will also be divided.
    Only the root is not divisible. If the root reaches 2n keys, then the tree is full, because its height does not exceed 3.
    If it is necessary to split the root, i.e. increase the height of the tree, then you need to drop IDX_LEVEL_LIMIT
    and set a variable.

    The function accepts: Chain - a chain of page addresses (from the bottom level to the root) through which the key has passed.
    The function returns -1 if the tree is full and 0 if a normal expansion has occurred.
*/
int split(void *addr_page, idx_page_content *new_key, CFG *config, Chain *cell_tail) {
u_int32_t *ptr, *count;
u_int32_t level, n, i, new_page_number;
idx_page_content *list, *median;
int flag, idx;
void *addr_new_page;
Page_registry *idx_registry;

    idx_registry = config->idx_page_registry;
    idx = config->idx;
    ptr = count = (u_int32_t *)addr_page;
    level = *(ptr + 1);
    ptr = ptr + IDX_SERVICE_RECORD_LEN - 1;
    list = upload_keys(*count, (ptr+1));
    list = add_new_key(list, new_key);
    n = (*count) / 2;
    for (i=0; i < n; i++) {
        *(++ptr) = list->ymd;
        *(++ptr) = list->srcIP;
        *(++ptr) = list->dstIP;
        *(++ptr) = list->offset_lower_level;
        *(++ptr) = list->db_page_number;
        *(++ptr) = list->offset_on_db_page;
        list = list->next;
    }
    *count = n;
    n =  n * IDX_DATA_RECORD_LEN;
    for (i=0; i < n; i++)
        *(++ptr) = 0;
    new_page_number = add_page(level);
    addr_new_page = map_page_from_hdd_to_registry(idx_registry, new_page_number, idx, cell_tail);
    ptr = count = (u_int32_t *)addr_new_page;
    ptr = ptr + IDX_SERVICE_RECORD_LEN - 1;
    n = 0;
    median = list;
    median->offset_lower_level = new_page_number;
    list = list->next;
    while (list) {
        *(++ptr) = list->ymd;
        *(++ptr) = list->srcIP;
        *(++ptr) = list->dstIP;
        *(++ptr) = list->offset_lower_level;
        *(++ptr) = list->db_page_number;
        *(++ptr) = list->offset_on_db_page;
        n++;
        list = list->next;
    }
    *count = n;
    flag = raise_median(median, cell_tail, config);
    destroy_list(list);
    return flag;
}

int raise_median(idx_page_content *median, Chain *cell_tail, CFG *config) {
Chain *cell;
u_int32_t *ptr, *count;
u_int32_t keys_limit, current_level;
idx_page_content *list;
int flag;

    cell = cell_tail->prev;
    ptr = count = (u_int32_t *)(cell->addr_page);
    current_level = *(ptr + 1);
    keys_limit = 2*N_IDX;

    if (*count < keys_limit) {
        list = upload_keys(*count, ptr);
        list = add_new_key(list, median);
        write_keys(ptr, list);
        destroy_list(list);
        return 0;
    }
    else {
        if (current_level == 0)
            return -1; /* tree is full */
        else {
            flag = split(cell->addr_page, median, config, cell);
            return flag;
        }
    }
}

