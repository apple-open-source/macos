#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include "decode.h"
#include "core.h"
#include "misc.h"


int xadd(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->result = rg->gprs[is->sourcea] + rg->gprs[is->sourceb];	/* Calculate sum */
	is->trakResult = trackReg(is, 0, gTset, 0, rg->trakGpr[is->sourcea], rg->trakGpr[is->sourceb], 0, 0);	/* Calculate result validity */
	
	return 1;
}


int xaddc(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	uint64_t ca, p0, ps;
	uint32_t bcar;

	ca = rg->gprs[is->sourcea] & rg->gprs[is->sourceb];	/* Calculate both 32- and 64-bit definite carry */
	p0 = rg->gprs[is->sourcea] ^ rg->gprs[is->sourceb];	/* Predict both 32- and 64-bit high bits */
	ps = rg->gprs[is->sourcea] + rg->gprs[is->sourceb];	/* Calculate partial sum */
	ca = ca | (p0 & ~ps);						/* See if addition carried out of 32- and/or 64-bit high bits */
	bcar = ((ca >> 61) & statCA64) | ((ca >> 30) & statCA32);	/* Position both carry flags for status */
	is->status = (is->status & ~(statCA64 | statCA32)) | bcar;	/* Set the carry bits */

	is->result = ps;							/* Set the result of operation */
	is->trakResult = trackReg(is, 0,  gTset, 0, rg->trakGpr[is->sourcea], rg->trakGpr[is->sourceb], 0, 0);	/* Calculate result validity */

	return 1;
}

int xadde(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	uint64_t ca, p0, ps, cain;
	uint32_t bcar;

	cain = (((is->status >> 2) & is->status) | ((is->status >> 1) & ~is->status)) & 1;	/* Calculate address mode dependent carry in */
	
	ca = rg->gprs[is->sourcea] & rg->gprs[is->sourceb];	/* Calculate both 32- and 64-bit definite carry */
	p0 = rg->gprs[is->sourcea] ^ rg->gprs[is->sourceb];	/* Predict both 32- and 64-bit high bits */
	ps = rg->gprs[is->sourcea] + rg->gprs[is->sourceb];	/* Calculate partial sum */
	is->result = ps + cain;						/* Add carry in */
	ca = ca | (p0 & ~ps);						/* See if addition carried out of 32- and/or 64-bit high bits */
	bcar = ((ca >> 61) & statCA64) | ((ca >> 30) & statCA32);	/* Position both carry flags for status */
	is->status = (is->status & ~(statCA64 | statCA32)) | bcar;	/* Set the carry bits */
	
	is->result = ps;							/* Set the result of operation */
	is->trakResult = trackReg(is, 0, gTset, 0, rg->trakGpr[is->sourcea], rg->trakGpr[is->sourceb], 0, 0);	/* Calculate result validity */
	strcpy(is->op, instb->opcode);
	return 1;
}

int xaddi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->result = is->btarg;						/* Decode already did our figuring for us */
	is->trakResult = is->trakBtarg;				/* Set tracking */
	return 1;
}


int xaddic(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	uint64_t ca, p0, ps;
	uint32_t bcar;

	ca = rg->gprs[is->sourcea] & is->immediate;	/* Calculate both 32- and 64-bit definite carry */
	p0 = rg->gprs[is->sourcea] ^ is->immediate;	/* Predict both 32- and 64-bit high bits */
	ps = rg->gprs[is->sourcea] + is->immediate;	/* Calculate partial sum */

	ca = ca | (p0 & ~ps);						/* See if addition carried out of 32- and/or 64-bit high bits */
	bcar = ((ca >> 61) & statCA64) | ((ca >> 30) & statCA32);	/* Position both carry flags for status */
	is->status = (is->status & ~(statCA64 | statCA32)) | bcar;	/* Set the carry bits */

	is->result = ps;							/* Set the result of operation */
	is->trakResult = is->trakBtarg;				/* Set tracking */

	return 1;
}


int xaddicdot(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	uint64_t ca, p0, ps;
	uint32_t bcar;

	ca = rg->gprs[is->sourcea] & is->immediate;	/* Calculate both 32- and 64-bit definite carry */
	p0 = rg->gprs[is->sourcea] ^ is->immediate;	/* Predict both 32- and 64-bit high bits */
	ps = rg->gprs[is->sourcea] + is->immediate;	/* Calculate partial sum */

	ca = ca | (p0 & ~ps);						/* See if addition carried out of 32- and/or 64-bit high bits */
	bcar = ((ca >> 61) & statCA64) | ((ca >> 30) & statCA32);	/* Position both carry flags for status */
	is->status = (is->status & ~(statCA64 | statCA32)) | bcar;	/* Set the carry bits */

	is->result = ps;							/* Set the result of operation */
	is->trakResult = is->trakBtarg;	/* Set tracking */

	return 1;
}


int xaddis(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->result = (is->btarg - is->immediate) + (is->immediate << 16);	/* Adjust decoded value to shifted immediate */
	is->trakResult |= gTset;					/* Set tracking */

	return 1;
}


int xaddme(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	uint64_t ca, p0, ps, cain;
	uint32_t bcar;

	cain = (((is->status >> 2) & is->status) | ((is->status >> 1) & ~is->status)) & 1;	/* Calculate address mode dependent carry in */
	cain = cain - 1;							/* Subtract one from the carry */

	ca = rg->gprs[is->sourcea] & cain;			/* Calculate both 32- and 64-bit definite carry */
	p0 = rg->gprs[is->sourcea] ^ cain;			/* Predict both 32- and 64-bit high bits */
	ps = rg->gprs[is->sourcea] + cain;			/* Calculate partial sum */

	ca = ca | (p0 & ~ps);						/* See if addition carried out of 32- and/or 64-bit high bits */
	bcar = ((ca >> 61) & statCA64) | ((ca >> 30) & statCA32);	/* Position both carry flags for status */
	is->status = (is->status & ~(statCA64 | statCA32)) | bcar;	/* Set the carry bits */

	is->result = ps + bcar;						/* Add carry in */
	is->trakResult = trackReg(is, 0, gTset, 0, rg->trakGpr[is->sourcea], 0, 0, 0);	/* Calculate result validity */
	
	return 1;
}


int xaddze(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	uint64_t ca, ps, cain;
	uint32_t bcar;

	cain = (((is->status >> 2) & is->status) | ((is->status >> 1) & ~is->status)) & 1;	/* Calculate address mode dependent carry in */

	ps = rg->gprs[is->sourcea] + cain;			/* Calculate sum */
	ca = rg->gprs[is->sourcea] & ~ps;			/* See if addition carried out of 32- and/or 64-bit high bits */
	bcar = ((ca >> 61) & statCA64) | ((ca >> 30) & statCA32);	/* Position both carry flags for status */
	is->status = (is->status & ~(statCA64 | statCA32)) | bcar;	/* Set the carry bits */

	is->result = ps;							/* Set the result of operation */
	is->trakResult = trackReg(is, 0, gTset, 0, rg->trakGpr[is->sourcea], 0, 0, 0);	/* Calculate result validity */

	return 1;
}

//
//
//


int xb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	int32_t bdi;

	bdi = inst & 0x03FFFFFC;
	if(0x02000000 & bdi) bdi = bdi | 0xFC000000;
	is->immediate = (int32_t)bdi;
	is->btarg = (int64_t)bdi;					/* Assume absolute branch for now */
	strcpy(is->op, instb->opcode);

	if(inst & 1) {
		strcat(is->op, "l");
		is->mods |= modSetLR;					/* Show that we need to set LR from PC */
	}

	is->trakBtarg = trackReg(is, 0, gTset, 0, gTgen, 0, 0, 0);	/* Set result validity */
	
	if(inst & 2) {
		strcat(is->op, "a");
		is->mods |= modBrAbs;					/* Show that immediate is absolute address */
		sprintf(is->oper, "0x%X",  (uint32_t)is->immediate);
		return 1;
	}
	
	is->btarg = is->btarg + addr;				/* If not absolute, calculate PC relative address */
	
	sprintf(is->oper, ".+0x%X",  (uint32_t)is->immediate);
	return 1;
}


int xbc(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	short bd;
	char xoper[64];
	int64_t bdi;

	is->target = (inst >> (31 - 10)) & 0x1F;
	if((is->target & 0x14) != 0x14) is->opr = isBrCond;		/* If not "branch always," set conditional branch */
	else is->opr = isBranch;					/* Otherwise it is unconditional */

	is->sourcea = (inst >> (31 - 15)) & 0x1F;
	bd = inst & 0xFFFC;
	is->immediate = inst & 0xFFFC;
	bdi = bd;

	is->btarg = bdi;							/* Assume absolute branch for now */

	if(inst & 1) {
		is->mods |= modSetLR;					/* Show that we need to set LR from PC */
	}

	is->trakBtarg = gTset;

	if(inst & 2) {
		is->mods |= modBrAbs;					/* Show that immediate is absolute address */
		is->trakBtarg |= gTgen;					/* Show generated */
		genbr("", is->target, is->sourcea, inst, bd, is->op, xoper);
		sprintf(is->oper, "%s0x%X",  xoper, bd);
	}

	is->btarg = is->btarg + addr;				/* If not absolute, calculate PC relative address */

	is->trakBtarg = trackReg(is, 0, gTset, 0, gTgen, 0, 0, 0);	/* Set result validity */

	genbr("", is->target, is->sourcea, inst, bd, is->op, xoper);
	sprintf(is->oper, "%s.+0x%X",  xoper, bd);
	
	return 1;
}


int xbclr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->target = (inst >> (31 - 10)) & 0x1F;	/* Get the BO field */

	if((is->target & 0x14) != 0x14) is->opr = isBrCond;		/* If not "branch always," set conditional branch */
	else is->opr = isBranch;					/* Otherwise it is unconditional */
	
	is->sourcea = (inst >> (31 - 15)) & 0x1F;	/* Get the BI field */

	is->btarg = rg->sprs[sPlr];					/* Set target to the LR */
	is->trakBtarg = trackReg(is, 0, gTset, 0, rg->trakSpr[sPlr], 0, 0, 0);	/* Target validity tracks link register */

	if(inst & 1) {
		is->mods |= modSetLR;					/* Show that we need to set LR from PC */
	}

	genbr("lr", is->target, is->sourcea, inst, 0, is->op, is->oper);	/* Format it */
	return 1;
}


int xbctr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->target = (inst >> (31 - 10)) & 0x1F;	/* Get the BO field */
	is->sourcea = (inst >> (31 - 15)) & 0x1F;	/* Get the BI field */

	is->btarg = rg->sprs[sPctr];				/* Set target to the CTR */
	is->trakBtarg = trackReg(is, 0, gTset, 0, rg->trakSpr[sPctr], 0, 0, 0);	/* Target validity tracks count register */

	if(inst & 1) {
		is->mods |= modSetLR;					/* Show that we need to set LR from PC */
	}

	genbr("ctr", is->target, is->sourcea, inst, 0, is->op, is->oper);	/* Format it */
	return 1;
}


//
//
//

int xicbi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int ra, rb;

	ra = (inst >> (31 - 15)) & 0x1F;
	rb = (inst >> (31 - 20)) & 0x1F;
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "r%d,r%d",  ra, rb);
	return 1;
}


int xisync(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	strcpy(is->op, instb->opcode);
	return 1;
}

//
//
//


int xlogi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	uint64_t imm;
	uint8_t	trak = gTset;

	imm = is->immediate;						/* Remember this */
	if(is->mods & modshft) imm = imm << 16;		/* Shift immediate if shifted form */
	
	switch(is->mods & modlop) {					/* Figure out what operation */

		case modand:
			is->result = rg->gprs[is->sourcea] & imm;	/* AND it */
			if(imm != 0) break;					/* Immediate is not zero, so we do not know result */

			trak = gTgen;						/* Set generated */
			break;

		case modor:
			is->result = rg->gprs[is->sourcea] | imm;	/* OR it */
			break;

		case modxor:
			is->result = rg->gprs[is->sourcea] ^ imm;	/* XOR it */
			break;
			
		default:
			printf("Invalid decode: inst = %08X\n", inst);
			exit(1);
			
	}
	
	is->trakResult = trackReg(is, 0, trak, 0, rg->trakGpr[is->sourcea], gTgen, 0, 0);	/* Calculate result validity */
	
	return 1;
}


int xlogx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	int samereg, genres;

	samereg = is->sourcea == is->sourceb;		/* Both sources same register? */
	genres = 0;									/* Assume we can't fingure out result with unknown sources */
	
	switch(is->mods & modlop) {					/* Figure out what operation */

		case modand:
			is->result = rg->gprs[is->sourcea] & rg->gprs[is->sourceb];	/* AND it */
			break;

		case modor:
			is->result = rg->gprs[is->sourcea] | rg->gprs[is->sourceb];	/* OR it */
			break;

		case modxor:
			is->result = rg->gprs[is->sourcea] ^ rg->gprs[is->sourceb];	/* XOR it */
			genres = samereg;					/* We can calculate result from unknown sources if the same */
			break;
			
		case modandc:
			is->result = rg->gprs[is->sourcea] & ~rg->gprs[is->sourceb];	/* ANDC it */
			genres = samereg;					/* We can calculate result from unknown sources if the same */
			break;

		case modeqv:
			is->result = ~(rg->gprs[is->sourcea] ^ rg->gprs[is->sourceb]);	/* EQV it */
			genres = samereg;					/* We can calculate result from unknown sources if the same */
			break;

		case modnand:
			is->result = ~(rg->gprs[is->sourcea] & rg->gprs[is->sourceb]);	/* NAND it */
			break;

		case modnor:
			is->result = ~(rg->gprs[is->sourcea] | rg->gprs[is->sourceb]);	/* NOR it */
			break;

		case modorc:
			is->result = rg->gprs[is->sourcea] | ~rg->gprs[is->sourceb];	/* ORC it */
			genres = samereg;					/* We can calculate result from unknown sources if the same */
			break;
			
		default:
			printf("Invalid decode: inst = %08X\n", inst);
			exit(1);
			
	}
	
	if(!genres) is->trakResult = trackReg(is, 0, gTset, 0, rg->trakGpr[is->sourcea], rg->trakGpr[is->sourceb], 0, 0);	/* Calculate result validity */
	else is->trakResult = trackReg(is, 0, gTset | gTgen, 0, rg->trakGpr[is->sourcea], rg->trakGpr[is->sourceb], 0, 0);
	
	return 1;
}

//                   SrcB  SrcC  SrcD
// rlwinm itypeM       sh    mb    me      rotl32
// rlwnm  itypeM       sh    mb    me      rotl32
// rlwimi itypeM       sh    mb    me      rotl32

// rldicl itypeMD      sh    mb    63
// rldicr itypeMD      sh     0    me
// rldic  itypeMD      sh    mb 63-sh
// rldimi itypeMD      sh    mb 63-sh

// rldcl  itypeMDS     sh    mb    63
// rldcr  itypeMDS     sh     0    me

// sld    itypeX       sh     0 63-sh
// slw    itypeX       sh     0 63-sh      rotl32
// srd    itypeX    64-sh    sh    63
// srw    itypeX    64-sh    sh    63      rotl32
// srawi  itypeX 
// srad   itypeX
// sraw   itypeX 

// sradi  itypeXS


int xrot(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	uint64_t mask, source;
	int bigshift;
	
	is->trakSourceb = gTnu;							/* Assume shift is hard coded */
	if(is->mods & modUseRB) {						/* Is shift in a register? */
		is->trakSourceb = rg->trakGpr[is->sourceb];	/* Yes, get register's tracking value */
		if((is->trakSourceb == gTnu) || (is->trakSourceb & gTundef)) {	/* Fail now if shift never set or undefined */
			is->trakResult = trackReg(is, 0, gTset | gTundef, 0, rg->trakGpr[is->sourcea], rg->trakGpr[is->sourceb], 0, 0);	/* Calculate result validity */
			return 1;								/* Exit like right now... */
		}
		is->sourceb = rg->gprs[is->sourceb];		/* Get the register contents */
	}
	
	if(is->mods & modROTL32) {						/* Is this ROTL32? */
		source =									/* Duplicate bottom 32 in top 32 */
			(rg->gprs[is->sourcea] << 32) | (rg->gprs[is->sourcea] & 0x00000000FFFFFFFFULL);
		bigshift = (is->sourceb >> 5) & 1;			/* Remember if shift is 32 - 63 */
		is->sourceb = is->sourceb & 0x1F;			/* Restrict to 31 bits only */
	}
	else {
		source = rg->gprs[is->sourcea];				/* Otherwise, use the full 64-bit register */
		bigshift = (is->sourceb >> 6) & 1;			/* Remember if shift is 64 - 127 */
		is->sourceb = is->sourceb & 0x3F;			/* Restrict to 63 bits only */
	}
	
	if(is->mods & modShift) {						/* Is this a shift? */
		if(bigshift) {								/* Was it in the forbidden range? */
			is->trakResult = gTset | gTgen;			/* Set result is always 0, so set generated and set */
			return 1;								/* Done... */
		}
			
		if(is->mods & modShRight) {					/* Is this a shift right? */
			is->sourcec = is->sourceb;				/* Yes, shift is really MS */
			is->sourceb = 64 - is->sourceb;			/* Change shift to rotate */
			is->sourced = 63;						/* Mask end is 63 */
		}
		else {
			is->sourcec = 0;						/* Mask start is always 0 */
			is->sourced = 63 - is->sourceb;			/* Mask end is 63 minus shift */
		}
		
		if(is->mods & modROTL32) is->sourcec += 32;	/* If ROTL32, adjust mask start */
	}
	
	if(is->mods & modZeroc) is->sourcec = 0;		/* Mask start is 0 */
	
	if(is->mods & mod63d) is->sourced = 63;			/* Mask end is 63 */
	
	if(is->mods & mod63minusd) {					/* Is mask end 63 - shift? */
		is->sourced = 63 - is->sourceb;				/* Set it */
		is->trakSourced = is->trakSourceb;			/* And propagate the tracking */
	}

	is->result = (source << is->sourceb) | (source >> (64 - is->sourceb));	/* Rotate the source */

	mask = (0xFFFFFFFFFFFFFFFFULL >> is->sourcec) & (0xFFFFFFFFFFFFFFFFULL << (63 - is->sourced));	/* Make the mask */
	if(is->sourcec > is->sourced) mask = ~mask;		/* If start is after end, invert the mask */

	is->result = is->result & mask;					/* Mask out the unwanted bits */
	
	is->trakTarget = gTnu;							/* Assume target tracking isn't applicable */
	if(is->mods & modInsert) {						/* Is this an insert operation? */
		is->result |= (rg->gprs[is->target] & ~mask);	/* Yes, move the bits from the target into result */
		is->trakTarget = rg->trakGpr[is->target];	/* And propagate the tracking */
	}
	
	is->trakResult = trackReg(is, 0, gTset, is->trakTarget, rg->trakGpr[is->sourcea], is->trakSourceb, 0, 0);	/* Set result tracking */

	return 1;
}

//
//
//

int xrfx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	strcpy(is->op, instb->opcode);
	return 1;
}


int xsc(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->immediate = (inst >> (31 - 26)) & 0x3F;
	strcpy(is->op, instb->opcode);
	if(is->immediate) sprintf(is->oper, "%lld", is->immediate);

	return 1;
}


int xtdi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "%d,r%d,0x%llX",  is->target, is->sourcea, is->immediate);
	return 1;
}



int xtwi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "%d,r%d,0x%llX",  is->target, is->sourcea, is->immediate);
	return 1;
}

int xmtmsrx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int l;

	l = (inst >> (31 - 15)) & 1;
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "r%d,%d",  is->target, l);
	is->target = (instb->dflags & dTrgnum) >> 8;
	return 1;
}

int xmw(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->memsize = (32 - is->target) * 4;		/* Claculate the number of bytes to access */

	return 1;
}


int xrt(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->trakResult = gTundef;					/* We are unsupported, so result is invalid */

	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "r%d",  is->target);
	is->target = (instb->dflags & dTrgnum) >> 8;
	return 1;
}

int xrtra(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->trakResult = gTundef;					/* We are unsupported, so result is invalid */

	strcpy(is->op, instb->opcode);
	if(is->mods & modSetCRF) strcat(is->op, ".");	/* Set condition register field? */
	sprintf(is->oper, "r%d,r%d",  is->target, is->sourcea);
	return 1;
}

int xrtrb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->trakResult = gTundef;					/* We are unsupported, so result is invalid */

	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "r%d,r%d",  is->target, is->sourceb);
	return 1;
}

int xftfb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "f%d,f%d",  is->target, is->sourceb);
	return 1;
}


//
//
//

int xext(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	uint64_t mask, sign;

	sign = 1 << ((is->memsize * 8) - 1);		/* Get mask for the the sign bit */
	mask = -sign;								/* Get a mask with the sign bit and all above set */
	sign = (rg->gprs[is->sourcea] & sign) ^ sign;	/* Isolate the sign bit and invert it */
	is->result = (rg->gprs[is->sourcea] | mask) + sign;	/* Filling the top with ones and adding the inverted sign would be a good idea */ 
	
	is->trakResult = trackReg(is, 0, gTset, 0, rg->trakGpr[is->sourcea], 0, 0, 0);	/* Calculate result validity */
	
	strcpy(is->op, instb->opcode);
	if(is->mods & modSetCRF) strcat(is->op, ".");	/* Set condition register field? */
	sprintf(is->oper, "r%d,r%d",  is->target, is->sourcea);

	return 1;
}

int xcmpi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	uint16_t xxx;

	is->target = is->target >> 2;				/* Extract just the CRF */

	if(inst & 0x00200000) strcpy(is->op, "cmpdi");
	else strcpy(is->op, "cmpwi");

	xxx = (uint16_t)is->immediate;

	sprintf(is->oper, "cr%d,r%d,0x%04X",  is->target, is->sourcea, xxx);
	return 1;
}


int xcmpli(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	uint16_t xxx;

	is->target = is->target >> 2;				/* Extract just the CRF */
	is->immediate = is->immediate & 0xFFFF;		/* Immediate value is unsigned */

	if(inst & 0x00200000) strcpy(is->op, "cmpldi");
	else strcpy(is->op, "cmplwi");

	xxx = (uint16_t)is->immediate;

	sprintf(is->oper, "cr%d,r%d,0x%04X",  is->target, is->sourcea, xxx);
	return 1;
}

//
//
//


int xmfcr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	int crf;

	is->sourcea = sPcr;							/* Source is from CR */
	is->targtype = rfGpr;						/* Target is a GPR */
	
	if(!(inst & 0x00100000)) {					/* Plain old mfcr? */
		crf = 0xFF;								/* Yup, it is all fields */
		strcpy(is->op, "mfcr");
		sprintf(is->oper, "r%d",  is->target);
	}
	else {
		crf = (inst >> (31 - 19)) & 0xFF;		/* New version uses selected fields */
		strcpy(is->op, "mfocrf");
		sprintf(is->oper, "r%d,0x%02X",  is->target, crf);
	}
	
	return 1;
}

int xmtcrf(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	int crf;

	is->target = sPcr;							/* Target is CR */
	is->targtype = rfSpr;						/* Target is an SPR */
	is->sourcea = (inst >> (31 - 10)) & 0x1F;	/* Get the source register */
	
	crf = (inst >> (31 - 19)) & 0xFF;			/* New version uses selected fields */

	if(!(inst & 0x00100000)) {					/* Plain old mfcr? */
		strcpy(is->op, "mtcrf");
	}
	else {
		strcpy(is->op, "mtocrf");
	}

	sprintf(is->oper, "0x%02X,r%d", crf, is->sourcea);
	return 1;
}


int xmfspr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	int spr;
	sprtb *sprp;

	spr = (inst >> (31 - 20)) & 0x3FF;			/* Get the source */
	spr = ((spr << 5) & 0x3E0) | ((spr >> 5) & 0x1F);	/* Flip to right order */
	is->sourcea = spr;
	
	sprp = decodeSPR(spr);						/* Look up the SPR */
	if(((uint32_t)sprp == 0) || (sprp->sprname[0] == 0)) {
		sprintf(is->oper, "r%d,%d", is->target, spr);
		is->trakSourcea = gTundef;				/* We don't know this register, so result is undefined */
	}
	else {
		if(sprp->sprflags & sprPriv) is->mods |= modPrv;	/* Mark as privileged */
		sprintf(is->oper, "r%d,%s", is->target, sprp->sprname);

		is->sourcea = sprp->sprint;				/* Convert to the internal code if any, otherwise -1 */
		if(is->sourcea == -1) {					/* Is this an unemulated SPR? */
			is->result = 0;						/* Yeah, set to zero */
			is->trakSourcea = gTundef;			/* Set to undefined */
		}
		else {									/* We do know this one */
			is->result = rg->sprs[sprp->sprint];	/* Grab the internal copy */
			is->trakSourcea = gTset | rg->trakSpr[sprp->sprint];	/* Set result to what we know */

			if(is->target == sPlr) {			/* Is this the LR? */
				is->mods |= modLRs;				/* Mark as LR */
			}
			else if(is->result == sPctr) {		/* Is the source the count register? */
				is->mods |= modCTRs;			/* Mark as CTR */
			}
		}
	}
	
	is->trakResult = trackReg(is, 0, gTset, rg->trakGpr[is->target], is->trakSourcea, 0, 0, 0);	/* Track it */

	return 1;
}

int xmfsr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	int sr;

	sr = (inst >> (31 - 15)) & 0x0F;
	is->trakResult = gTset | gTundef;
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "r%d,sr%d",  is->target, sr);
	return 1;
}

int xmtsr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int sr;

	sr = (inst >> (31 - 15)) & 0x0F;
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "sr%d,r%d",  sr, is->target);
	return 1;
}


int xmftb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int tbr;

	is->target = (inst >> (31 - 10)) & 0x1F;
	tbr = (inst >> (31 - 20)) & 0x3FF;
	tbr = ((tbr << 5) & 0x3E0) | ((tbr >> 5) & 0x1F);
	is->sourcea = tbr;

	if((tbr < 268) || (tbr > 269)) return 0;	/* This is not the right value */

	is->result = 0;
	is->trakResult = gTset | gTundef;			/* Instruction unsupported, so mark destination register value as undefined */

	strcpy(is->op, instb->opcode);
	if(tbr == 269) strcat(is->op, "u");
	sprintf(is->oper, "r%d",  is->target);

	return 1;
}

int xmtspr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int spr;
	sprtb *sprp;

	is->sourcea = is->target;					/* The register is the source here */
	is->result = rg->gprs[is->sourcea];			/* Get the source value */
	
	spr = (inst >> (31 - 20)) & 0x3FF;			/* Get the target */
	spr = ((spr << 5) & 0x3E0) | ((spr >> 5) & 0x1F);	/* Flip to right order */
	is->target = spr;							/* Save the raw SPR */
	sprp = decodeSPR(spr);						/* Look up the SPR */				
	if(((uint32_t)sprp == 0) || (sprp->sprname[0] == 0)) {
		sprintf(is->oper, "%d,r%d", is->target, is->sourcea);
		is->trakResult = trackReg(is, 0, gTundef | gTset, 0, rg->trakGpr[is->sourcea], 0, 0, 0);	/* Calculate result validity */
	}
	else {
		if(sprp->sprflags & sprPriv) is->mods |= modPrv;	/* Mark as privileged */
		sprintf(is->oper, "%s,r%d", sprp->sprname, is->sourcea);

		is->target = sprp->sprint;				/* Convert to the internal code if any, otherwise -1 */
		if(is->target == -1) {					/* Is this an unemulated SPR? */
			is->result = 0;						/* Yeah, set to zero */
			is->trakResult = trackReg(is, 0, gTundef | gTset, 0, rg->trakGpr[is->sourcea], 0, 0, 0);	/* Calculate result validity */
		}

		if(is->target == sPlr) {				/* Is the target the link register? */
			is->mods |= modLRs;					/* Mark as LR */
			is->trakResult = trackReg(is, 0, gTset | gTlr, 0, rg->trakGpr[is->sourcea], 0, 0, 0);	/* Calculate result validity */
		}	
		else if(is->target == sPctr) {			/* Is the target the count register? */
			is->mods |= modCTRs;				/* Mark as CTR */
			is->trakResult = trackReg(is, 0, gTset, 0, rg->trakGpr[is->sourcea], 0, 0, 0);	/* Calculate result validity */
		}
	}
	return 1;
}

int xmcrf(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	int crs, crd;

	is->target = sPcr;							/* Target is CR */
	is->targtype = rfSpr;						/* Target is an SPR */
	crd = (inst >> (31 - 8)) & 7;
	crs = (inst >> (31 - 13)) & 7;
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "cr%d,cr%d",  crd, crs);
	return 1;
}

int xattn(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->immediate = (inst >> (31 - 20)) & 0x7FFF;
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "0x%llX",  is->immediate);
	return 1;
}

int xsrawi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int sh;

	sh = (inst >> (31 - 20)) & 0x1F;
	strcpy(is->op, instb->opcode);
	if(is->mods & modSetCRF) {					/* Set condition register field? */
		strcat(is->op, ".");					/* Yes, set in string */
	}
	sprintf(is->oper, "r%d,r%d,%d",  is->target, is->sourcea, sh);

	is->trakResult = gTset| gTundef;			/* Set but undefined */
	return 1;
}

int xfcmpx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int bf;

	bf = (inst >> (31 - 8)) & 7;
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "cr%d,f%d,f%d",  bf, is->sourcea, is->sourceb);
	return 1;
}

int xmcrfs(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int bf, bfa;

	bf = (inst >> (31 - 8)) & 7;
	bfa = (inst >> (31 - 13)) & 7;
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "cr%d,%d",  bf, bfa);
	return 1;
}

int xmcrxr(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int bf;

	bf = (inst >> (31 - 8)) & 7;
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "cr%d",  bf);
	return 1;
}

int xmffs(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	strcpy(is->op, instb->opcode);
	if(is->mods & modSetCRF) strcat(is->op, ".");	/* Set the record bit if needed */
	sprintf(is->oper, "f%d",  is->target);
	is->sourcea = (instb->dflags & dTrgnum) >> 8;
	return 1;
}

int xmtfsfi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int bf, u;

	bf = (inst >> (31 - 8)) & 7;
	u = (inst >> (31 - 20)) & 0xF;
	strcpy(is->op, instb->opcode);
	if(is->mods & modSetCRF) strcat(is->op, ".");	/* Set the record bit if needed */
	sprintf(is->oper, "%d,%d",  bf, u);
	is->target = sPfpscr;
	is->targtype = rfSpr;
	return 1;
}





//
//
//


int xlswi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	if(!is->sourceb) is->sourceb = 32;
	is->memsize = is->sourceb;
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "r%d,r%d,%d",  is->target, is->sourcea, is->sourceb);
	is->trakResult = gTundef | gTset;			/* Results are undefined because we do not implement this instruction */
	return 1;
}


int xstswi(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	if(!is->sourceb) is->sourceb = 32;
	is->memsize = is->sourceb;
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "r%d,r%d,%d",  is->target, is->sourcea, is->sourceb);
	return 1;
}

int xcmp(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->target = is->target >> 2;				/* Extract just the CRF */

	if(inst & 0x00200000) strcpy(is->op, "cmpd");
	else strcpy(is->op, "cmpw");

	sprintf(is->oper, "cr%d,r%d,r%d",  is->target, is->sourcea, is->sourceb);
	return 1;
}


int xcmpl(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	is->target = is->target >> 2;				/* Extract just the CRF */

	if(inst & 0x00200000) strcpy(is->op, "cmpld");
	else strcpy(is->op, "cmplw");

	sprintf(is->oper, "cr%d,r%d,r%d",  is->target, is->sourcea, is->sourceb);
	return 1;
}

//
//
//



char *synctypes[] = {"sync", "lwsync", "ptesync", "badsync"};

int xsync(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int l;


	l = (inst >> (31 - 10)) & 3;
	if(l > 2) return 0;
	strcpy(is->op, synctypes[l]);
	return 1;
}


int xdss(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int strm;

	if(inst & 0x02000000) strcpy(is->op, "dssall");
	else {
		strm = (inst >> (31 - 10)) & 0x1F;
		strcpy(is->op, "dss");
		sprintf(is->oper, "%d", strm);
	}
	return 1;
}


int xdstx(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {
	int strm;

	strm = (inst >> (31 - 10)) & 3;

	strcpy(is->op, instb->opcode);
	if(inst & 0x02000000) strcat(is->op, "t");
	sprintf(is->oper, "r%d,r%d,%d",  is->sourcea, is->sourceb, strm);
	return 1;
}


int xtlbiex(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	int l;


	l = (inst >> (31 - 10)) & 1;

	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "r%d,%d",  is->sourceb, l);
	return 1;
}

int xtrap(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "%d,r%d,r%d",  is->target, is->sourcea, is->sourceb);
	return 1;
}

int xrb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "r%d",  is->sourceb);
	return 1;
}


int xrarb(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "r%d,r%d",  is->sourcea, is->sourceb);

	return 1;
}

int xoponly(uint32_t inst, ppcinst *instb, uint64_t addr, istate *is, regfile *rg) {

	strcpy(is->op, instb->opcode);

	return 1;
}



//
//
//

char *bops[] = {
	"bdnz",		// 00000
	"bdnz",		// 00001
	"bdz",		// 00010
	"bdz",		// 00011
	"b",		// 00100
	"b",		// 00101
	"b",		// 00110
	"b",		// 00111
	"bdnz",		// 01000
	"bdnz",		// 01001
	"bdz",		// 01010
	"bdz",		// 01011
	"b",		// 01100
	"b",		// 01101
	"b",		// 01110
	"b",		// 01111
	"bdnz",		// 10000
	"bdnz",		// 10001
	"bdz",		// 10010
	"bdz",		// 10011
	"b",		// 10100
	"b",		// 10101
	"b",		// 10110
	"b",		// 10111
	"bdnz",		// 11000
	"bdnz",		// 11001
	"bdz",		// 11010
	"bdz",		// 11011
	"b",		// 11100
	"b",		// 11101
	"b",		// 11110
	"b"			// 11111
};

char *conds[] = {
	"ge",
	"le",
	"ne",
	"ns",
	"lt",
	"gt",
	"eq",
	"so"
};
	
//
//
//

void genbr(char *src, int bo, int bi, uint32_t inst, int disp, char *op, char *oper) {
	int cr, cond;

	oper[0] = 0;								/* Clear out CR operand */
	cr = (bi >> 2) & 7;							/* Isolate the condition register field */
	cond = bi & 3;								/* Isolate the condition register field bit */
	strcpy(op, bops[bo]);						/* Copy in basic branch type */
	
	if(!(bo & 0x10)) {							/* Need condition register field? */
		strcat(op, conds[cond | ((bo >> 1) & 4)]);	/* Add condition */
	}
	
	strcat(op, src);							/* Stuff on the source of the target (CTR, LR, or none) */
	
	if(inst & 1) strcat(op, "l");				/* Add link designation */
	if(inst & 2) strcat(op, "a");				/* Add absolute designation */
	
	if((bo & 0x14) == 0x14) {					/* Are these the "branch always" guys? */
		if((bo != 0x14) || (bi != 31) || !(inst & 1)) return;	/* If not the special one, return... */
		strcpy(op,"bcl");						/* Set special op */
		strcpy(oper, "20,31,");					/* Set the special operand */
		return;
	}
	
/* Need hint bits here */

	if(cr) {									/* Only add CR if not CR0 */
		if(src[0] != 0) sprintf(oper, "cr%d", cr);	/* Stick the CR field in the operand, but no comma if to register */
		else sprintf(oper, "cr%d,", cr);		/* Stick the CR field in the operand, add comma */
	}
	return;
}

