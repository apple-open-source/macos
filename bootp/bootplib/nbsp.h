
typedef struct {
    char *	name;
    char *	path;
} nbspEntry_t;

typedef void * 	nbspList_t;

int		nbspList_count(nbspList_t list);
nbspEntry_t *	nbspList_element(nbspList_t list, int i);
void		nbspList_print(nbspList_t list);
void		nbspList_free(nbspList_t * list);
nbspList_t	nbspList_init();



