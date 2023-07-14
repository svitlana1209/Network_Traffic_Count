void open_page_registry(CFG *);
void close_page_registry(CFG *);
void upload_to_database(hashtable *);
void ht_to_db(hashtable *, CFG *);
long long int locate_record(HashKey *, int, Page_registry *);
void update_rec_in_db(CFG *, long long int, HashKey *);
void add_rec_to_db(CFG *, HashKey *);

