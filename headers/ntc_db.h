void open_page_registry(CFG *);
void close_page_registry(CFG *);
void upload_to_database(hashtable *);
long long int locate_record(HashKey *, int, Page_registry *);
int compare_keys(u_int32_t, u_int32_t, u_int32_t, HashKey *);
void * locate_page_in_registry(Page_registry *, u_int32_t);
void * map_page_from_hdd_to_registry(Page_registry *, u_int32_t, int, Chain *);
bool page_in_chain(void *, Chain *);
void update_rec_in_db(CFG *, long long int *, HashKey *);
void add_rec_to_db(CFG *, HashKey *);
void unload_page(Page_registry *, void *);
int add_key_to_idx(HashKey *, u_int32_t, u_int32_t, CFG *);
Chain * new_cell(Chain *, void *);
void destroy_chain(Chain *);
u_int32_t choice_offset(u_int32_t *, HashKey *);
u_int32_t add_page_lower_level(u_int32_t);
