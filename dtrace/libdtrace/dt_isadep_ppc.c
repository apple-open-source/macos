/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)dt_isadep.c	1.14	06/02/22 SMI"

#if defined(__ppc__) || defined(__ppc64__)

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>

#include <dt_impl.h>
#include <dt_pid.h>

// Files from libscanalyzer
#include "decode.h"
#include "Scanalyzer.h"
#include "sym.h"
#include "misc.h"
#include "core.h"

//                        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
static
uint8_t probeable[256] = {0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,	// 0 0 0 0 - common
			  0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,	// 0 0 0 1 - common, exit
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0 0 1 0 - priv
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0 0 1 1 - priv, exit
			  0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,	// 0 1 0 0 - common, call
			  0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,	// 0 1 0 1 - common, call, exit
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0 1 1 0 - priv, call
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0 1 1 1 - priv, call, exit
			  0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,	// 1 0 0 0 - common, branch target
			  0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,	// 1 0 0 1 - common, branch target, exit
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 1 0 1 0 - priv, branch target
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 1 0 1 1 - priv, branch target, exit
			  0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,	// 1 1 0 0 - common, branch target, call
			  0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,	// 1 1 0 1 - common, branch target, call, exit
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 1 1 1 0 - priv, branch target, call
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};	// 1 1 1 1 - priv, branch target, call, exit

int
dt_pid_create_entry_probe(struct ps_prochandle *P, dtrace_hdl_t *dtp,
			  fasttrap_probe_spec_t *ftp, const GElf_Sym *symp)
{                
#if defined(__APPLE__)
	ftp->ftps_probe_type = DTFTP_ENTRY;
	ftp->ftps_pc = symp->st_value; // Keep st_value as uint64_t
#endif
	ftp->ftps_size = (size_t)symp->st_size;
	ftp->ftps_noffs = 1;
	ftp->ftps_offs[0] = 0;
	
#if defined(__APPLE__)
	// We cannot instrument all entry instructions, some are unsafe
	uint8_t *text;
	char dmodel = Pstatus(P)->pr_dmodel;
	
	/*
	 * We allocate a few extra bytes at the end so we don't have to check
	 * for overrunning the buffer.
	 */
	if ((text = calloc(1, symp->st_size + 4)) == NULL) {
		dt_dprintf("mr sparkle: malloc() failed\n");
		return (DT_PROC_ERR);
	}
	
	if (Pread(P, text, symp->st_size, symp->st_value) != symp->st_size) {
		dt_dprintf("mr sparkle: Pread() failed\n");
		free(text);
		return (DT_PROC_ERR);
	}
	
	uint8_t *isnflgs  = InvokeScanalyzer(dmodel == PR_MODEL_LP64, _dtrace_scanalyzer, symp->st_value, symp->st_size, (uint32_t *)text, ftp->ftps_mod, ftp->ftps_func);   /* Analyze the code */
	if(isnflgs == (uint8_t *)0) {				/* Were there any instructions? */
		dt_dprintf("scanalyzer found no exectable instructions\n");
		free(text);								/* Toss our cookies */
		return (-1);								/* No instructions, nothing to do... */
	}
		
	if((isnflgs[0] & 15) == isRsvn) {			/* Do we start with a reservation instruction? */
		dt_dprintf("entry point is an unprobeable reservation type instruction\n");
		free(isnflgs);
		free(text);
		return (0);
	}
		
	if(!probeable[isnflgs[0]]) {				/* Are we allowed to probe this instruction? */
		dt_dprintf("entry point is an unprobeable instruction\n");
		free(isnflgs);
		free(text);
		return (0);
	}
		
	free(isnflgs);
	free(text);
#endif
	
	if (ioctl(dtp->dt_ftfd, FASTTRAPIOC_MAKEPROBE, ftp) != 0) {
		dt_dprintf("fasttrap probe creation ioctl failed: %s\n",
			   strerror(errno));
		return (dt_set_errno(dtp, errno));
	}
	
	return (1);
}

/*ARGSUSED*/
int
dt_pid_create_return_probe(struct ps_prochandle *P, dtrace_hdl_t *dtp,
			   fasttrap_probe_spec_t *ftp, const GElf_Sym *symp, uint64_t *stret)
{
	uint8_t *text;
	ulong_t i;
	char dmodel = Pstatus(P)->pr_dmodel;
	
	/*
	 * We allocate a few extra bytes at the end so we don't have to check
	 * for overrunning the buffer.
	 */
	if ((text = calloc(1, symp->st_size + 4)) == NULL) {
		dt_dprintf("mr sparkle: malloc() failed\n");
		return (DT_PROC_ERR);
	}
	
	if (Pread(P, text, symp->st_size, symp->st_value) != symp->st_size) {
		dt_dprintf("mr sparkle: Pread() failed\n");
		free(text);
		return (DT_PROC_ERR);
	}
	
	ftp->ftps_size = (size_t)symp->st_size;
	ftp->ftps_noffs = 0;
#if defined(__APPLE__)
	ftp->ftps_probe_type = DTFTP_RETURN;
	ftp->ftps_pc = symp->st_value;

	uint8_t *isnflgs  = InvokeScanalyzer(dmodel == PR_MODEL_LP64, _dtrace_scanalyzer, symp->st_value, symp->st_size, (uint32_t *)text, ftp->ftps_mod, ftp->ftps_func);   /* Analyze the code */
	if(isnflgs == NULL) {
		dt_dprintf("scanalyzer found no exectable instructions\n");
		free(text);
		return (-1);
	}
	
	int32_t icnt = (ftp->ftps_size + 3) / 4;	/* Get the number of instructions */
	for(i = 0; i < icnt; i++) {
		if((isnflgs[i] & isnExit)) {
			/*
			 * Invariant: no return instruction can be a reservation type.
			 */
			if (probeable[isnflgs[i]]) {
				dt_dprintf("return at offset 0x%x\n", (i << 2));
				ftp->ftps_offs[ftp->ftps_noffs++] = (i << 2);	/* Save offset of instruction */
			} else {
				dt_dprintf("return at offset 0x%x is an unprobeable instruction\n", (i << 2));
			}
		}
	}
	free(isnflgs);
#endif
	
	free(text);
	if (ftp->ftps_noffs > 0) {
		if (ioctl(dtp->dt_ftfd, FASTTRAPIOC_MAKEPROBE, ftp) != 0) {
			dt_dprintf("fasttrap probe creation ioctl failed: %s\n",
				   strerror(errno));
			return (dt_set_errno(dtp, errno));
		}
	}
	
	return (ftp->ftps_noffs);
}

/*ARGSUSED*/
int
dt_pid_create_offset_probe(struct ps_prochandle *P, dtrace_hdl_t *dtp,
			   fasttrap_probe_spec_t *ftp, const GElf_Sym *symp, ulong_t off)
{
#if defined(__APPLE__)
	if (off & 0x3)
		return DT_PROC_ALIGN;
	
	ftp->ftps_probe_type = DTFTP_OFFSETS;
	ftp->ftps_pc = symp->st_value;
#endif
	ftp->ftps_size = (size_t)symp->st_size;
	ftp->ftps_noffs = 1;
	
	if (strcmp("-", ftp->ftps_func) == 0) {
		/*
		 * APPLE NOTE: No way to run Scanalyzer safely. You're on your own...
		 */
		ftp->ftps_offs[0] = off;
	} else {
		uint8_t *text;
		ulong_t i;
		char dmodel = Pstatus(P)->pr_dmodel;
		
		if ((text = malloc(symp->st_size)) == NULL) {
			dt_dprintf("mr sparkle: malloc() failed\n");
			return (DT_PROC_ERR);
		}
		
		if (Pread(P, text, symp->st_size, symp->st_value) !=
		    symp->st_size) {
			dt_dprintf("mr sparkle: Pread() failed\n");
			free(text);
			return (DT_PROC_ERR);
		}
		
#if defined(__APPLE__)		
		uint8_t *isnflgs  = InvokeScanalyzer(dmodel == PR_MODEL_LP64, _dtrace_scanalyzer, symp->st_value, symp->st_size, (uint32_t *)text, ftp->ftps_mod, ftp->ftps_func);   /* Analyze the code */
		if(isnflgs == NULL) {
			dt_dprintf("scanalyzer found no executable instructions\n");
			free(text);
			return (-1);
		}
		
		/*
		 * We need to see if *any* instructions in this method are reservation types.
		 * If so, we cannot currently tell which instructions are safe to instrument,
		 * and the entire method must be skipped.
		 */
		uint32_t icnt = (ftp->ftps_size + 3) / 4;
		for(i = 0; i < icnt; i++) {
			if((isnflgs[i] & 15) == isRsvn) {
				dt_dprintf("instruction at offset 0x%x is a reservation type, skipping this function\n", i * 4);
				free(isnflgs);
				free(text);
				return 0;
			}
		}
		
		if(probeable[isnflgs[off / 4]]) {
			ftp->ftps_offs[0] = off;
		} else {
			dt_dprintf("instruction at offset 0x%lx is not probeable\n", off);
			free(isnflgs);
			free(text);
			return 0;
		}
		
		free(isnflgs);
#endif
		free(text);
	}
	
	if (ioctl(dtp->dt_ftfd, FASTTRAPIOC_MAKEPROBE, ftp) != 0) {
		dt_dprintf("fasttrap probe creation ioctl failed: %s\n",
			   strerror(errno));
		return (dt_set_errno(dtp, errno));
	}
	
	return (1);
}

/*ARGSUSED*/
int
dt_pid_create_glob_offset_probes(struct ps_prochandle *P, dtrace_hdl_t *dtp,
				 fasttrap_probe_spec_t *ftp, const GElf_Sym *symp, const char *pattern)
{
	uint8_t *text;
	ulong_t i, end;
	char dmodel = Pstatus(P)->pr_dmodel;
	
	if ((text = malloc(symp->st_size)) == NULL) {
		dt_dprintf("mr sparkle: malloc() failed\n");
		return (DT_PROC_ERR);
	}
	
	if (Pread(P, text, symp->st_size, symp->st_value) != symp->st_size) {
		dt_dprintf("mr sparkle: Pread() failed\n");
		free(text);
		return (DT_PROC_ERR);
	}
		
#if defined(__APPLE__)
	ftp->ftps_probe_type = DTFTP_OFFSETS;
	ftp->ftps_pc = symp->st_value;
#endif
	ftp->ftps_size = (size_t)symp->st_size;
	ftp->ftps_noffs = 0;
	
#if defined(__APPLE__)		
	uint8_t *isnflgs  = InvokeScanalyzer(dmodel == PR_MODEL_LP64, _dtrace_scanalyzer, symp->st_value, symp->st_size, (uint32_t *)text, ftp->ftps_mod, ftp->ftps_func);   /* Analyze the code */
	if(isnflgs == NULL) {
		dt_dprintf("scanalyzer found no executable instructions\n");
		free(text);
		return (-1);
	}
	
	/*
	 * We need to see if *any* instructions in this method are reservation types.
	 * If so, we cannot currently tell which instructions are safe to instrument,
	 * and the entire method must be skipped.
	 */
	uint32_t icnt = (ftp->ftps_size + 3) / 4;
	for(i = 0; i < icnt; i++) {
		if((isnflgs[i] & 15) == isRsvn) {
			dt_dprintf("instruction at offset 0x%x is a reservation type, skipping this function\n", i * 4);
			free(isnflgs);
			free(text);
			return 0;
		}
	}
	
	end = ftp->ftps_size;
	
	if (strcmp("*", pattern) == 0) {
		for (i = 0; i < end; i += 4) {
			if (probeable[isnflgs[i / 4]]) {
				ftp->ftps_offs[ftp->ftps_noffs++] = i;
			} else {
				dt_dprintf("instruction at offset 0x%lx is not probeable\n", i);
			}
		}
	} else {
		char name[sizeof (i) * 2 + 1];
		
		for (i = 0; i < end; i += 4) {
			(void) snprintf(name, sizeof (name), "%x", i);
			if (gmatch(name, pattern)) {
				if (probeable[isnflgs[i / 4]]) {
					ftp->ftps_offs[ftp->ftps_noffs++] = i;
				} else {
					dt_dprintf("instruction at offset 0x%lx is not probeable\n", i);
				}				
			}
		}
	}
	
	free(isnflgs);
#endif /* __APPLE__ */
	free(text);
	if (ftp->ftps_noffs > 0) {
		if (ioctl(dtp->dt_ftfd, FASTTRAPIOC_MAKEPROBE, ftp) != 0) {
			dt_dprintf("fasttrap probe creation ioctl failed: %s\n",
				   strerror(errno));
			return (dt_set_errno(dtp, errno));
		}
	}
	
	return (ftp->ftps_noffs);
}

#endif // __ppc__ || __ppc64__
