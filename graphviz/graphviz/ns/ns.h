void ns_setminlength(Agedge_t *e, int len);
void ns_setweight(Agedge_t *e, int weight);
void ns_solve(Agraph_t *g, unsigned int flags);
int  ns_getrank(Agnode_t *n);
void ns_setrank(Agnode_t *n, int rank);
void ns_clean(Agraph_t *g);

#define  NS_VERBOSE		(1 << 0)
#define  NS_VALIDATE	(1 << 1)
#define  NS_ATTACHATTRS (1 << 2)
#define  NS_DEBUG		(1 << 3)
#define  NS_NORMALIZE	(1 << 4)
