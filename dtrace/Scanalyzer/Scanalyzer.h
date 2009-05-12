
extern uint8_t *InvokeScanalyzer(char is64Bit, char shouldDisassemble, uint64_t addr, uint64_t size, uint32_t* mem, char* libray, char* function);
extern uint8_t *Scanalyzer(uint64_t entry, uint64_t addr, uint64_t bytes, uint32_t *mem, char *fname, char *function);
extern void scancode(istate *is, regfile *rg, uint8_t *isnflgs);
extern void complete(istate *is, regfile *rg);

extern int forcefetch;
extern int s64bit;

