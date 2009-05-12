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

int forcefetch = 0;
int s64bit = 0;
char *spaces = "                      ";

void checkrg(regfile *rg);

uint8_t *InvokeScanalyzer(char is64Bit, char shouldDisassemble, uint64_t addr, uint64_t size, uint32_t* mem, char* library, char* function) {

	symdesc xsym;
	uint8_t *isnflgs;

	s64bit = is64Bit;
	image = 1;					/* We are passing a binary image, not a mach-o */
	dis = shouldDisassemble;
	ctrace = 0;
	trace = 0;
	stats = 0;
	level = 0;
	corehd = 0;	
	quikcore = 0;
	
	(void)syminitcore((uint32_t *)mem, addr, size);	/* Initialize memory/symbol stuff */
	int32_t ret = symindex(-1, &xsym);				// Index -1 is a pseudo-symbol for the entire __text section 
	if(ret) {								// Not even a text section???
		printf("File does not contain a __text section\n");
		return 0;
	}
		
	isnflgs = Scanalyzer(addr, addr, size, mem, library, function);   /* Analyze the code */
	symfree();										/* Toss all symbol tables */

	return isnflgs;
}

uint8_t *Scanalyzer(uint64_t entry, uint64_t addr, uint64_t bytes, uint32_t *mem, char *fname, char *function) {

	istate *is;
	regfile *rg;
	uint8_t *isnflgs;
	uint64_t funcstart;
	int i, insts;

	if(stats | trace) printf("\nTracing analysis of %s in %s\n\n", function, fname);

	insts = (bytes + 3) / 4;						/* Instructions in routine */
	if(!insts) return (uint8_t *)0;					/* If size is less than one instruction, just return */

	isnflgs = malloc(insts);						/* Get a flag for each potential instruction */
	if(!isnflgs) {
		printf("Can't allocate instruction flags block\n");
		return 0;
	}
	bzero((void *)isnflgs, insts);					/* Clear to zero */
	
	if(dis || trace) {								/* Dissasemble or trace? */
		disbuf = malloc(insts * 512);				/* Get enough space for the whole disassembly */
		if(!disbuf) {
			printf("Can't allocate dissasembly output buffer\n");
			free(isnflgs);
			return 0;
		}
		bzero((void *)disbuf, insts * 512);			/* Clear to zero */
	}
	
	level = 0;										/* Set level to 0 */
	
	is = getistate(0);								/* Allocate a brand new instruction state */
	
	is->fstart = addr;								/* Remember start of function */
	is->fsize = bytes;								/* And its size */
	
	rg = getregfile(0);								/* Allocate a brand new register file */
	
	for(i = 0; i < 32; i++) rg->gprs[i] = 0xFFFFFFFFD0000000ULL | (i << 20);	/* Put some known values to make sure addresses don't collide */
	rg->gprs[1] = 0xFFFFFFFFC0000000ULL;			/* Set initial stack (R1) value */
	rg->trakGpr[1] = gTstack | gTset;				/* Indicate that we have a stack value in R1 */
	rg->sprs[sPlr] = 0xFFFFFFFF80000000ULL;			/* Set a "known" return address */
	is->exit = 0xFFFFFFFF80000000ULL;				/* Set the "known" return address */
	rg->trakSpr[sPlr] = gTlr | gTset;				/* Define the contents */
	rg->sprs[sPctr] = addr;							/* Pretend like we got here via BCTR */
	rg->trakSpr[sPctr] = gTinfnc | gTset;			/* Define the contents */

	if(s64bit) {
		rg->sprs[sPmsr] = 0x800000000000ULL;		/* Set MSR to 64-bit */
		rg->addrmask = 0xFFFFFFFFFFFFFFFFULL;		/* Set address mode to 64-bit */
		rg->ifaddrmask = 0xFFFFFFFFFFFFFFFCULL;		/* Set address mode to 64-bit ifetch */
	}
	else {
		rg->sprs[sPmsr] = 0x000000000000ULL;		/* Set MSR to 32-bit */
		rg->addrmask = 0x00000000FFFFFFFFULL;		/* Set address mode to 32-bit */
		rg->ifaddrmask = 0x00000000FFFFFFFCULL;		/* Set address mode to 32-bit ifetch */
	}
	funcstart = rg->pc = entry & rg->ifaddrmask;	/* Set the PC */

	for(i = 0; i < 32; i++) {						/* Initialize 32 registers */
		rg->vprs[i].vw[0] = 0x7FFFDEAD;				/* Architectural empty value */
		rg->vprs[i].vw[1] = 0x7FFFDEAD;				/* Architectural empty value */
		rg->vprs[i].vw[2] = 0x7FFFDEAD;				/* Architectural empty value */
		rg->vprs[i].vw[3] = 0x7FFFDEAD;				/* Architectural empty value */
		rg->trakVpr[i] = gTset;						/* Show that value is set */
	}

	rg->sprs[sPvscr] = 0;							/* Set the VSCR */
	rg->sprs[sPvscr + 1] = 0x0000000000010000ULL;	/* Set the VSCR with Java mode off */
	rg->trakSpr[sPvscr] = gTset;

	for(i = 0; i < 32; i += 2) {					/* Initialize 32 fpr registers */
		rg->fprs[i] = 0xC24BC19587859393ULL;		/* Just like the kernel */
		rg->fprs[i + 1] = 0xE681A2C88599855AULL;
		rg->trakFpr[i] = gTset;						/* Show that value is set */
		rg->trakFpr[i + 1] = gTset;					/* Show that value is set */
	}
	
	scancode(is, rg, isnflgs);						/* Go scan the code */
	
	if(forcefetch) {								/* Should we force decode the unreachable instructions? */
		for(i = 0; i < insts; i++) {				/* Yes, find everything that was not reached */
			if(isnflgs[i]) continue;				/* Forget this one, we already saw it... */
			
			rg->pc = (addr + (i * 4)) & rg->ifaddrmask;	/* Generate the PC */
			scancode(is, rg, isnflgs);				/* Go scan it */
		}
	}
	
	if(dis) disassemble(addr, mem, isnflgs, insts, fname, function);	/* Dissassemble if so requested... */
	
	gendtrace(addr, funcstart, mem, isnflgs, insts, fname, function);	/* Generate dtrace stuff */
	
	tossistate(is);									/* Toss instruction decode/execution structure */
	tossregfile(rg);								/* Toss register file structure */
	freecore();										/* Toss all of the COW memory */
	if(disbuf) free(disbuf);						/* Toss the disassembly buffer */
	disbuf = (disb *)0;
	
	if(stats | trace) {
		printf("\n");
		printf("istatemalloc = %10d, istatefree = %10d\n", istatemalloc, istatefree);
		printf("regmalloc    = %10d, regfree    = %10d\n", regmalloc, regfree);
		printf("coremalloc   = %10d, corefree   = %10d\n", coremalloc, corefree);
	}
	
	return isnflgs;
}

void scancode(istate *is, regfile *rg, uint8_t *isnflgs) {

	istate *ist;
	regfile *rgt;
	uint32_t offs, xoffs;
	char xfunc[512];
	char extra[512];
	char dumpdata[512];
	char traktrak[512];
	uint64_t instruction, signmask, btstart, bttarg, memaddr, btentry;
	uint8_t trak, *srcaddr;
	int i, msz, wsz, treg, isnkind;

	while(1) {

		checkrg(rg);								/* (TEST/DEBUG) */

		btstart = 0;								/* Set that we are not within a branch table now */

		if(rg->pc & 3) {							/* Invalid PC address? */
			printf("***** Unaligned PC address: %08X.%08X\n", (uint32_t)(rg->pc >> 32), (uint32_t)rg->pc);
			exit(1);
		}

		offs = rg->pc - is->fstart;					/* Get byte offset into code (if pc < start, we get a huge unsigned number) */
		if(offs >= is->fsize) {						/* If we've gone outside out code, leave... */
			if(trace)  printf("                                 (Instruction address (%08X.%08X) outside of function, exiting)\n", (uint32_t)(rg->pc >> 32), (uint32_t)rg->pc);
			return;
		}
		
		offs = offs / 4;							/* Convert to instruction offset */
		if(isnflgs[offs] & ~isnBTarg) {				/* If instruction has been seen (except as just a branch target) leave... */
			if(trace) printf("                                 (Instruction at %08X.%08X previously seen, exiting)\n", (uint32_t)(rg->pc >> 32), (uint32_t)rg->pc);
			return;
		}

		readcore(rg, rg->pc, 4, rg->ifaddrmask, 0, &instruction, &trak);	/* Fetch the instruction */
		is->instimg = (uint32_t)instruction;		/* Save the instruction */
		is->pc = rg->pc;							/* Remember the PC in the intermediate state */
		decode((uint32_t)instruction, rg->pc, is, rg);	/* Decode and execute instruction.  Results not committed yet. */

		isnkind = is->opr;							/* Remember what kind of instruction we found */
		if(is->mods & modRsvn) isnkind = isRsvn;	/* If this is reservation type instruction, mark it specially */
		isnflgs[offs] = (isnflgs[offs] & isnBTarg) | isnkind;	/* Remember the type of instruction we found */
		
		if(is->opr == isInvalid) {					/* Was this a bogus instruction? */
			if(disbuf) sprintf((char *)&disbuf[offs].out[0], "%08X     .long       0x%08X    * %c%c%c%c *", (uint32_t)instruction, (uint32_t)instruction,
				xtran[(uint8_t)(instruction >> 24)], xtran[(uint8_t)(instruction >> 16)], 
				xtran[(uint8_t)(instruction >> 8)], xtran[(uint8_t)instruction]);	/* If disassembly buffer, do it */
			if(trace)  printf("%08X.%08X %02X %s\n", (uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, isnflgs[offs], &disbuf[offs].out[0]);
			rg->pc = rg->pc + 4;					/* Step to next word */
			rg->pc &= rg->ifaddrmask;				/* Pin to addressing mode */
			continue;								/* Go check it out... */
		}
		
		if(is->mods & modPrv) isnflgs[offs] |= isnPriv;	/* Mark as privileged instruction */
		
		extra[0] = 0;								/* Clear the extra information */
		traktrak[0] = 0;							/* Clear the extra information */
		dumpdata[0] = 0;							/* Clear the extra information */

		switch(is->opr) {

// *******************************

			case isScalar:
	
				is->trakResult &= ~gTinfnc;			/* Clear the "in function" flag */
				if((is->targtype == rfGpr) && 		/* If GPR and defined and in function, remember it */
					!(is->trakResult & gTundef) && 
					((is->result - is->fstart) < is->fsize)) {	/* Does the result point into the function? */
					is->trakResult |= gTinfnc;		/* Set the flag */

					if((is->result & 3) == 0) {		/* We can only have a branch table that is 4-byte aligned */
						if(trace) printf("                                 (Potential branch table at %08X.%08X)\n", (uint32_t)(is->result >> 32), (uint32_t)is->result);
						readcore(rg, is->result, 4, rg->ifaddrmask, 0, &btentry, &trak);	/* Fetch the possible branch table entry */
						if(btentry & 0x0000000080000000ULL) btentry |= 0xFFFFFFFF00000000ULL;	/* Extend sign */
						bttarg = btentry + is->result;	/* Add entry to table address */
						if(trace) printf("                                 (Fetched entry %08llX relocates to %08X.%08X)\n", btentry, (uint32_t)(bttarg >> 32), (uint32_t)bttarg);
						if(((bttarg - is->fstart) < is->fsize) && (is->result != 0)) {	/* If the relocated value still points into the function, assume this is a branch table */
							is->trakResult |= gTbrtbl;	/* Mark branch table */
							if(trace) printf("                                 (Branch table found at %08X.%08X)\n", (uint32_t)(is->result >> 32), (uint32_t)is->result);
						}
						else {
							if(trace) printf("                                 (Branch table not found at %08X.%08X)\n", (uint32_t)(is->result >> 32), (uint32_t)is->result);
						}
					}
				}
				if(disbuf) {						/* If tracing or disassembling */
					sprintf(traktrak, "%02X %02X %02X %02X %02X %02X = %02X", is->trakExplicit, is->trakTarget, is->trakSourcea, is->trakSourceb, 
						is->trakSourcec, is->trakSourced, is->trakResult);
					if(!(is->trakResult & gTundef)) sprintf(extra, "(%08X.%08X)", (uint32_t)(is->result >> 32), (uint32_t)is->result);
					sprintf((char *)&disbuf[offs].out[0], "%08X     %-12s%-20s %s %s", (uint32_t)instruction, is->op, is->oper, traktrak, extra);
					if(trace) printf("%08X.%08X %02X %s\n", (uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, isnflgs[offs], (char *)&disbuf[offs].out[0]);
				}
	
				complete(is, rg);					/* Complete the instruction */
	
				rg->pc += 4;						/* Bump instruction counter */
				rg->pc &= rg->ifaddrmask;			/* Pin to addressing mode */
				if((rg->pc - is->fstart) >= is->fsize) isnflgs[offs] |= isnExit;	/* If we step off end next, indicate exit */
				break;

// *******************************
			
			case isBrCond:
			case isBranch:

				is->btarg &= rg->ifaddrmask;		/* Pin to addressing mode */

				is->trakBtarg &= ~gTinfnc;			/* Clear the "in function" flag */
				if(!(is->trakBtarg & gTundef) && 	/* If defined and in function, remember it */
					((is->btarg - is->fstart) < is->fsize)) is->trakBtarg |= gTinfnc;
					
				if(disbuf) sprintf(traktrak, "%02X %02X %02X %02X %02X %02X = %02X", is->trakExplicit, is->trakTarget, is->trakSourcea, is->trakSourceb, 
					is->trakSourcec, is->trakSourced, is->trakBtarg);


				if(is->mods & modSetLR) {			/* Are we going to set the LR? */
					if((is->trakBtarg & gTundef) || (is->btarg != ((rg->pc + 4) & rg->ifaddrmask))) {	/* Is the branch target undefined or not equal to PC + 4? */
						isnflgs[offs] |= isnCall;	/* Yeah, this is a call and not just to get PC */
					}
				}

				if(is->trakBtarg & gTundef) isnflgs[offs] |= isnExit;	/* If we don't know where we are going, treat as if exit */
				else {
					if(!(is->trakBtarg & gTinfnc)) {	/* Is the branch to outside of the function? */
						if(!(is->mods & modSetLR)) isnflgs[offs] |= isnExit;	/* If no LR set, this is an exit point */
						
						if(disbuf) {				/* If tracing or disassembling */
							symaddr(is->btarg, xfunc);	/* Get the function name */
							sprintf(extra, "(%08X.%08X) ---> %s", (uint32_t)(is->btarg >> 32), (uint32_t)is->btarg, xfunc);
						}
					}
					else {
						isnflgs[(is->btarg - is->fstart) / 4] |= isnBTarg;	/* Mark target of the branch */
						if(disbuf) sprintf(extra, "(%08X.%08X)", (uint32_t)(is->btarg >> 32), (uint32_t)is->btarg);
					}
				}

				if(disbuf) {						/* If tracing or disassembling */
					sprintf((char *)&disbuf[offs].out[0], "%08X     %-12s%-20s %s %s", (uint32_t)instruction, is->op, is->oper, traktrak, extra);
					if(trace) printf("%08X.%08X %02X %s\n", (uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, isnflgs[offs], (char *)&disbuf[offs].out[0]);
				}

				if(is->mods & modSetLR) {			/* Should we set the link register? */
					rg->sprs[sPlr] = rg->pc + 4;	/* Record the PC in the link register */
					rg->sprs[sPlr] &= rg->ifaddrmask;	/* Pin to addressing mode */

					rg->trakSpr[sPlr] = gTlr | gTgen | gTset;	/* Remember that we generated this from the PC */
					rg->trakSpr[sPlr] &= ~gTinfnc;	/* Clear the "in function" flags */
					if((rg->sprs[sPlr] - is->fstart) < is->fsize) rg->trakSpr[sPlr] |= gTinfnc;	/* See if this value could be an address in function */	

					if(trace) printf("                                 (LR set to %08X.%08X)\n", (uint32_t)((rg->pc + 4) >> 32), (uint32_t)(rg->pc + 4));
				}

				if(is->opr == isBrCond) {			/* Conditional branch? */

					level++;						/* Bump to the next level */
					if(trace) printf("                                 (Taken conditional branch from %08X.%08X to %08X.%08X, lvl=%d)\n", 
						(uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, (uint32_t)(is->btarg >> 32), (uint32_t)is->btarg, level);

					ist = getistate(is);			/* Allocate a brand new instruction state */
					rgt = getregfile(rg);			/* Allocate a brand new register file */

					rgt->pc = is->btarg;			/* Set new branch target */
					scancode(ist, rgt, isnflgs);	/* Take the conditional branch */

					tossistate(ist);				/* Toss instruction decode/execution structure */
					tossregfile(rgt);				/* Toss register file structure */
					level--;						/* Decrement the level */
					
					if(trace) printf("                                 (Not taken conditional branch from %08X.%08X, lvl=%d)\n", 
						(uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, level);
					rg->pc = rg->pc + 4;			/* Step to the next instruction */
					rg->pc &= rg->ifaddrmask;		/* Pin to addressing mode */
					if((rg->pc - is->fstart) >=is->fsize) isnflgs[offs] |= isnExit;	/* If we step off end next, indicate exit */
				}
				else {								/* This is the unconditional branch case */
				
					if(isnflgs[offs] & isnCall) {	/* Is this a call? */
						if(is->trakBtarg & gTinfnc) {	/* Is the target within the function? */

							level++;				/* Bump to the next level */
							if(trace) printf("                                 (Call from %08X.%08X to %08X.%08X - internal function, lvl=%d)\n", 
								(uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, (uint32_t)(is->btarg >> 32), (uint32_t)is->btarg, level);
		
							ist = getistate(is);	/* Allocate a brand new instruction state (no register state this time) */
						
							ist->clevel++;			/* Show that we are in an internal function */
							ist->freturn = rg->pc + 4;	/* Remember the return address */

							rg->pc = is->btarg;		/* Set new branch target */
							scancode(ist, rg, isnflgs);	/* Call into the function */

							level--;				/* Decrement the level */
							if(trace) printf("                                 (Return from %08X.%08X to %08X.%08X - internal function, lvl=%d)\n", 
								(uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, (uint32_t)(ist->freturn >> 32), (uint32_t)ist->freturn, level);
							rg->pc = ist->freturn;	/* Make sure of PC in case this is a forced return */
		
							tossistate(ist);		/* Toss instruction decode/execution structure */

						}
						else {						/* This call is to an external function */
							rg->pc = rg->pc + 4;	/* Yes, don't take it yet... */
							rg->pc &= rg->ifaddrmask;	/* Pin to addressing mode */
						}
						if((rg->pc - is->fstart) >= is->fsize) isnflgs[offs] |= isnExit;	/* If we step off end next, indicate exit */
					}
					else {							/* Unconditional branch */
						if(isnflgs[offs] & isnExit)  {	/* Unknown or out-of-function target */
							if(trace) {
								if(is->trakBtarg & gTundef) printf("                                 (Uncond branch from %08X.%08X to %08X.%08X - unknown address, exiting)...\n", 
									(uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, (uint32_t)(is->btarg >> 32), (uint32_t)is->btarg);
								else printf("                                 (Uncond branch from %08X.%08X to %08X.%08X - out of function, exiting...)\n", 
									(uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, (uint32_t)(is->btarg >> 32), (uint32_t)is->btarg);
							}
							return;					/* This is an exit or undefined target, so return */
						}
						else {						/* This is a normal internal branch */
//							printf("******* %08X.%08X %08X.%08X %08X %08X\n", (uint32_t)(is->freturn >> 32), (uint32_t)is->freturn, (uint32_t)(is->btarg >> 32), (uint32_t)is->btarg, is->clevel, (is->mods & (modLR | modCTR)));
							if(is->mods & (modLR | modCTR)) {	/* Is this a return from an internal function call? */
								if(is->clevel && (is->freturn == is->btarg)) {	/* Are we returning from an internal function call? */
									return;			/* Exit stacked function call */
								}
							}

							rg->pc = is->btarg;		/* Set new branch target */
							if(trace) printf("                                 (Unconditional branch to %08X.%08X)\n", (uint32_t)(rg->pc >> 32), (uint32_t)rg->pc);
						}
					}
				}
				break;

// *******************************
				
			case isWrite:

				dumpdata[0] = 0;

				if(disbuf) sprintf(traktrak, "%02X %02X %02X %02X %02X %02X = %02X", is->trakExplicit, is->trakTarget, is->trakSourcea, 
					is->trakSourceb, is->trakSourcec, is->trakSourced, is->trakBtarg);

				if(!(is->mods & modUnimpl) && is->trakBtarg && !(is->trakBtarg & gTundef)) {	/* Do we have a valid memory address to go after? */

					treg = is->target;				/* Remember the target so we don't trash it */
					memaddr = is->btarg;			/* Remember target so we don't trash it */
					msz = is->memsize;				/* Get total bytes to write */

					if(is->mods & modM4) wsz = 4;	/* Write size is 4 bytes */
					else if(is->mods & modM8) wsz = 8;	/* Write size is 8 bytes */
					else wsz = is->memsize;			/* This is a normal size write */

					if(disbuf) sprintf(extra, "(%08X.%08X) -", (uint32_t)((memaddr & rg->addrmask) >> 32), (uint32_t)(memaddr & rg->addrmask));

					while(msz) {

						if(wsz > msz) wsz = msz;	/* Clamp to what's left */

						srcaddr = (uint8_t *)&rg->rgx[is->targtype - 1][treg];	/* Address of data to write */
						srcaddr = srcaddr + (8 - wsz);	/* Adjust source address by length of data */
						trak = rg->trx[is->targtype - 1][treg];	/* Set tracking */
						trak |= gTset;				/* Mark memory as set */

						writecore(rg, memaddr, wsz, rg->addrmask, 0, srcaddr, trak);	/* Dump a hunk */
	
						if(disbuf) {				/* Are we tracing or disassembling? */
							sprintf(dumpdata, "%s %02X/", dumpdata, trak);
							for(i = 0; i < wsz; i++) sprintf(dumpdata, "%s%02X", dumpdata, srcaddr[i]);
						}
						
						memaddr += wsz;				/* Advance target to next spot */
						msz = msz - wsz;			/* Reduce size by length written */
						treg++;						/* Step to next target (source actually) register */
						
					}
				}

				if(disbuf) {						/* If tracing or disassembling */
					sprintf((char *)&disbuf[offs].out[0], "%08X     %-12s%-20s %s %s%s", (uint32_t)instruction, is->op, is->oper, traktrak, extra, dumpdata);
					if(trace) printf("%08X.%08X %02X %s\n", (uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, isnflgs[offs], (char *)&disbuf[offs].out[0]);
				}

				if(is->mods & modUpd) {				/* Update mode? */
					rg->gprs[is->sourcea] = is->btarg;	/* Set the base register */
					rg->trakGpr[is->sourcea] = is->trakBtarg | gTset;	/* Set the validity */
					if(trace) printf("                                 (R%d set to %08X.%08X, trak = %02X)\n", is->sourcea, (uint32_t)(is->btarg >> 32), (uint32_t)is->btarg, is->trakBtarg);
				}

				rg->pc += 4;						/* Bump instruction counter */
				rg->pc &= rg->ifaddrmask;			/* Pin to addressing mode */
				if((rg->pc - is->fstart) >= is->fsize) isnflgs[offs] |= isnExit;	/* If we step off end next, indicate exit */
				break;

// *******************************
				
			case isRead:

				if(disbuf) sprintf(traktrak, "%02X %02X %02X %02X %02X %02X = %02X", is->trakExplicit, is->trakTarget, is->trakSourcea, 
					is->trakSourceb, is->trakSourcec, is->trakSourced, is->trakBtarg);

				if(!(is->mods & modUnimpl) && (is->targtype != rfNone) && !(is->trakBtarg & gTundef)) {	/* Do we have both a target and a valid memory address to go after? */

					treg = is->target;				/* Remember the target (source actually) so we don't trash it */
					memaddr = is->btarg;			/* Remember target (source actually) so we don't trash it */
					msz = is->memsize;				/* Get total bytes to fetch */

					if(is->mods & modM4) wsz = 4;	/* Fetch size is 4 bytes */
					else if(is->mods & modM8) wsz = 8;	/* Fetch size is 8 bytes */
					else wsz = is->memsize;			/* This is a normal size fetch */

					if(disbuf) sprintf(extra, "(%08X.%08X) -", (uint32_t)((memaddr & rg->addrmask) >> 32), (uint32_t)(memaddr & rg->addrmask));

					while(msz) {

						if(wsz > msz) wsz = msz;	/* Clamp to what's left */

						srcaddr = (uint8_t *)&rg->rgx[is->targtype - 1][treg];	/* Address of data to read */
						srcaddr = srcaddr + (8 - wsz);	/* Adjust source address by length of data */

						readcore(rg, memaddr, wsz, rg->addrmask, 0, &is->result, &is->trakResult);	/* Fetch the data */

						if(is->mods & modSxtnd) {	/* Do we need to fill in the sign? */
							signmask = 0xFFFFFFFFFFFFFFFFULL << ((is->memsize * 8) - 1);	/* Make a mask with the sign bit and all to the left set */
							if(is->result & signmask) is->result |= signmask;	/* If any of those bits are on, turn them all on */
						}

						rg->rgx[is->targtype - 1][treg] = is->result;	/* Commit the fetch */
						if((is->targtype == rfGpr) && ((is->result - is->fstart) < is->fsize)) is->trakResult |= gTinfnc;	/* Remember if GPR value matches an address in the function */
						trak = rg->trx[is->targtype - 1][treg] = is->trakResult;	/* Set tracking */
	
						if(disbuf) {				/* Are we tracing or disassembling? */
							sprintf(dumpdata, "%s %02X/", dumpdata, trak);
							for(i = 0; i < wsz; i++) sprintf(dumpdata, "%s%02X", dumpdata, srcaddr[i]);
						}
						
						memaddr += wsz;				/* Advance target to next spot */
						msz = msz - wsz;			/* Reduce size by length written */
						treg++;						/* Step to next target (source actually) register */
						
					}
				}

				if(disbuf) {						/* If tracing or disassembling */
					sprintf((char *)&disbuf[offs].out[0], "%08X     %-12s%-20s %s %s%s", (uint32_t)instruction, is->op, is->oper, traktrak, extra, dumpdata);
					if(trace) printf("%08X.%08X %02X %s\n", (uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, isnflgs[offs], (char *)&disbuf[offs].out[0]);
				}

				if(is->mods & modUpd) {				/* Update mode? */
					rg->gprs[is->sourcea] = is->btarg;	/* Set the base register */
					rg->trakGpr[is->sourcea] = is->trakBtarg | gTset;	/* Set the validity */
					if(trace) printf("                                 (R%d set to %08X.%08X, trak = %02X)\n", is->sourcea, (uint32_t)(is->btarg >> 32), (uint32_t)is->btarg, is->trakBtarg);
				}

				rg->pc += 4;						/* Bump instruction counter */
				rg->pc &= rg->ifaddrmask;			/* Pin to addressing mode */
				if((rg->pc - is->fstart) >= is->fsize) isnflgs[offs] |= isnExit;	/* If we step off end next, indicate exit */
				
				if((is->mods & modUnimpl) || (is->targtype != rfGpr)) break;	/* If this instruction is not a GPR or implemented, exit now */
				if(!(is->trakBtarg & gTbrtbl)) break;	/* Exit if not a possible branch table */
				
				btstart = is->btarg;				/* Assume this starts a branch table */
				
				while(!((is->trakBtarg | is->trakResult) & gTundef)) {	/* Check for branch table so long as these are defined */								

					xoffs = ((btstart + is->result) - is->fstart);	/* Get offset into routine */

					if(!is->result) break;			/* This can't be a branch table entry if it is zero... */
					if((is->result & 3) != 0) break;	/* Branch must be 4-byte aligned */
					if(xoffs >= is->fsize) break;	/* Are we outside the function? */

					isnflgs[(is->btarg - is->fstart) / 4] = isBranchTbl;	/* Mark this as a branch table entry */

					level++;						/* Bump to the next level */
					if(trace) printf("                                 (Branch table entry at %08X.%08X [%08X.%08X], branch to %08X.%08X, lvl=%d)\n", 
						(uint32_t)(is->btarg >> 32), (uint32_t)is->btarg, 
						(uint32_t)(is->result >> 32), (uint32_t)is->result, 
						(uint32_t)((btstart + is->result) >> 32), (uint32_t)(btstart + is->result), 
						level);
					ist = getistate(is);			/* Allocate a brand new instruction state */
					rgt = getregfile(rg);			/* Allocate a brand new register file */

					rgt->pc = (btstart + is->result) & rg->ifaddrmask;	/* Set branch table target */
					
					isnflgs[(rgt->pc - is->fstart) / 4] |= isnBTarg;	/* Mark table target as branch target */

					if(disbuf) {					/* Tracing or disassembling? */					
						sprintf(extra, "0x%08X", (uint32_t)is->result);	/* Format the branch table offset */
						sprintf(dumpdata, "---> %08X.%08X", (uint32_t)(rgt->pc >> 32), (uint32_t)rgt->pc);	/* Format the branch table offset */
						sprintf((char *)&disbuf[(is->btarg - is->fstart) / 4].out[0], 
							"%08X     %-12s%-20s %s", (uint32_t)is->result, ".long", extra, dumpdata);
					}
					
					scancode(ist, rgt, isnflgs);	/* Take the conditional branch */

					tossistate(ist);				/* Toss instruction decode/execution structure */
					tossregfile(rgt);				/* Toss register file structure */
					level--;						/* Decrement the level */
					if(trace) printf("                                 (Return from branch table entry at %08X.%08X, lvl=%d\n", 
						(uint32_t)(is->btarg >> 32), (uint32_t)is->btarg, level);

					is->btarg = is->btarg + 4;		/* Set to next entry */
					if((is->btarg - is->fstart) >= is->fsize) break;	/* If we step off end, table is finished... */

					readcore(rg, is->btarg, 4, rg->ifaddrmask, 0, &is->result, &is->trakResult);	/* Fetch the next branch table entry */
				}

				btstart = 0;						/* Now out of branch table */
				break;

// *******************************
				
			default:

				if(disbuf) {						/* If tracing or disassembling */
					sprintf((char *)&disbuf[offs].out[0], "%08X     %-12s%-20s", (uint32_t)instruction, is->op, is->oper);
					if(trace) printf("%08X.%08X %02X %s\n", (uint32_t)(rg->pc >> 32), (uint32_t)rg->pc, isnflgs[offs], (char *)&disbuf[offs].out[0]);
				}

				complete(is, rg);					/* Complete the instruction */
				rg->pc += 4;						/* Bump instruction counter */
				rg->pc &= rg->ifaddrmask;			/* Pin to addressing mode */
				if((rg->pc - is->fstart) >= is->fsize) isnflgs[offs] |= isnExit;	/* If we step off end next, indicate exit */
				break;
		}


	}

	printf("\n");
	return;

}

void complete(istate *is, regfile *rg) {

	if(is->targtype == rfNone) return;				/* Leave if no target... */
	if(is->target == -1) return;					/* Leave if unsupported register */

	if(is->targtype > rfSpr) diedie("bad targtype");	/* (TEST/DEBUG) Check for valid target type */
	if(is->target > 32) diedie("bad target");		/* (TEST/DEBUG) Check for maximum target value */

	if(is->targtype != rfVpr) {						/* If not VMX, do standard way */
		((uint64_t *)(rg->rgx[is->targtype - 1]))[is->target] = is->result;	/* Set the result */
		((uint8_t *)(rg->trx[is->targtype - 1]))[is->target] = is->trakResult;	/* Set the result */
	}
	else {
		rg->vprs[is->target].vw[0] = ((vrs *)&is->result)->vw[0];	/* Set VMX register */
		rg->vprs[is->target].vw[1] = ((vrs *)&is->result)->vw[1];	/* Set VMX register */
		rg->vprs[is->target].vw[2] = ((vrs *)&is->result)->vw[2];	/* Set VMX register */
		rg->vprs[is->target].vw[3] = ((vrs *)&is->result)->vw[3];	/* Set VMX register */
		rg->trakVpr[is->target] = is->trakResult;	/* Remember tracking */
	}
	checkrg(rg);									/* (TEST/DEBUG) */
}

void checkrg(regfile *rg) {

	corebk *xcore;
	int i;

	while(rg) {

		if(rg->rgx[rfGpr - 1] != &rg->gprs[0]) diedie("trashed gpr xlate");
		if(rg->rgx[rfFpr - 1] != &rg->fprs[0]) diedie("trashed fpr xlate");
		if(rg->rgx[rfVpr - 1] != (uint64_t *)&rg->vprs[0]) diedie("trashed vpr xlate");
		if(rg->rgx[rfSpr - 1] != &rg->sprs[0]) diedie("trashed spr xlate");

		if(rg->trx[rfGpr - 1] != &rg->trakGpr[0]) diedie("trashed gpr tracking xlate");
		if(rg->trx[rfFpr - 1] != &rg->trakFpr[0]) diedie("trashed fpr tracking xlate");
		if(rg->trx[rfVpr - 1] != &rg->trakVpr[0]) diedie("trashed vpr tracking xlate");
		if(rg->trx[rfSpr - 1] != &rg->trakSpr[0]) diedie("trashed spr tracking xlate");
		
		rg = rg->rgBack;
	}
	
	xcore = corehd;
	
	while(xcore) {
		for(i = 0; i < sizeof(corebk); i++) {
			if((i >= 8) && (i < 12)) continue;
			if(((char *)xcore)[i] != 0x55) {
				diedie("free core overlay");
				break;
			}
		}
		xcore = xcore->core;
	}
}
	
