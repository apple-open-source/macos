typedef struct disb {
	uint8_t out[512];
} disb;

#define xiNR	0
#define xiBR	1
#define xiSC	2
#define xiTR	3
#define xiBT	4
#define xiSR	5
#define xiIN	6

#define xiBRx	7
#define xiSCx	8
#define xiTRx	9
#define xiSRx	10

#define xiSRp	11
#define xiSRpx	12
#define xiBRc	13
#define xiBRcx	14
#define xiRIpx	15

#define xiFunVec 16
#define xiRsv	17

extern uint8_t trackReg(istate *is, uint8_t cl, uint8_t ex, uint8_t tt, uint8_t ta, uint8_t tb, uint8_t tc, uint8_t td);
extern uint8_t trackRegNS(istate *is, uint8_t cl, uint8_t ex, uint8_t tt, uint8_t ta, uint8_t tb, uint8_t tc, uint8_t td);
extern istate *getistate(istate *ois);
extern regfile *getregfile(regfile *org);
extern void tossistate(istate *is);
extern void tossregfile(regfile *rg);
extern void pprint(char *data, uint64_t addr, int len, int indent);
extern void disassemble(uint64_t addr, uint32_t *mem, uint8_t *isnflgs, int insts, char *fname, char *function);
extern void gendtrace(uint64_t addr, uint64_t funcstart, uint32_t *mem, uint8_t *isnflgs, int insts, char *fname, char *function);
extern void diedie(char *xx);

extern int dis;
extern int stats;
extern int trace;
extern disb *disbuf;
extern uint32_t level;
extern char xtran[];
extern uint32_t regfree;
extern uint32_t regmalloc;
extern uint32_t istatefree;
extern uint32_t istatemalloc;
