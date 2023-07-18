void open_page_registry(CFG *);
void close_page_registry(CFG *);
void upload_to_database(hashtable *);
long long int locate_record(HashKey *, int, Page_registry *);
int compare_keys(u_int32_t, u_int32_t, u_int32_t, HashKey *);

