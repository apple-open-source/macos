#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include "decode.h"
#include "core.h"
#include "misc.h"

#include "ppcinsts.h"
#include "sprs.h"

void decode(uint32_t inst, uint64_t pc, istate *is, regfile *rg) {
	int32_t majop, xop, ret;
	iType *it;
	
	ppcinst *ppci, *fop;
	
	is->opr = isNone;							/* Set bogus instruction */
	is->op[0] = 0;								/* Clear disassembly string */
	is->oper[0] = 0;							/* Clear disassembly string */

	majop = (inst >> 26) & 0x3F;				/* Pick off major op code */

	ppci = majops[majop];						/* Get the op code table */
	if(!(uint32_t)ppci) {						/* Is it defined? */
//		printf("Major op code %d not defined\n", majop);
		is->opr = isInvalid;					/* Show bogus instruuction */
		return;
	}


	fop = (ppcinst *)0;
	while(1) {									/* Search for the extended op */
		if(ppci->extended < 0) break;			/* Have we finished? */ 
		it = ppci->insttype;					/* Get instruction type */
		xop = (inst >> it->ishift) & it->imask;	/* Get the extended op code */
		if(ppci->extended == xop) {				/* Is this what we are looking for? */
			fop = ppci;							/* Remember we found it */
			break;
		}
		ppci++;									/* Step to the next one */
	}

	if(!(uint32_t)fop) {						/* Did we find it? */
//		printf("Extended op code not found, maj = %d, ext = %d\n", majop, xop);
		is->opr = isInvalid;					/* Show bogus instruuction */
		return;
	}

	is->opr = fop->opr;							/* Set the internal op code */
	is->memsize = fop->oprsize;					/* Set the default length */
	is->mods = fop->mods;						/* Set the default modification flags */
	is->targtype = fop->dflags & dTrgtyp;		/* Set the register file for target */
	
	is->trakBtarg   = gTnu;						/* Initialize tracking to never used */
	is->trakResult  = gTnu;						/* Initialize tracking to never used */
	is->trakExplicit = gTnu;					/* Initialize tracking to never used */
	is->trakTarget  = gTnu;						/* Initialize tracking to never used */
	is->trakSourcea = gTnu;						/* Initialize tracking to never used */
	is->trakSourceb = gTnu;						/* Initialize tracking to never used */
	is->trakSourcec = gTnu;						/* Initialize tracking to never used */
	is->trakSourced = gTnu;						/* Initialize tracking to never used */
	
	is->btarg = 0;
	is->result = 0;
	
	ret = (*fop->insttype->idecode)(inst, fop, is, rg);	/* Do instruction form pre-processing */
	if(!ret) {
		is->opr = isInvalid;					/* Show bogus instruction */
		return;
	}
	
	if((uint32_t)fop->xinst) {					/* Is there a specific instruction decode/emulator? */
		ret = (*fop->xinst)(inst, fop, pc, is, rg);	/* Call the decoder if it exists... */
		if(!ret) is->opr = isInvalid;			/* Show bogus instruction */
	}

	return;

}

//

int dcdFormA(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	uint8_t ex;

	is->target  = (inst >> (31 - 10)) & 0x1F;	/* Decode registers */
	is->sourcea = (inst >> (31 - 15)) & 0x1F;
	is->sourceb = (inst >> (31 - 20)) & 0x1F;
	is->sourcec = (inst >> (31 - 25)) & 0x1F;
	
	strcpy(is->op, instb->opcode);				/* Print the opcode */
	if(inst & 1) {								/* Set condition register field? */
		strcat(is->op, ".");					/* Yes, set in string */
		is->mods |= modSetCRF;					/* Tell emulation about it */
	}

	ex = gTset;									/* Show that target has been set */
	if(is->mods & modUnimpl) ex |= gTundef;		/* If not implemented, set to undefined */

	switch(is->mods &modRgVal) {				/* Which sub form? */

		case modFAtabc:
			is->trakResult = trackReg(is, 0, ex, rg->trakFpr[is->target], rg->trakFpr[is->sourcea], rg->trakFpr[is->sourceb], rg->trakFpr[is->sourcec], 0);	/* Track register */
			sprintf(is->oper, "f%d,f%d,f%d,f%d",  is->target, is->sourcea, is->sourceb, is->sourcec);
			break;
			
		case modFAtab:
			is->trakResult = trackReg(is, 0, ex, rg->trakFpr[is->target], rg->trakFpr[is->sourcea], rg->trakFpr[is->sourceb], 0, 0);	/* Track register */
			sprintf(is->oper, "f%d,f%d,f%d",  is->target, is->sourcea, is->sourceb);
			break;
			
		case modFAtb:
			is->trakResult = trackReg(is, 0, ex, rg->trakFpr[is->target], 0, rg->trakFpr[is->sourceb], 0, 0);	/* Track register */
			sprintf(is->oper, "f%d,f%d",  is->target, is->sourceb);
			break;
			
		case modFAtac:
			is->trakResult = trackReg(is, 0, ex, rg->trakFpr[is->target], 0, 0, rg->trakFpr[is->sourcec], 0);	/* Track register */
			sprintf(is->oper, "f%d,f%d",  is->target, is->sourcec);
			break;
			
		default:
			return 0;
			break;
		
	}

	return 1;
}
		
int dcdFormB(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	return 1;									/* This is the BC instruction. */

}


//
//	Note that for the D-Forms, we assume that R0 for source A follows addressing rules,
//	i.e., if RA = R0 then RA is assumed to contain 0 and the immediate value is sign
//	extended.  Instructions with different rules must perform their own decode.
//
		
int dcdFormD(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	uint8_t ex = 0;

	if(instb->dflags & dSkip) return 1;			/* Leave if no decode processing... */

	if(instb->dflags & dSwapTA) {				/* Are the RT and RA fields swapped? */
		is->sourcea = (inst >> (31 - 10)) & 0x1F;	/* Set target register */
		is->target = (inst >> (31 - 15)) & 0x1F;	/* Set sourcea register */
	}
	else {										/* Not swapped */
		is->target = (inst >> (31 - 10)) & 0x1F;	/* Set target register */
		is->sourcea = (inst >> (31 - 15)) & 0x1F;	/* Set sourcea register */
	}
	
	is->immediate = inst & 0xFFFF;				/* Set the immediate or displacement value */
	if((!(instb->dflags & dImmUn)) && (is->immediate & 0x8000)) is->immediate |= 0xFFFFFFFFFFFF0000ULL;	/* Sign extend */
	
	if(is->sourcea) {							/* Is RA 0? */
		is->btarg = rg->gprs[is->sourcea] + is->immediate;	/* Calculate target */
		is->trakSourcea = rg->trakGpr[is->sourcea];	/* Set byte target validity */
	}
	else {
		is->btarg = is->immediate;				/* Byte target is immediate value */
		is->trakSourcea = gTgen | gTset;		/* Byte target is fully generated */
	}

	if(!(instb->dflags & dNoFmt)) {				/* Should we format the output string? */
		strcpy(is->op, instb->opcode);
		if(instb->dflags & dBd) {				/* Base/displacement format? */
			if(is->mods & modTFpu) sprintf(is->oper, "f%d,0x%04X(r%d)",  is->target, (uint16_t)is->immediate, is->sourcea);
			else sprintf(is->oper, "r%d,0x%X(r%d)",  is->target, (uint16_t)is->immediate, is->sourcea);
		}
		else {
			if(is->mods & modTFpu) sprintf(is->oper, "f%d,r%d,0x%04X",  is->target, is->sourcea, (uint16_t)is->immediate);
			else sprintf(is->oper, "r%d,r%d,0x%04X",  is->target, is->sourcea, (uint16_t)is->immediate);
		}
	}

	if(!(is->mods & modTnomod)) {				/* Does this modify the target? */
		ex = gTset;								/* Yes, show that it has been set */
		if(is->mods & modUnimpl) ex |= gTundef;	/* If unimplmented, we know not the result */
	}
	
	is->trakResult = trackReg(is, 0, ex, rg->trakGpr[is->target], is->trakSourcea, gTgen | gTset, 0, 0);	/* Track result and target */
	is->trakBtarg = is->trakResult | gTset;		/* Always mark btarg as set */
	return 1;

}

//
//
//
		
int dcdFormDS(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	uint8_t ex = 0;

	if(instb->dflags & dSkip) return 1;			/* Leave if no decode processing... */

	is->target = (inst >> (31 - 10)) & 0x1F;	/* Set target register */
	is->sourcea = (inst >> (31 - 15)) & 0x1F;	/* Set base register */

	is->immediate = inst & 0xFFFC;				/* Set the immediate or displacement value */
	if((!(instb->dflags & dImmUn)) && (is->immediate & 0x8000)) is->immediate |= 0xFFFFFFFFFFFF0000ULL;	/* Sign extend */
	
	if(is->sourcea) {							/* Is RA 0? */
		is->btarg = rg->gprs[is->sourcea] + is->immediate;	/* Calculate target */
		is->trakSourcea = rg->trakGpr[is->sourcea];	/* Byte target follows RA validity */
	}
	else {
		is->btarg = is->immediate;				/* Byte target is immediate value */
		is->trakSourcea = gTgen;				/* Byte target is fully generated */
	}

	if(!(instb->dflags & dNoFmt)) {				/* Should we format the output string? */
		strcpy(is->op, instb->opcode);
		sprintf(is->oper, "r%d,0x%04X(r%d)",  is->target, (uint16_t)is->immediate, is->sourcea);
	}

	if(!(is->mods & modTnomod)) {				/* Does this modify the target? */
		ex = gTset;								/* Yes, show that it has been set */
		if(is->mods & modUnimpl) ex |= gTundef;	/* If unimplmented, we know not the result */
	}
	
	is->trakResult = trackReg(is, 0, ex, rg->trakGpr[is->target], is->trakSourcea, gTgen | gTset, 0, 0);	/* Track result and target */
	is->trakBtarg = is->trakResult | gTset;		/* Always mark btarg as set */
	return 1;
}
		
int dcdFormI(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	return 1;

}
		
int dcdFormM(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	uint8_t ex = 0;
	
	is->target = (inst >> (31 - 15)) & 0x1F;
	is->sourcea = (inst >> (31 - 10)) & 0x1F;
	is->sourceb = (inst >> (31 - 20)) & 0x1F;
	is->sourcec = (inst >> (31 - 25)) & 0x1F;
	is->sourced = (inst >> (31 - 30)) & 0x1F;

	strcpy(is->op, instb->opcode);
	if(inst & 1) {								/* Set condition register field? */
		strcat(is->op, ".");					/* Yes, set in string */
		is->mods |= modSetCRF;					/* Tell emulation about it */
	}
	
	if(!(is->mods & modUseRB)) {				/* Is RB in use? */
		is->trakSourceb = gTgen | gTset;		/* No, set immediate value to set and generated */
		sprintf(is->oper, "r%d,r%d,%d,%d,%d",  is->target, is->sourcea, is->sourceb, is->sourcec, is->sourced);
	}
	else {										/* sourceb is a register */
		is->trakSourceb = rg->trakGpr[is->sourceb];	/* Get tracking for sourceb */
		sprintf(is->oper, "r%d,r%d,r%d,%d,%d",  is->target, is->sourcea, is->sourceb, is->sourcec, is->sourced);
	}
	
	is->sourcec += 32;							/* Offset mask start by 32 */
	is->sourced += 32;							/* Offset mask end by 32 */

	if(!(is->mods & modTnomod)) {				/* Does this modify the target? */
		ex = gTset;								/* Yes, show that it has been set */
		if(is->mods & modUnimpl) ex |= gTundef;	/* If unimplmented, we know not the result */
	}

	is->trakResult = trackReg(is, 0, ex, rg->trakGpr[is->target], is->trakSourcea, is->trakSourceb, 0, 0);	/* Set result to set but undefined (execute will change this) */
	
	return 1;
}
		
int dcdFormMD(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	uint8_t ex = 0;

	is->target = (inst >> (31 - 15)) & 0x1F;
	is->sourcea = (inst >> (31 - 10)) & 0x1F;
	is->sourceb = ((inst >> (31 - 20)) & 0x1F) | ((inst << 4) & 0x20);
	is->sourcec = (inst >> (31 - 26)) & 0x3F;
	is->sourcec = (is->sourcec >> 1 | (is->sourcec << 5)) & 0x3F;	/* Unscramble the encoding */
	is->sourced = is->sourcec;					/* Duplicate because we won't know if this is MB or ME until execute */

	strcpy(is->op, instb->opcode);
	if(inst & 1) {								/* Set condition register field? */
		strcat(is->op, ".");					/* Yes, set in string */
		is->mods |= modSetCRF;					/* Tell emulation about it */
	}
	
	if(!(is->mods & modTnomod)) {				/* Does this modify the target? */
		ex = gTset;								/* Yes, show that it has been set */
		if(is->mods & modUnimpl) ex |= gTundef;	/* If unimplmented, we know not the result */
	}	

	is->trakResult = trackReg(is, 0, ex, rg->trakGpr[is->target], rg->trakGpr[is->sourcea], gTgen | gTset, gTgen | gTset, gTgen | gTset);	/* Set result to set but undefined (execute will change this) */

	sprintf(is->oper, "r%d,r%d,%d,%d",  is->target, is->sourcea, is->sourceb, is->sourcec);
	return 1;

}
		
int dcdFormMDS(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	uint8_t ex = 0;

	is->target = (inst >> (31 - 15)) & 0x1F;
	is->sourcea = (inst >> (31 - 10)) & 0x1F;
	is->sourceb = ((inst >> (31 - 20)) & 0x1F);
	is->sourcec = (inst >> (31 - 26)) & 0x3F;
	is->sourcec = (is->sourcec >> 1 | (is->sourcec << 5)) & 0x3F;	/* Unscramble the encoding */
	is->sourced = is->sourcec;					/* Duplicate because we won't know if this is MB or ME until execute */

	strcpy(is->op, instb->opcode);
	if(inst & 1) {								/* Set condition register field? */
		strcat(is->op, ".");					/* Yes, set in string */
		is->mods |= modSetCRF;					/* Tell emulation about it */
	}
	
	if(!(is->mods & modTnomod)) {				/* Does this modify the target? */
		ex = gTset;								/* Yes, show that it has been set */
		if(is->mods & modUnimpl) ex |= gTundef;	/* If unimplmented, we know not the result */
	}	

	is->trakResult = trackReg(is, 0, ex, rg->trakGpr[is->target], /* Set anticipated result */
		rg->trakGpr[is->sourcea], rg->trakGpr[is->sourceb], 
		gTgen | gTset, gTgen | gTset);

	sprintf(is->oper, "r%d,r%d,r%d,%d",  is->target, is->sourcea, is->sourceb, is->sourcec);
	return 1;
}
		
int dcdFormSC(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	return 1;

}
		
int dcdFormVA(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	uint8_t ex = 0;

	is->target = (inst >> (31 - 10)) & 0x1F;
	is->sourcea = (inst >> (31 - 15)) & 0x1F;
	is->sourceb = (inst >> (31 - 20)) & 0x1F;
	is->sourcec = (inst >> (31 - 25)) & 0x1F;

	if(!(is->mods & modTnomod)) {				/* Does this modify the target? */
		ex = gTset;								/* Yes, show that it has been set */
		if(is->mods & modUnimpl) ex |= gTundef;	/* If unimplmented, we know not the result */
	}	

	is->trakResult = trackReg(is, 0, ex, rg->trakVpr[is->target], /* Set anticipated result */
		rg->trakVpr[is->sourcea], rg->trakVpr[is->sourceb], 
		rg->trakVpr[is->sourcec], 0);

	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "v%d,v%d,v%d,v%d",  is->target, is->sourcea, is->sourceb, is->sourcec);
	return 1;
}
		
int dcdFormVX(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	int simm;
	uint8_t ex = 0;

	strcpy(is->op, instb->opcode);

	is->trakTarget = 0;

	switch(is->mods & 0x0000E000) {			/* Check subform */

		case mod3op:
			is->target = (inst >> (31 - 10)) & 0x1F;
			is->sourcea = (inst >> (31 - 15)) & 0x1F;
			is->sourceb = (inst >> (31 - 20)) & 0x1F;
			is->trakTarget = rg->trakVpr[is->target];	/* Set tracking */
			is->trakSourcea = rg->trakVpr[is->sourcea];	/* Set tracking */
			is->trakSourceb = rg->trakVpr[is->sourceb];	/* Set tracking */
			sprintf(is->oper, "v%d,v%d,v%d",  is->target, is->sourcea, is->sourceb);
			break;
			
		case mod2op:
			is->target = (inst >> (31 - 10)) & 0x1F;
			is->sourcea = (inst >> (31 - 20)) & 0x1F;
			is->trakTarget = rg->trakVpr[is->target];	/* Set tracking */
			is->trakSourcea = rg->trakVpr[is->sourcea];	/* Set tracking */
			sprintf(is->oper, "v%d,v%d",  is->target, is->sourcea);
			break;
			
		case mod1opt:
			is->target = (inst >> (31 - 10)) & 0x1F;
			is->trakTarget = rg->trakVpr[is->target];	/* Set tracking */
			sprintf(is->oper, "v%d",  is->target);
			break;
			
		case mod1opb:
			is->sourcea = (inst >> (31 - 20)) & 0x1F;	/* Note that we rename sourceb to sourcea */
			is->trakSourcea = rg->trakVpr[is->sourcea];	/* Set tracking */
			sprintf(is->oper, "v%d",  is->sourcea);
			break;
			
		case moduim:
			is->target = (inst >> (31 - 10)) & 0x1F;
			is->immediate = (inst >> (31 - 15)) & 0x1F;
			is->sourcea = (inst >> (31 - 20)) & 0x1F;
			is->trakTarget = rg->trakVpr[is->target];	/* Set tracking */
			is->trakSourcea = rg->trakVpr[is->sourcea];	/* Set tracking */
			sprintf(is->oper, "v%d,v%d,%lld",  is->target, is->sourcea, is->immediate);
			break;
			
		case modsim:
			is->target = (inst >> (31 - 10)) & 0x1F;
			simm = (inst >> (31 - 15)) & 0x1F;
			if(simm & 0x10) {
				simm = simm | 0xFFFFFFF0;
				is->immediate = simm | 0xFFFFFFFFFFFFFFF0ULL;
			}
			sprintf(is->oper, "v%d,%d",  is->target, simm);
			break;
		
		default:
			printf("Form VX decode error: inst = %08X, mod = %08X\n", inst, is->mods);
			exit(1);
			break;
	}			

	if(!(is->mods & modTnomod)) {				/* Does this modify the target? */
		ex = gTset;								/* Show that it has been set */
		if(is->mods & modUnimpl) ex |= gTundef;	/* If unimplmented, we know not the result */
	}

	ex = trackReg(is, 0, ex, rg->trakVpr[is->target], /* Track register */
		rg->trakVpr[is->sourcea], rg->trakVpr[is->sourceb], 0, 0);
		
/*	Set the result to the new tracking or keep the old if we don't modify the target */
	is->trakResult = (is->mods & modTnomod) ? is->trakTarget : ex;	
	
	return 1;
}
		
int dcdFormVXR(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	uint8_t ex = 0;

	strcpy(is->op, instb->opcode);

	is->target = (inst >> (31 - 10)) & 0x1F;
	is->sourcea = (inst >> (31 - 15)) & 0x1F;
	is->sourceb = (inst >> (31 - 20)) & 0x1F;

	if(inst & 0x00000400) strcat(is->op, ".");

	if(!(is->mods & modTnomod)) {				/* Does this modify the target? */
		ex = gTset;								/* Show that it has been set */
		if(is->mods & modUnimpl) ex |= gTundef;	/* If unimplmented, we know not the result */
	}
	
	is->trakResult = trackReg(is, 0, ex, rg->trakVpr[is->target], rg->trakVpr[is->sourcea], rg->trakVpr[is->sourceb], 0, 0);	/* Track register */

	sprintf(is->oper, "v%d,v%d,v%d",  is->target, is->sourcea, is->sourceb);

	return 1;
}
		
int dcdFormX(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	uint8_t ex = 0;

	if(instb->dflags & dSkip) return 1;			/* Leave if no decode processing... */

	if(instb->dflags & dSwapTA) {				/* Are the RT and RA fields swapped? */
		is->sourcea = (inst >> (31 - 10)) & 0x1F;	/* Set target register */
		is->target = (inst >> (31 - 15)) & 0x1F;	/* Set sourcea register */
	}
	else {										/* Not swapped */
		is->sourcea = (inst >> (31 - 15)) & 0x1F;	/* Set sourcea register */
		is->target = (inst >> (31 - 10)) & 0x1F;	/* Set target register */
	}
	
	is->sourceb = (inst >> (31 - 20)) & 0x1F;

	if(is->sourcea) {							/* Is RA non-zero? */
		is->btarg = rg->gprs[is->sourcea] + rg->gprs[is->sourceb];	/* Calculate target */
		is->trakBtarg = trackReg(is, 0, gTset, 0, rg->trakGpr[is->sourcea], rg->trakGpr[is->sourceb], 0, 0);	/* Set byte target validity */
	}
	else {
		is->btarg = rg->gprs[is->sourceb];		/* Calculate target */
		is->trakBtarg = trackReg(is, 0, gTset, 0, gTgen, rg->trakGpr[is->sourceb], 0, 0);	/* Set byte target validity */
	}
	
	if(is->opr == isRead) {						/* Special case reads in order to handle branch tables */
		 if(is->sourcea && (rg->trakGpr[is->sourcea] & gTbrtbl)){	/* See if base register is a pointer to a branch table */
			is->btarg = rg->gprs[is->sourcea];	/* Yes, skip applying index */
			is->trakBtarg = trackReg(is, 0, gTset, 0, rg->trakGpr[is->sourcea], 0, 0, 0);	/* Set byte target validity */	
			if(trace) printf("                                 (Load from branch table, index register discarded)\n");
		}
		else if(rg->trakGpr[is->sourceb] & gTbrtbl) {	/* See if index register is a pointer to a branch table (base wasn't) */
			is->btarg = rg->gprs[is->sourceb];	/* Yes, Don't use the base */
			is->trakBtarg = trackReg(is, 0, gTset, 0, 0, rg->trakGpr[is->sourceb], 0, 0);	/* Set byte target validity */	
			if(trace) printf("                                 (Load from branch table, base register discarded)\n");
		}
	}
	
	if(inst & 1) is->mods |= modSetCRF;			/* Set condition register field? */

	if(!(is->mods & modTnomod)) {				/* Does this modify the target? */
		ex = gTset;								/* Show that it has been set */
		if(is->mods & modUnimpl) ex |= gTundef;	/* If unimplmented, we know not the result */
	}
	is->trakResult = trackRegNS(is, 0, ex, 0, is->trakBtarg ,0, 0, 0);	/* Track register but don't record components */

	if(!(instb->dflags & dNoFmt)) {				/* Should we format the output string? */
		strcpy(is->op, instb->opcode);
		if(is->mods & modSetCRF) strcat(is->op, ".");	/* Set condition register field? */
		if(is->mods & modTFpu) sprintf(is->oper, "f%d,r%d,r%d",  is->target, is->sourcea, is->sourceb);
		else if(is->mods & modTVec) sprintf(is->oper, "v%d,r%d,r%d",  is->target, is->sourcea, is->sourceb);
		else sprintf(is->oper, "r%d,r%d,r%d",  is->target, is->sourcea, is->sourceb);
	}
	
	return 1;
}
		
int dcdFormXFL(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	int ffld;
	
	ffld = (inst >> (31 - 14)) & 0xFF;
	is->target = sPfpscr;
	is->targtype = rfSpr;
	is->sourceb = (inst >> (31 - 20)) & 0x1F;

	strcpy(is->op, instb->opcode);
	if(inst & 1) {								/* Set condition register field? */
		strcat(is->op, ".");					/* Yes, set in string */
		is->mods |= modSetCRF;					/* Tell emulation about it */
	}
	sprintf(is->oper, "%d,f%d",  ffld, is->sourceb);
	return 1;
}
		
int dcdFormXFX(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	is->target = (inst >> (31 - 10)) & 0x1F;	/* Extract target or source */
	strcpy(is->op, instb->opcode);				/* Move in the opcode */
	
	is->trakResult = trackReg(is, 0, gTset | gTundef, 0, 0, 0, 0, 0);	/* We usually don't emulate, so remember set but undefined */

	return 1;
}
		
int dcdFormXL(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	is->target = (inst >> (31 - 10)) & 0x1F;
	is->sourcea = (inst >> (31 - 15)) & 0x1F;
	is->sourceb = (inst >> (31 - 20)) & 0x1F;
	if(instb->dflags & dSkip) return 1;			/* Do no more processing here */
	strcpy(is->op, instb->opcode);
	sprintf(is->oper, "cr%d,cr%d,cr%d",  is->target, is->sourcea, is->sourceb);
	return 1;
}
		
int dcdFormXO(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

	uint8_t ui = 0;

	is->target = (inst >> (31 - 10)) & 0x1F;
	is->sourcea = (inst >> (31 - 15)) & 0x1F;
	is->sourceb = (inst >> (31 - 20)) & 0x1F;

	strcpy(is->op, instb->opcode);
	if(inst & 0x00000400) {						/* Should we check overflow? */
		strcat(is->op, "o");					/* Display it */
		is->mods |= modOflow;					/* Remember we want it */
	}
	if(inst & 1) {								/* Set condition register field? */
		strcat(is->op, ".");					/* Yes, set in string */
		is->mods |= modSetCRF;					/* Tell emulation about it */
	}

	if(!(is->mods & modTnomod)) {				/* Does this modify the target? */
		ui = gTset;
		if(is->mods & modUnimpl) ui |= gTundef;	/* If unimplmented, we know not the result */
	}

	is->trakResult = trackReg(is, 0, ui, rg->trakGpr[is->target], rg->trakGpr[is->sourcea], rg->trakGpr[is->sourceb], 0, 0);

	sprintf(is->oper, "r%d,r%d,r%d",  is->target, is->sourcea, is->sourceb);
	return 1;
}
		
int dcdFormXS(uint32_t inst, struct ppcinst *instb, istate *is, regfile *rg) {

 	uint8_t ex;

	is->sourcea = (inst >> (31 - 10)) & 0x1F;	/* Get source */
	is->target = (inst >> (31 - 15)) & 0x1F;	/* Get target */
	is->sourceb = ((inst << 4) & 0x20) | ((inst >> (31 - 20)) & 0x1F);	/* Get shift value */

	if(!(is->mods & modTnomod)) {				/* Does this modify the target? */
		ex = gTset;
		if(is->mods & modUnimpl) ex |= gTundef;	/* If unimplmented, we know not the result */
		is->trakResult = trackReg(is, 0, ex, rg->trakGpr[is->target], rg->trakGpr[is->sourcea], 0, 0, 0);
	}
	
	strcpy(is->op, instb->opcode);
	if(inst & 1) strcat(is->op, ".");
	sprintf(is->oper, "r%d,r%d,%d",  is->target, is->sourcea, is->sourceb);
	return 1;
}	

sprtb *decodeSPR(uint32_t spr) {

	if(spr > sprmax) return 0;					/* Leave if this is too big */
	return &sprs[spr];							/* Point to it and then leave... */

}
