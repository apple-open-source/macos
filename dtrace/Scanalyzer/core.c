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

corebk *corehd = 0;
corebk *quikcore = 0;
int ctrace = 0;
uint32_t coremalloc = 0;
uint32_t corefree = 0;

corebk *getcore(void) {

	corebk *xcore;
	int i;

	if((xcore = corehd)) corehd = xcore->core;		/* If there is a free core block, grab it */
	else {											/* Nothing free */
		xcore = malloc(sizeof(corebk));					/* Get a core block */
		if(!xcore) {
			printf("Can't allocate core block\n");
			exit(1);
		}
		coremalloc++;
	}

	bzero((void *)xcore, sizeof(corebk));			/* Clear */
	for(i = 0; i < 128; i++) xcore->trakBytes[i] = gTundef;	/* Set all to undefined */

	if(ctrace) printf("-----> getcore %08X\n", xcore);	
	
	return xcore;
	
}

void tosscore(regfile *rg) {

	corebk *xcore;
	int i;
	
	while (rg->core) {								/* Go until there are no more to do... */
		xcore = rg->core;							/* Remember the guy we are tossing */
		if(ctrace) printf("-----> tosscore %08X.%08X in %08X, level %d\n", (uint32_t)(rg->core->addr >> 32), (uint32_t)rg->core->addr, rg->core, rg->level);
		if(quikcore == xcore) quikcore = 0;			/* Clear quick lookup if we are tossing it */
		rg->core = xcore->core;						/* Unlink it */
		for(i = 0; i < sizeof(corebk); i++) ((char *)xcore)[i] = 0x55;	/* (TEST/DEBUG) Fill with known stuff */
		xcore->core = corehd;						/* Move top kink */
		corehd = xcore;								/* Link it */
	}
}

void freecore(void) {

	corebk *xcore;
	int i;
	
	while (corehd) {								/* Keep going until we are done */
		if(ctrace) {
			printf("-----> freecore %08X.%08X in %08X\n", (uint32_t)(corehd->addr >> 32), (uint32_t)corehd->addr, corehd);
			fsync(stdout);
		}
		xcore = corehd;								/* Remember the next guy */
		corehd = corehd->core;						/* Unlink it */
		for(i = 0; i < sizeof(corebk); i++) {		/* (TEST/DEBUG) */
			if((i >= 8) && (i < 12)) continue;		/* (TEST/DEBUG) */
			if(((char *)xcore)[i] != 0x55) {		/* (TEST/DEBUG) */
				diedie("free core overlay found in freecore");		/* (TEST/DEBUG) */
				break;								/* (TEST/DEBUG) */
			}
		}
		free(xcore);								/* Free the memory */
		corefree++;
	}

	corehd = 0;										/* Clear for the next time around */
	quikcore = 0;									/* Clear this as well */
}

void readcore(regfile *rg, uint64_t addr, uint32_t size, uint64_t addrmask, uint32_t flags, uint64_t *fetch, uint8_t *trak) {

	corebk *core;
	uint8_t trakFetch, trakAny;
	int i, snko, srco, bleft, blen, xtrk;
	union {
		uint64_t dword;
		uint8_t byte[8];
	} mem;
	
	addr &= addrmask;								/* Wrap to address size */
	
	if(ctrace) printf("******** reading %08X.%08X, current level = %d\n", (uint32_t)(addr >> 32), (uint32_t)addr, rg->level);

	mem.dword = 0;									/* Clear it out */
	xtrk = 0;										/* Show we haven't merged any tracking yet */
	snko = 8 - size;								/* Get offset into register */
	trakFetch = gTundef;							/* Set undefined to start */
	
	while(size) {									/* Fetch it all */

		addr &= addrmask;							/* Wrap to address size */
		blen = size;
		bleft = 128 - ((int)addr & 127);			/* Get the number of bytes left in the addressed line */
		if(bleft < blen) {							/* Can we get it all from the line? */
			blen = bleft;							/* No, clamp it */
			if(ctrace) printf("******** Read spills %d bytes into next 128 byte line\n", size - blen);
		}

		core = findcore(rg, addr & -128ULL);		/* Find the cached value if we have it */
		if(core) {									/* We found it */

			srco = addr & 127ULL;					/* Get offset into core line */
			if(!xtrk) trakFetch = 0xFF;				/* Set to all flags if first time */
			xtrk = 1;								/* Show we merged */
			trakAny = 0;							/* Set tracking to nothing */

			for(i = 0; i < blen; i++) {				/* Process all bytes */
				mem.byte[snko + i] = core->bytes[srco + i];	/* Fetch it all */
				trakFetch &= core->trakBytes[srco + i];	/* Accumulate common tracking info for this fetch */
				trakAny |= core->trakBytes[srco + i];	/* Accumulate any tracking info for this fetch */
			}
	
/*			If any undefined bytes, set undefined and set set only if all bytes were set */
	
			if(trakAny & gTundef) trakFetch = (trakFetch & gTset) | gTundef;	
		}
		else {										/* Missing memory */
			for(i = 0; i < blen; i++) mem.byte[snko + i] = 0xAA;	/* Fill with junk */
			trakFetch = (trakFetch & gTset) | gTundef;	/* Merge an undefined into tracking */
			xtrk = 1;								/* Show merge done */
		}
		
		snko += blen;								/* Bump register offset */
		addr += blen;								/* Bump to next */
		size -= blen;								/* Back off by what we read */
	}

	*fetch = mem.dword;								/* Set the fetched core */
	*trak = trakFetch;								/* Set the validity of the fetch */

	return;



}

void writecore(regfile *rg, uint64_t addr, uint32_t size, uint64_t addrmask, uint32_t flags, uint8_t *src, uint8_t trak) {
	corebk *core;
	int i, snko, srco, bleft, blen;
	
	addr &= addrmask;								/* Wrap to address size */

	if(ctrace) printf("******** writing %08X.%08X (size = %d, trak = %02X), current level = %d\n", (uint32_t)(addr >> 32), (uint32_t)addr, size, trak, rg->level);
	
	srco = 0;										/* Clear source offset */
	
	while(size) {									/* Write all hunks */

		addr &= addrmask;							/* Wrap to address size */
		blen = size;								/* Assume we can move it all */
		bleft = 128 - ((int)addr & 127);			/* Get the number of bytes left in the addressed line */
		if(bleft < blen) {							/* Can we get it all from the line? */
			blen = bleft;							/* No, clamp it */
			if(ctrace) printf("******** Write spills %d bytes into next 128 byte line\n", size - blen);
		}

		core = promotecore(rg, addr & -128ULL);		/* Get a copy of the data locally */
		if(core) {									/* We found it */
			snko = addr & 127ULL;					/* Get offset into core line */
			for(i = 0; i < blen; i++) {				/* Process all bytes */
				core->bytes[snko + i] = src[srco + i];	/* Copy it all */
				core->trakBytes[snko + i] = trak;	/* Set the tracking */
			}
		}

		srco += blen;								/* Bump register offset */
		addr += blen;								/* Bump to next */
		size -= blen;								/* Back off by what we read */
	}
}

corebk *promotecore(regfile *rg, uint64_t addr) {

	corebk *core, *newcore, *lastcore, *curcore;

	core = findcore(rg, addr);						/* Find and possibly install the line */
	if(!core) return 0;								/* Some kind of error, oh well, forget it... */
	
	if(core->rg->level == rg->level) return core;	/* If line is at our level, return it... */

	if(ctrace) printf("******** pulling %08X.%08X from level %d to level %d\n", (uint32_t)(addr >> 32), (uint32_t)addr, core->rg->level, rg->level);
	
	newcore = getcore();							/* Get a core block */
	bcopy((void *)core, (void *)newcore, sizeof(corebk));	/* Duplicate the original */
	newcore->rg = rg;								/* Set the register file */
	
	curcore = rg->core;								/* Pick up the current chain */
	lastcore = 0;									/* Show that we are at the first one */
	while(curcore) {								/* Run the list */
		if(curcore->addr > addr) break;				/* Went too far... */
		lastcore = curcore;							/* Remember previous */
		curcore = curcore->core;					/* Skip to the next */
	}

	if(lastcore) {									/* Do we need to hang this off the start? */
		newcore->core = lastcore->core;				/* Move the pointer to the next to our hunk */
		lastcore->core = newcore;					/* Point the previous guy to us */
	}
	else {
		newcore->core = rg->core;					/* Set us to point to the old first */
		rg->core = newcore;							/* Make us first */
	}

	quikcore = newcore;								/* Hint this one */
	return newcore;									/* Pass back the new one */

}

corebk *findcore(regfile *rg, uint64_t addr) {

	corebk *lastcore;
	regfile *rgc;
	uint32_t ret;
	int i;

	lastcore = 0;									/* Shut up the compiler */

	addr = addr & -128LL;							/* Round down to line */

	if(ctrace) printf("******** finding %08X.%08X\n", (uint32_t)(addr >> 32), (uint32_t)addr);
	
	if(quikcore && (quikcore->rg->level == rg->level) && (quikcore->addr == addr)) {	/* If this what we want take it and leave */
		if(ctrace) printf("******** found   %08X.%08X in quikcore, corebk = %08X\n", (uint32_t)(addr >> 32), (uint32_t)addr, quikcore);
		return quikcore;
	}
	
	rgc = rg;
	while(rgc) {									/* Run all the lists */
		quikcore = rgc->core;						/* Pick up the current chain */
		lastcore = 0;								/* Show that we are at the first one */
		while(quikcore) {							/* Run the list */
			if(quikcore->addr == addr) {			/* Found it... */
				if(ctrace) printf("******** found   %08X.%08X in level %d, corebk = %08X\n", (uint32_t)(addr >> 32), (uint32_t)addr, rgc->level, quikcore);
				return quikcore;					/* Found it */
			}
			if(quikcore->addr > addr) break;		/* Went too far... */
			lastcore = quikcore;					/* Remember previous */
			quikcore = quikcore->core;				/* Skip to the next */
		}
		if(!rgc->rgBack) break;						/* Leave if we just did the last... */
		rgc = rgc->rgBack;							/* Step back in time */
	}

	if(ctrace) printf("******** notfnd  %08X.%08X\n", (uint32_t)(addr >> 32), (uint32_t)addr);
	
	quikcore = getcore();							/* Get a core block */
	quikcore->addr = addr;							/* Set the memory address */
	quikcore->rg = rgc;								/* Set the "owning" register file to the base */

	if(lastcore) {									/* Do we need to hang this off the start? */
		quikcore->core = lastcore->core;			/* No, move the pointer to the next to our hunk */
		lastcore->core = quikcore;					/* Point the previous guy to us */
	}
	else {
		quikcore->core = rgc->core;					/* Set us to point to the old first */
		rgc->core = quikcore;						/* Make us first */
	}

	ret = symfetch(addr, 128, 128, (char *)&quikcore->bytes[0]);	/* Fetch memory from the file if it is there */
	if(ret) {										/* We found the memory... */
		if(ctrace) printf("******** loading %08X.%08X into level %d, ret = %d\n", (uint32_t)(quikcore->addr >> 32), (uint32_t)quikcore->addr, rgc->level, ret);
		if(ctrace) pprint((char *)&quikcore->bytes[0], quikcore->addr, 128, 17);
		for(i = 0; i < 128; i++) quikcore->trakBytes[i] = gTset;	/* Set all to set */
		return quikcore;							/* Return it */
	}
	else {
		if(ctrace) printf("******** filling %08X.%08X in level %d\n", (uint32_t)(quikcore->addr >> 32), (uint32_t)quikcore->addr, rgc->level);
		for(i = 0; i < 128; i++) quikcore->trakBytes[i] = gTset;	/* Set all to set */
		return quikcore;							/* Return it */
	}

}
