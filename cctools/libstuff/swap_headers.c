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
#include <mach-o/loader.h>
#import <mach/m68k/thread_status.h>
#import <mach/ppc/thread_status.h>
#import <mach/m88k/thread_status.h>
#import <mach/i860/thread_status.h>
#import <mach/i386/thread_status.h>
#import <mach/hppa/thread_status.h>
#import <mach/sparc/thread_status.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"
#include "stuff/errors.h"

/*
 * swap_object_headers() swaps the object file headers from the host byte sex
 * into the non-host byte sex.  It returns TRUE if it can and did swap the
 * headers else returns FALSE and does not touch the headers and prints an error
 * using the error() routine.
 */
__private_extern__
enum bool
swap_object_headers(
struct mach_header *mh,
struct load_command *load_commands)
{
    unsigned long i;
    enum byte_sex target_byte_sex;
    struct load_command *lc, l;
    struct segment_command *sg;
    struct section *s;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct symseg_command *ss;
    struct fvmlib_command *fl;
    struct thread_command *ut;
    struct ident_command *id;
    struct dylib_command *dl;
    struct sub_framework_command *sub;
    struct sub_umbrella_command *usub;
    struct sub_library_command *lsub;
    struct sub_client_command *csub;
    struct prebound_dylib_command *pbdylib;
    struct dylinker_command *dyld;
    struct routines_command *rc;
    struct twolevel_hints_command *hints;
    struct prebind_cksum_command *cs;
    unsigned long flavor, count, nflavor;
    char *p, *state;

	/*
	 * Make a pass through the load commands checking them to the level
	 * that they can be parsed and then swapped.
	 */
	for(i = 0, lc = load_commands; i < mh->ncmds; i++){
	    l = *lc;
	    /* check load command size for a multiple of sizeof(long) */
	    if(lc->cmdsize % sizeof(long) != 0){
		error("in swap_object_headers(): malformed load command %lu "
		      "(cmdsize not a multiple of sizeof(long))", i);
		return(FALSE);
	    }
	    /* check that load command does not extends past end of commands */
	    if((char *)lc + lc->cmdsize >
	       (char *)load_commands + mh->sizeofcmds){
		error("in swap_object_headers(): truncated or malformed load "
		      "command %lu (extends past the end of the all load "
		      "commands)", i);
		return(FALSE);
	    }
	    /* check that the load command size is not zero */
	    if(lc->cmdsize == 0){
		error("in swap_object_headers(): malformed load command %lu "
		      "(cmdsize is zero)", i);
		return(FALSE);
	    }
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		if(sg->cmdsize != sizeof(struct segment_command) +
				     sg->nsects * sizeof(struct section)){
		    error("in swap_object_headers(): malformed load command "
			  "(inconsistant cmdsize in LC_SEGMENT command %lu for "
			  "the number of sections)", i);
		    return(FALSE);
		}
		break;

	    case LC_SYMTAB:
		st = (struct symtab_command *)lc;
		if(st->cmdsize != sizeof(struct symtab_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_SYMTAB command %lu has incorrect cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_DYSYMTAB:
		dyst = (struct dysymtab_command *)lc;
		if(dyst->cmdsize != sizeof(struct dysymtab_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_DYSYMTAB command %lu has incorrect cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_SYMSEG:
		ss = (struct symseg_command *)lc;
		if(ss->cmdsize != sizeof(struct symseg_command)){
		    error("in swap_object_headers(): malformed load command "
			  "(LC_SYMSEG command %lu has incorrect cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_IDFVMLIB:
	    case LC_LOADFVMLIB:
		fl = (struct fvmlib_command *)lc;
		if(fl->cmdsize < sizeof(struct fvmlib_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(%s command %lu has too small cmdsize field)",
			  fl->cmd == LC_IDFVMLIB ? "LC_IDFVMLIB" :
			  "LC_LOADFVMLIB", i);
		    return(FALSE);
		}
		if(fl->fvmlib.name.offset >= fl->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (name.offset field of %s command %lu "
			  "extends past the end of all load commands)",
			  fl->cmd == LC_IDFVMLIB ? "LC_IDFVMLIB" :
			  "LC_LOADFVMLIB", i);
		    return(FALSE);
		}
		break;

	    case LC_ID_DYLIB:
	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
		dl = (struct dylib_command *)lc;
		if(dl->cmdsize < sizeof(struct dylib_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(%s command %lu has too small cmdsize field)",
			  dl->cmd == LC_ID_DYLIB ? "LC_ID_DYLIB" :
			  (dl->cmd == LC_LOAD_DYLIB ? "LC_LOAD_DYLIB" :
			  "LC_LOAD_WEAK_DYLIB"), i);
		    return(FALSE);
		}
		if(dl->dylib.name.offset >= dl->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (name.offset field of %s command %lu "
			  "extends past the end of all load commands)",
			  dl->cmd == LC_ID_DYLIB ? "LC_ID_DYLIB" :
			  (dl->cmd == LC_LOAD_DYLIB ? "LC_LOAD_DYLIB" :
			  "LC_LOAD_WEAK_DYLIB"), i);
		    return(FALSE);
		}
		break;

	    case LC_SUB_FRAMEWORK:
		sub = (struct sub_framework_command *)lc;
		if(sub->cmdsize < sizeof(struct sub_framework_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_SUB_FRAMEWORK command %lu has too small cmdsize "
			  "field)", i);
		    return(FALSE);
		}
		if(sub->umbrella.offset >= sub->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (umbrella.offset field of "
			  "LC_SUB_FRAMEWORK command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_SUB_UMBRELLA:
		usub = (struct sub_umbrella_command *)lc;
		if(usub->cmdsize < sizeof(struct sub_umbrella_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_SUB_UMBRELLA command %lu has too small cmdsize "
			  "field)", i);
		    return(FALSE);
		}
		if(usub->sub_umbrella.offset >= usub->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (sub_umbrella.offset field of "
			  "LC_SUB_UMBRELLA command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_SUB_LIBRARY:
		lsub = (struct sub_library_command *)lc;
		if(lsub->cmdsize < sizeof(struct sub_library_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_SUB_LIBRARY command %lu has too small cmdsize "
			  "field)", i);
		    return(FALSE);
		}
		if(lsub->sub_library.offset >= lsub->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (sub_library.offset field of "
			  "LC_SUB_LIBRARY command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_SUB_CLIENT:
		csub = (struct sub_client_command *)lc;
		if(csub->cmdsize < sizeof(struct sub_client_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_SUB_CLIENT command %lu has too small cmdsize "
			  "field)", i);
		    return(FALSE);
		}
		if(csub->client.offset >= csub->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (client.offset field of "
			  "LC_SUB_CLIENT command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_PREBOUND_DYLIB:
		pbdylib = (struct prebound_dylib_command *)lc;
		if(pbdylib->cmdsize < sizeof(struct prebound_dylib_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_PREBOUND_DYLIB command %lu has too small "
			  "cmdsize field)", i);
		    return(FALSE);
		}
		if(pbdylib->name.offset >= pbdylib->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (name.offset field of "
			  "LC_PREBOUND_DYLIB command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		if(pbdylib->linked_modules.offset >= pbdylib->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (linked_modules.offset field of "
			  "LC_PREBOUND_DYLIB command %lu extends past the end "
			  "of all load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_ID_DYLINKER:
	    case LC_LOAD_DYLINKER:
		dyld = (struct dylinker_command *)lc;
		if(dyld->cmdsize < sizeof(struct dylinker_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(%s command %lu has too small cmdsize field)",
			  dyld->cmd == LC_ID_DYLINKER ? "LC_ID_DYLINKER" :
			  "LC_LOAD_DYLINKER", i);
		    return(FALSE);
		}
		if(dyld->name.offset >= dyld->cmdsize){
		    error("in swap_object_headers(): truncated or malformed "
			  "load commands (name.offset field of %s command %lu "
			  "extends past the end of all load commands)",
			  dyld->cmd == LC_ID_DYLINKER ? "LC_ID_DYLINKER" :
			  "LC_LOAD_DYLINKER", i);
		    return(FALSE);
		}
		break;

	    case LC_UNIXTHREAD:
	    case LC_THREAD:
		ut = (struct thread_command *)lc;
		state = (char *)ut + sizeof(struct thread_command);

	    	if(mh->cputype == CPU_TYPE_MC680x0){
		    struct m68k_thread_state_regs *cpu;
		    struct m68k_thread_state_68882 *fpu;
		    struct m68k_thread_state_user_reg *user_reg;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((unsigned long *)state);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			state += sizeof(unsigned long);
			switch(flavor){
			case M68K_THREAD_STATE_REGS:
			    if(count != M68K_THREAD_STATE_REGS_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M68K_THREAD_STATE_REGS_COUNT for "
				    "flavor number %lu which is a M68K_THREAD_"
				    "STATE_REGS flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (struct m68k_thread_state_regs *)state;
			    state += sizeof(struct m68k_thread_state_regs);
			    break;
			case M68K_THREAD_STATE_68882:
			    if(count != M68K_THREAD_STATE_68882_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M68K_THREAD_STATE_68882_COUNT for "
				    "flavor number %lu which is a M68K_THREAD_"
				    "STATE_68882 flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    fpu = (struct m68k_thread_state_68882 *)state;
			    state += sizeof(struct m68k_thread_state_68882);
			    break;
			case M68K_THREAD_STATE_USER_REG:
			    if(count != M68K_THREAD_STATE_USER_REG_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M68K_THREAD_STATE_USER_REG_COUNT for "
				    "flavor number %lu which is a M68K_THREAD_"
				    "STATE_USER_REG flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    user_reg =
				(struct m68k_thread_state_user_reg *)state;
			    state += sizeof(struct m68k_thread_state_user_reg);
			    break;
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor for flavor number %lu in %s command"
				" %lu can't byte swap it)", nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
	    	if(mh->cputype == CPU_TYPE_POWERPC){
		    ppc_thread_state_t *cpu;
		    ppc_float_state_t *fpu;
		    ppc_exception_state_t *except;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((unsigned long *)state);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			state += sizeof(unsigned long);
			switch(flavor){
			case PPC_THREAD_STATE:
			    if(count != PPC_THREAD_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not PPC_THREAD_STATE_COUNT for "
				    "flavor number %lu which is a PPC_THREAD_"
				    "STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (ppc_thread_state_t *)state;
			    state += sizeof(ppc_thread_state_t);
			    break;
			case PPC_FLOAT_STATE:
			    if(count != PPC_FLOAT_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not PPC_FLOAT_STATE_COUNT for "
				    "flavor number %lu which is a PPC_FLOAT_"
				    "STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    fpu = (ppc_float_state_t *)state;
			    state += sizeof(ppc_float_state_t);
			    break;
			case PPC_EXCEPTION_STATE:
			    if(count != PPC_EXCEPTION_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not PPC_EXCEPTION_STATE_COUNT for "
				    "flavor number %lu which is a PPC_EXCEPT"
				    "ION_STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    except = (ppc_exception_state_t *)state;
			    state += sizeof(ppc_exception_state_t);
			    break;
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor for flavor number %lu in %s command"
				" %lu can't byte swap it)", nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
	    	if(mh->cputype == CPU_TYPE_MC88000){
		    m88k_thread_state_grf_t *cpu;
		    m88k_thread_state_xrf_t *fpu;
		    m88k_thread_state_user_t *user;
		    m88110_thread_state_impl_t *spu;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((unsigned long *)state);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			state += sizeof(unsigned long);
			switch(flavor){
			case M88K_THREAD_STATE_GRF:
			    if(count != M88K_THREAD_STATE_GRF_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M88K_THREAD_STATE_GRF_COUNT for "
				    "flavor number %lu which is a M88K_THREAD_"
				    "STATE_GRF flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (m88k_thread_state_grf_t *)state;
			    state += sizeof(m88k_thread_state_grf_t);
			    break;
			case M88K_THREAD_STATE_XRF:
			    if(count != M88K_THREAD_STATE_XRF_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M88K_THREAD_STATE_XRF_COUNT for "
				    "flavor number %lu which is a M88K_THREAD_"
				    "STATE_XRF flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    fpu = (m88k_thread_state_xrf_t *)state;
			    state += sizeof(m88k_thread_state_xrf_t);
			    break;
			case M88K_THREAD_STATE_USER:
			    if(count != M88K_THREAD_STATE_USER_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M88K_THREAD_STATE_USER_COUNT for "
				    "flavor number %lu which is a M88K_THREAD_"
				    "STATE_USER flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    user = (m88k_thread_state_user_t *)state;
			    state += sizeof(m88k_thread_state_user_t);
			    break;
			case M88110_THREAD_STATE_IMPL:
			    if(count != M88110_THREAD_STATE_IMPL_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not M88110_THREAD_STATE_IMPL_COUNT for "
				    "flavor number %lu which is a M88110_THREAD"
				    "_STATE_IMPL flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    spu = (m88110_thread_state_impl_t *)state;
			    state += sizeof(m88110_thread_state_impl_t);
			    break;
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor for flavor number %lu in %s command"
				" %lu can't byte swap it)", nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
	    	if(mh->cputype == CPU_TYPE_I860){
		    struct i860_thread_state_regs *cpu;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((unsigned long *)state);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			state += sizeof(unsigned long);
			switch(flavor){
			case I860_THREAD_STATE_REGS:
			    if(count != I860_THREAD_STATE_REGS_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not I860_THREAD_STATE_REGS_COUNT for "
				    "flavor number %lu which is a I860_THREAD_"
				    "STATE_REGS flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (struct i860_thread_state_regs *)state;
			    state += sizeof(struct i860_thread_state_regs);
			    break;
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor for flavor number %lu in %s command"
				" %lu can't byte swap it)", nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
	    	if(mh->cputype == CPU_TYPE_I386){
		    i386_thread_state_t *cpu;
		    i386_thread_fpstate_t *fpu;
		    i386_thread_exceptstate_t *exc;
		    i386_thread_cthreadstate_t *user;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((unsigned long *)state);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			state += sizeof(unsigned long);
			switch(flavor){
			case i386_THREAD_STATE:
			    if(count != i386_THREAD_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not i386_THREAD_STATE_COUNT for flavor "
				    "number %lu which is a i386_THREAD_STATE "
				    "flavor in %s command %lu)", nflavor,
				    ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				    "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (i386_thread_state_t *)state;
			    state += sizeof(i386_thread_state_t);
			    break;
			case i386_THREAD_FPSTATE:
			    if(count != i386_THREAD_FPSTATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not i386_THREAD_FPSTATE_COUNT for flavor "
				    "number %lu which is a i386_THREAD_FPSTATE "
				    "flavor in %s command %lu)", nflavor,
				    ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				    "LC_THREAD", i);
				return(FALSE);
			    }
			    fpu = (i386_thread_fpstate_t *)state;
			    state += sizeof(i386_thread_fpstate_t);
			    break;
			case i386_THREAD_EXCEPTSTATE:
			    if(count != i386_THREAD_EXCEPTSTATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not i386_THREAD_EXCEPTSTATE_COUNT for "
				    "flavor number %lu which is a i386_THREAD_"
				    "EXCEPTSTATE flavor in %s command %lu)",
				    nflavor,
				    ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				    "LC_THREAD", i);
				return(FALSE);
			    }
			    exc = (i386_thread_exceptstate_t *)state;
			    state += sizeof(i386_thread_fpstate_t);
			    break;
			case i386_THREAD_CTHREADSTATE:
			    if(count != i386_THREAD_CTHREADSTATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not i386_THREAD_CTHREADSTATE_COUNT for "
				    "flavor number %lu which is a i386_THREAD_"
				    "CTHREADSTATE flavor in %s command %lu)",
				    nflavor,
				    ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				    "LC_THREAD", i);
				return(FALSE);
			    }
			    user = (i386_thread_cthreadstate_t *)state;
			    state += sizeof(i386_thread_fpstate_t);
			    break;
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor for flavor number %lu in %s command"
				" %lu can't byte swap it)", nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
	        if(mh->cputype == CPU_TYPE_HPPA){
		    struct hp_pa_integer_thread_state *cpu;
		    struct hp_pa_frame_thread_state *frame;
		    struct hp_pa_fp_thread_state *fpu;

		    nflavor = 0;
		    p = (char *)ut + ut->cmdsize;
		    while(state < p){
			flavor = *((unsigned long *)state);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			state += sizeof(unsigned long);
			switch(flavor){
			case HPPA_INTEGER_THREAD_STATE:
			    if(count != HPPA_INTEGER_THREAD_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not HPPA_INTEGER_THREAD_STATE_COUNT for "
				    "flavor number %lu which is a HPPA_INTEGER"
				    "_THREAD_STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    cpu = (struct hp_pa_integer_thread_state *)state;
			    state += sizeof(struct hp_pa_integer_thread_state);
			    break;
			case HPPA_FRAME_THREAD_STATE:
			    if(count != HPPA_FRAME_THREAD_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not HPPA_FRAME_THREAD_STATE_COUNT for "
				    "flavor number %lu which is a HPPA_FRAME"
				    "_THREAD_STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    frame = (struct hp_pa_frame_thread_state *)state;
			    state += sizeof(struct hp_pa_frame_thread_state);
			    break;
			case HPPA_FP_THREAD_STATE:
			    if(count != HPPA_FP_THREAD_STATE_COUNT){
				error("in swap_object_headers(): malformed "
				    "load commands (count "
				    "not HPPA_FP_THREAD_STATE_COUNT for "
				    "flavor number %lu which is a HPPA_FP"
				    "_THREAD_STATE flavor in %s command %lu)",
				    nflavor, ut->cmd == LC_UNIXTHREAD ? 
				    "LC_UNIXTHREAD" : "LC_THREAD", i);
				return(FALSE);
			    }
			    fpu = (struct hp_pa_fp_thread_state *)state;
			    state += sizeof(struct hp_pa_fp_thread_state);
			    break;
			default:
			    error("in swap_object_headers(): malformed "
				"load commands (unknown "
				"flavor for flavor number %lu in %s command"
				" %lu can't byte swap it)", nflavor,
				ut->cmd == LC_UNIXTHREAD ? "LC_UNIXTHREAD" :
				"LC_THREAD", i);
			    return(FALSE);
			}
			nflavor++;
		    }
		    break;
		}
		if (mh->cputype == CPU_TYPE_SPARC) {
		  struct sparc_thread_state_regs *cpu;
		  struct sparc_thread_state_fpu *fpu;

		  nflavor = 0;
		  p = (char *)ut + ut->cmdsize;
		  while (state < p) {
		    flavor = *((unsigned long *) state);
		    state += sizeof(unsigned long);
		    count = *((unsigned int *) state);
		    state += sizeof(unsigned long);
		    switch (flavor) {
		    case SPARC_THREAD_STATE_REGS:
		      if (count != SPARC_THREAD_STATE_REGS_COUNT) {
			error("in swap_object_headers(): malformed "
			      "load commands (count "
			      "not SPARC_THREAD_STATE_REGS_COUNT for "
			      "flavor number %lu which is a SPARC_THREAD_"
			      "STATE_REGS flavor in %s command %lu)",
			      nflavor, ut->cmd == LC_UNIXTHREAD ? 
			      "LC_UNIXTHREAD" : "LC_THREAD", i);
			return(FALSE);
		      }
		      cpu = (struct sparc_thread_state_regs *) state;
		      state += sizeof(struct sparc_thread_state_regs);
		      break;
		    case SPARC_THREAD_STATE_FPU:
		      if (count != SPARC_THREAD_STATE_FPU_COUNT) {
			error("in swap_object_headers(): malformed "
			      "load commands (count "
			      "not SPARC_THREAD_STATE_FPU_COUNT for "
			      "flavor number %lu which is a SPARC_THREAD_"
			      "STATE_FPU flavor in %s command %lu)",
			      nflavor, ut->cmd == LC_UNIXTHREAD ? 
			      "LC_UNIXTHREAD" : "LC_THREAD", i);
			return(FALSE);
		      }
		      fpu = (struct sparc_thread_state_fpu *) state;
		      state += sizeof(struct sparc_thread_state_fpu);
		      break;
		    }
		  }
		  break;
		}
		    
		error("in swap_object_headers(): malformed load commands "
		    "(unknown cputype (%d) and cpusubtype (%d) of object and "
                    "can't byte swap %s command %lu)", mh->cputype, 
		    mh->cpusubtype, ut->cmd == LC_UNIXTHREAD ?
		    "LC_UNIXTHREAD" : "LC_THREAD", i);
		return(FALSE);
	    case LC_IDENT:
		id = (struct ident_command *)lc;
		if((char *)id + id->cmdsize >
		   (char *)load_commands + mh->sizeofcmds){
		    error("in swap_object_headers(): truncated or malformed "
			"load commands (cmdsize field of LC_IDENT command %lu "
			"extends past the end of the load commands)", i);
		    return(FALSE);
		}
		break;

	    case LC_ROUTINES:
		rc = (struct routines_command *)lc;
		if(rc->cmdsize != sizeof(struct routines_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_ROUTINES command %lu has incorrect cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_TWOLEVEL_HINTS:
		hints = (struct twolevel_hints_command *)lc;
		if(hints->cmdsize != sizeof(struct twolevel_hints_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_TWOLEVEL_HINTS command %lu has incorrect "
			  "cmdsize", i);
		    return(FALSE);
		}
		break;

	    case LC_PREBIND_CKSUM:
		cs = (struct prebind_cksum_command *)lc;
		if(cs->cmdsize != sizeof(struct prebind_cksum_command)){
		    error("in swap_object_headers(): malformed load commands "
			  "(LC_PREBIND_CKSUM command %lu has incorrect cmdsize",
			  i);
		    return(FALSE);
		}
		break;

	    default:
		error("in swap_object_headers(): malformed load commands "
		      "(unknown load command %lu)", i);
		return(FALSE);
	    }

	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    /* check that next load command does not extends past the end */
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds){
		error("in swap_object_headers(): truncated or malformed load "
		      "commands (load command %lu extends past the end of all "
		      "load commands)", i + 1);
		return(FALSE);
	    }
	}
	/* check for an inconsistant size of the load commands */
	if((char *)load_commands + mh->sizeofcmds != (char *)lc){
	    error("in swap_object_headers(): malformed load commands "
		  "(inconsistant sizeofcmds field in mach header)");
	    return(FALSE);
	}


	/*
	 * Now knowing the load commands can be parsed swap them.
	 */
	target_byte_sex = get_host_byte_sex() == BIG_ENDIAN_BYTE_SEX ?
			  LITTLE_ENDIAN_BYTE_SEX : BIG_ENDIAN_BYTE_SEX;
	for(i = 0, lc = load_commands; i < mh->ncmds; i++){
	    l = *lc;
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		swap_section(s, sg->nsects, target_byte_sex);
		swap_segment_command(sg, target_byte_sex);
		break;

	    case LC_SYMTAB:
		st = (struct symtab_command *)lc;
		swap_symtab_command(st, target_byte_sex);
		break;

	    case LC_DYSYMTAB:
		dyst = (struct dysymtab_command *)lc;
		swap_dysymtab_command(dyst, target_byte_sex);
		break;

	    case LC_SYMSEG:
		ss = (struct symseg_command *)lc;
		swap_symseg_command(ss, target_byte_sex);
		break;

	    case LC_IDFVMLIB:
	    case LC_LOADFVMLIB:
		fl = (struct fvmlib_command *)lc;
		swap_fvmlib_command(fl, target_byte_sex);
		break;

	    case LC_ID_DYLIB:
	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
		dl = (struct dylib_command *)lc;
		swap_dylib_command(dl, target_byte_sex);
		break;

	    case LC_SUB_FRAMEWORK:
		sub = (struct sub_framework_command *)lc;
		swap_sub_framework_command(sub, target_byte_sex);
		break;

	    case LC_SUB_UMBRELLA:
		usub = (struct sub_umbrella_command *)lc;
		swap_sub_umbrella_command(usub, target_byte_sex);
		break;

	    case LC_SUB_LIBRARY:
		lsub = (struct sub_library_command *)lc;
		swap_sub_library_command(lsub, target_byte_sex);
		break;

	    case LC_SUB_CLIENT:
		csub = (struct sub_client_command *)lc;
		swap_sub_client_command(csub, target_byte_sex);
		break;

	    case LC_PREBOUND_DYLIB:
		pbdylib = (struct prebound_dylib_command *)lc;
		swap_prebound_dylib_command(pbdylib, target_byte_sex);
		break;

	    case LC_ID_DYLINKER:
	    case LC_LOAD_DYLINKER:
		dyld = (struct dylinker_command *)lc;
		swap_dylinker_command(dyld, target_byte_sex);
		break;

	    case LC_UNIXTHREAD:
	    case LC_THREAD:
		ut = (struct thread_command *)lc;
		state = (char *)ut + sizeof(struct thread_command);
		p = (char *)ut + ut->cmdsize;
		swap_thread_command(ut, target_byte_sex);

	    	if(mh->cputype == CPU_TYPE_MC680x0){
		    struct m68k_thread_state_regs *cpu;
		    struct m68k_thread_state_68882 *fpu;
		    struct m68k_thread_state_user_reg *user_reg;

		    while(state < p){
			flavor = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(flavor);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(count);
			state += sizeof(unsigned long);
			switch(flavor){
			case M68K_THREAD_STATE_REGS:
			    cpu = (struct m68k_thread_state_regs *)state;
			    swap_m68k_thread_state_regs(cpu, target_byte_sex);
			    state += sizeof(struct m68k_thread_state_regs);
			    break;
			case M68K_THREAD_STATE_68882:
			    fpu = (struct m68k_thread_state_68882 *)state;
			    swap_m68k_thread_state_68882(fpu, target_byte_sex);
			    state += sizeof(struct m68k_thread_state_68882);
			    break;
			case M68K_THREAD_STATE_USER_REG:
			    user_reg =
				(struct m68k_thread_state_user_reg *)state;
			    swap_m68k_thread_state_user_reg(user_reg,
							    target_byte_sex);
			    state += sizeof(struct m68k_thread_state_user_reg);
			    break;
			}
		    }
		    break;
		}
	    	if(mh->cputype == CPU_TYPE_POWERPC){
		    ppc_thread_state_t *cpu;
		    ppc_float_state_t *fpu;
		    ppc_exception_state_t *except;

		    while(state < p){
			flavor = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(flavor);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(count);
			state += sizeof(unsigned long);
			switch(flavor){
			case PPC_THREAD_STATE:
			    cpu = (ppc_thread_state_t *)state;
			    swap_ppc_thread_state_t(cpu, target_byte_sex);
			    state += sizeof(ppc_thread_state_t);
			    break;
			case PPC_FLOAT_STATE:
			    fpu = (ppc_float_state_t *)state;
			    swap_ppc_float_state_t(fpu, target_byte_sex);
			    state += sizeof(ppc_float_state_t);
			case PPC_EXCEPTION_STATE:
			    except = (ppc_exception_state_t *)state;
			    swap_ppc_exception_state_t(except, target_byte_sex);
			    state += sizeof(ppc_exception_state_t);
			}
		    }
		    break;
		}
	    	if(mh->cputype == CPU_TYPE_MC88000){
		    m88k_thread_state_grf_t *cpu;
		    m88k_thread_state_xrf_t *fpu;
		    m88k_thread_state_user_t *user;
		    m88110_thread_state_impl_t *spu;

		    while(state < p){
			flavor = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(flavor);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(count);
			state += sizeof(unsigned long);
			switch(flavor){
			case M88K_THREAD_STATE_GRF:
			    cpu = (m88k_thread_state_grf_t *)state;
			    swap_m88k_thread_state_grf_t(cpu,
							 target_byte_sex);
			    state += sizeof(m88k_thread_state_grf_t);
			    break;
			case M88K_THREAD_STATE_XRF:
			    fpu = (m88k_thread_state_xrf_t *)state;
			    swap_m88k_thread_state_xrf_t(fpu,
							 target_byte_sex);
			    state += sizeof(m88k_thread_state_xrf_t);
			    break;
			case M88K_THREAD_STATE_USER:
			    user = (m88k_thread_state_user_t *)state;
			    swap_m88k_thread_state_user_t(user,
							  target_byte_sex);
			    state += sizeof(m88k_thread_state_user_t);
			    break;
			case M88110_THREAD_STATE_IMPL:
			    spu = (m88110_thread_state_impl_t *)state;
			    swap_m88110_thread_state_impl_t(spu,
							  target_byte_sex);
			    state += sizeof(m88110_thread_state_impl_t);
			    break;
			}
		    }
		    break;
		}
	    	if(mh->cputype == CPU_TYPE_I860){
		    struct i860_thread_state_regs *cpu;

		    while(state < p){
			flavor = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(flavor);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(count);
			state += sizeof(unsigned long);
			switch(flavor){
			case I860_THREAD_STATE_REGS:
			    cpu = (struct i860_thread_state_regs *)state;
			    swap_i860_thread_state_regs(cpu, target_byte_sex);
			    state += sizeof(struct i860_thread_state_regs);
			    break;
			}
		    }
		    break;
		}
	    	if(mh->cputype == CPU_TYPE_I386){
		    i386_thread_state_t *cpu;
		    i386_thread_fpstate_t *fpu;
		    i386_thread_exceptstate_t *exc;
		    i386_thread_cthreadstate_t *user;

		    while(state < p){
			flavor = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(flavor);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(count);
			state += sizeof(unsigned long);
			switch(flavor){
			case i386_THREAD_STATE:
			    cpu = (i386_thread_state_t *)state;
			    swap_i386_thread_state(cpu, target_byte_sex);
			    state += sizeof(i386_thread_state_t);
			    break;
			case i386_THREAD_FPSTATE:
			    fpu = (i386_thread_fpstate_t *)state;
			    swap_i386_thread_fpstate(fpu, target_byte_sex);
			    state += sizeof(i386_thread_fpstate_t);
			    break;
			case i386_THREAD_EXCEPTSTATE:
			    exc = (i386_thread_exceptstate_t *)state;
			    swap_i386_thread_exceptstate(exc, target_byte_sex);
			    state += sizeof(i386_thread_exceptstate_t);
			    break;
			case i386_THREAD_CTHREADSTATE:
			    user = (i386_thread_cthreadstate_t *)state;
			    swap_i386_thread_cthreadstate(user,target_byte_sex);
			    state += sizeof(i386_thread_cthreadstate_t);
			    break;
			}
		    }
		    break;
		}
	    	if(mh->cputype == CPU_TYPE_HPPA){
		    struct hp_pa_integer_thread_state *cpu;
		    struct hp_pa_frame_thread_state *frame;
		    struct hp_pa_fp_thread_state *fpu;

		    while(state < p){
			flavor = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(flavor);
			state += sizeof(unsigned long);
			count = *((unsigned long *)state);
			*((unsigned long *)state) = SWAP_LONG(count);
			state += sizeof(unsigned long);
			switch(flavor){
			case HPPA_INTEGER_THREAD_STATE:
			    cpu = (struct hp_pa_integer_thread_state *)state;
			    swap_hppa_integer_thread_state(cpu,
							 target_byte_sex);
			    state += sizeof(struct hp_pa_integer_thread_state);
			    break;
			case HPPA_FRAME_THREAD_STATE:
			    frame = (struct hp_pa_frame_thread_state *)state;
			    swap_hppa_frame_thread_state(frame,
							 target_byte_sex);
			    state += sizeof(struct hp_pa_frame_thread_state);
			    break;
			case HPPA_FP_THREAD_STATE:
			    fpu = (struct hp_pa_fp_thread_state *)state;
			    swap_hppa_fp_thread_state(fpu,
						     target_byte_sex);
			    state += sizeof(struct hp_pa_fp_thread_state);
			    break;
			}
		    }
		    break;
		}

		if (mh->cputype == CPU_TYPE_SPARC) {
		  struct sparc_thread_state_regs *cpu;
		  struct sparc_thread_state_fpu *fpu;

		  while (state < p) {
		    flavor = *((unsigned long *) state);
		    *((unsigned long *) state) = SWAP_LONG(flavor);
		    state += sizeof(unsigned long);
		    count = *((unsigned int *) state);
		    *((unsigned int *) state) = SWAP_LONG(count);
		    state += sizeof(unsigned long);
		    switch (flavor) {
		    case SPARC_THREAD_STATE_REGS:
		      cpu = (struct sparc_thread_state_regs *) state;
		      swap_sparc_thread_state_regs(cpu, target_byte_sex);
		      state += sizeof(struct sparc_thread_state_regs);
		      break;
		    case SPARC_THREAD_STATE_FPU:
		      fpu = (struct sparc_thread_state_fpu *) state;
		      swap_sparc_thread_state_fpu(fpu, target_byte_sex);
		      state += sizeof(struct sparc_thread_state_fpu);
		      break;
		    }
		  }
		  break;
		}
		break;

	    case LC_IDENT:
		id = (struct ident_command *)lc;
		swap_ident_command(id, target_byte_sex);
		break;

	    case LC_ROUTINES:
		rc = (struct routines_command *)lc;
		swap_routines_command(rc, target_byte_sex);
		break;

	    case LC_TWOLEVEL_HINTS:
		hints = (struct twolevel_hints_command *)lc;
		swap_twolevel_hints_command(hints, target_byte_sex);
		break;

	    case LC_PREBIND_CKSUM:
		cs = (struct prebind_cksum_command *)lc;
		swap_prebind_cksum_command(cs, target_byte_sex);
		break;
	    }

	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	}
	swap_mach_header(mh, target_byte_sex);

	return(TRUE);
}
