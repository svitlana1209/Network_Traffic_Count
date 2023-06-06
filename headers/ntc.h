#define HTSIZE 65521

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


