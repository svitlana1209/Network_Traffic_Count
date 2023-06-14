u_int32_t get_hash(u_int32_t, u_int32_t);
hashtable * locate_hash(hashtable *, u_int32_t);
HashKey * locate_hkey(hashtable *, Queue *);
void update_hashkey(HashKey *, Queue *);
void append_new_hashkey(hashtable *, Queue *);
hashtable * append_to_hashtable(hashtable *, u_int32_t, Queue *);
void destroy_hashtable(hashtable *);
void update_ht(hashtable *, Queue *);
hashtable * locate_in_ht(hashtable *, Queue *);


