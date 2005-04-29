/*
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "top.h"

/*
 * Function pointers that point to abstract printing functions.
 */
static samp_skip_t *samp_skipl;
static samp_print_t *samp_printl;
static samp_print_t *samp_println;
static samp_vprint_t *samp_vprintln;
static samp_vprint_t *samp_veprint;

static const libtop_tsamp_t *samp_tsamp;

/* Function prototypes. */
static boolean_t
samp_p_eprint(void *a_user_data, const char *a_format, ...);
static int
samp_p_sort(void *a_data, const libtop_psamp_t *a_a, const libtop_psamp_t *a_b);
static int
samp_p_sort_subcomp(top_sort_key_t a_key, boolean_t a_ascend,
    const libtop_psamp_t *a_a, const libtop_psamp_t *a_b);
static boolean_t
samp_p_print(void);
static boolean_t
samp_p_header_print(void);
static boolean_t
samp_p_legend_print();
static boolean_t
samp_p_proc_print(const libtop_psamp_t *a_psamp);
static const char *
samp_p_vm_size_change(unsigned long long a_prev, unsigned long long a_curr,
    unsigned a_prev_seq);
static char *
samp_p_vm_size_delta(unsigned long long a_size, unsigned long long a_prev,
    char *a_buf, unsigned a_bufsize);
static const char *
samp_p_unsigned_change(unsigned a_prev, unsigned a_curr, unsigned a_prev_seq);
static char *
samp_p_unsigned_delta(unsigned a_size, unsigned a_prev, char *a_buf,
    unsigned a_bufsize);
static char *
samp_p_float_render(float a_float, int a_max_frac_digits, char *a_buf,
    unsigned a_bufsize);
static char *
samp_p_int_render(int a_int, char *a_buf, unsigned a_bufsize);
static char *
samp_p_unsigned_render(unsigned a_unsigned, char *a_buf, unsigned a_bufsize);
static char *
samp_p_username_render(uid_t a_uid, char *a_buf, unsigned a_bufsize);
static char *
samp_p_vm_size_render(unsigned long long a_size, char *a_buf,
    unsigned a_bufsize);
static char *
samp_p_usage_render(const libtop_psamp_t *a_psamp, char *a_buf,
    unsigned a_bufsize);
static char *
samp_p_time_render(const libtop_psamp_t *a_psamp, char *a_buf,
    unsigned a_bufsize);
#ifdef TOP_DEPRECATED
static boolean_t
samp_p_deprecated_header_print(void);
static boolean_t
samp_p_deprecated_legend_print();
static boolean_t
samp_p_deprecated_proc_print(const libtop_psamp_t *a_psamp);
static char *
samp_p_deprecated_vm_size_delta(unsigned long long a_size,
    unsigned long long a_prev, char *a_buf, unsigned a_bufsize);
static char *
samp_p_deprecated_vm_size_render(unsigned long long a_size, char *a_buf,
    unsigned a_bufsize);
static char *
samp_p_deprecated_time_render(const libtop_psamp_t *a_psamp, char *a_buf,
    unsigned a_bufsize);
#endif


/* Initialize printing function pointers and libtop. */
boolean_t
samp_init(samp_skip_t *a_skipl, samp_print_t *a_printl, samp_print_t *a_println,
    samp_vprint_t *a_vprintln, samp_vprint_t *a_veprint)
{
	boolean_t	retval;

	samp_skipl = a_skipl;
	samp_printl = a_printl;
	samp_println = a_println;
	samp_vprintln = a_vprintln;
	samp_veprint = a_veprint;

	if (libtop_init(samp_p_eprint, NULL)) {
		retval = TRUE;
		goto RETURN;
	}
	samp_tsamp = libtop_tsamp();

	retval = FALSE;
	RETURN:
	return retval;
}

/* Shut down. */
void
samp_fini(void)
{
	libtop_fini();
}

/* Take a sample. */
boolean_t
samp_run(void)
{
	boolean_t	retval;

#ifdef TOP_DEPRECATED
	/*
	 * If using the deprecated output mode, turn off framework and memory
	 * region reporting unless they are actually being displayed.
	 */
	if (top_opt_x) {
		if (top_opt_c != 'n') {
			top_opt_f = FALSE;
			top_opt_r = FALSE;
		}
	}
#endif

	if (libtop_sample(top_opt_r, top_opt_f)) {
		retval = TRUE;
		goto RETURN;
	}

	libtop_psort(samp_p_sort, NULL);

	if (samp_p_print()) {
		retval = TRUE;
		goto RETURN;
	}

	retval = FALSE;
	RETURN:
	return retval;
}

/* Print an error. */
static boolean_t
samp_p_eprint(void *a_user_data, const char *a_format, ...)
{
	boolean_t	retval;
	va_list		ap;

	va_start(ap, a_format);
	retval = samp_veprint(FALSE, a_format, ap);
	va_end(ap);

	return retval;
}

/* Sorting function passed to libtop. */
static int
samp_p_sort(void *a_data, const libtop_psamp_t *a_a, const libtop_psamp_t *a_b)
{
	int	retval;

	retval = samp_p_sort_subcomp(top_opt_o, top_opt_o_ascend, a_a, a_b);
	if (retval == 0) {
		retval = samp_p_sort_subcomp(top_opt_O, top_opt_O_ascend, a_a,
		    a_b);
	}

	return retval;
}

/* Actual psamp sorting function, used by samp_p_sort(). */
static int
samp_p_sort_subcomp(top_sort_key_t a_key, boolean_t a_ascend,
    const libtop_psamp_t *a_a, const libtop_psamp_t *a_b)
{
	int	retval;

	switch (a_key) {
	case TOP_SORT_command:
		retval = strcmp(a_a->command, a_b->command);
		if (retval < 0) {
			retval = -1;
		} else if (retval > 0) {
			retval = 1;
		}
		break;
	case TOP_SORT_cpu: {
		struct timeval	tv_a, tv_b;

		switch (top_opt_c) {
		case 'a':
			timersub(&a_a->total_time, &a_a->b_total_time, &tv_a);
			timersub(&a_b->total_time, &a_b->b_total_time, &tv_b);
			break;
		case 'd':
		case 'e':
		case 'n':
			timersub(&a_a->total_time, &a_a->p_total_time, &tv_a);
			timersub(&a_b->total_time, &a_b->p_total_time, &tv_b);
			break;
		default:
			assert(0);
		}

		if (tv_a.tv_sec < tv_b.tv_sec) {
			retval = -1;
		} else if (tv_a.tv_sec > tv_b.tv_sec) {
			retval = 1;
		} else {
			if (tv_a.tv_usec < tv_b.tv_usec) {
				retval = -1;
			} else if (tv_a.tv_usec > tv_b.tv_usec) {
				retval = 1;
			} else {
				retval = 0;
			}
		}
		break;
	}
	case TOP_SORT_pid:
		if (a_a->pid < a_b->pid) {
			retval = -1;
		} else if (a_a->pid > a_b->pid) {
			retval = 1;
		} else {
			retval = 0;
		}
		break;
	case TOP_SORT_prt:
		if (a_a->prt < a_b->prt) {
			retval = -1;
		} else if (a_a->prt > a_b->prt) {
			retval = 1;
		} else {
			retval = 0;
		}
		break;
	case TOP_SORT_reg:
		if (a_a->reg < a_b->reg) {
			retval = -1;
		} else if (a_a->reg > a_b->reg) {
			retval = 1;
		} else {
			retval = 0;
		}
		break;
	case TOP_SORT_rprvt:
		if (a_a->rprvt < a_b->rprvt) {
			retval = -1;
		} else if (a_a->rprvt > a_b->rprvt) {
			retval = 1;
		} else {
			retval = 0;
		}
		break;
	case TOP_SORT_rshrd:
		if (a_a->rshrd < a_b->rshrd) {
			retval = -1;
		} else if (a_a->rshrd > a_b->rshrd) {
			retval = 1;
		} else {
			retval = 0;
		}
		break;
	case TOP_SORT_rsize:
		if (a_a->rsize < a_b->rsize) {
			retval = -1;
		} else if (a_a->rsize > a_b->rsize) {
			retval = 1;
		} else {
			retval = 0;
		}
		break;
	case TOP_SORT_th:
		if (a_a->th < a_b->th) {
			retval = -1;
		} else if (a_a->th > a_b->th) {
			retval = 1;
		} else {
			retval = 0;
		}
		break;
	case TOP_SORT_time: {
		struct timeval	tv_a, tv_b;

		switch (top_opt_c) {
		case 'a':
			timersub(&a_a->total_time, &a_a->b_total_time, &tv_a);
			timersub(&a_b->total_time, &a_b->b_total_time, &tv_b);
			break;
		case 'd':
		case 'e':
		case 'n':
			tv_a = a_a->total_time;
			tv_b = a_b->total_time;
			break;
		default:
			assert(0);
		}

		if (tv_a.tv_sec < tv_b.tv_sec) {
			retval = -1;
		} else if (tv_a.tv_sec > tv_b.tv_sec) {
			retval = 1;
		} else {
			if (tv_a.tv_usec < tv_b.tv_usec) {
				retval = -1;
			} else if (tv_a.tv_usec > tv_b.tv_usec) {
				retval = 1;
			} else {
				retval = 0;
			}
		}
		break;
	}
	case TOP_SORT_uid:
		/* Handle the == case first, since it's common. */
		if (a_a->uid == a_b->uid) {
			retval = 0;
		} else if (a_a->uid < a_b->uid) {
			retval = -1;
		} else {
			retval = 1;
		}
		break;
	case TOP_SORT_username:
		/* Handle the == case first, since it's common. */
		if (a_a->uid == a_b->uid) {
			retval = 0;
		} else {
			const char	*user_a, *user_b;

			user_a = libtop_username(a_a->uid);
			user_b = libtop_username(a_b->uid);

			retval = strcmp(user_a, user_b);
			if (retval < 0) {
				retval = -1;
			} else if (retval > 0) {
				retval = 1;
			}
		}
		break;
	case TOP_SORT_vprvt:
		if (a_a->vprvt < a_b->vprvt) {
			retval = -1;
		} else if (a_a->vprvt > a_b->vprvt) {
			retval = 1;
		} else {
			retval = 0;
		}
		break;
	case TOP_SORT_vsize:
		if (a_a->vsize < a_b->vsize) {
			retval = -1;
		} else if (a_a->vsize > a_b->vsize) {
			retval = 1;
		} else {
			retval = 0;
		}
		break;
	default:
		assert(0);
		break;
	}

	if (a_ascend == FALSE) {
		/* Reverse the result. */
		retval *= -1;
	}

	return retval;
}

/* Print results of a sample. */
static boolean_t
samp_p_print(void)
{
	boolean_t		retval;
	const libtop_psamp_t	*psamp;
	unsigned		i;

#ifdef TOP_DEPRECATED
	if (top_opt_x) {
		/* Header and legend. */
		if (samp_p_deprecated_header_print()
		    || (top_opt_n > 0 && (samp_skipl()
		    || samp_p_deprecated_legend_print()))) {
			retval = TRUE;
			goto RETURN;
		}

		/* Processes. */
		for (i = 1, psamp = libtop_piterate();
		     psamp != NULL && i <= top_opt_n;
		     psamp = libtop_piterate()) {
			if (samp_p_deprecated_proc_print(psamp)) {
				retval = TRUE;
				goto RETURN;
			}
			i++;
		}
	} else {
		/* Header and legend. */
		if (samp_p_header_print()
		    || (top_opt_n > 0 && (samp_skipl()
		    || samp_p_legend_print()))) {
			retval = TRUE;
			goto RETURN;
		}

		/* Processes. */
		for (i = 1, psamp = libtop_piterate();
		     psamp != NULL && i <= top_opt_n;
		     psamp = libtop_piterate()) {
			if (top_opt_U == FALSE || psamp->uid == top_opt_U_uid) {
				if (samp_p_proc_print(psamp)) {
					retval = TRUE;
					goto RETURN;
				}
				i++;
			}
		}
	}
#else
	/* Header and legend. */
	if (samp_p_header_print()
	    || (top_opt_n > 0 && (samp_skipl()
	    || samp_p_legend_print()))) {
		retval = TRUE;
		goto RETURN;
	}

	/* Processes. */
	for (i = 1, psamp = libtop_piterate();
	     psamp != NULL && i <= top_opt_n;
	     psamp = libtop_piterate()) {
		if (top_opt_U == FALSE || psamp->uid == top_opt_U_uid) {
			if (samp_p_proc_print(psamp)) {
				retval = TRUE;
				goto RETURN;
			}
			i++;
		}
	}
#endif

	retval = FALSE;
	RETURN:
	return retval;
}

/* Print header. */
static boolean_t
samp_p_header_print(void)
{
	boolean_t	retval;
	struct timeval	tval;
	struct tm	tm;
	time_t		time;
	unsigned	i;
	unsigned long long userticks, systicks, idleticks, totalticks;
	float		cpu_user, cpu_sys, cpu_idle;
	unsigned long long mem_wired, mem_active, mem_inactive, mem_used;
	unsigned long long mem_free;
	unsigned long long n_ipackets, n_opackets, n_ibytes, n_obytes;
	unsigned long long d_rops, d_wops, d_rbytes, d_wbytes;
	unsigned long long purgeable_mem;
	/* Make big enough for 100 years: " (HHHHHH:MM:SS)". */
	char		time_acc[16];
	/* Make strings large enough for ", NNNNN <MAXLEN>\0". */
	char		sstr[LIBTOP_NSTATES][LIBTOP_STATE_MAXLEN + 9];
	char		nstr[LIBTOP_NSTATES][6];
	char		loadavg[3][6];
	char		cpu[3][7];
	char		fw_code[6], fw_data[6], fw_linkedit[6];
	char		vprvt[6], fw_private[6], rshrd[6];
	char		wired[6], active[6], inactive[6], used[6], free[6];
	char		vsize[6], fw_vsize[6];
	natural_t	pageins, pageouts;
	char		net_ibytes[6], net_obytes[6];
	char		disk_rbytes[6], disk_wbytes[6];
	char		xsu_total[6], xsu_used[6], xsu_avail[6];
	char		purgeable[6];


	/* Get the date and time. */
	gettimeofday(&tval, NULL);
	time = tval.tv_sec;
	localtime_r(&time, &tm);

	if (top_opt_c == 'a') {
		struct timeval	time;
		unsigned	sec, min, hour;

		/* Print time since start. */
		timersub(&samp_tsamp->time, &samp_tsamp->b_time, &time);
		sec = time.tv_sec;
		min = sec / 60;
		hour = min / 60;

		snprintf(time_acc, sizeof(time_acc), " (%u:%02u:%02u)",
		    hour, min % 60, sec % 60);
	} else {
		time_acc[0] = '\0';
	}

	/* Render state count strings. */
	for (i = 0; i < LIBTOP_NSTATES; i++) {
		if (samp_tsamp->state_breakdown[i] != 0) {
			sprintf(sstr[i], ", %s %s",
			    samp_p_unsigned_render(
			    samp_tsamp->state_breakdown[i], nstr[i],
			    sizeof(nstr[i])), libtop_state_str(i));
		} else {
			sstr[i][0] = '\0';
		}
	}

	/* Calculate CPU usage. */
	switch (top_opt_c) {
	case 'a':
		userticks = (samp_tsamp->cpu.cpu_ticks[CPU_STATE_USER]
		    + samp_tsamp->cpu.cpu_ticks[CPU_STATE_NICE])
		    - (samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_USER]
		    + samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_NICE]);
		systicks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_SYSTEM]
		    - samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_SYSTEM];
		idleticks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_IDLE]
		    - samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_IDLE];
		break;
	case 'd':
	case 'n':
		userticks = (samp_tsamp->cpu.cpu_ticks[CPU_STATE_USER]
		    + samp_tsamp->cpu.cpu_ticks[CPU_STATE_NICE])
		    - (samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_USER]
		    + samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_NICE]);
		systicks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_SYSTEM]
		    - samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_SYSTEM];
		idleticks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_IDLE]
		    - samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_IDLE];
		break;
	case 'e':
		userticks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_USER]
		    + samp_tsamp->cpu.cpu_ticks[CPU_STATE_NICE];
		systicks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_SYSTEM];
		idleticks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_IDLE];
		break;
	default:
		assert(0);
	}
	totalticks = userticks + systicks + idleticks;
	if (totalticks != 0) {
		cpu_user = ((float)(100 * userticks)) / ((float)totalticks);
		cpu_sys = ((float)(100 * systicks)) / ((float)totalticks);
		cpu_idle = ((float)(100 * idleticks)) / ((float)totalticks);
	} else {
		cpu_user = 0.0;
		cpu_sys = 0.0;
		cpu_idle = 0.0;
	}

	mem_wired = (unsigned long long) samp_tsamp->vm_stat.wire_count * samp_tsamp->pagesize;
	mem_active = (unsigned long long) samp_tsamp->vm_stat.active_count * samp_tsamp->pagesize;
	mem_inactive = (unsigned long long) samp_tsamp->vm_stat.inactive_count * samp_tsamp->pagesize;
	mem_used = (unsigned long long) mem_wired + mem_active + mem_inactive;
	mem_free = (unsigned long long) samp_tsamp->vm_stat.free_count * samp_tsamp->pagesize;

	if (samp_println("\
Time: %04d/%02d/%02d %02d:%02d:%02d%s.  Threads: %u.  Procs: %u%s%s%s%s%s%s%s.",
	    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	    tm.tm_hour, tm.tm_min, tm.tm_sec,
	    time_acc,
	    samp_tsamp->threads, samp_tsamp->nprocs,
	    sstr[0], sstr[1], sstr[2], sstr[3], sstr[4], sstr[5], sstr[6])
	    || samp_println("\
LoadAvg: %5s, %5s, %5s.  CPU: %5s%% user, %5s%% sys, %5s%% idle.",
	    samp_p_float_render(samp_tsamp->loadavg[0], 2, loadavg[0],
	    sizeof(loadavg[0])),
	    samp_p_float_render(samp_tsamp->loadavg[1], 2, loadavg[1],
	    sizeof(loadavg[1])),
	    samp_p_float_render(samp_tsamp->loadavg[2], 2, loadavg[2],
	    sizeof(loadavg[2])),
	    samp_p_float_render(cpu_user, 1, cpu[0], sizeof(cpu[0])),
	    samp_p_float_render(cpu_sys, 1, cpu[1], sizeof(cpu[1])),
	    samp_p_float_render(cpu_idle, 1, cpu[2], sizeof(cpu[2])))
	    ) {
		retval = TRUE;
		goto RETURN;
	}

	if (top_opt_f) {
		if (samp_println("\
SharedLibs: num = %4u, resident = %5s code, %5s data, %5s linkedit.",
		    samp_tsamp->fw_count,
		    samp_p_vm_size_render(samp_tsamp->fw_code, fw_code,
		    sizeof(fw_code)),
		    samp_p_vm_size_render(samp_tsamp->fw_data, fw_data,
		    sizeof(fw_data)),
		    samp_p_vm_size_render(samp_tsamp->fw_linkedit, fw_linkedit,
		    sizeof(fw_linkedit)))) {
			retval = TRUE;
			goto RETURN;
		}
	}

	if (top_opt_r) {
		if (samp_println("\
MemRegions: num = %5u, resident = %5s + %5s private, %5s shared.",
		    samp_tsamp->reg,
		    samp_p_vm_size_render(samp_tsamp->rprvt
		    - samp_tsamp->fw_private, vprvt, sizeof(vprvt)),
		    samp_p_vm_size_render(samp_tsamp->fw_private, fw_private,
		    sizeof(fw_private)),
		    samp_p_vm_size_render(samp_tsamp->rshrd, rshrd,
		    sizeof(rshrd)))) {
			retval = TRUE;
			goto RETURN;
		}
	}

	switch (top_opt_c) {
	case 'a':
		pageins = samp_tsamp->vm_stat.pageins
		    - samp_tsamp->b_vm_stat.pageins;
		pageouts = samp_tsamp->vm_stat.pageouts
		    - samp_tsamp->b_vm_stat.pageouts;
		break;
	case 'd':
		pageins = samp_tsamp->vm_stat.pageins
		    - samp_tsamp->p_vm_stat.pageins;
		pageouts = samp_tsamp->vm_stat.pageouts
		    - samp_tsamp->p_vm_stat.pageouts;
		break;
	case 'e':
	case 'n':
		pageins = samp_tsamp->vm_stat.pageins;
		pageouts = samp_tsamp->vm_stat.pageouts;
		break;
	default:
		assert(0);
	}

	if (samp_println("\
PhysMem: %5s wired, %5s active, %5s inactive, %5s used, %5s free.",
	    samp_p_vm_size_render(mem_wired, wired, sizeof(wired)),
	    samp_p_vm_size_render(mem_active, active, sizeof(active)),
	    samp_p_vm_size_render(mem_inactive, inactive, sizeof(inactive)),
	    samp_p_vm_size_render(mem_used, used, sizeof(used)),
	    samp_p_vm_size_render(mem_free, free, sizeof(free)))) {
		retval = TRUE;
		goto RETURN;
	}
	if (top_opt_f) {
		if (samp_println("\
VirtMem: %5s + %5s, %10u pagein%s, %10u pageout%s.",
		    samp_p_vm_size_render(samp_tsamp->vsize, vsize,
		    sizeof(vsize)),
		    samp_p_vm_size_render(samp_tsamp->fw_vsize, fw_vsize,
		    sizeof(fw_vsize)),
		    pageins, pageins == 1 ? "" : "s",
		    pageouts, pageouts == 1 ? "" : "s")
		    ) {
			retval = TRUE;
			goto RETURN;
		}
	} else {
		/* Do not report fw_vsize. */
		if (samp_println("\
VirtMem: %5s, %10u pagein%s %10u pageout%s.",
		    samp_p_vm_size_render(samp_tsamp->vsize, vsize,
		    sizeof(vsize)),
		    pageins, pageins == 1 ? ", " : "s,",
		    pageouts, pageouts == 1 ? "" : "s")
		    ) {
			retval = TRUE;
			goto RETURN;
		}
	}

	if (top_opt_S) {
		purgeable_mem = (samp_tsamp->vm_stat.purgeable_count *
				 samp_tsamp->pagesize);

		if (samp_println("Swap: %5s total, %5s used, %5s free.  "
				 "Purgeable: %5s  %10u purges",
				 (samp_tsamp->xsu_is_valid ?
				  samp_p_vm_size_render(
					  samp_tsamp->xsu.xsu_total,
					  xsu_total,
					  sizeof (xsu_total)) :
				  "n/a"),
				 (samp_tsamp->xsu_is_valid ?
				  samp_p_vm_size_render(
					  samp_tsamp->xsu.xsu_used,
					  xsu_used,
					  sizeof (xsu_used)) :
				  "n/a"),
				 (samp_tsamp->xsu_is_valid ?
				  samp_p_vm_size_render(
					  samp_tsamp->xsu.xsu_avail,
					  xsu_avail,
					  sizeof (xsu_avail)) :
				  "n/a"),
				 (samp_tsamp->purgeable_is_valid ?
				  samp_p_vm_size_render(
					  purgeable_mem,
					  purgeable,
					  sizeof (purgeable)) :
				  "n/a"),
				 samp_tsamp->vm_stat.purges)) {
			retval = TRUE;
			goto RETURN;
		}
	}

	switch (top_opt_c) {
	case 'a':
		n_ipackets = samp_tsamp->net_ipackets
		    - samp_tsamp->b_net_ipackets;
		n_opackets = samp_tsamp->net_opackets
		    - samp_tsamp->b_net_opackets;
		n_ibytes = samp_tsamp->net_ibytes - samp_tsamp->b_net_ibytes;
		n_obytes = samp_tsamp->net_obytes - samp_tsamp->b_net_obytes;

		d_rops = samp_tsamp->disk_rops - samp_tsamp->b_disk_rops;
		d_wops = samp_tsamp->disk_wops - samp_tsamp->b_disk_wops;
		d_rbytes = samp_tsamp->disk_rbytes - samp_tsamp->b_disk_rbytes;
		d_wbytes = samp_tsamp->disk_wbytes - samp_tsamp->b_disk_wbytes;
		break;
	case 'd':
		n_ipackets = samp_tsamp->net_ipackets
		    - samp_tsamp->p_net_ipackets;
		n_opackets = samp_tsamp->net_opackets
		    - samp_tsamp->p_net_opackets;
		n_ibytes = samp_tsamp->net_ibytes - samp_tsamp->p_net_ibytes;
		n_obytes = samp_tsamp->net_obytes - samp_tsamp->p_net_obytes;

		d_rops = samp_tsamp->disk_rops - samp_tsamp->p_disk_rops;
		d_wops = samp_tsamp->disk_wops - samp_tsamp->p_disk_wops;
		d_rbytes = samp_tsamp->disk_rbytes - samp_tsamp->p_disk_rbytes;
		d_wbytes = samp_tsamp->disk_wbytes - samp_tsamp->p_disk_wbytes;
		break;
	case 'e':
		n_ipackets = samp_tsamp->net_ipackets;
		n_opackets = samp_tsamp->net_opackets;
		n_ibytes = samp_tsamp->net_ibytes;
		n_obytes = samp_tsamp->net_obytes;

		d_rops = samp_tsamp->disk_rops;
		d_wops = samp_tsamp->disk_wops;
		d_rbytes = samp_tsamp->disk_rbytes;
		d_wbytes = samp_tsamp->disk_wbytes;
		break;
	case 'n':
		/* Skip network and disk statistics. */
		retval = FALSE;
		goto RETURN;
	default:
		assert(0);
	}

	if (samp_println("\
Networks: packets = %10llu in, %10llu out, data = %5s in, %5s out.",
	    n_ipackets, n_opackets,
	    samp_p_vm_size_render(n_ibytes, net_ibytes, sizeof(net_ibytes)),
	    samp_p_vm_size_render(n_obytes, net_obytes, sizeof(net_obytes)))
	    || samp_println("\
Disks: operations = %10llu in, %10llu out, data = %5s in, %5s out.",
	    d_rops, d_wops,
	    samp_p_vm_size_render(d_rbytes, disk_rbytes, sizeof(disk_rbytes)),
	    samp_p_vm_size_render(d_wbytes, disk_wbytes, sizeof(disk_wbytes)))
	    ) {
		retval = TRUE;
		goto RETURN;
	}

	retval = FALSE;
	RETURN:
	return retval;
}

/* Print legend for process information. */
static boolean_t
samp_p_legend_print(void)
{
	boolean_t	retval;
	const char	*user;
	const char	*reg, *rprvt, *rshrd, *rsize, *vprvt, *vsize;
	const char	*faults, *pageins, *cow_faults, *msgs_sent;
	const char	*msgs_rcvd, *bsyscall, *msyscall, *cswitch;
	const char	*prt;

	if (top_opt_t) {
		user = " USERNAME";
	} else {
		user = "   UID";
	}

	if (top_opt_r) {
		reg = "  REG";
		if (top_opt_w) {
			rprvt = " RPRVT( delta)";
			rshrd = " RSHRD( delta)";
			rsize = " RSIZE( delta)";
			vprvt = " VPRVT( delta)";
			vsize = " VSIZE( delta)";
		} else {
			rprvt = " RPRVT ";
			rshrd = " RSHRD ";
			rsize = " RSIZE ";
			vprvt = " VPRVT ";
			vsize = " VSIZE ";
		}
	} else {
		reg = "";
		rprvt = "";
		rshrd = "";
		vprvt = "";
		if (top_opt_w) {
			rsize = " RSIZE( delta)";
			vsize = " VSIZE( delta)";
		} else {
			rsize = " RSIZE ";
			vsize = " VSIZE ";
		}
	}

	if (top_opt_w) {
		prt = " PRT(delta)";
	} else {
		prt = " PRT ";
	}

	switch (top_opt_c) {
	case 'a':
	case 'd':
	case 'e':
		faults = "     FAULTS";
		pageins = "    PAGEINS";
		cow_faults = " COW_FAULTS";
		msgs_sent = "  MSGS_SENT";
		msgs_rcvd = "  MSGS_RCVD";
		bsyscall = "   BSYSCALL";
		msyscall = "   MSYSCALL";
		cswitch = "    CSWITCH";
		break;
	case 'n':
		faults = "";
		pageins = "";
		cow_faults = "";
		msgs_sent = "";
		msgs_rcvd = "";
		bsyscall = "";
		msyscall = "";
		cswitch = "";
		break;
	default:
		assert(0);
	}
		
	if (samp_println("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
	    "  PID",
	    user,
	    reg, rprvt, rshrd, rsize, vprvt, vsize,
	    " TH",
	    prt,
	    faults, pageins, cow_faults, msgs_sent, msgs_rcvd,
	    bsyscall, msyscall, cswitch,
	    "   TIME",
	    "  %CPU",
	    " COMMAND"
	    )) {
		retval = TRUE;
		goto RETURN;
	}

	retval = FALSE;
	RETURN:
	return retval;
}

/* Print information for a psamp. */
static boolean_t
samp_p_proc_print(const libtop_psamp_t *a_psamp)
{
	boolean_t	retval;
	char		user[9];
	char		reg[5], rprvt[6], rshrd[6], rsize[6];
	char		vprvt[6], vsize[6];
	char		th[3], prt[4];
	char		faults[11], pageins[11], cow_faults[11];
	char		msgs_sent[11], msgs_rcvd[11];
	char		bsyscall[11], msyscall[11], cswitch[11];
	char		time[7], cpu[6];
	const char	*format;
	const char	*c_prt, *c_rprvt, *c_rshrd, *c_rsize;
	const char	*c_vprvt, *c_vsize;
	unsigned	l_user, l_reg, l_rprvt, l_rshrd, l_vprvt;
	unsigned	l_faults, l_pageins, l_cow_faults;
	unsigned	l_msgs_sent, l_msgs_rcvd;
	unsigned	l_bsyscall, l_msyscall, l_cswitch;
	unsigned	l_c_prt, l_c_rprvt, l_c_rshrd, l_c_rsize;
	unsigned	l_c_vprvt, l_c_vsize;
	char		d_prt[8], d_rprvt[9], d_rshrd[9], d_rsize[9];
	char		d_vprvt[9], d_vsize[9];

	/*
	 * Render username/uid.
	 *
	 * The format string is set here, since there is no other reasonable way
	 * to deal with the left/right justification of username/uid.
	 */
	if (top_opt_t) {
		l_user = sizeof("USERNAME") - 1;
		samp_p_username_render(a_psamp->uid, user, l_user + 1);
		format = "%5u %-*s%*s%*s%*s%*s%*s %5s%*s%*s%*s %5s%*s %2s "
		    "%3s%*s%*s%*s%*s%*s%*s%*s%*s%*s %6s %5s %s";
	} else {
		l_user = sizeof("  UID") - 1;
		samp_p_unsigned_render(a_psamp->uid, user, l_user + 1);
		format = "%5u %*s%*s%*s%*s%*s%*s %5s%*s%*s%*s %5s%*s %2s "
		    "%3s%*s%*s%*s%*s%*s%*s%*s%*s%*s %6s %5s %s";
	}

	/*
	 * Render memory statistics.
	 */
	if (top_opt_r) {
		l_reg = sizeof(" REG");
		samp_p_unsigned_render(a_psamp->reg, reg, sizeof(reg));

		l_rprvt = sizeof("RPRVT");
		samp_p_vm_size_render(a_psamp->rprvt, rprvt, sizeof(rprvt));

		l_rshrd = sizeof("RSHRD");
		samp_p_vm_size_render(a_psamp->rshrd, rshrd, sizeof(rshrd));

		l_vprvt = sizeof("VPRVT");
		samp_p_vm_size_render(a_psamp->vprvt, vprvt, sizeof(vprvt));

		if (top_opt_w) {
			l_c_rprvt = sizeof("  delta");
			c_rprvt = d_rprvt;
			samp_p_vm_size_delta(a_psamp->rprvt, a_psamp->p_rprvt,
			    d_rprvt, sizeof(d_rprvt));

			l_c_rshrd = sizeof("  delta");
			c_rshrd = d_rshrd;
			samp_p_vm_size_delta(a_psamp->rshrd, a_psamp->p_rshrd,
			    d_rshrd, sizeof(d_rshrd));

			l_c_vprvt = sizeof("  delta");
			c_vprvt = d_vprvt;
			samp_p_vm_size_delta(a_psamp->vprvt, a_psamp->p_vprvt,
			    d_vprvt, sizeof(d_vprvt));
		} else {
			l_c_rprvt = 1;
			c_rprvt = samp_p_vm_size_change(a_psamp->p_rprvt,
			    a_psamp->rprvt, a_psamp->p_seq);

			l_c_rshrd = 1;
			c_rshrd = samp_p_vm_size_change(a_psamp->p_rshrd,
			    a_psamp->rshrd, a_psamp->p_seq);

			l_c_vprvt = 1;
			c_vprvt = samp_p_vm_size_change(a_psamp->p_vprvt,
			    a_psamp->vprvt, a_psamp->p_seq);
		}
	} else {
		l_reg = 0;
		reg[0] = '\0';

		l_rprvt = 0;
		rprvt[0] = '\0';
		l_c_rprvt = 0;
		c_rprvt = "";

		l_rshrd = 0;
		rshrd[0] = '\0';
		l_c_rshrd = 0;
		c_rshrd = "";

		l_vprvt = 0;
		vprvt[0] = '\0';
		l_c_vprvt = 0;
		c_vprvt = "";
	}
	samp_p_vm_size_render(a_psamp->rsize, rsize, sizeof(rsize));
	samp_p_vm_size_render(a_psamp->vsize, vsize, sizeof(vsize));
	if (top_opt_w) {
		l_c_rsize = sizeof("  delta");
		c_rsize = d_rsize;
		samp_p_vm_size_delta(a_psamp->rsize, a_psamp->p_rsize,
		    d_rsize, sizeof(d_rsize));

		l_c_vsize = sizeof("  delta");
		c_vsize = d_vsize;
		samp_p_vm_size_delta(a_psamp->vsize, a_psamp->p_vsize,
		    d_vsize, sizeof(d_vsize));
	} else {
		l_c_rsize = 1;
		c_rsize = samp_p_vm_size_change(a_psamp->p_rsize,
		    a_psamp->rsize, a_psamp->p_seq);

		l_c_vsize = 1;
		c_vsize = samp_p_vm_size_change(a_psamp->p_vsize,
		    a_psamp->vsize, a_psamp->p_seq);
	}

	/* Render number of threads. */
	samp_p_unsigned_render(a_psamp->th, th, sizeof(th));

	/* Render Number of ports. */
	samp_p_unsigned_render(a_psamp->prt, prt, sizeof(prt));
	if (top_opt_w) {
		l_c_prt = sizeof(" delta");
		c_prt = d_prt;
		samp_p_unsigned_delta(a_psamp->prt, a_psamp->p_prt, d_prt,
		    sizeof(d_prt));
	} else {
		l_c_prt = 1;
		c_prt = samp_p_unsigned_change(a_psamp->p_prt, a_psamp->prt,
		    a_psamp->p_seq);
	}

	/* Render event counters. */
	switch (top_opt_c) {
	case 'a':
		l_faults = sizeof("    FAULTS");
		samp_p_int_render(a_psamp->events.faults
		    - a_psamp->b_events.faults, faults, sizeof(faults));

		l_pageins = sizeof("   PAGEINS");
		samp_p_int_render(a_psamp->events.pageins
		    - a_psamp->b_events.pageins, pageins, sizeof(pageins));

		l_cow_faults = sizeof("COW_FAULTS");
		samp_p_int_render(a_psamp->events.cow_faults
		    - a_psamp->b_events.cow_faults, cow_faults,
		    sizeof(cow_faults));

		l_msgs_sent = sizeof(" MSGS_SENT");
		samp_p_int_render(a_psamp->events.messages_sent
		    - a_psamp->b_events.messages_sent, msgs_sent,
		    sizeof(msgs_sent));

		l_msgs_rcvd = sizeof(" MSGS_RCVD");
		samp_p_int_render(a_psamp->events.messages_received
		    - a_psamp->b_events.messages_received, msgs_rcvd,
		    sizeof(msgs_rcvd));

		l_bsyscall = sizeof("  BSYSCALL");
		samp_p_int_render(a_psamp->events.syscalls_unix
		    - a_psamp->b_events.syscalls_unix, bsyscall,
		    sizeof(bsyscall));

		l_msyscall = sizeof("  MSYSCALL");
		samp_p_int_render(a_psamp->events.syscalls_mach
		    - a_psamp->b_events.syscalls_mach, msyscall,
		    sizeof(msyscall));

		l_cswitch = sizeof("   CSWITCH");
		samp_p_int_render(a_psamp->events.csw
		    - a_psamp->b_events.csw, cswitch, sizeof(cswitch));
		break;
	case 'd':
		l_faults = sizeof("    FAULTS");
		samp_p_int_render(a_psamp->events.faults
		    - a_psamp->p_events.faults, faults, sizeof(faults));

		l_pageins = sizeof("   PAGEINS");
		samp_p_int_render(a_psamp->events.pageins
		    - a_psamp->p_events.pageins, pageins, sizeof(pageins));

		l_cow_faults = sizeof("COW_FAULTS");
		samp_p_int_render(a_psamp->events.cow_faults
		    - a_psamp->p_events.cow_faults, cow_faults,
		    sizeof(cow_faults));

		l_msgs_sent = sizeof(" MSGS_SENT");
		samp_p_int_render(a_psamp->events.messages_sent
		    - a_psamp->p_events.messages_sent, msgs_sent,
		    sizeof(msgs_sent));

		l_msgs_rcvd = sizeof(" MSGS_RCVD");
		samp_p_int_render(a_psamp->events.messages_received
		    - a_psamp->p_events.messages_received, msgs_rcvd,
		    sizeof(msgs_rcvd));

		l_bsyscall = sizeof("  BSYSCALL");
		samp_p_int_render(a_psamp->events.syscalls_unix
		    - a_psamp->p_events.syscalls_unix, bsyscall,
		    sizeof(bsyscall));

		l_msyscall = sizeof("  MSYSCALL");
		samp_p_int_render(a_psamp->events.syscalls_mach
		    - a_psamp->p_events.syscalls_mach, msyscall,
		    sizeof(msyscall));

		l_cswitch = sizeof("   CSWITCH");
		samp_p_int_render(a_psamp->events.csw
		    - a_psamp->p_events.csw, cswitch, sizeof(cswitch));
		break;
	case 'e':
		l_faults = sizeof("    FAULTS");
		samp_p_int_render(a_psamp->events.faults, faults,
		    sizeof(faults));

		l_pageins = sizeof("   PAGEINS");
		samp_p_int_render(a_psamp->events.pageins, pageins,
		    sizeof(pageins));

		l_cow_faults = sizeof("COW_FAULTS");
		samp_p_int_render(a_psamp->events.cow_faults, cow_faults,
		    sizeof(cow_faults));

		l_msgs_sent = sizeof(" MSGS_SENT");
		samp_p_int_render(a_psamp->events.messages_sent, msgs_sent,
		    sizeof(msgs_sent));

		l_msgs_rcvd = sizeof(" MSGS_RCVD");
		samp_p_int_render(a_psamp->events.messages_received,
		    msgs_rcvd, sizeof(msgs_rcvd));

		l_bsyscall = sizeof("  BSYSCALL");
		samp_p_int_render(a_psamp->events.syscalls_unix, bsyscall,
		    sizeof(bsyscall));

		l_msyscall = sizeof("  MSYSCALL");
		samp_p_int_render(a_psamp->events.syscalls_mach, msyscall,
		    sizeof(msyscall));

		l_cswitch = sizeof("   CSWITCH");
		samp_p_int_render(a_psamp->events.csw, cswitch,
		    sizeof(cswitch));
		break;
	case 'n':
		l_faults = 0;
		faults[0] = '\0';

		l_pageins = 0;
		pageins[0] = '\0';

		l_cow_faults = 0;
		cow_faults[0] = '\0';

		l_msgs_sent = 0;
		msgs_sent[0] = '\0';

		l_msgs_rcvd = 0;
		msgs_rcvd[0] = '\0';

		l_bsyscall = 0;
		bsyscall[0] = '\0';

		l_msyscall = 0;
		msyscall[0] = '\0';

		l_cswitch = 0;
		cswitch[0] = '\0';
		break;
	default:
		assert(0);
	}

	/* Render time. */
	samp_p_time_render(a_psamp, time, sizeof(time));

	/* Render CPU usage. */
	samp_p_usage_render(a_psamp, cpu, sizeof(cpu));

	/* Print. */
	if (samp_println(format,
	    a_psamp->pid,
	    l_user, user,
	    l_reg, reg,
	    l_rprvt, rprvt, l_c_rprvt, c_rprvt,
	    l_rshrd, rshrd, l_c_rshrd, c_rshrd,
	    rsize, l_c_rsize, c_rsize,
	    l_vprvt, vprvt, l_c_vprvt, c_vprvt,
	    vsize, l_c_vsize, c_vsize,
	    th,
	    prt, l_c_prt, c_prt,
	    l_faults, faults,
	    l_pageins, pageins,
	    l_cow_faults, cow_faults,
	    l_msgs_sent, msgs_sent,
	    l_msgs_rcvd, msgs_rcvd,
	    l_bsyscall, bsyscall,
	    l_msyscall, msyscall,
	    l_cswitch, cswitch,
	    time, cpu, a_psamp->command)) {
		retval = TRUE;
		goto RETURN;
	}

	retval = FALSE;
	RETURN:
	return retval;
}

/*
 * Return " ", "+", or "-", depending on whether a VM size stayed the same,
 * increased, or decreased, respectively.
 */
static const char *
samp_p_vm_size_change(unsigned long long a_prev, unsigned long long a_curr,
    unsigned a_prev_seq)
{
	const char	*retval;

	if (a_prev_seq == 0 || a_prev == a_curr) {
		retval = " ";
	} else if (a_prev < a_curr) {
		retval = "+";
	} else {
		retval = "-";
	}

	return retval;
}

/*
 * Render a VM size delta.  If a_size and a_prev differ, the delta is rendered
 * in parentheses.  Otherwise a zero-length string is rendered.
 */
static char *
samp_p_vm_size_delta(unsigned long long a_size, unsigned long long a_prev,
    char *a_buf, unsigned a_bufsize)
{
	char		buf[7], *p;

	assert(a_bufsize >= 9);

	if (a_size == a_prev) {
		a_buf[0] = '\0';
	} else if (a_size > a_prev) {
		/* Growth. */
		samp_p_vm_size_render(a_size - a_prev, buf, sizeof(buf));
		snprintf(a_buf, a_bufsize, "(%*s)", a_bufsize - 3, buf);
	} else {
		/* Shrinkage.  Prepend a nestled '-' to the value. */
		samp_p_vm_size_render(a_prev - a_size, &buf[1],
		    sizeof(buf) - 1);

		buf[0] = ' ';
		for (p = buf; *p == ' '; p++) {
			/* Do nothing. */
			assert(*p != '\0');
		}
		p--;
		*p = '-';
		snprintf(a_buf, a_bufsize, "(%*s)", a_bufsize - 3, buf);
	}

	return a_buf;
}

/*
 * Return " ", "+", or "-", depending on whether an unsigned value stayed the
 * same, increased, or decreased, respectively.
 */
static const char *
samp_p_unsigned_change(unsigned a_prev, unsigned a_curr, unsigned a_prev_seq)
{
	const char	*retval;

	if (a_prev_seq == 0 || a_prev == a_curr) {
		retval = " ";
	} else if (a_prev < a_curr) {
		retval = "+";
	} else {
		retval = "-";
	}

	return retval;
}

/*
 * Render a delta between unsigned numbers.  If a_size and a_prev differ, the
 * delta is rendered in parentheses.  Otherwise a zero-length string is
 * rendered.
 */
static char *
samp_p_unsigned_delta(unsigned a_size, unsigned a_prev, char *a_buf,
    unsigned a_bufsize)
{
	char	buf[6];

	assert(a_bufsize >= 8);

	if (a_size == a_prev) {
		a_buf[0] = '\0';
	} else {
		samp_p_int_render((int)(a_size - a_prev), buf, sizeof(buf));
		snprintf(a_buf, a_bufsize, "(%*s)", a_bufsize - 3, buf);
	}

	return a_buf;
}

/* Render a float, or render an overflow string if the value won't fit. */
static char *
samp_p_float_render(float a_float, int a_max_frac_digits, char *a_buf,
    unsigned a_bufsize)
{
	int	len;

	assert(a_float >= 0.0);
	assert(a_bufsize >= 2);

	len = a_bufsize - 1;
	while (a_max_frac_digits >= 0
	    && (len = snprintf(a_buf, a_bufsize, "%.*f",
	    a_max_frac_digits--, a_float)) >= a_bufsize) {
		/* Do nothing. */
	}
	if (len >= a_bufsize) {
		/* Not enough room to display the actual number. */
		memset(a_buf, '>', a_bufsize - 1);
		a_buf[a_bufsize - 1] = '\0';
	}

	return a_buf;
}

/* Render an integer, or render an overflow string if the value won't fit. */
static char *
samp_p_int_render(int a_int, char *a_buf, unsigned a_bufsize)
{
	int	len;

	assert(a_bufsize >= 2);

	len = snprintf(a_buf, a_bufsize, "%d", a_int);
	if (len >= a_bufsize) {
		/* Not enough room to display the actual number. */
		memset(a_buf, '>', a_bufsize - 1);
		a_buf[a_bufsize - 1] = '\0';
	}

	return a_buf;
}

/*
 * Render an unsigned integer, or render an overflow string if the value won't
 * fit.
 */
static char *
samp_p_unsigned_render(unsigned a_unsigned, char *a_buf, unsigned a_bufsize)
{
	int	len;

	assert(a_bufsize >= 2);

	len = snprintf(a_buf, a_bufsize, "%u", a_unsigned);
	if (len >= a_bufsize) {
		/* Not enough room to display the actual number. */
		memset(a_buf, '>', a_bufsize - 1);
		a_buf[a_bufsize - 1] = '\0';
	}

	return a_buf;
}

/* Render the username associated with a_uid. */
static char *
samp_p_username_render(uid_t a_uid, char *a_buf, unsigned a_bufsize)
{
	const char	*user;

	user = libtop_username(a_uid);
	if (user != NULL) {
		strlcpy(a_buf, user, a_bufsize);
	} else {
		strlcpy(a_buf, "???", a_bufsize);
	}

	return a_buf;
}

/*
 * Render a memory size in units of B, K, M, or G, depending on the value.
 *
 * a_size is ULL, since there are places where VM sizes are capable of
 * overflowing 32 bits, particularly when VM stats are multiplied by the
 * pagesize.
 */
static char *
samp_p_vm_size_render(unsigned long long a_size, char *a_buf,
    unsigned a_bufsize)
{
	assert(a_bufsize >= 6);

	if (a_size < 1024) {
		/* 1023B. */
		snprintf(a_buf, a_bufsize, "%4lluB", a_size);
	} else if (a_size < (1024ULL * 1024ULL)) {
		/* K. */
		if (a_size < 10ULL * 1024ULL) {
			/* 9.99K */
			snprintf(a_buf, a_bufsize, "%1.2fK",
			    ((double)a_size) / 1024);
		} else if (a_size < 100ULL * 1024ULL) {
			/* 99.9K */
			snprintf(a_buf, a_bufsize, "%2.1fK",
			    ((double)a_size) / 1024);
		} else {
			/* 1023K */
			snprintf(a_buf, a_bufsize, "%4lluK",
			    a_size / 1024ULL);
		}
	} else if (a_size < (1024ULL * 1024ULL * 1024ULL)) {
		/* M. */
		if (a_size < 10ULL * 1024ULL * 1024ULL) {
			/* 9.99M */
			snprintf(a_buf, a_bufsize, "%1.2fM",
			    ((double)a_size) / (1024 * 1024));
		} else if (a_size < 100ULL * 1024ULL * 1024ULL) {
			/* 99.9M */
			snprintf(a_buf, a_bufsize, "%2.1fM",
			    ((double)a_size) / (1024 * 1024));
		} else {
			/* 1023M */
			snprintf(a_buf, a_bufsize, "%4lluM",
			    a_size / (1024ULL * 1024ULL));
		}
	} else if (a_size < (1024ULL * 1024ULL * 1024ULL * 1024ULL)) {
		/* G. */
		if (a_size < 10ULL * 1024ULL * 1024ULL * 1024ULL) {
			/* 9.99G. */
			snprintf(a_buf, a_bufsize, "%1.2fG",
			    ((double)a_size) / (1024 * 1024 * 1024));
		} else if (a_size < 100ULL * 1024ULL * 1024ULL * 1024ULL) {
			/* 99.9G. */
			snprintf(a_buf, a_bufsize, "%2.1fG",
			    ((double)a_size) / (1024 * 1024 * 1024));
		} else {
			/* 1023G */
			snprintf(a_buf, a_bufsize, "%4lluG",
			    a_size / (1024ULL * 1024ULL * 1024ULL));
		}
	} else if (a_size < (1024ULL * 1024ULL * 1024ULL * 1024ULL)) {
		/* T. */
		if (a_size < 10ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL) {
			/* 9.99T. */
			snprintf(a_buf, a_bufsize, "%1.2fT",
				 ((double)a_size) /
				 (1024ULL * 1024ULL * 1024ULL * 1024ULL));
		} else if (a_size < (100ULL * 1024ULL * 1024ULL * 1024ULL
				     * 1024ULL)) {
			/* 99.9T. */
			snprintf(a_buf, a_bufsize, "%2.1fT",
				 ((double)a_size) /
				 (1024ULL * 1024ULL * 1024ULL * 1024ULL));
		} else {
			/* 1023T */
			snprintf(a_buf, a_bufsize, "%4lluT",
				 a_size /
				 (1024ULL * 1024ULL * 1024ULL * 1024ULL));
		}
	} else {
		/* P. */
		if (a_size < (10ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL
			      * 1024ULL)) {
			/* 9.99P. */
			snprintf(a_buf, a_bufsize, "%1.2fP",
				 ((double)a_size) /
				 (1024ULL * 1024ULL * 1024ULL * 1024ULL
				  * 1024ULL));
		} else if (a_size < (100ULL * 1024ULL * 1024ULL * 1024ULL
				     * 1024ULL)) {
			/* 99.9P. */
			snprintf(a_buf, a_bufsize, "%2.1fP",
				 ((double)a_size) /
				 (1024ULL * 1024ULL * 1024ULL * 1024ULL
				  * 1024ULL));
		} else {
			/* 1023P */
			snprintf(a_buf, a_bufsize, "%4lluP",
				 a_size /
				 (1024ULL * 1024ULL * 1024ULL * 1024ULL
				  * 1024ULL));
		}
	}

	return a_buf;
}

/* Render the percent of CPU usage for a_psamp. */
static char *
samp_p_usage_render(const libtop_psamp_t *a_psamp, char *a_buf,
    unsigned a_bufsize)
{
	int	whole, part;

	if (a_psamp->p_seq == 0) {
		whole = 0;
		part = 0;
	} else {
		struct timeval		elapsed_tv, used_tv;
		unsigned long long	elapsed_us, used_us;

		switch (top_opt_c) {
		case 'a':
			timersub(&samp_tsamp->time, &samp_tsamp->b_time,
			    &elapsed_tv);
			timersub(&a_psamp->total_time, &a_psamp->b_total_time,
			    &used_tv);
			break;
		case 'e':
		case 'd':
		case 'n':
			timersub(&samp_tsamp->time, &samp_tsamp->p_time,
			    &elapsed_tv);
			timersub(&a_psamp->total_time, &a_psamp->p_total_time,
			    &used_tv);
		break;
		default:
			assert(0);
		}

		elapsed_us = (unsigned long long)elapsed_tv.tv_sec * 1000000ULL
		    + (unsigned long long)elapsed_tv.tv_usec;
		used_us = (unsigned long long)used_tv.tv_sec * 1000000ULL
		    + (unsigned long long)used_tv.tv_usec;

		whole = (used_us * 100ULL) / elapsed_us;
		part = (((used_us * 100ULL) - (whole * elapsed_us)) * 10ULL)
		    / elapsed_us;
	}

	snprintf(a_buf, a_bufsize, "%3d.%01d", whole, part);

	return a_buf;
}

/* Render the time consumed by a_psamp. */
static char *
samp_p_time_render(const libtop_psamp_t *a_psamp, char *a_buf,
    unsigned a_bufsize)
{
	struct timeval	tv;
	unsigned	usec, sec, min, hour, day;

	switch (top_opt_c) {
	case 'a':
		timersub(&a_psamp->total_time, &a_psamp->b_total_time, &tv);
		break;
	case 'd':
	case 'e':
	case 'n':
		tv = a_psamp->total_time;
		break;
	default:
		assert(0);
	}
	usec = tv.tv_usec;
	sec = tv.tv_sec;
	min = sec / 60;
	hour = min / 60;
	day = hour / 24;

	if (sec < 60) {
		snprintf(a_buf, a_bufsize, "%u.%02us", sec, usec / 10000);
	} else if (min < 60) {
		snprintf(a_buf, a_bufsize, "%um%02us", min, sec - min * 60);
	} else if (hour < 24) {
		snprintf(a_buf, a_bufsize, "%uh%02um", hour, min - hour * 60);
	} else if (day < 100) {
		snprintf(a_buf, a_bufsize, "%ud%02uh", day, hour - day * 24);
	} else {
		snprintf(a_buf, a_bufsize, "%ud", day);
	}

	return a_buf;
}

#ifdef TOP_DEPRECATED
static boolean_t
samp_p_deprecated_header_print(void)
{
	boolean_t	retval;
	char		hbuf[120], buf[81];
	int		width, len;
	struct timeval	tval;
	struct tm	tm;
	time_t		time;
	unsigned	i;
	unsigned long long userticks, systicks, idleticks, totalticks;
	float		cpu_user, cpu_sys, cpu_idle;
	unsigned long long mem_wired, mem_active, mem_inactive, mem_used;
	unsigned long long mem_free;
	unsigned long long n_ipackets, n_opackets, n_ibytes, n_obytes;
	unsigned long long d_rops, d_wops, d_rbytes, d_wbytes;
	unsigned long long purgeable_mem;
	/* Make strings large enough for ", NNNNN <MAXLEN>\0". */
	char		sstr[LIBTOP_NSTATES][LIBTOP_STATE_MAXLEN + 9];
	char		nstr[LIBTOP_NSTATES][6];
	char		fw_code[6], fw_data[6], fw_linkedit[6];
	char		vprvt[6], fw_private[6], rshrd[6];
	char		wired[6], active[6], inactive[6], used[6], free[6];
	char		vsize[6], fw_vsize[6];
	char		xsu_used[6], xsu_avail[6];
	char		purgeable[6];

	/* Get the date and time. */
	gettimeofday(&tval, NULL);
	time = tval.tv_sec;
	localtime_r(&time, &tm);

	/* Render state count strings. */
	for (i = 0; i < LIBTOP_NSTATES; i++) {
		if (samp_tsamp->state_breakdown[i] != 0) {
			sprintf(sstr[i], ", %s %s",
			    samp_p_unsigned_render(
			    samp_tsamp->state_breakdown[i], nstr[i],
			    sizeof(nstr[i])), libtop_state_str(i));
		} else {
			sstr[i][0] = '\0';
		}
	}

	/* Calculate CPU usage. */
	switch (top_opt_c) {
	case 'a':
		userticks = (samp_tsamp->cpu.cpu_ticks[CPU_STATE_USER]
		    + samp_tsamp->cpu.cpu_ticks[CPU_STATE_NICE])
		    - (samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_USER]
		    + samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_NICE]);
		systicks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_SYSTEM]
		    - samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_SYSTEM];
		idleticks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_IDLE]
		    - samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_IDLE];
		break;
	case 'd':
	case 'n':
		userticks = (samp_tsamp->cpu.cpu_ticks[CPU_STATE_USER]
		    + samp_tsamp->cpu.cpu_ticks[CPU_STATE_NICE])
		    - (samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_USER]
		    + samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_NICE]);
		systicks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_SYSTEM]
		    - samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_SYSTEM];
		idleticks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_IDLE]
		    - samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_IDLE];
		break;
	case 'e':
		userticks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_USER]
		    + samp_tsamp->cpu.cpu_ticks[CPU_STATE_NICE];
		systicks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_SYSTEM];
		idleticks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_IDLE];
		break;
	default:
		assert(0);
	}
	if(userticks < 0)
	    userticks = 0;
	if(systicks < 0)
	    systicks = 0;
	if(idleticks < 0)
	    idleticks = 0;
	totalticks = userticks + systicks + idleticks;
	if (totalticks != 0) {
		cpu_user = ((float)(100 * userticks)) / ((float)totalticks);
		cpu_sys = ((float)(100 * systicks)) / ((float)totalticks);
		cpu_idle = ((float)(100 * idleticks)) / ((float)totalticks);
	} else {
		cpu_user = 0.0;
		cpu_sys = 0.0;
		cpu_idle = 0.0;
	}

	mem_wired = (unsigned long long) samp_tsamp->vm_stat.wire_count * samp_tsamp->pagesize;
	mem_active = (unsigned long long) samp_tsamp->vm_stat.active_count * samp_tsamp->pagesize;
	mem_inactive = (unsigned long long) samp_tsamp->vm_stat.inactive_count
	    * samp_tsamp->pagesize;
	mem_used = (unsigned long long) mem_wired + mem_active + mem_inactive;
	mem_free = (unsigned long long) samp_tsamp->vm_stat.free_count * samp_tsamp->pagesize;

	/*
	 * Jump through hoops to get the time string in the same place as in
	 * the old top.
	 */
	switch (top_opt_c) {
	case 'a':
		width = 105;
		break;
	case 'e':
		width = 117;
		break;
	case 'd':
		width = 81;
		break;
	case 'n':
		if (top_opt_w) {
			width = 120;
		} else {
			width = 81;
		}
		break;
	default:
		assert(0);
	}
	assert(width <= sizeof(hbuf));
	len = snprintf(hbuf, sizeof(hbuf), "\
Processes:  %u total%s%s%s%s%s%s%s... %u threads",
	    samp_tsamp->nprocs,
	    sstr[0], sstr[1], sstr[2], sstr[3], sstr[4], sstr[5], sstr[6],
	    samp_tsamp->threads);
	memset(&hbuf[len], ' ', sizeof(hbuf) - len);
	len = (width - 11)
	    + snprintf(&hbuf[width - 11], sizeof(hbuf) - (width - 11),
	    " %02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (top_opt_c == 'a') {
		struct timeval	time;
		unsigned	sec, min, hour;

		/* Print time since start. */
		timersub(&samp_tsamp->time, &samp_tsamp->b_time, &time);
		sec = time.tv_sec;
		min = sec / 60;
		hour = min / 60;

		memset(&hbuf[len], ' ', sizeof(hbuf) - len);
		snprintf(&hbuf[108], sizeof(hbuf) - 108,
		    "%u:%02u:%02u", hour, min % 60, sec % 60);
	}
	if (samp_println("%s", hbuf)) {
		retval = TRUE;
		goto RETURN;
	}

	if (samp_println("\
Load Avg:  %.2f, %.2f, %.2f     CPU usage:  %.1f%% user, %.1f%% sys, %.1f%% idle",
	    samp_tsamp->loadavg[0], samp_tsamp->loadavg[1],
	    samp_tsamp->loadavg[2],
	    cpu_user, cpu_sys, cpu_idle
	    )) {
		retval = TRUE;
		goto RETURN;
	}

	switch (top_opt_c) {
	case 'a':
		n_ipackets = samp_tsamp->net_ipackets
		    - samp_tsamp->b_net_ipackets;
		n_opackets = samp_tsamp->net_opackets
		    - samp_tsamp->b_net_opackets;
		n_ibytes = samp_tsamp->net_ibytes - samp_tsamp->b_net_ibytes;
		n_obytes = samp_tsamp->net_obytes - samp_tsamp->b_net_obytes;

		d_rops = samp_tsamp->disk_rops - samp_tsamp->b_disk_rops;
		d_wops = samp_tsamp->disk_wops - samp_tsamp->b_disk_wops;
		d_rbytes = samp_tsamp->disk_rbytes - samp_tsamp->b_disk_rbytes;
		d_wbytes = samp_tsamp->disk_wbytes - samp_tsamp->b_disk_wbytes;

		len = snprintf(buf, sizeof(buf), "Networks:%10llu ipkts/%lluK",
		    n_ipackets, n_ibytes / 1024);
		memset(&buf[len], ' ', sizeof(buf) - len);
		snprintf(&buf[36], sizeof(buf) - 36, "%10llu opkts /%lluK",
		    n_opackets, n_obytes / 1024);
		if (samp_println("%s", buf)) {
			retval = TRUE;
			goto RETURN;
		}

		len = snprintf(buf, sizeof(buf), "Disks:   %10llu reads/%lluK",
		    d_rops, d_rbytes / 1024);
		memset(&buf[len], ' ', sizeof(buf) - len);
		snprintf(&buf[36], sizeof(buf) - 36, "%10llu writes/%lluK",
		    d_wops, d_wbytes / 1024);
		if (samp_println("%s", buf)) {
			retval = TRUE;
			goto RETURN;
		}

		if (samp_println("VM:      %10u pageins         %10u pageouts",
		    samp_tsamp->vm_stat.pageins - samp_tsamp->b_vm_stat.pageins,
		    samp_tsamp->vm_stat.pageouts
		    - samp_tsamp->b_vm_stat.pageouts)) {
			retval = TRUE;
			goto RETURN;
		}

		break;
	case 'd':
		n_ipackets = samp_tsamp->net_ipackets
		    - samp_tsamp->p_net_ipackets;
		n_opackets = samp_tsamp->net_opackets
		    - samp_tsamp->p_net_opackets;
		n_ibytes = samp_tsamp->net_ibytes - samp_tsamp->p_net_ibytes;
		n_obytes = samp_tsamp->net_obytes - samp_tsamp->p_net_obytes;

		d_rops = samp_tsamp->disk_rops - samp_tsamp->p_disk_rops;
		d_wops = samp_tsamp->disk_wops - samp_tsamp->p_disk_wops;
		d_rbytes = samp_tsamp->disk_rbytes - samp_tsamp->p_disk_rbytes;
		d_wbytes = samp_tsamp->disk_wbytes - samp_tsamp->p_disk_wbytes;

		len = snprintf(buf, sizeof(buf), "Networks:%10llu ipkts/%lluK",
		    n_ipackets, n_ibytes / 1024);
		memset(&buf[len], ' ', sizeof(buf) - len);
		snprintf(&buf[36], sizeof(buf) - 36, "%10llu opkts /%lluK",
		    n_opackets, n_obytes / 1024);
		if (samp_println("%s", buf)) {
			retval = TRUE;
			goto RETURN;
		}

		len = snprintf(buf, sizeof(buf), "Disks:   %10llu reads/%lluK",
		    d_rops, d_rbytes / 1024);
		memset(&buf[len], ' ', sizeof(buf) - len);
		snprintf(&buf[36], sizeof(buf) - 36, "%10llu writes/%lluK",
		    d_wops, d_wbytes / 1024);
		if (samp_println("%s", buf)) {
			retval = TRUE;
			goto RETURN;
		}

		if (samp_println("VM:      %10u pageins         %10u pageouts",
		    samp_tsamp->vm_stat.pageins - samp_tsamp->p_vm_stat.pageins,
		    samp_tsamp->vm_stat.pageouts
		    - samp_tsamp->p_vm_stat.pageouts)) {
			retval = TRUE;
			goto RETURN;
		}
		break;
	case 'e':
		n_ipackets = samp_tsamp->net_ipackets;
		n_opackets = samp_tsamp->net_opackets;
		n_ibytes = samp_tsamp->net_ibytes;
		n_obytes = samp_tsamp->net_obytes;

		d_rops = samp_tsamp->disk_rops;
		d_wops = samp_tsamp->disk_wops;
		d_rbytes = samp_tsamp->disk_rbytes;
		d_wbytes = samp_tsamp->disk_wbytes;

		len = snprintf(buf, sizeof(buf), "Networks:%10llu ipkts/%lluK",
		    n_ipackets, n_ibytes / 1024);
		memset(&buf[len], ' ', sizeof(buf) - len);
		snprintf(&buf[36], sizeof(buf) - 36, "%10llu opkts /%lluK",
		    n_opackets, n_obytes / 1024);
		if (samp_println("%s", buf)) {
			retval = TRUE;
			goto RETURN;
		}

		len = snprintf(buf, sizeof(buf), "Disks:   %10llu reads/%lluK",
		    d_rops, d_rbytes / 1024);
		memset(&buf[len], ' ', sizeof(buf) - len);
		snprintf(&buf[36], sizeof(buf) - 36, "%10llu writes/%lluK",
		    d_wops, d_wbytes / 1024);
		if (samp_println("%s", buf)) {
			retval = TRUE;
			goto RETURN;
		}

		if (samp_println("VM:      %10u pageins         %10u pageouts",
		    samp_tsamp->vm_stat.pageins,
		    samp_tsamp->vm_stat.pageouts)) {
			retval = TRUE;
			goto RETURN;
		}
		break;
	case 'n':
		if (samp_println("\
SharedLibs: num = %4u, resident = %s code, %s data, %s LinkEdit",
		    samp_tsamp->fw_count,
		    samp_p_vm_size_render(samp_tsamp->fw_code, fw_code,
		    sizeof(fw_code)),
		    samp_p_vm_size_render(samp_tsamp->fw_data, fw_data,
		    sizeof(fw_data)),
		    samp_p_vm_size_render(samp_tsamp->fw_linkedit, fw_linkedit,
		    sizeof(fw_linkedit)))) {
			retval = TRUE;
			goto RETURN;
		}

		if (samp_println("\
MemRegions: num = %5u, resident = %s + %s private, %s shared",
		    samp_tsamp->reg,
		    samp_p_vm_size_render(samp_tsamp->rprvt
		    - samp_tsamp->fw_private, vprvt, sizeof(vprvt)),
		    samp_p_vm_size_render(samp_tsamp->fw_private, fw_private,
		    sizeof(fw_private)),
		    samp_p_vm_size_render(samp_tsamp->rshrd, rshrd,
		    sizeof(rshrd)))) {
			retval = TRUE;
			goto RETURN;
		}

		if (samp_println("\
PhysMem:  %s wired, %s active, %s inactive, %s used, %s free",
		    samp_p_vm_size_render(mem_wired, wired, sizeof(wired)),
		    samp_p_vm_size_render(mem_active, active, sizeof(active)),
		    samp_p_vm_size_render(mem_inactive, inactive,
		    sizeof(inactive)),
		    samp_p_vm_size_render(mem_used, used, sizeof(used)),
		    samp_p_vm_size_render(mem_free, free, sizeof(free)))) {
			retval = TRUE;
			goto RETURN;
		}

		if (samp_println(
		    "VM: %s + %s   %u(%u) pageins, %u(%u) pageouts",
		    samp_p_vm_size_render(samp_tsamp->vsize, vsize,
		    sizeof(vsize)),
		    samp_p_vm_size_render(samp_tsamp->fw_vsize, fw_vsize,
		    sizeof(fw_vsize)),
		    samp_tsamp->vm_stat.pageins,
		    samp_tsamp->vm_stat.pageins - samp_tsamp->p_vm_stat.pageins,
		    samp_tsamp->vm_stat.pageouts, 
		    samp_tsamp->vm_stat.pageouts
		    - samp_tsamp->p_vm_stat.pageouts)) {
			retval = TRUE;
			goto RETURN;
		}
		if (top_opt_S) {
			purgeable_mem = (samp_tsamp->vm_stat.purgeable_count *
					 samp_tsamp->pagesize);
			if (samp_println("Swap: %s + %s free       "
					 "Purgeable: %s  %u(%u) pages purged",
					 (samp_tsamp->xsu_is_valid ?
					  samp_p_vm_size_render(
						  samp_tsamp->xsu.xsu_used,
						  xsu_used,
						  sizeof (xsu_used)) :
					  "n/a"),
					 (samp_tsamp->xsu_is_valid ?
					  samp_p_vm_size_render(
						  samp_tsamp->xsu.xsu_avail,
						  xsu_avail,
						  sizeof (xsu_avail)) :
					  "n/a"),
					 (samp_tsamp->purgeable_is_valid ?
					  samp_p_vm_size_render(
						  purgeable_mem,
						  purgeable,
						  sizeof (purgeable)) :
					  "n/a"),
					 samp_tsamp->vm_stat.purges,
					 (samp_tsamp->vm_stat.purges
					  - samp_tsamp->p_vm_stat.purges))) {
				retval = TRUE;
				goto RETURN;
			}
		}
		break;
	default:
		assert(0);
	}

	retval = FALSE;
	RETURN:
	return retval;
}

static boolean_t
samp_p_deprecated_legend_print(void)
{
	boolean_t	retval;
	const char	*th, *prts;
	const char	*reg, *vprvt, *rprvt, *rshrd, *rsize, *vsize;
	const char	*faults, *pageins, *cow_faults, *msgs_sent;
	const char	*msgs_rcvd, *bsyscall, *msyscall, *cswitch;

	switch (top_opt_c) {
	case 'a':
	case 'e':
		th = "";
		prts = "";
		reg = "";
		vprvt = "";
		rprvt = "";
		rshrd = "";
		rsize = "";
		vsize = "";
		faults = "  FAULTS  ";
		pageins = " PAGEINS ";
		cow_faults = " COW_FAULTS";
		msgs_sent = " MSGS_SENT ";
		msgs_rcvd = " MSGS_RCVD ";
		bsyscall = " BSDSYSCALL";
		msyscall = " MACHSYSCALL";
		cswitch = " CSWITCH";
		break;
	case 'd':
		th = "";
		prts = "";
		reg = "";
		vprvt = "";
		rprvt = "";
		rshrd = "";
		rsize = "";
		vsize = "";
		faults = " FAULTS";
		pageins = " PGINS";
		cow_faults = "/COWS";
		msgs_sent = " MSENT";
		msgs_rcvd = "/MRCVD";
		bsyscall = "  BSD";
		msyscall = "/MACH ";
		cswitch = "   CSW";
		break;
	case 'n':
		th = " #TH";
		reg = " #MREGS";
		if (top_opt_w) {
			prts = " #PRTS(delta)";
			vprvt = " VPRVT";
			rprvt = "  RPRVT(delta)";
			rshrd = "  RSHRD(delta)";
			rsize = "  RSIZE(delta)";
			vsize = "  VSIZE(delta)";
		} else {
			prts = " #PRTS";
			vprvt = "";
			rprvt = " RPRVT ";
			rshrd = " RSHRD ";
			rsize = " RSIZE ";
			vsize = " VSIZE ";
		}
		faults = "";
		pageins = "";
		cow_faults = "";
		msgs_sent = "";
		msgs_rcvd = "";
		bsyscall = "";
		msyscall = "";
		cswitch = "";
		break;
	default:
		assert(0);
	}

	if (samp_println("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
	    "  PID", " COMMAND   ", "   %CPU", "   TIME  ",
	    th, prts, reg, vprvt, rprvt, rshrd, rsize, vsize,
	    faults, pageins, cow_faults, msgs_sent, msgs_rcvd,
	    bsyscall, msyscall, cswitch)) {
		retval = TRUE;
		goto RETURN;
	}

	retval = FALSE;
	RETURN:
	return retval;
}

static boolean_t
samp_p_deprecated_proc_print(const libtop_psamp_t *a_psamp)
{
	boolean_t	retval;
	char		command[11];
	char		cpu[6], time[9];
	char		th[3], prt[4];
	char		reg[7], vprvt[6], rprvt[6], rshrd[6];
	char		rsize[6], vsize[6];
	const char	*c_prt, *c_rprvt, *c_rshrd, *c_rsize, *c_vsize;
	unsigned	l_c_prt;
	unsigned	l_vprvt, l_rprvt, l_rshrd, l_rsize, l_vsize;
	unsigned	l_c_rprvt, l_c_rshrd, l_c_rsize, l_c_vsize;
	char		d_prt[8];
	char		d_rprvt[8], d_rshrd[8], d_rsize[8], d_vsize[8];

	/* Render command. */
	snprintf(command, sizeof(command), "%s", a_psamp->command);

	/* Render CPU usage. */
	samp_p_usage_render(a_psamp, cpu, sizeof(cpu));

	/* Render time. */
	samp_p_deprecated_time_render(a_psamp, time, sizeof(time));

	switch (top_opt_c) {
	case 'a':
		/* Print. */
		if (samp_println("%5d %-10s %5s%% %8s  %-8d %-8d %-10d %-10d %-10d %-10d %-11d %-10d",
		    a_psamp->pid, command, cpu, time,
		    a_psamp->events.faults - a_psamp->b_events.faults,
		    a_psamp->events.pageins - a_psamp->b_events.pageins,
		    a_psamp->events.cow_faults - a_psamp->b_events.cow_faults,
		    a_psamp->events.messages_sent
		    - a_psamp->b_events.messages_sent,
		    a_psamp->events.messages_received
		    - a_psamp->b_events.messages_received,
		    a_psamp->events.syscalls_unix
		    - a_psamp->b_events.syscalls_unix,
		    a_psamp->events.syscalls_mach
		    - a_psamp->b_events.syscalls_mach,
		    a_psamp->events.csw - a_psamp->b_events.csw)) {
			retval = TRUE;
			goto RETURN;
		}
		break;
	case 'd':
		/* Print. */
		if (samp_println("%5d %-10s %5s%% %8s %6d %5d/%-4d %5d/%-5d  %3d/%-4d %6d",
		    a_psamp->pid, command, cpu, time,
		    a_psamp->events.faults - a_psamp->p_events.faults,
		    a_psamp->events.pageins - a_psamp->p_events.pageins,
		    a_psamp->events.cow_faults - a_psamp->p_events.cow_faults,
		    a_psamp->events.messages_sent
		    - a_psamp->p_events.messages_sent,
		    a_psamp->events.messages_received
		    - a_psamp->p_events.messages_received,
		    a_psamp->events.syscalls_unix
		    - a_psamp->p_events.syscalls_unix,
		    a_psamp->events.syscalls_mach
		    - a_psamp->p_events.syscalls_mach,
		    a_psamp->events.csw - a_psamp->p_events.csw)) {
			retval = TRUE;
			goto RETURN;
		}
		break;
	case 'e':
		/* Print. */
		if (samp_println("%5d %-10s %5s%% %8s  %-8d %-8d %-10d %-10d %-10d %-10d %-11d %-10d",
		    a_psamp->pid, command, cpu, time,
		    a_psamp->events.faults,
		    a_psamp->events.pageins,
		    a_psamp->events.cow_faults,
		    a_psamp->events.messages_sent,
		    a_psamp->events.messages_received,
		    a_psamp->events.syscalls_unix,
		    a_psamp->events.syscalls_mach,
		    a_psamp->events.csw)) {
			retval = TRUE;
			goto RETURN;
		}
		break;
	case 'n':
		/* Render number of threads. */
		samp_p_unsigned_render(a_psamp->th, th, sizeof(th));

		/* Render Number of ports. */
		samp_p_unsigned_render(a_psamp->prt, prt, sizeof(prt));
		if (top_opt_w) {
			l_c_prt = sizeof(" delta");
			c_prt = d_prt;
			samp_p_unsigned_delta(a_psamp->prt, a_psamp->p_prt,
			    d_prt, sizeof(d_prt));
		} else {
			l_c_prt = 0;
			c_prt = "";
		}

		/*
		 * Render memory statistics.
		 */
		samp_p_unsigned_render(a_psamp->reg, reg, sizeof(reg) - 1);
		strncat(reg, " ", sizeof(reg) - strlen(reg) - 1);

		if (top_opt_w) {
			l_vprvt = sizeof("VPRVT");
			samp_p_deprecated_vm_size_render(a_psamp->vprvt, vprvt,
			    sizeof(vprvt));
		} else {
			l_vprvt = 0;
			vprvt[0] = '\0';
		}

		samp_p_deprecated_vm_size_render(a_psamp->rprvt, rprvt,
		    sizeof(rprvt));

		samp_p_deprecated_vm_size_render(a_psamp->rshrd, rshrd,
		    sizeof(rshrd));

		samp_p_deprecated_vm_size_render(a_psamp->rsize, rsize,
		    sizeof(rsize));

		samp_p_deprecated_vm_size_render(a_psamp->vsize, vsize,
		    sizeof(vsize));

		if (top_opt_w) {
			l_rprvt = 6;
			l_c_rprvt = sizeof(" delta");
			c_rprvt = d_rprvt;
			samp_p_deprecated_vm_size_delta(a_psamp->rprvt,
			    a_psamp->p_rprvt, d_rprvt, sizeof(d_rprvt));

			l_rshrd = 6;
			l_c_rshrd = sizeof(" delta");
			c_rshrd = d_rshrd;
			samp_p_deprecated_vm_size_delta(a_psamp->rshrd,
			    a_psamp->p_rshrd, d_rshrd, sizeof(d_rshrd));

			l_rsize = 6;
			l_c_rsize = sizeof(" delta");
			c_rsize = d_rsize;
			samp_p_deprecated_vm_size_delta(a_psamp->rsize,
			    a_psamp->p_rsize, d_rsize, sizeof(d_rsize));

			l_vsize = 6;
			l_c_vsize = sizeof(" delta");
			c_vsize = d_vsize;
			samp_p_deprecated_vm_size_delta(a_psamp->vsize,
			    a_psamp->p_vsize, d_vsize, sizeof(d_vsize));
		} else {
			l_rprvt = 5;
			l_c_rprvt = 1;
			c_rprvt = samp_p_vm_size_change(a_psamp->p_rprvt,
			    a_psamp->rprvt, a_psamp->p_seq);

			l_rshrd = 5;
			l_c_rshrd = 1;
			c_rshrd = samp_p_vm_size_change(a_psamp->p_rshrd,
			    a_psamp->rshrd, a_psamp->p_seq);

			l_rsize = 5;
			l_c_rsize = 1;
			c_rsize = samp_p_vm_size_change(a_psamp->p_rsize,
			    a_psamp->rsize, a_psamp->p_seq);

			l_vsize = 5;
			l_c_vsize = 1;
			c_vsize = samp_p_vm_size_change(a_psamp->p_vsize,
			    a_psamp->vsize, a_psamp->p_seq);
		}

		/* Print. */
		if (samp_println("%5d %-10s %5s%% %8s %3s %5s%*s %6s%*s %*s%*s %*s%*s %*s%*s %*s%*s",
			a_psamp->pid, command, cpu, time,
			th, prt, l_c_prt, c_prt,
			reg,
			l_vprvt, vprvt,
			l_rprvt, rprvt, l_c_rprvt, c_rprvt,
			l_rshrd, rshrd, l_c_rshrd, c_rshrd,
			l_rsize, rsize, l_c_rsize, c_rsize,
			l_vsize, vsize, l_c_vsize, c_vsize)) {
			retval = TRUE;
			goto RETURN;
		}

		break;
	default:
		assert(0);
	}

	retval = FALSE;
	RETURN:
	return retval;
}

static char *
samp_p_deprecated_vm_size_delta(unsigned long long a_size,
    unsigned long long a_prev, char *a_buf, unsigned a_bufsize)
{
	char		buf[6], *p;

	assert(a_bufsize >= 8);

	if (a_size == a_prev) {
		a_buf[0] = '\0';
	} else if (a_size > a_prev) {
		/* Growth. */
		samp_p_deprecated_vm_size_render(a_size - a_prev, buf,
		    sizeof(buf));
		snprintf(a_buf, a_bufsize, "(%*s)", a_bufsize - 3, buf);
	} else {
		/* Shrinkage.  Prepend a nestled '-' to the value. */
		samp_p_deprecated_vm_size_render(a_prev - a_size, &buf[1],
		    sizeof(buf) - 1);

		buf[0] = ' ';
		for (p = buf; *p == ' '; p++) {
			/* Do nothing. */
			assert(*p != '\0');
		}
		p--;
		*p = '-';
		snprintf(a_buf, a_bufsize, "(%*s)", a_bufsize - 3, buf);
	}

	return a_buf;
}

static char *
samp_p_deprecated_vm_size_render(unsigned long long a_size, char *a_buf,
    unsigned a_bufsize)
{
	assert(a_bufsize >= 5);

	if (a_size < (1024ULL * 1024ULL)) {
		/* K. */
		snprintf(a_buf, a_bufsize, "%.0fK",
		    ((double)a_size) / 1024);
	} else if (a_size < (1024ULL * 1024ULL * 1024ULL)) {
		/* M. */
		if (a_size < 10ULL * 1024ULL * 1024ULL) {
			/* 9.99M */
			snprintf(a_buf, a_bufsize, "%1.*fM",
			    a_bufsize - 4, ((double)a_size) / (1024 * 1024));
		} else if (a_size < 100ULL * 1024ULL * 1024ULL) {
			/* 99.9M */
			snprintf(a_buf, a_bufsize, "%2.*fM",
			    a_bufsize - 5, ((double)a_size) / (1024 * 1024));
		} else {
			/* 1023M */
			snprintf(a_buf, a_bufsize, "%4lluM",
			    a_size / (1024ULL * 1024ULL));
		}
	} else {
		if (a_size < 10ULL * 1024ULL * 1024ULL * 1024ULL) {
			/* 9.99G. */
			snprintf(a_buf, a_bufsize, "%1.*fG",
			    a_bufsize - 4,
			    ((double)a_size) / (1024 * 1024 * 1024));
		} else if (a_size < 100ULL * 1024ULL * 1024ULL * 1024ULL) {
			/* 99.9G. */
			snprintf(a_buf, a_bufsize, "%2.*fG",
			    a_bufsize - 5,
			    ((double)a_size) / (1024 * 1024 * 1024));
		} else {
			/* 1023G */
			snprintf(a_buf, a_bufsize, "%4lluM",
			    a_size / (1024ULL * 1024ULL * 1024ULL));
		}
	}

	return a_buf;
}

static char *
samp_p_deprecated_time_render(const libtop_psamp_t *a_psamp, char *a_buf,
    unsigned a_bufsize)
{
	struct timeval	tv;
	unsigned	usec, sec, min, hour;

	switch (top_opt_c) {
	case 'a':
		timersub(&a_psamp->total_time, &a_psamp->b_total_time, &tv);
		break;
	case 'd':
	case 'e':
	case 'n':
		tv = a_psamp->total_time;
		break;
	default:
		assert(0);
	}
	usec = tv.tv_usec;
	sec = tv.tv_sec;
	min = sec / 60;
	hour = min / 60;

	if (min < 100) {
		snprintf(a_buf, a_bufsize, "%u:%02u.%02u", min, sec % 60,
		    usec / 10000);
	} else if (hour < 100) {
		snprintf(a_buf, a_bufsize, "%u:%02u:%02u", hour, min % 60,
		    sec % 60);
	} else {
		snprintf(a_buf, a_bufsize, "%u hrs", hour);
	}

	return a_buf;
}
#endif
