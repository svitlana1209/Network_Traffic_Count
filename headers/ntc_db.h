void open_page_registry(CFG *);
void close_page_registry(CFG *);
void upload_to_database(hashtable *);
long long int locate_record(HashKey *, int, Page_registry *);
int compare_keys(u_int32_t, u_int32_t, u_int32_t, HashKey *);
void * locate_page_in_registry(Page_registry *, u_int32_t);
void * map_page_from_hdd_to_registry(Page_registry **, u_int32_t, int, Chain *);
bool page_in_chain(void *, Chain *);

