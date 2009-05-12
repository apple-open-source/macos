typedef union vrs {
	uint32_t	vw[4];				/* Word length vector registers */
	uint16_t	vh[8];				/* Halfword length vector registers */
	uint8_t		vb[16];				/* Byte length vector registers */
} vrs;

typedef struct regfile {
	uint64_t	*rgx[4];			/* Index to register files */
	uint8_t		*trx[4];			/* Index to tracking tables */
	vrs			vprs[32];			/* Vector registers */	
	uint64_t	gprs[32];			/* General registers */
	uint64_t	fprs[32];			/* Floating point registers */
	uint64_t	sprs[32];			/* Special registers */
	uint64_t	pc;					/* PC */
	uint64_t	addrmask;			/* Address size mask */
	uint64_t	ifaddrmask;			/* Address size mask for instruction fetches */
	
	uint32_t	level;
	struct regfile *rgBack;
	struct corebk  *core;
	
	uint8_t		trakVpr[32];		/* VPR tracking state */
	uint8_t		trakGpr[32];		/* GPR tracking state */
	uint8_t		trakFpr[32];		/* FPR tracking state */
	uint8_t		trakSpr[32];		/* SPR tracking state */
} regfile;

#define rfNone	0
#define rfGpr	1
#define rfFpr	2
#define rfVpr	3
#define rfSpr	4

#define sPlr	0
#define sPctr	1
#define sPcr	2
#define sPxer	3
#define sPmsr	4
#define sPfpscr	5
#define sPvrsave 6
#define sPvscr	7
#define sPrsrvd 8
#define sPtb	9
#define sPunimp 255

#define gTstack 0x80				/* Content stack based */
#define gTlr	0x40				/* Content link register */
#define gTgen	0x20				/* Content generated, i.e. not from memory or LR/CTR */
#define gTrsvd1	0x10				/* reserved */
#define gTbrtbl	0x08				/* Contains a pointer to a branch table */
#define gTinfnc	0x04				/* Content could be an address within the function */
#define gTundef 0x02				/* Content unknown */
#define gTset	0x01				/* Content has been set */
#define gTnu	0x00				/* Content never used or null */

#define strakany (gTstack | gTlr | gTbrtbl | gTundef)
#define strakall (gTgen | gTinfnc)

typedef struct istate {
	uint64_t	pc;					/* Program counter */
	uint64_t	btarg;				/* Branch or memory access target */
	int64_t		immediate;			/* Immediate/offset field */
	uint64_t	result;				/* Uncommitted result */ 
	uint64_t	resultx;			/* Uncommitted result - 2nd part if VMX */ 
	uint64_t	fstart;				/* Start of function under analysis */
	uint64_t	exit;				/* "Exit to" address */
	uint64_t	freturn;			/* "Return to" address */
	uint32_t	fsize;				/* Size of function */
	uint32_t	instimg;			/* Instruction image */
	uint32_t	opr;				/* Operation */
	uint32_t	memsize;			/* Length of memory access */
	uint32_t	mods;				/* Operation modifiers */
	uint32_t	status;				/* Current status */
#define statOF64 0x80000000
#define statOF32 0x40000000
#define statCA64 0x00000004
#define statCA32 0x00000002
#define stat64b  0x00000001
	uint32_t	clevel;				/* Internal function call level */
	uint32_t	target;				/* Target (also source for store-type operations) */
	uint32_t	targtype;			/* Target register file */
	uint32_t	sourcea;			/* Source A */
	uint32_t	sourceb;			/* Source B */
	uint32_t	sourcec;			/* Source C */
	uint32_t	sourced;			/* Source D */


	struct istate *isBack;

//	Register/result tracking
	uint8_t		trakBtarg;			/* Branch target tracking state */
	uint8_t		trakResult;			/* Result tracking state */
	uint8_t		trakTarget;			/* Track original target */
	uint8_t		trakClear;			/* Flags to clear before applying explicit */
	uint8_t		trakExplicit;		/* Explicit tracking info */
	uint8_t		trakSourcea;		/* Track original sourcea */
	uint8_t		trakSourceb;		/* Track original sourceb */
	uint8_t		trakSourcec;		/* Track original sourcec */
	uint8_t		trakSourced;		/* Track original sourcec */
	
	char 		op[64];				/* Operation code */
	char 		oper[64];			/* Operands */
} istate;

//
//		istate->mod flags
//

//		Register field validity flags
#define modRgVal	0xF0000000		/* Register validity field */
#define modTgVal  	0x80000000		/* Target valid */
#define modSaVal  	0x40000000		/* Source A valid */
#define modSbVal  	0x20000000		/* Source B valid */
#define modScVal  	0x10000000		/* Source C valid */

//		Various instruction form A register validities
#define modFAtabc	0xF0000000		/* Form A - FRT, FRA, FTB, FRC */
#define modFAtab	0xE0000000		/* Form A - FRT, FRA, FTB */
#define modFAtb		0xA0000000		/* Form A - FRT, FRB */
#define modFAtac	0xB0000000		/* Form A - FRT, FRA, FRC */

//		Trap modifiers
#define	modFDtoa	0xC0000000		/* Register field validity for trap immediate */

//		Common modifiers
#define modRsvn		0x00800000		/* Reservation-type instruction */
#define modTFpu		0x00400000		/* Target register is an FPU */
#define modTVec		0x00200000		/* Target register is a VPR */
#define modOflow	0x00100000		/* Check for overflow */
#define modTnomod	0x00080000		/* Target is not modified */
#define modPrv		0x00040000		/* Privileged operation/register */
#define modSetCRF	0x00020000		/* Set condition register field */
#define modUnimpl	0x00010000		/* Instruction unimplemented */
#define modSpec		0x0000FFFF		/* Modifiers for specific instructions */

//		Storage access modifiers
#define modUpd		0x00008000		/* Update form of memory access */
#define modSxtnd	0x00004000		/* Sign extend after fetch */
#define modM4		0x00002000		/* Move multiple to 4 byte registers */
#define modM8		0x00001000		/* Move multiple to 8 byte registers */

//		Branch modifiers
#define modFBBOBI	0xC0000000		/* Form B field validity */
#define modSetLR	0x00008000		/* Set LR from PC */
#define modBrAbs	0x00004000		/* Branch is absolute */
#define modCond		0x00002000		/* Branch is conditional */
#define modLR		0x00001000		/* Branch to link register */
#define modCTR		0x00000800		/* Branch to count register */

//		Add/Subtract modifiers
#define	modShifted	0x00008000		/* Immediate operand shifted left 16 */
#define modImmo		0x00004000		/* Immediate field only used (i.e., source register 0 assumed to contain 0 */

//		Move SPR modifiers
#define	modLRs		0x00008000		/* Using link register */
#define	modCTRs		0x00004000		/* Using count register */

//		Shift modifiers
#define	modSHtabc	0xF0000000		/* All targets/sources valid, immediate implied */
#define modROTL32	0x00008000		/* Duplicate bottom 32 bits in top 32 bits */
#define modUseRB	0x00004000		/* Use RB as sourceb */
#define modInsert	0x00002000		/* Insert type instruction */
#define modZeroc	0x00001000		/* Set sourcec to 0 */
#define modDplus32	0x00000800		/* Add 32 to sourced */
#define mod63minusd	0x00000400		/* Subtract sourced from 63 */
#define mod63d		0x00000200		/* Set sourced to 63 */
#define modShRight	0x00000100		/* Shift right */
#define modShift	0x00000080		/* Shift */

//		VMX modifiers
#define modVtabc	0xF0000000		/* Target, sourcea, sourceb, sourcec */
#define modVtab		0xE0000000		/* Target, sourcea, sourceb */
#define modVta		0xC0000000		/* Target and sourcea fields are valid */
#define modVtb		0xA0000000		/* Target, sourceb */
#define modVt		0x80000000		/* Target */
#define modVab		0x60000000		/* sourcea, sourceb */
#define modVa		0x40000000		/* sourcea */
#define modVb		0x20000000		/* sourceb */
#define mod4op		0x00000000		/* Three operands */
#define mod3op		0x00002000		/* Three operands */
#define mod2op		0x00004000		/* Two operands */
#define mod1opt		0x00006000		/* One operand, target */
#define mod1opb		0x00008000		/* One operand, sourceb */
#define moduim		0x0000A000		/* Has UIM */
#define modsim		0x0000C000		/* Has SIM */

//		Logical operation modifiers
#define	modlop		0x0000F000		/* Specific operation */
#define modand		0x00001000		/* AND */
#define modor		0x00002000		/* OR */
#define modxor		0x00003000		/* XOR */
#define modandc		0x00004000		/* ANDC */
#define modeqv		0x00005000		/* EQV */
#define modnand		0x00006000		/* NAND */
#define modnor		0x00007000		/* NOR */
#define modorc		0x00008000		/* ORC */
#define modshft		0x00000001		/* Immediate shifted */

struct ppcinst;
typedef int  (*instx)(uint32_t, struct ppcinst *, uint64_t, istate *, regfile *);
typedef int  (*iformx)(uint32_t, struct ppcinst *, istate *, regfile *);

typedef struct iType {
	int32_t		ishift;			/* Shift to right justify extended op code */
	int32_t		imask;			/* Mask to isolate extended op code after shift */
	iformx		idecode;		/* Decode routine */
	uint32_t	iencode;		/* Encode Routine */
} iType;

typedef struct ppcinst {
	int32_t		major;			/* Major op code */
	int32_t		extended;		/* Extended op code */
	iType		*insttype;		/* Pointer to instruction type table */
	instx		xinst;			/* Routine to handle op code */
	char		*opcode;		/* Op code */
	uint32_t	mods;			/* Default instruction modifier flags */
	uint32_t	opr;			/* Internal operation code */
	uint32_t	oprsize;		/* Default operation size */
	uint32_t	dflags;			/* Decode flags */
#define dSkip	0x80000000		/* Skip any decode procssing */
#define dNoFmt	0x40000000		/* Skip building formatted strings */
#define dSwapTA	0x20000000		/* Swap RT and RA */
#define	dImmUn	0x10000000		/* Immediate value is unsigned */
#define	dBd		0x08000000		/* Base/displacement form */
#define dTrgnum	0x0000FF00		/* Internal register number of target for special registers */
#define dTrgtyp	0x000000FF		/* Register file ID of target */
} ppcinst;


typedef struct sprtb {
	uint32_t sprnum;			/* SPR number */
	char	*sprname;			/* SPR name */
	uint32_t sprflags;			/* SPR flags */
	uint32_t sprint;			/* SPR internal number */
#define sprNorm 1
#define sprPriv 2
#define sprLR 4
#define sprCTR 8

} sprtb;

//
//	Instruction classifiers
//

#define isnType		0x0F
#define isnFlags	0xF0
#define isnBTarg	0x80
#define isnCall		0x40
#define isnPriv		0x20
#define isnExit		0x10

enum {
	isNone		= 0,
	isBranch	= 1,
	isBrCond	= 2,
	isRead		= 3,
	isWrite		= 4,
	isSysCall	= 5,
	isTrap		= 6,
	isRFI		= 7,
	isFpu		= 8,
	isVec		= 9,
	isScalar	= 10,
	isBranchTbl	= 11,
	isRsvn		= 12,
	isInvalid	= 15
};


extern void decode(uint32_t inst, uint64_t pc, istate *is, regfile *rg);
extern sprtb *decodeSPR(uint32_t spr);

extern int dcdFormA(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormB(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormD(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormDS(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormI(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormM(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormMD(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormMDS(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormSC(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormVA(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormVX(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormVXR(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormX(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormXFL(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormXFS(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormXFX(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormXL(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormXO(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);
extern int dcdFormXS(uint32_t inst, struct ppcinst *instb, istate *is, struct regfile *rg);

extern int xadd(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xaddc(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xadde(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xaddi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xaddic(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xaddicdot(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xaddis(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xaddme(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xaddze(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xand(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xandc(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xattn(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xbc(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xbclr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xbctr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xcmp(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xcmpi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xcmpl(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xcmpli(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xcntlzd(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xcntlzw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdcbf(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdcbst(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdcbt(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdcbtst(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdcbz(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdivd(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdivdu(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdivw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdivwu(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdss(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdstx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xdstst(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xeciwx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xecowx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xeieio(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xeqv(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xextsb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xextsh(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xextsw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfabs(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfadd(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfadds(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfcfid(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfcmpo(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfcmpu(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfctid(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfctidz(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfctiw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfctiwz(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfmr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfnabs(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfneg(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfrsp(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xicbi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xisync(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlbzux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlbzx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xldarx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xldux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xldx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlfdux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlfdx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlfsux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlfsx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlhaux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlhax(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlhbrx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlhzux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlhzx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlogi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlswi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlswx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlvebx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlvehx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlvewx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlvx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlvxl(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlwarx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlwaux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlwax(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlwbrx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlwzux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlwzx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmcrf(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmcrfs(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmcrxr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmfcr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmffs(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmfmsr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmfocrf(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmfspr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmfsr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmfsrin(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmftb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmfvscr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtcrf(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtfsb0(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtfsb1(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtfsfi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtmsr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtmsrd(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtocrf(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtspr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtsr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtsrin(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtvscr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmulhd(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmulhdu(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmulhw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmulhwu(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmulld(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmulli(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmullw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xnand(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xneg(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xnor(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xor(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xorc(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xrfx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xrldimi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsc(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xslbia(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xslbie(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xslbmfee(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xslbmfev(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xslbmte(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsld(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xslw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsrad(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsradi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsraw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsrawi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsrd(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsrw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstbux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstbx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstd(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstdcxdot(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstdu(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstdux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstdx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstfdux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstfdx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstfiwx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstfsux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstfsx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsthbrx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsthux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsthx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstswi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstswx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstvebx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstvehx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstvewx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstvx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstvxl(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstwbrx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstwcxdot(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstwux(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xstwx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsubf(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsubfc(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsubfe(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsubfic(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsubfme(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsubfze(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xsync(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xtd(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xtdi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xtlbia(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xtlbie(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xtlbiel(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xtlbsync(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xtw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xtw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xxor(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);

extern int xrt(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xoponly(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xtrap(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xrt(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xlogx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xrot(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xrarb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xmtmsrx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xrtrb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xrtra(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xtlbiex(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xrb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xfcmpx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xftfb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xtwi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);
extern int xext(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg);

extern void genbr(char *dst, int bo, int bi, uint32_t inst, int disp, char *op, char *oper);
