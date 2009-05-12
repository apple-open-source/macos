typedef struct corebk {
	uint64_t		addr;
	struct corebk	*core;
	struct regfile	*rg;
	uint8_t			trakBytes[128];
	uint8_t			bytes[128];
} corebk;

extern corebk *corehd;
extern corebk *quikcore;
extern int ctrace;
extern uint32_t coremalloc, corefree;

extern corebk *getcore(void);
extern void writecore(regfile *rg, uint64_t addr, uint32_t size, uint64_t addrmask, uint32_t flags, uint8_t *src, uint8_t trak);
extern void readcore(regfile *rg, uint64_t addr, uint32_t size, uint64_t addrmask, uint32_t flags, uint64_t *fetch, uint8_t *trak);
extern corebk *findcore(regfile *rg, uint64_t addr);
corebk *promotecore(regfile *rg, uint64_t addr);
void tosscore(regfile *rg);
void freecore(void);
