#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <time.h>
#include <ntc.h>
#include <ntc_db.h>
#include <ntc_net.h>
#include <ntc_tools.h>

CFG cfg;
bool flag;

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
         config->idx_page_count = 1;

    }
    else {
         config->idx_page_count = *(ptr + 3);
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
void *start_page;

    db_ree  = config->db_page_registry;
    start_page = db_ree->page_addr;
    ptr = (u_int32_t*)start_page;
    *(ptr+4) = config->db_records;

    idx_ree  = config->idx_page_registry;
    start_page = idx_ree->page_addr;
    ptr = (u_int32_t*)start_page;
    *(ptr+3) = config->idx_page_count;

    while (db_ree) {
        t = db_ree->next;
        msync(db_ree->page_addr, PAGE_SIZE, MS_ASYNC);
        munmap(db_ree->page_addr, PAGE_SIZE);
        free(db_ree);
        db_ree = t;
    }
    while (idx_ree) {
        t = idx_ree->next;
        msync(idx_ree->page_addr, PAGE_SIZE, MS_ASYNC);
        munmap(idx_ree->page_addr, PAGE_SIZE);
        free(idx_ree);
        idx_ree = t;
    }
}

void upload_to_database(hashtable *ht) {
const char *fname_db="ntc.db", *fname_idx="ntc.idx";
struct stat st;
HashKey *hkey;
long long int poz;
u_int8_t new;
float i;
int count;

    new = 0;
    if ((cfg.db  = open (fname_db,  O_CREAT|O_RDWR|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
        quit("\nCan't write ntc.db\n");
    if ((cfg.idx = open (fname_idx, O_CREAT|O_RDWR|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
        quit("\nCan't write ntc.idx\n");
    if (fstat(cfg.db, &st) == -1)
        quit ("fstat error.");
    if (fstat(cfg.idx, &st) == -1)
        quit ("fstat error.");
    if (st.st_size < PAGE_SIZE) {
        ftruncate (cfg.db, PAGE_SIZE);
        new = 1;
    }
    if (st.st_size < PAGE_SIZE) ftruncate (cfg.idx, PAGE_SIZE);
    open_page_registry(&cfg);

    count = ht_count(ht);
    i = 1;
    if (new == 1)
        ht_to_db(ht, &cfg, count);
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
            printf("\r%s Uploading data to the database: %d%% %s", WHITE_TEXT, (int)((i/count)*100), RESET);
            fflush(stdout);
            i++;
            ht =  ht->next;
        }
    }
    gen_db_report(cfg.idx, cfg.idx_page_registry, cfg.db, cfg.db_page_registry);
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

    address = 0;
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
        change_page = 0;

        rez_compare = compare(ymd_field, srcIP, dstIP, hkey);
        if (rez_compare == 0) {
            address = page_db;
            address = (address << 32) | offset_db;
            return address;
        }
        else {
            if (rez_compare < 0) {
                next_offset = offset_previous;  /* New_key < Key_i, take 'offset' to the left of the Key_i */
                change_page = 1;
            }
            else {
                if (*count == i) {
                    next_offset = offset_i;     /* Last key on the page. Take last 'offset', because New_key > Key_last */
                    change_page = 1;
                }
                else {
                    offset_previous = offset_i; /* The key is not the last, we take the next key from the page */
                    i++;
                    ptr++;
                }
            }
            if (change_page == 1) {
                if (next_offset == 0 || next_offset == 0xFFFFFFFF)
                    return 0;                   /* There is no key in the tree. */
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

    ymd_hkey = ((hkey->year) << 16) | (((hkey->month) << 8) | hkey->day);

    if ((ymd_hkey == ymd_db) && (hkey->srcIP == srcIP_db) && (hkey->dstIP == dstIP_db)) return 0;
    if (ymd_hkey > ymd_db) return  1;
    if (ymd_hkey < ymd_db) return -1;
    if (hkey->srcIP > srcIP_db) return  1;
    if (hkey->srcIP < srcIP_db) return -1;
    if (hkey->dstIP > dstIP_db) return  1;
    if (hkey->dstIP < dstIP_db) return -1;

    return 0;
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
        while (tmp) {
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
    while (cell_current) {
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
    if (addr_page == NULL)
        addr_page = map_page_from_hdd_to_registry(db_registry, page_for_write, db, NULL);
    ptr = (u_int32_t *)addr_page;
    ptr = ptr + offset_on_page + 3;
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

    for (tmp = registry; tmp != NULL; tmp = tmp->next) {
        if (tmp->page_addr == addr_page)
            break;
    }
    if (tmp == NULL)
        quit("Page unload error\n");
    if (tmp != registry) {
        head = tmp->prev;
        tail = tmp->next;
        head->next = tail;
        if (tail != NULL)
            tail->prev = head;
        free(tmp);
        msync(addr_page, PAGE_SIZE, MS_ASYNC);
        munmap(addr_page, PAGE_SIZE);
    }
    else {
        quit("Can't unload the core page\n");
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

    while (cell) {
        tmp = cell->next;
        free(cell);
        cell = tmp;
    }
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

    ymd_hkey = ((hkey->year) << 16) | (((hkey->month) << 8) | hkey->day);
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
        if (list == NULL)
            quit("Error adding ney key\n");
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
    new_key->next = NULL;
    new_key->prev = NULL;

    return new_key;
}

idx_page_content * add_new_key(idx_page_content *head, idx_page_content *new_key) {
int rez_compare;
idx_page_content *list, *tmp;
HashKey *hkey;

    hkey = make_hkey(new_key);
    list = head;
    while (list) {
        rez_compare = compare(list->ymd, list->srcIP, list->dstIP, hkey);
        if (rez_compare < 0)  {
            if (list->prev == NULL) {
                list->prev = new_key;
                new_key->next = list;
                new_key->prev = NULL;
                free(hkey);
                return new_key;
            }
            else {
                tmp = list->prev;
                list->prev = new_key;
                new_key->next = list;
                new_key->prev = tmp;
                tmp->next = new_key;
                free(hkey);
                return head;
            }
        }
        if (list->next == NULL) {
            list->next = new_key;
            new_key->prev = list;
            new_key->next = NULL;
            free(hkey);
            return head;
        }
        list = list->next;
    }
    return NULL;
}

HashKey * make_hkey(idx_page_content *new_key) {
HashKey *hkey;
u_int16_t y1;
u_int8_t d1, m1;

    d1 =  new_key->ymd & 0x000000FF;
    m1 = (new_key->ymd & 0x0000FFFF) >> 8;
    y1 =  new_key->ymd >> 16;

    hkey = (HashKey *)malloc(sizeof(HashKey));
    hkey->year  = y1;
    hkey->month = m1;
    hkey->day   = d1;
    hkey->srcIP = new_key->srcIP;
    hkey->dstIP = new_key->dstIP;
    hkey->vol   = 0;
    hkey->packs = 0;
    hkey->next  = NULL;
    hkey->prev  = NULL;
    return hkey;
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

    while (list) {
        tmp = list->next;
        free(list);
        list = tmp;
    }
}

void ht_to_db(hashtable *ht, CFG *config, int count) {
HashKey *hkey;
float i;

    i = 1;
    while (ht) {
        hkey = ht->ptr_to_hashkey;
        while (hkey) {
            add_rec_to_db(config, hkey);
            hkey = hkey->next;
        }
        printf("\r%s Uploading data to the database: %d%% %s", WHITE_TEXT, (int)((i/count)*100), RESET);
        fflush(stdout);
        i++;
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
idx_page_content *list, *median, *h_list;
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
    if (list == NULL)
        quit("Error adding ney key\n");
    h_list = list;
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
    destroy_list(h_list);
    flag = raise_median(median, cell_tail, config);
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
        flag = 0;
    }
    else {
        if (current_level == 0)
            flag = -1; /* tree is full */
        else
            flag = split(cell->addr_page, median, config, cell);
    }
    return flag;
}

void gen_db_report(int idx, Page_registry *idx_registry, int db, Page_registry *db_registry) {
container *dates, *ip;

    dates = select_dist(idx, idx_registry, 1);
    ip = select_dist(idx, idx_registry, 2);
    print_tree_content(idx, idx_registry, db, db_registry, dates, ip);
    destroy_container(dates);
    destroy_container(ip);
}

container * select_dist(int idx, Page_registry *idx_registry, u_int8_t par) {
u_int32_t *ptr, *count, *offset, i;
void *addr_page;
container *cont;
Chain *cell_head, *cell_tail;

    addr_page = idx_registry->page_addr;      /* core IDX page */
    ptr = count = (u_int32_t *)(addr_page);
    offset = ptr + 4;                         /* offset_0 */
    ptr = ptr + IDX_SERVICE_RECORD_LEN - 1;   /* field YEAR+MONTH+DAY - 1 */

    cont = (container *)malloc(sizeof(container));
    cont->next = NULL;
    cont->prev = NULL;
    cont->mas = (u_int32_t *)malloc(sizeof(u_int32_t)*par);
    if (par == 1)
        cont->mas[0] = *(ptr + 1);    // ymd
    else {
        cont->mas[0] = *(ptr + 2);    // srcIP
        cont->mas[1] = *(ptr + 3);    // dstIP
    }
    for (i=1; i<=(*count)+1; i++) {                                                   // L0
        if (*offset != 0) {
            cell_head = (Chain *)malloc(sizeof(Chain));
            cell_head->addr_page = idx_registry->page_addr;
            cell_head->next = NULL;
            cell_head->prev = NULL;
            cell_tail = cell_head;
            addr_page = locate_page_in_registry(idx_registry, *offset);
            if (addr_page == NULL)
                addr_page = map_page_from_hdd_to_registry(idx_registry, *offset, idx, NULL);
            cell_tail = new_cell(cell_tail, addr_page);
            cont = check_offset(addr_page, cont, idx_registry, idx, cell_tail, par);  // L1 --> L2 --> ... next (if exist)
            destroy_chain(cell_head);
        }
        if (i > (*count))                                                             // L0
            break;
        ptr++;         /* field YEAR+MONTH+DAY */
        if (check_new_elem(cont, ptr, par) == false)
            cont = add_new_elem(cont, ptr, par);
        offset = ptr + 3;
        ptr = ptr + 5;
    }
    return cont;
}

bool check_new_elem(container *cont, u_int32_t *ptr, u_int8_t par)  {
    while (cont) {
        if (compare_cont_elem(ptr, cont->mas, par) == 0)
            return true;
        else
            cont = cont->next;
    }
    return false;
}

int compare_cont_elem(u_int32_t *ptr, u_int32_t *mas, u_int8_t par) {

    if (par == 1) {
        if (*ptr == mas[0]) return  0;
        if (*ptr >  mas[0]) return  1;
        if (*ptr <  mas[0]) return -1;
    }
    else {
        if ((*(ptr+1) == mas[0]) && (*(ptr+2) == mas[1])) return 0;
        if (*(ptr+1) > mas[0]) return  1;
        if (*(ptr+1) < mas[0]) return -1;
        if (*(ptr+2) > mas[1]) return  1;
        if (*(ptr+2) < mas[1]) return -1;
    }
    return 0;
}

container * add_new_elem(container *cont, u_int32_t *ptr, u_int8_t par) {
container *new_elem, *tmp, *list;

    new_elem = (container *)malloc(sizeof(container));
    new_elem->mas = (u_int32_t *)malloc(sizeof(u_int32_t)*par);
    if (par == 1)
        new_elem->mas[0] = *ptr;          // ymd
    else {
        new_elem->mas[0] = *(ptr + 1);    // srcIP
        new_elem->mas[1] = *(ptr + 2);    // dstIP
    }
    list = cont;
    while (list) {
        if (compare_cont_elem(ptr, list->mas, par) < 0) {
            if (list->prev == NULL) {
                new_elem->prev = NULL;
                new_elem->next = list;
                list->prev = new_elem;
                return new_elem;
            }
            else {
                tmp = list->prev;
                list->prev = new_elem;
                new_elem->next = list;
                new_elem->prev = tmp;
                tmp->next = new_elem;
                return cont;
            }
        }
        if (list->next == NULL) {
            list->next = new_elem;
            new_elem->prev = list;
            new_elem->next = NULL;
            return cont;
        }
        list = list->next;
    }
    return NULL;
}

container * check_offset(void *addr_page, container *cont, Page_registry *idx_registry, int idx, Chain *cell_tail, u_int8_t par) {
u_int32_t *ptr, *count, *offset, i;
void *addr_p;

    ptr = count = (u_int32_t *)addr_page;
    offset = ptr + 4;                         /* offset_0 */
    ptr = ptr + IDX_SERVICE_RECORD_LEN - 1;   /* field YEAR+MONTH+DAY - 1 */
    flag = false;
    for (i=1; i<=(*count)+1; i++) {                                                            // L1
        if ((*offset !=0) && (*offset != 0xFFFFFFFF)) {                                        // go to the next Level (if exist)
            addr_p = locate_page_in_registry(idx_registry, *offset);
            if (addr_p == NULL)
                addr_p = map_page_from_hdd_to_registry(idx_registry, *offset, idx, cell_tail);
            cell_tail = new_cell(cell_tail, addr_p);
            cont = check_offset(addr_p, cont, idx_registry, idx, cell_tail, par);              // L2 --> ... next lower level (if exist)
            if (flag) {
                cell_tail = cat_tail(cell_tail);
                flag = false;
            }
        }
        if (i > (*count))                                                                       // L1
            break;
        ptr++;         /* field YEAR+MONTH+DAY */
        if (check_new_elem(cont, ptr, par) == false)
            cont = add_new_elem(cont, ptr, par);
        offset = ptr + 3;
        ptr = ptr + 5;
    }
    if (*offset == 0xFFFFFFFF)
        flag = true;
    return cont;
}

void destroy_container(container *cont) {
container *tmp;

    while (cont) {
        tmp = cont->next;
        free(cont->mas);
        free(cont);
        cont = tmp;
    }
}

Chain * cat_tail(Chain *cell_tail) {
Chain *tmp;

    tmp = cell_tail;
    cell_tail = cell_tail->prev;
    cell_tail->next = NULL;
    free(tmp);
    return cell_tail;
}

void print_tree_content(int idx, Page_registry *idx_registry, int db, Page_registry *db_registry, container *dates, container *ip) {
u_int32_t *ptr;
u_int32_t page_db, offset_on_page, packs;
void *addr_page;
struct tm *time_ptr;
time_t the_time;
FILE *report;
u_int8_t ip_s[16], ip_d[16];
char report_name[33], dt[11];
container *tmp;
long long int address, vol;
HashKey *hkey;

    (void) time (&the_time);
    time_ptr = localtime (&the_time);
    strftime (report_name, 33, "ntc-%Y-%m-%d__%H-%M-%S__db.log", time_ptr);
    if ((report =  fopen (report_name,  "w")) == NULL)
        quit ("\nCan't create the db report\n");

    fprintf(report, "                                    +---------------------+\n");
    fprintf(report, "                                    |   VOLUME / PACKETS  |\n");
    fprintf(report, "------------------------------------+");
    tmp = dates;
    while (tmp) {
        fprintf(report, "---------------------+");
        tmp = tmp->next;
    }
    fprintf(report, "\n");
    fprintf(report, "        IPsrc             IPdst     |");
    tmp = dates;
    while (tmp) {
        int_date_to_str((tmp->mas)[0], dt);
        fprintf(report, "  %14s     |", dt);
        tmp = tmp->next;
    }
    fprintf(report, "\n");
    fprintf(report, "------------------------------------+");
    tmp = dates;
    while (tmp) {
        fprintf(report, "---------------------+");
        tmp = tmp->next;
    }
    fprintf(report, "\n");
    hkey = (HashKey *)malloc(sizeof(HashKey));
    tmp = dates;
    while (ip) {
        intaddr_to_string((ip->mas)[0], ip_s);
        intaddr_to_string((ip->mas)[1], ip_d);
        fprintf(report, "%15s   %15s   |", ip_s, ip_d);
        while (dates) {
            hkey->day   = (dates->mas)[0] & 0x000000FF;
            hkey->month =((dates->mas)[0] & 0x0000FF00) >> 8;
            hkey->year  = (dates->mas)[0] >> 16;
            hkey->srcIP = (ip->mas)[0];
            hkey->dstIP = (ip->mas)[1];
            address = locate_record(hkey, idx, idx_registry);
            if (address > 0) {
                page_db = address >> 32;
                offset_on_page = address;
                addr_page = locate_page_in_registry(db_registry, page_db);
                if (addr_page == NULL)
                    addr_page = map_page_from_hdd_to_registry(db_registry, page_db, db, NULL);
                ptr = (u_int32_t *)addr_page;
                ptr = ptr + offset_on_page + 3;
                vol = *((long long int *)ptr);
                packs = *(ptr + 2);
                fprintf(report, " %10lld %7d  |", vol, packs);
            }
            else
                fprintf(report, "                     |");
            dates = dates->next;
        }
        fprintf(report, "\n");
        dates = tmp;
        ip = ip->next;
    }
    free(hkey);
    fprintf(report, "\n----- END OF REPORT -----\n");
    fclose(report);
    printf("\n The DB report file: %s%s%s\n", CYAN_TEXT, report_name, RESET);
}
