typedef struct pkt {
    u_int8_t	buff[1516];
    u_int32_t	size;
} packet;

typedef struct q {
    u_int16_t	year;
    u_int8_t	month;
    u_int8_t	day;
    u_int32_t	srcIP;
    u_int32_t	dstIP;
    u_int32_t	vol;
    struct q	*next, *prev;
} Queue;

typedef struct hash_key {
    u_int16_t		year;
    u_int8_t		month;
    u_int8_t		day;
    u_int32_t		srcIP;
    u_int32_t		dstIP;
    long long int	vol;
    u_int32_t		packs;
    struct hash_key	*next, *prev;
} HashKey;

typedef struct ht {
    u_int32_t		hash;
    HashKey		*ptr_to_hashkey;
    struct ht		*next, *prev;
} hashtable;

typedef struct fk {
    u_int16_t	year;
    u_int8_t	month;
    u_int8_t	day;
    u_int32_t	srcIP;
    u_int32_t	dstIP;
} forkey;

typedef struct total_trafic {
    long long int	in;
    long long int	out;
} total;

typedef struct dyn {
    u_int32_t	src_x;
    u_int32_t	dst_y;
    u_int32_t	vol1;
    u_int32_t	packs1;
    u_int32_t	src_y;
    u_int32_t	dst_x;
    u_int32_t	vol2;
    u_int32_t	packs2;
} dyn_struct;

void call_init();
void call_exit();
void generate_report(hashtable *);
void destroy_queue(Queue *);
void destroy_ht(hashtable *);
void upload_to_database(hashtable *);
void * waiting_for_key (void *);
void * process_queue(void *);
void * display_info(void *);
