#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "sym.h"
#include "decode.h"
#include "core.h"
#include "Scanalyzer.h"
#include "misc.h"

char xtran[256] = {
/*  x0   x1   x2   x3   x4   x5   x6   x7   x8   x9   xA   xB   xC   xD   xE   xF   	   */
	'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',  /* 0x */
	'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',  /* 1x */
	' ', '!', '"', '#', '$', '%', '&',0x27, '(', ')', '*', '+', ',', '-', '.', '/',  /* 2x */
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',  /* 3x */
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',  /* 4x */
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[',0x5C, ']', '^', '_',  /* 5x */
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',  /* 6x */
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', '.',  /* 7x */
	'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',  /* 8x */
	'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',  /* 9x */
	'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',  /* Ax */
	'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',  /* Bx */
	'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',  /* Cx */
	'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',  /* Dx */
	'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',  /* Ex */
	'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',  /* Fx */
};

int dis = 0;
int stats = 0;
int trace = 0;
disb *disbuf = 0;
uint32_t level = 0;
uint32_t regfree = 0;
uint32_t regmalloc = 0;
uint32_t istatefree = 0;
uint32_t istatemalloc = 0;

void tossregfile(regfile *rg) {

	int i;

	if(rg->core) tosscore(rg);						/* Any core? Then release it */
//	if(trace) printf("-----> tossregfile %08X\n", rg);	
	for(i = 0; i < sizeof(regfile); i++) ((char *)rg)[i] = 0x55;
	free(rg);
	regfree++;
	return;

}

istate *getistate(istate *ois) {

	istate *is;

	is = malloc(sizeof(istate));					/* Get a state block */
	if(!is) {
		printf("Can't allocate instruction state block\n");
		exit(1);
	}
	istatemalloc++;

	if(!(uint32_t)ois) bzero((void *)is, sizeof(istate));	/* Clear to zero if no old state */
	else bcopy(ois, is, sizeof(istate));			/* Copy the old state to new */

	is->isBack = ois;								/* Back chain */

//	if(trace) printf("-----> getistate %08X\n", is);	

	return is;

}

void tossistate(istate *is) {

	int i;

//	if(trace) printf("-----> tossistate %08X\n", is);	
	for(i = 0; i < sizeof(istate); i++) ((char *)is)[i] = 0x55;
	free(is);										/* Release the instruction state */
	istatefree++;
}

regfile *getregfile(regfile *org) {

	regfile *rg;

	rg = malloc(sizeof(regfile));					/* Get a state block */
	if(!rg) {
		printf("Can't allocate register file block\n");
		exit(1);
	}
	regmalloc++;

	if(!(uint32_t)org) bzero((void *)rg, sizeof(regfile));	/* Clear to zero if no old register file */
	else bcopy(org, rg, sizeof(regfile));			/* Copy the old register file to new */

	rg->level = level;								/* Set the level associated with this */
	rg->rgBack = org;								/* Back chain */
	rg->core = 0;									/* Core starts fresh */

	rg->rgx[rfGpr - 1] = &rg->gprs[0];
	rg->rgx[rfFpr - 1] = &rg->fprs[0];
	rg->rgx[rfVpr - 1] = (uint64_t *)&rg->vprs[0];
	rg->rgx[rfSpr - 1] = &rg->sprs[0];

	rg->trx[rfGpr - 1] = &rg->trakGpr[0];
	rg->trx[rfFpr - 1] = &rg->trakFpr[0];
	rg->trx[rfVpr - 1] = &rg->trakVpr[0];
	rg->trx[rfSpr - 1] = &rg->trakSpr[0];

//	if(trace) printf("-----> getregfile %08X\n", rg);	
	return rg;

}

// cl is tracking bits to always clear
// ex is tracking bits to explicitly set
// tt is tracking bits for the target
// ta is tracking bits for sourcea
// tb is tracking bits for sourceb
// tc is tracking bits for sourcec
// td is tracking bits for sourced
//
// tt does not participate in any calculations
// 
// No input is used unless at least one bit is set.
//
// gTstack is set if set in any source input
// gTlr is set if set in any input
// gTgen is set only if set in all source inputs	
// gTbrtbl is set if set in any source input
// gTinfnc is assumed to be 0 an all source inputs
// gTundef is set if set in any source input
// gTset is assumed to be 0 an all source inputs
//
// The result of ANDed with ~cl
// Then the final result is formed by ORing the previous calculations with ex
//

uint8_t trackReg(istate *is, uint8_t cl, uint8_t ex, uint8_t tt, uint8_t ta, uint8_t tb, uint8_t tc, uint8_t td) {

	is->trakClear  = cl;				/* Remember tracking used */
	is->trakExplicit  = ex;				/* Remember tracking used */
	is->trakTarget  = tt;				/* Remember tracking used */
	is->trakSourcea = ta;				/* Remember tracking used */
	is->trakSourceb = tb;				/* Remember tracking used */
	is->trakSourcec = tc;				/* Remember tracking used */
	is->trakSourced = td;				/* Remember tracking used */
	
	return trackRegNS(is, cl, ex, tt, ta, tb, tc, td);	/* Pass through for the calculations */


}


uint8_t trackRegNS(istate *is, uint8_t cl, uint8_t ex, uint8_t tt, uint8_t ta, uint8_t tb, uint8_t tc, uint8_t td) {

	uint8_t trakall, trakany, trak;
	int numused;

	trakall = 0xFF;						/* Start with all flags set */
	trakany = 0;						/* Start with all flags set */
	numused = 0;
	
	if(ta && !(ta & gTundef)) {			/* Do not use if undefined or never used */
		trakany |= ta;					/* OR everything together */
		trakall &= ta;					/* AND everything together */
		numused++;						/* Count that we used a source */
//		printf("ta %02X %02X %02X %d\n", ta, trakany, trakall, numused);
	}
	if(tb && !(tb & gTundef)) {			/* Do not use if undefined or never used */
		trakany |= tb;					/* OR everything together */
		trakall &= tb;					/* AND everything together */
		numused++;						/* Count that we used a source */
//		printf("tb %02X %02X %02X %d\n", tb, trakany, trakall, numused);
	}
	if(tc && !(tc & gTundef)) {			/* Do not use if undefined or never used */
		trakany |= tc;					/* OR everything together */
		trakall &= tc;					/* AND everything together */
		numused++;						/* Count that we used a source */
//		printf("tc %02X %02X %02X %d\n", tc, trakany, trakall, numused);
	}
	if(td && !(td & gTundef)) {			/* Do not use if undefined or never used */
		trakany |= td;					/* OR everything together */
		trakall &= td;					/* AND everything together */
		numused++;						/* Count that we used a source */
//		printf("td %02X %02X %02X %d\n", td, trakany, trakall, numused);
	}

	if(!numused) trakall = 0;			/* If there were no inputs, this should be 0 */
	trakall &= strakall;				/* Clear unused bits - note that gTset is cleared */	
	trakany &= strakany;				/* Clear unused bits - note that gTset is cleared */
	
	trak = trakany | trakall;			/* Form intermediate tracking result */
	
	trak &= ~cl;						/* Force clear bits */
	trak |= ex;							/* And set the explicit bits */
//	printf("xx %02X %02X %02X %02X\n", trakall, trakany, trakany | trakall, trak);
	
	if(trak & gTundef) trak = trak & (gTset | gTundef);	/* If tracking has gone undefined, just remember if it was ever set */

	return (trak);					/* Simple combine */

}

void pprint(char *data, uint64_t addr, int len, int indent) {

	int fline, i, j, k, cc;
	char idnt[256];

	for(i = 0; i < indent; i++) idnt[i] = ' ';			/* Clear the indent buffer */
	idnt[i] = 0;										/* Set terminator */
	
	fline = (len + 15) & -16;								/* Round up to full line size */
	
	for(i = 0; i < fline; i += 16) {						/* Step for each line */
		
		cc = i;												/* Current spot */

		printf("%s%08X.%08X   ", idnt, (uint32_t)((addr + i) >> 32), (uint32_t)addr + i);
		for(j = 0; j < 4; j++) {							/* Step for each word */
			for(k = 0; k < 4; k++) {						/* Step for each byte */
				if(cc < len) printf("%02X", (data[cc] & 0xFF));		/* Display byte */
				else printf("  ");							/* Space it instead */
				cc++;										/* Next character */
			}
			printf(" ");									/* Add a space */
		}
		
		cc = i;												/* Start over */

		printf(" * ");										/* Seperator */
		for(j = 0; j < 4; j++) {							/* Step for each word */
			for(k = 0; k < 4; k++) {						/* Step for each byte */
				if(cc < len) printf("%c", xtran[(data[cc] & 0xFF)]);	/* Display character */
				else printf(" ");							/* Space it instead */
				cc++;										/* Next character */
			}
			printf(" ");									/* Add a space */
		}
		printf(" *\n");										/* Seperator */
	}
	
}

void disassemble(uint64_t addr, uint32_t *mem, uint8_t *isnflgs, int insts, char *fname, char *function) {

	int i, btab, nr;
	
	printf("\nDisassembling %s in %s\n\n", function, fname);
	
	btab = 0;
	nr = 0;
	
	for(i = 0; i < insts; i++) {
		if(nr) {											/* In a not reachable stream? */
			if(disbuf[i].out[0] == 0) continue;				/* Still? */
			printf("%08X.%08X - %08X.%08X    **** Unreachable ****\n\n", (uint32_t)((addr + (nr * 4)) >> 32), (uint32_t)addr + (nr * 4) , 
				(uint32_t)((addr + ((i - 1) * 4)) >> 32), (uint32_t)addr + ((i - 1) * 4));
			nr = 0;
		}
		else {												/* We are not in a reachable stream */
			if(disbuf[i].out[0] == 0) {						/* Did we hit a non-reachable? */
				printf("\n");								/* Pause for dramatic efffect */
				nr = i;										/* Remember start */
				continue;									/* Get the next one */
			}
		}
		
		if(isnflgs[i] & isnBTarg) printf("\n");
		if(!btab && (isnflgs[i] & 0xF) == isBranchTbl) {
			printf("\n");
			btab = 1;
		}
		else if(btab && (isnflgs[i] & 0xF) != isBranchTbl) {
			printf("\n");
			btab = 0;
		}
		
		printf("%08X.%08X %02X %s\n", (uint32_t)((addr + (i * 4)) >> 32), (uint32_t)addr + (i * 4), 
			isnflgs[i], &disbuf[i].out[0]);
	}	

	if(nr) {												/* In a not reachable stream? */
		printf("%08X.%08X - %08X.%08X    **** Unreachable ****\n\n", (uint32_t)((addr + (nr * 4)) >> 32), (uint32_t)addr + (nr * 4) , 
			(uint32_t)((addr + ((i - 1) * 4)) >> 32), (uint32_t)addr + ((i - 1) * 4));
		nr = 0;
	}
	return;
}

char *isnname[] = {
	"**** Unreachable ****",
	"Branch",
	"Syscall",
	"Trap",
	"Branch table",
	"Scalar",
	"Invalid",
	"Branch/exit",
	"Syscall/exit",
	"Trap/exit",
	"Scalar/exit",
	"Privileged",
	"Privileged/exit",
	"Call",
	"Call/exit",
	"RFI",
	"Pre-link call vectors",
	"larx/stcx."
};

uint8_t xisn[256] = {   
					   xiNR,   xiSR,   xiSR,   xiSR,   xiSR,   xiSC,   xiTR,    255,	// 0 0 0 0
	 				   xiSR,   xiSR,   xiSR,   xiBT,  xiRsv,    255,    255,   xiIN,
	 				  
					    255,  xiBRx,  xiBRx,  xiSRx,  xiSRx,  xiSCx,  xiTRx,    255,	// 0 0 0 1
	 				  xiSRx,  xiSRx,  xiSRx,    255,  xiRsv,    255,    255,    255,
	 				  
					    255,    255,    255,  xiSRp,  xiSRp,    255,    255, xiRIpx,	// 0 0 1 0
	 				  xiSRp,  xiSRp,  xiSRp,    255,  xiRsv,    255,    255,    255,
	 				  
					    255,    255,    255, xiSRpx, xiSRpx,    255,    255, xiRIpx,	// 0 0 1 1
	 				 xiSRpx, xiSRpx, xiSRpx,    255,  xiRsv,    255,    255,    255,
	 				  
					    255,   xiSR,   xiSR,    255,    255,    255,    255,    255,	// 0 1 0 0
	 				    255,    255,    255,    255,  xiRsv,    255,    255,    255,
	 				  
					    255, xiBRcx, xiBRcx,    255,    255,    255,    255,    255,	// 0 1 0 1
	 				    255,    255,    255,    255,  xiRsv,    255,    255,    255,
	 				  
					    255,    255,    255,    255,    255,    255,    255,    255,	// 0 1 1 0
	 				    255,    255,    255,    255,  xiRsv,    255,    255,    255,
	 				  
					    255,    255,    255,    255,    255,    255,    255,    255,	// 0 1 1 1
	 				    255,    255,    255,    255,  xiRsv,    255,    255,    255,
	 				  
					    255,   xiSR,   xiSR,   xiSR,   xiSR,   xiSC,   xiTR,    255,	// 1 0 0 0
	 				   xiSR,   xiSR,   xiSR,   xiBT,  xiRsv,    255,    255,   xiIN,
	 				  
					    255,  xiBRx,  xiBRx,  xiSRx,  xiSRx,  xiSCx,  xiTRx,    255,	// 1 0 0 1
	 				  xiSRx,  xiSRx,  xiSRx,    255,  xiRsv,    255,    255,    255,
	 				  
					    255,    255,    255,  xiSRp,  xiSRp,    255,    255,    255,	// 1 0 1 0
	 				  xiSRp,  xiSRp,  xiSRp,    255,  xiRsv,    255,    255,    255,
	 				  
					    255,    255,    255, xiSRpx, xiSRpx,    255,    255, xiRIpx,	// 1 0 1 1
	 				 xiSRpx, xiSRpx, xiSRpx,    255,  xiRsv,    255,    255,    255,
	 				  
					    255,   xiSR,   xiSR,    255,    255,    255,    255,    255,	// 1 1 0 0
	 				    255,    255,    255,    255,  xiRsv,    255,    255,    255,
	 				  
					    255, xiBRcx, xiBRcx,    255,    255,    255,    255,    255,	// 1 1 0 1
	 				    255,    255,    255,    255,  xiRsv,    255,    255,    255,
	 				  
					    255,    255,    255,    255,    255,    255,    255,    255,	// 1 1 1 0
	 				    255,    255,    255,    255,  xiRsv,    255,    255,    255,
	 				  
					    255,    255,    255,    255,    255,    255,    255,    255,	// 1 1 1 1
	 				    255,    255,    255,    255,  xiRsv,    255,    255,    255
	 				  
 };	 				  
	 				  
char *ent[2] = { "", "Entry/" };

void gendtrace(uint64_t addr, uint64_t funcstart, uint32_t *mem, uint8_t *isnflgs, int insts, char *fname, char *function) {

	int i, lastinst, entinst;
	uint32_t lasttype, currtype;
	
	lasttype = xisn[isnflgs[0]];				/* Start the whole thing off */
	if(lasttype == 255) {						/* Invalid? */
		printf("Invalid isnflgs %02X at %08X.%08X\n", isnflgs[0], (uint32_t)(addr >> 32), (uint32_t)addr);
		exit(1);
	}
	if((lasttype == xiNR) && (mem[0] == 0x60000000)) lasttype = xiSR;	/* If the first instrucion wasn't reached and it is a nop, treat as scalar */
	lastinst = 0;
	entinst = (addr - funcstart) / 4;			/* Get instruction number of the entry point */
	if(entinst == 0) lasttype = lasttype | 0x100;	/* Mark as entry instruction */

	if (dis)
		printf("\nAnalysis of %s in %s\n\n", function, fname);
	
	for(i = lastinst; i < insts; i++) {
	
		currtype = xisn[isnflgs[i]];			/* Convert to a common tag */
		if(lasttype == 255) {					/* Invalid? */
			printf("Invalid isnflgs %02X at %08X.%08X\n", isnflgs[i], (uint32_t)((addr + (uint64_t)(i * 4)) >> 32), 
				(uint32_t)(addr + (uint64_t)(i * 4)));
			exit(1);
		}
		if((currtype == xiNR) && (mem[i] == 0x60000000)) currtype = xiSR;	/* If not reached and a nop, treat as scalar */
		if(entinst == i) currtype = currtype | 0x100;	/* Mark as entry instruction */
		if(currtype == lasttype) continue;		/* Keep going if same as last */

//		Print junk only if this is not a normal scalar

		if((lasttype > 255) || ((lasttype & 0xFF) != xiSR)) {
			if (dis) 
				printf("%08X.%08X - %08X.%08X: %02X %03X %s%s\n", 
				       (uint32_t)(addr + ((uint64_t)(lastinst * 4)) >> 32), (uint32_t)(addr + ((uint64_t)(lastinst * 4))), 
				       (uint32_t)((addr + (uint64_t)((i - 1) * 4)) >> 32), (uint32_t)(addr + (uint64_t)((i - 1) * 4)),
				       isnflgs[lastinst], lasttype,
				       ent[lasttype >> 8], isnname[lasttype & 0xFF]);
		}
		
		lasttype = currtype;					/* Remember our new type */
		lastinst = i;							/* Remember last instruction */
	}
	
	if(lasttype == xiNR) lasttype = xiFunVec;	/* If we finished with an "unreachable" assume it is a funk function vector */
	
	if (stats | trace) {
		printf("%08X.%08X - %08X.%08X: %02X %03X %s%s\n", 
		       (uint32_t)(addr + ((uint64_t)(lastinst * 4)) >> 32), (uint32_t)(addr + ((uint64_t)(lastinst * 4))), 
		       (uint32_t)((addr + (uint64_t)((i - 1) * 4)) >> 32), (uint32_t)(addr + (uint64_t)((i - 1) * 4)),
		       isnflgs[lastinst], lasttype,
		       ent[lasttype >> 8],isnname[lasttype & 0xFF]);
	}
	
	return;
}

void diedie(char *xx) {

	printf("ARGGGHHHHHHH!!!!!!! - %s\n", xx);
	return;
}
