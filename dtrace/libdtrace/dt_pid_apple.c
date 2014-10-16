/*
 * Copyright (c) 2006-2008 Apple Computer, Inc.  All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

// This must be done *after* any references to Foundation.h!
#define uint_t  __Solaris_uint_t

#include <pthread.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <stdlib.h>

#include <sys/dtrace.h>
#include <dtrace.h>

#include <dt_proc.h>
#include <dt_pid.h>

/*
 * FIX ME. This is a direct cut & paste from
 * dt_pid.c. It is probably already out of sync.
 */
typedef struct dt_pid_probe {
	dtrace_hdl_t *dpp_dtp;
	dt_pcb_t *dpp_pcb;
	dt_proc_t *dpp_dpr;
	struct ps_prochandle *dpp_pr;
	fasttrap_provider_type_t dpp_provider_type;
	const char *dpp_mod;
	char *dpp_func;
	const char *dpp_name;
	const char *dpp_obj;
	uintptr_t dpp_pc;
	size_t dpp_size;
	Lmid_t dpp_lmid;
	uint_t dpp_nmatches;
	uint64_t dpp_stret[4];
	GElf_Sym dpp_last;
	uint_t dpp_last_taken;
} dt_pid_probe_t;

int strisglob(const char *s);
void dt_proc_bpdisable(dt_proc_t *dpr);
void dt_proc_bpenable(dt_proc_t *dpr);
int dt_pid_per_sym(dt_pid_probe_t *pp, const GElf_Sym *symp, const char *func);
int dt_pid_error(dtrace_hdl_t *dtp, dt_pcb_t *pcb, dt_proc_t *dpr, fasttrap_probe_spec_t *ftp, dt_errtag_t tag, const char *fmt, ...);

/*
 * OBJC provider methods
 */

static int dt_pid_objc_filt(void *arg, const GElf_Sym *sym, const char *class_name, const char *method_name)
{
	dt_pid_probe_t *pp = arg;
        
        // Handle the class name first.
	if ((strisglob(pp->dpp_mod) && gmatch(class_name, pp->dpp_mod)) || (strcmp(class_name, pp->dpp_mod) == 0)) {
                // Now check the method name.
                if ((strisglob(pp->dpp_func) && gmatch(method_name, pp->dpp_func)) || (strcmp(method_name, pp->dpp_func) == 0)) {
                        pp->dpp_obj = class_name;
                        // At this point, we can cheat and use the normal pid probe code.
                        return dt_pid_per_sym(pp, sym, method_name);
                }
        }
        
	return (0);
}

int dt_pid_create_objc_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp, dt_pcb_t *pcb, dt_proc_t *dpr)
{
	dt_pid_probe_t pp;
	int ret = 0;
        
	/*
	 * Disable breakpoints so they don't interfere with our disassembly.
	 */
	dt_proc_bpdisable(dpr);
        
	pp.dpp_dtp = dtp;
	pp.dpp_dpr = dpr;
	pp.dpp_pr = dpr->dpr_proc;
	pp.dpp_pcb = pcb;
        
	/*
	 * We can only trace dynamically-linked executables (since we've
	 * hidden some magic in ld.so.1 as well as libc.so.1).
	 */
	prmap_t thread_local_map;
	if (Pname_to_map(pp.dpp_pr, PR_OBJ_LDSO, &thread_local_map) == NULL) {
		return (dt_pid_error(dtp, pcb, dpr, NULL, D_PROC_DYN,
				     "process %s has no dyld, and cannot be instrumented",
				     &pdp->dtpd_provider[3]));
	}
	        
	pp.dpp_provider_type = DTFTP_PROVIDER_OBJC;
	pp.dpp_mod = pdp->dtpd_mod[0] != '\0' ? pdp->dtpd_mod : "*";
	pp.dpp_func = pdp->dtpd_func[0] != '\0' ? pdp->dtpd_func : "*";
	pp.dpp_name = pdp->dtpd_name[0] != '\0' ? pdp->dtpd_name : "*";
	pp.dpp_last_taken = 0;
        
        //
        // We set up some default values that normally change per module.
        //
        pp.dpp_lmid = LM_ID_BASE;
        pp.dpp_stret[0] = 0;
        pp.dpp_stret[1] = 0;
        pp.dpp_stret[2] = 0;
        pp.dpp_stret[3] = 0;
        
        /*
         * We're working in the objc namespace, symbols are in "library, function" style.
         * We have to look at every symbol in every owner, even without globbing. 
         */
        	
        ret = Pobjc_method_iter(pp.dpp_pr, (proc_objc_f*)dt_pid_objc_filt, &pp);
        
	dt_proc_bpenable(dpr);
        
	return (ret);
}

// Because of the statically cached array, this function is not thread safe.
int pidFromProcessName(char* name)
{
	// Most of this code is from [VMUProcInfo getProcessIds]
	// The reason we don't call this code directly is because we only get the pids,
	// and then we have to turn around and construct a VMUProcInfo for each pid to
	// populate our proc table.

	static struct kinfo_proc *procBuf = NULL;
	static int numberOfProcs;

	if (procBuf == NULL) {
		size_t bufSize;
		int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL};
		int err;

		// Try to find out how many processes are around so we can
		// size the buffer appropriately.  sysctl's man page specifically suggests
		// this approach, and says it returns a bit larger size than needed to handle
		// any new processes created between then and now.
    
		err = sysctl(mib, 4, NULL, &bufSize, NULL, 0);
    
		if((err < 0) && (err != ENOMEM)) {
			// enomem says we didn't have enough room for the entire buffer.
			// We don't care about that, we're trying to get the size of a buffer
			// suitable for getting all the data.
			perror("Failure calling sysctl to get process list buffer size");
			return -1;
		}
    
		// Now for the real access.  Create the buffer, and grab the
		// data.
		procBuf = (struct kinfo_proc *)calloc(1, bufSize);
    
		if(sysctl(mib, 4, procBuf, &bufSize, NULL, 0) < 0) {
			//perror("Failure calling sysctl to get proc buf");
			free(procBuf);
			return -1;
		}
    
		numberOfProcs = (int)(bufSize / (sizeof(struct kinfo_proc)));
	}

	int i;
	for (i=0; i<numberOfProcs; i++) {
		if (strncmp(name, procBuf[i].kp_proc.p_comm, MAXCOMLEN) == 0)
			return procBuf[i].kp_proc.p_pid;
	}

	return -1;
}

