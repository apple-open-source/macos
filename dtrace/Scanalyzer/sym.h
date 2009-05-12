typedef struct symdesc {

	uint64_t	val;
	uint32_t	index;
	int			type;
	uint32_t	size;
	int			sect;
	char		*name;

} symdesc;

extern int image;

extern int syminit(char *fname);
extern int syminitcore(uint32_t *coreimage, uint64_t vmaddr, uint64_t ssize);
extern void symaddr(uint32_t addr, char *symbol);
extern uint64_t symsym(char *symbol);
extern uint64_t symstart(void);
extern uint32_t symfetch(uint64_t addr, uint32_t bytes, uint32_t align, char *buffer);
extern int symindex(uint32_t index, symdesc *symd);
extern void loadsyms(char *fname);
extern void symfree(void);
