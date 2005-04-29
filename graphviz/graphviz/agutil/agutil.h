/* attribute handling */

double	ag_scan_float(void *obj, char *name, double low, double high, double defval);
int	ag_scan_int(void *obj, char *name, int low, int high, int defval);


/* a node queue */
typedef struct Nqueue_s {
	Agnode_t	**store,**limit,**head,**tail;
} Nqueue;

Nqueue		*Nqueue_new(Agraph_t *g);
void		Nqueue_free(Agraph_t *g, Nqueue *q);
void		Nqueue_insert(Nqueue *q, Agnode_t *n);
Agnode_t	 *Nqueue_remove(Nqueue *q);
