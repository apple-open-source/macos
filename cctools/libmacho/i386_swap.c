/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifdef SHLIB
#undef environ
#endif

#import <mach-o/i386/swap.h>

void
swap_i386_thread_state(
i386_thread_state_t *cpu,
enum NXByteOrder target_byte_sex)
{
	cpu->eax = NXSwapLong(cpu->eax);
	cpu->ebx = NXSwapLong(cpu->ebx);
	cpu->ecx = NXSwapLong(cpu->ecx);
	cpu->edx = NXSwapLong(cpu->edx);
	cpu->edi = NXSwapLong(cpu->edi);
	cpu->esi = NXSwapLong(cpu->esi);
	cpu->ebp = NXSwapLong(cpu->ebp);
	cpu->esp = NXSwapLong(cpu->esp);
	cpu->ss = NXSwapLong(cpu->ss);
	cpu->eflags = NXSwapLong(cpu->eflags);
	cpu->eip = NXSwapLong(cpu->eip);
	cpu->cs = NXSwapLong(cpu->cs);
	cpu->ds = NXSwapLong(cpu->ds);
	cpu->es = NXSwapLong(cpu->es);
	cpu->fs = NXSwapLong(cpu->fs);
	cpu->gs = NXSwapLong(cpu->gs);
}

void
swap_i386_thread_fpstate(
i386_thread_fpstate_t *fpu,
enum NXByteOrder target_byte_sex)
{
    struct swapped_fp_control {
	union {
	    struct {
		unsigned short
			    :3,
		    /*inf*/ :1,
		    rc	    :2,
		    pc	    :2,
			    :2,
		    precis  :1,
		    undfl   :1,
		    ovrfl   :1,
		    zdiv    :1,
		    denorm  :1,
		    invalid :1;
	    } fields;
	    unsigned short half;
	} u;
    } sfpc;

    struct swapped_fp_status {
	union {
	    struct {
		unsigned short
		    busy    :1,
		    c3	    :1,
		    tos	    :3,
		    c2	    :1,
		    c1	    :1,
		    c0	    :1,
		    errsumm :1,
		    stkflt  :1,
		    precis  :1,
		    undfl   :1,
		    ovrfl   :1,
		    zdiv    :1,
		    denorm  :1,
		    invalid :1;
	    } fields;
	    unsigned short half;
	} u;
    } sfps;

    struct swapped_fp_tag {
	union {
	    struct {
		unsigned short
		    tag7 :2,
		    tag6 :2,
		    tag5 :2,
		    tag4 :2,
		    tag3 :2,
		    tag2 :2,
		    tag1 :2,
		    tag0 :2;
	    } fields;
	    unsigned short half;
	} u;
    } sfpt;

    struct swapped_fp_data_reg {
	unsigned short mant;
	unsigned short mant1 :16,
		       mant2 :16,
		       mant3 :16;
	union {
	    struct {
		unsigned short sign :1,
			       exp  :15;
	    } fields;
	    unsigned short half;
	} u;
    } sfpd;

    struct swapped_sel {
	union {
	    struct {
    		unsigned short
		    index :13,
		    ti    :1,
		    rpl   :2;
	    } fields;
	    unsigned short half;
	} u;
    } ss;

    enum NXByteOrder host_byte_sex;
    unsigned long i;

	host_byte_sex = NXHostByteOrder();

	fpu->environ.ip = NXSwapLong(fpu->environ.ip);
	fpu->environ.opcode = NXSwapShort(fpu->environ.opcode);
	fpu->environ.dp = NXSwapLong(fpu->environ.dp);

	if(target_byte_sex == host_byte_sex){
	    memcpy(&sfpc, &(fpu->environ.control),
		   sizeof(struct swapped_fp_control));
	    sfpc.u.half = NXSwapShort(sfpc.u.half);
	    fpu->environ.control.rc = sfpc.u.fields.rc;
	    fpu->environ.control.pc = sfpc.u.fields.pc;
	    fpu->environ.control.precis = sfpc.u.fields.precis;
	    fpu->environ.control.undfl = sfpc.u.fields.undfl;
	    fpu->environ.control.ovrfl = sfpc.u.fields.ovrfl;
	    fpu->environ.control.zdiv = sfpc.u.fields.zdiv;
	    fpu->environ.control.denorm = sfpc.u.fields.denorm;
	    fpu->environ.control.invalid = sfpc.u.fields.invalid;

	    memcpy(&sfps, &(fpu->environ.status),
		   sizeof(struct swapped_fp_status));
	    sfps.u.half = NXSwapShort(sfps.u.half);
	    fpu->environ.status.busy = sfps.u.fields.busy;
	    fpu->environ.status.c3 = sfps.u.fields.c3;
	    fpu->environ.status.tos = sfps.u.fields.tos;
	    fpu->environ.status.c2 = sfps.u.fields.c2;
	    fpu->environ.status.c1 = sfps.u.fields.c1;
	    fpu->environ.status.c0 = sfps.u.fields.c0;
	    fpu->environ.status.errsumm = sfps.u.fields.errsumm;
	    fpu->environ.status.stkflt = sfps.u.fields.stkflt;
	    fpu->environ.status.precis = sfps.u.fields.precis;
	    fpu->environ.status.undfl = sfps.u.fields.undfl;
	    fpu->environ.status.ovrfl = sfps.u.fields.ovrfl;
	    fpu->environ.status.zdiv = sfps.u.fields.zdiv;
	    fpu->environ.status.denorm = sfps.u.fields.denorm;
	    fpu->environ.status.invalid = sfps.u.fields.invalid;

	    memcpy(&sfpt, &(fpu->environ.tag),
		   sizeof(struct swapped_fp_tag));
	    sfpt.u.half = NXSwapShort(sfpt.u.half);
	    fpu->environ.tag.tag7 = sfpt.u.fields.tag7;
	    fpu->environ.tag.tag6 = sfpt.u.fields.tag6;
	    fpu->environ.tag.tag5 = sfpt.u.fields.tag5;
	    fpu->environ.tag.tag4 = sfpt.u.fields.tag4;
	    fpu->environ.tag.tag3 = sfpt.u.fields.tag3;
	    fpu->environ.tag.tag2 = sfpt.u.fields.tag2;
	    fpu->environ.tag.tag1 = sfpt.u.fields.tag1;
	    fpu->environ.tag.tag0 = sfpt.u.fields.tag0;

	    memcpy(&ss, &(fpu->environ.cs),
		   sizeof(struct swapped_sel));
	    ss.u.half = NXSwapShort(ss.u.half);
	    fpu->environ.cs.index = ss.u.fields.index;
	    fpu->environ.cs.ti = ss.u.fields.ti;
	    fpu->environ.cs.rpl = ss.u.fields.rpl;

	    memcpy(&ss, &(fpu->environ.ds),
		   sizeof(struct swapped_sel));
	    ss.u.half = NXSwapShort(ss.u.half);
	    fpu->environ.ds.index = ss.u.fields.index;
	    fpu->environ.ds.ti = ss.u.fields.ti;
	    fpu->environ.ds.rpl = ss.u.fields.rpl;
	
	    for(i = 0; i < 8; i++){
		memcpy(&sfpd, &(fpu->stack.ST[i]),
		       sizeof(struct swapped_fp_data_reg));
		fpu->stack.ST[i].mant = NXSwapShort(sfpd.mant);
		fpu->stack.ST[i].mant1 = NXSwapShort(sfpd.mant1);
		fpu->stack.ST[i].mant2 = NXSwapShort(sfpd.mant2);
		fpu->stack.ST[i].mant3 = NXSwapShort(sfpd.mant3);
		sfpd.u.half = NXSwapShort(sfpd.u.half);
		fpu->stack.ST[i].exp = sfpd.u.fields.exp;
		fpu->stack.ST[i].sign = sfpd.u.fields.sign;
	    }
	}
	else{
	    sfpc.u.fields.rc = fpu->environ.control.rc;
	    sfpc.u.fields.pc = fpu->environ.control.pc;
	    sfpc.u.fields.precis = fpu->environ.control.precis;
	    sfpc.u.fields.undfl = fpu->environ.control.undfl;
	    sfpc.u.fields.ovrfl = fpu->environ.control.ovrfl;
	    sfpc.u.fields.zdiv = fpu->environ.control.zdiv;
	    sfpc.u.fields.denorm = fpu->environ.control.denorm;
	    sfpc.u.fields.invalid = fpu->environ.control.invalid;
	    sfpc.u.half = NXSwapShort(sfpc.u.half);
	    memcpy(&(fpu->environ.control), &sfpc,
		   sizeof(struct swapped_fp_control));

	    sfps.u.fields.busy = fpu->environ.status.busy;
	    sfps.u.fields.c3 = fpu->environ.status.c3;
	    sfps.u.fields.tos = fpu->environ.status.tos;
	    sfps.u.fields.c2 = fpu->environ.status.c2;
	    sfps.u.fields.c1 = fpu->environ.status.c1;
	    sfps.u.fields.c0 = fpu->environ.status.c0;
	    sfps.u.fields.errsumm = fpu->environ.status.errsumm;
	    sfps.u.fields.stkflt = fpu->environ.status.stkflt;
	    sfps.u.fields.precis = fpu->environ.status.precis;
	    sfps.u.fields.undfl = fpu->environ.status.undfl;
	    sfps.u.fields.ovrfl = fpu->environ.status.ovrfl;
	    sfps.u.fields.zdiv = fpu->environ.status.zdiv;
	    sfps.u.fields.denorm = fpu->environ.status.denorm;
	    sfps.u.fields.invalid = fpu->environ.status.invalid;
	    sfps.u.half = NXSwapShort(sfps.u.half);
	    memcpy(&(fpu->environ.status), &sfps,
		   sizeof(struct swapped_fp_status));

	    sfpt.u.fields.tag7 = fpu->environ.tag.tag7;
	    sfpt.u.fields.tag6 = fpu->environ.tag.tag6;
	    sfpt.u.fields.tag5 = fpu->environ.tag.tag5;
	    sfpt.u.fields.tag4 = fpu->environ.tag.tag4;
	    sfpt.u.fields.tag3 = fpu->environ.tag.tag3;
	    sfpt.u.fields.tag2 = fpu->environ.tag.tag2;
	    sfpt.u.fields.tag1 = fpu->environ.tag.tag1;
	    sfpt.u.fields.tag0 = fpu->environ.tag.tag0;
	    sfpt.u.half = NXSwapShort(sfpt.u.half);
	    memcpy(&(fpu->environ.tag), &sfpt,
		   sizeof(struct swapped_fp_tag));

	    ss.u.fields.index = fpu->environ.cs.index;
	    ss.u.fields.ti = fpu->environ.cs.ti;
	    ss.u.fields.rpl = fpu->environ.cs.rpl;
	    ss.u.half = NXSwapShort(ss.u.half);
	    memcpy(&(fpu->environ.cs), &ss,
		   sizeof(struct swapped_sel));

	    ss.u.fields.index = fpu->environ.ds.index;
	    ss.u.fields.ti = fpu->environ.ds.ti;
	    ss.u.fields.rpl = fpu->environ.ds.rpl;
	    ss.u.half = NXSwapShort(ss.u.half);
	    memcpy(&(fpu->environ.cs), &ss,
		   sizeof(struct swapped_sel));

	    for(i = 0; i < 8; i++){
		sfpd.mant = NXSwapShort(fpu->stack.ST[i].mant);
		sfpd.mant1 = NXSwapShort(fpu->stack.ST[i].mant1);
		sfpd.mant2 = NXSwapShort(fpu->stack.ST[i].mant2);
		sfpd.mant3 = NXSwapShort(fpu->stack.ST[i].mant3);
		sfpd.u.fields.exp = fpu->stack.ST[i].exp;
		sfpd.u.fields.sign = fpu->stack.ST[i].sign;
		sfpd.u.half = NXSwapShort(sfpd.u.half);
		memcpy(&(fpu->stack.ST[i]), &sfpd,
		       sizeof(struct swapped_fp_data_reg));
	    }
	}
}

void
swap_i386_thread_exceptstate(
i386_thread_exceptstate_t *exc,
enum NXByteOrder target_byte_sex)
{
    struct swapped_err_code {
	union {
	    struct err_code_normal {
		unsigned int		:16,
				index	:13,
				tbl	:2,
				ext	:1;
	    } normal;
	    struct err_code_pgfault {
		unsigned int		:29,
				user	:1,
				wrtflt	:1,
				prot	:1;
	    } pgfault;
	    unsigned long word;
	} u;
    } sec;
    unsigned long word;
    enum NXByteOrder host_byte_sex;

	host_byte_sex = NXHostByteOrder();

	exc->trapno = NXSwapLong(exc->trapno);
	if(exc->trapno == 14){
	    if(target_byte_sex == host_byte_sex){
		memcpy(&sec, &(exc->err), sizeof(struct swapped_err_code));
		sec.u.word = NXSwapLong(sec.u.word);
		exc->err.pgfault.user   = sec.u.pgfault.user;
		exc->err.pgfault.wrtflt = sec.u.pgfault.wrtflt;
		exc->err.pgfault.prot   = sec.u.pgfault.prot;
	    }
	    else{
		sec.u.pgfault.prot   = exc->err.pgfault.prot;
		sec.u.pgfault.wrtflt = exc->err.pgfault.wrtflt;
		sec.u.pgfault.user   = exc->err.pgfault.user;
		sec.u.word = NXSwapLong(sec.u.word);
		memcpy(&(exc->err), &sec, sizeof(struct swapped_err_code));
	    }
	}
	else{
	    if(target_byte_sex == host_byte_sex){
		memcpy(&sec, &(exc->err), sizeof(struct swapped_err_code));
		sec.u.word = NXSwapLong(sec.u.word);
		word = sec.u.normal.index;
		exc->err.normal.index = NXSwapLong(word);
		exc->err.normal.tbl   = sec.u.normal.tbl;
		exc->err.normal.ext   = sec.u.normal.ext;
	    }
	    else{
		sec.u.normal.ext   = exc->err.normal.ext;
		sec.u.normal.tbl   = exc->err.normal.tbl;
		word = exc->err.normal.index;
		sec.u.normal.index = NXSwapLong(word);
		sec.u.word = NXSwapLong(sec.u.word);
		memcpy(&(exc->err), &sec, sizeof(struct swapped_err_code));
	    }
	}
}

void
swap_i386_thread_cthreadstate(
i386_thread_cthreadstate_t *user,
enum NXByteOrder target_byte_sex)
{
	user->self = NXSwapLong(user->self);
}
