
typedef struct {
    ni_id		dir;
    NIDomain_t *	domain;
    PLCache_t		list;
} AFPUsers_t;

void		AFPUsers_free(AFPUsers_t * users);
boolean_t	AFPUsers_set_password(AFPUsers_t * users, 
				      PLCacheEntry_t * entry,
				      u_char * passwd);
boolean_t	AFPUsers_init(AFPUsers_t * users, NIDomain_t * domain);
boolean_t	AFPUsers_create(AFPUsers_t * users, gid_t gid,
				uid_t start, int count);
void		AFPUsers_print(AFPUsers_t * users);




