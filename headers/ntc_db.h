void open_page_registry(CFG *);
void close_page_registry(CFG *);
void upload_to_database(hashtable *);
long long int locate_record(HashKey *, int, Page_registry *);
int compare(u_int32_t, u_int32_t, u_int32_t, HashKey *);
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
u_int32_t add_page(u_int32_t);
void add_key_to_current_idx_page(u_int32_t *, HashKey *, u_int32_t, u_int32_t);
idx_page_content * upload_keys(u_int32_t, u_int32_t *);
idx_page_content * key(HashKey *, u_int32_t, u_int32_t, u_int32_t);
idx_page_content * add_new_key(idx_page_content *, idx_page_content *);
HashKey * make_hkey(idx_page_content *);
void write_keys(u_int32_t *, idx_page_content *);
void destroy_list(idx_page_content *);
void ht_to_db(hashtable *, CFG *, int);
int split(void *, idx_page_content *, CFG *, Chain *);
int raise_median(idx_page_content *, Chain *, CFG *);
void gen_db_report(int, Page_registry *, int, Page_registry *);
container * select_dist(int, Page_registry *, u_int8_t);
bool check_new_elem(container *, u_int32_t *, u_int8_t);
int compare_cont_elem(u_int32_t *, u_int32_t *, u_int8_t);
container * add_new_elem(container *, u_int32_t *, u_int8_t);
container * check_offset(void *, container *, Page_registry *, int, Chain *, u_int8_t);
void destroy_container(container *);
Chain * cat_tail(Chain *);
void print_tree_content(int, Page_registry *, int, Page_registry *, container *, container *);
