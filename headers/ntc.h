#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)*2 /* Page size for DB/IDX file */
#define IDX_LEVEL_LIMIT        3  /* Number of levels in the index tree */
#define NUMBER_PAGES_OPEN     32  /* Number of open pages in the Registy */
#define DB_DATA_RECORD_LEN     6  /* Number of 4-byte fields in the DB data record.  Fields x 4 byte (field size) = 4 x 4 = 16 bytes + 8 bytes vol (long long int) = 24 bytes in DB data record (24/4 = 6 fields). */
#define IDX_DATA_RECORD_LEN    6  /* Number of 4-byte fields in the IDX data record. Fields x 4 byte (field size) = 6 x 4 = 24 bytes in IDX data record */
#define DB_SERVICE_RECORD_LEN  5  /* Number of fields in the DB service record.  Fields x 4 byte (field size) = 5 x 4 = 20 bytes in DB service record */
#define IDX_SERVICE_RECORD_LEN 8  /* Number of fields in the IDX service record. Fields x 4 byte (field size) = 8 x 4 = 32 bytes in IDX service record */
#define N_DB (PAGE_SIZE/(DB_DATA_RECORD_LEN*4))-1   /* Max number of records for DB page (minus one for service record): (8192/24)-1 = 340 max records per DB page; (340x24)+20=8180 */
#define N_IDX (PAGE_SIZE/(IDX_DATA_RECORD_LEN*4))/2 /* Max/2 number of keys on IDX page = 170 keys. ((170x2)x 24 bytes)+32 bytes of service record = 8192 bytes on IDX page */

typedef struct pkt {
    u_int8_t  buff[1516];
    u_int32_t size;
} packet;

typedef struct q {
    u_int16_t year;
    u_int8_t  month;
    u_int8_t  day;
    u_int32_t srcIP;
    u_int32_t dstIP;
    u_int32_t vol;
    struct q  *next, *prev;
} Queue;

typedef struct hash_key {
    u_int16_t       year;
    u_int8_t        month;
    u_int8_t        day;
    u_int32_t       srcIP;
    u_int32_t       dstIP;
    long long int   vol;
    u_int32_t       packs;
    struct hash_key *next, *prev;
} HashKey;

typedef struct ht {
    u_int32_t hash;
    HashKey   *ptr_to_hashkey;
    struct ht *next, *prev;
} hashtable;

typedef struct fk {
    u_int16_t year;
    u_int8_t  month;
    u_int8_t  day;
    u_int32_t srcIP;
    u_int32_t dstIP;
} forkey;

typedef struct total_trafic {
    long long int in;
    long long int out;
} total;

typedef struct dyn {
    u_int32_t src_x;
    u_int32_t dst_y;
    u_int32_t vol1;
    u_int32_t packs1;
    u_int32_t src_y;
    u_int32_t dst_x;
    u_int32_t vol2;
    u_int32_t packs2;
} dyn_struct;

typedef struct reestr {
    void          *page_addr;
    u_int32_t     page_number;
    struct reestr *next, *prev;
} Page_registry;

typedef struct Chn {
    void        *addr_page;
    struct Chn  *next, *prev;
} Chain;

typedef struct cfg {
    int           db, idx;
    u_int32_t     db_records;
    u_int32_t     idx_page_count;
    Page_registry *db_page_registry, *idx_page_registry;
} CFG;

typedef struct idx_page_cont {
    u_int32_t ymd;
    u_int32_t srcIP;
    u_int32_t dstIP;
    u_int32_t offset_lower_level;
    u_int32_t db_page_number;
    u_int32_t offset_on_db_page;
    struct idx_page_cont *next, *prev;
} idx_page_content;

void call_init();
void call_exit();
void destroy_queue(Queue *);
void upload_to_database(hashtable *);
void * waiting_for_key (void *);
void * process_queue(void *);
void * display_info();
hashtable * get_ht_addr();
