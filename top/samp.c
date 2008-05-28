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
#include <libutil.h>

/*
 * Function pointers that point to abstract printing functions.
 */
static samp_skip_t *samp_skipl;
static samp_print_t *samp_printl;
static samp_print_t *samp_println;
static samp_vprint_t *samp_vprintln;
static samp_vprint_t *samp_veprint;

static const libtop_tsamp_t *samp_tsamp;

extern int disp_bufsize;
extern char *disp_lbuf;

/* Function prototypes. */
char * pad(char *string, unsigned int width, char just, boolean_t terminate);
static boolean_t
samp_p_eprint(void *a_user_data, const char *a_format, ...);
static int
samp_p_sort(void *a_data, const libtop_psamp_t *a_a, const libtop_psamp_t *a_b);
static int
samp_p_sort_subcomp(top_sort_key_t a_key, const libtop_psamp_t *a_a, const libtop_psamp_t *a_b);
static boolean_t
samp_p_print(void);
boolean_t
samp_p_header_print(void);
static boolean_t
samp_p_legend_print(void);
static boolean_t
samp_p_proc_print(const libtop_psamp_t *a_psamp);
static char *
samp_p_vm_size_delta(unsigned long long a_size, unsigned long long a_prev,
    char *a_buf, unsigned a_bufsize);
static char *
samp_p_unsigned_delta(unsigned a_size, unsigned a_prev, char *a_buf,
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
		   unsigned a_bufsize, boolean_t deprecated);

/* Initialize printing function pointers and libtop. */
boolean_t
samp_init(samp_skip_t *a_skipl, samp_print_t *a_printl, samp_print_t *a_println,
    samp_vprint_t *a_vprintln, samp_vprint_t *a_veprint)
{
	samp_skipl = a_skipl;
	samp_printl = a_printl;
	samp_println = a_println;
	samp_vprintln = a_vprintln;
	samp_veprint = a_veprint;

	if (libtop_init(samp_p_eprint, NULL)) return TRUE;

	libtop_set_interval(top_opt_i);
	samp_tsamp = libtop_tsamp();

	return FALSE;
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
	/*
	 * If using the deprecated output mode, turn off framework and memory
	 * region reporting unless they are actually being displayed.
	 */
	if (top_opt_x && top_opt_c != 'n') {
	  top_opt_f = FALSE;
	  top_opt_r = FALSE;
	}

	if (libtop_sample(top_opt_r, top_opt_f)) return TRUE;

	libtop_psort(samp_p_sort, NULL);

	return samp_p_print();
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

	retval = samp_p_sort_subcomp(top_opt_o, a_a, a_b) * (top_opt_o_ascend?1:-1);
	if (retval == 0) retval = samp_p_sort_subcomp(top_opt_O, a_a, a_b) * (top_opt_O_ascend?1:-1);

	return retval;
}

#define COMP(a,b) (((a)==(b)?0: \
		             ((a)<(b)?-1:1)))

/* Actual psamp sorting function, used by samp_p_sort(). */
static int
samp_p_sort_subcomp(top_sort_key_t a_key, const libtop_psamp_t *a_a, const libtop_psamp_t *a_b)
{
	struct timeval	tv_a, tv_b;
	const char	*user_a, *user_b;

	switch (a_key) {
	case TOP_SORT_command: return COMP(strcmp(a_a->command, a_b->command),0);
	case TOP_SORT_pid:     return COMP(a_a->pid, a_b->pid);
	case TOP_SORT_prt:     return COMP(a_a->prt, a_b->prt);
	case TOP_SORT_reg:     return COMP(a_a->reg, a_b->reg);
	case TOP_SORT_rprvt:   return COMP(a_a->rprvt, a_b->rprvt);
	case TOP_SORT_rshrd:   return COMP(a_a->rshrd, a_b->rshrd);
	case TOP_SORT_rsize:   return COMP(a_a->rsize, a_b->rsize);
	case TOP_SORT_th:      return COMP(a_a->th, a_b->th);
	case TOP_SORT_vprvt:   return COMP(a_a->vprvt, a_b->vprvt);
	case TOP_SORT_vsize:   return COMP(a_a->vsize, a_b->vsize);
	case TOP_SORT_uid:     return COMP(a_a->uid, a_b->uid);
	case TOP_SORT_cpu: 
	case TOP_SORT_time: 
	  if(top_opt_c=='a') {
	  timersub(&a_a->total_time, &a_a->b_total_time, &tv_a);
	  timersub(&a_b->total_time, &a_b->b_total_time, &tv_b);
	  } else {
	    if(a_key==TOP_SORT_cpu) {
	      timersub(&a_a->total_time, &a_a->p_total_time, &tv_a);
	      timersub(&a_b->total_time, &a_b->p_total_time, &tv_b);
	    } else {
	      tv_a = a_a->total_time;
	      tv_b = a_b->total_time;
	    }
	  }
	  if(tv_a.tv_sec == tv_b.tv_sec) return COMP(tv_a.tv_usec,tv_b.tv_usec);
	  else return COMP(tv_a.tv_sec,tv_b.tv_sec);
	case TOP_SORT_username:
		/* Handle the == case first, since it's common. */
	  if (a_a->uid == a_b->uid) return 0;
	  
	  user_a = libtop_username(a_a->uid);
	  user_b = libtop_username(a_b->uid);

	  return COMP(strcmp(user_a, user_b),0);
	}
	
	assert(0);  // default, should never be reached
}

/* Print results of a sample. */
static boolean_t
samp_p_print(void)
{
	const libtop_psamp_t	*psamp;
	unsigned		i;

	/* Header and legend. */
	if (samp_p_header_print()
	    || (top_opt_n > 0 && (samp_skipl()
				  || samp_p_legend_print()))) return TRUE;

	/* Processes. */
	for (i = 1, psamp = libtop_piterate();
	     psamp != NULL && i <= top_opt_n;
	     psamp = libtop_piterate()) {
	  if (top_opt_U == FALSE || psamp->uid == top_opt_U_uid) {
	    if (samp_p_proc_print(psamp)) return TRUE;
	    i++;
	  }
	}
	
	return FALSE;
}

/* Print header. */
boolean_t
samp_p_header_print(void)
{
	struct timeval	tval;
	struct tm	tm;
	time_t		time;
	unsigned int    i,hour,min,sec;
	unsigned long long userticks, systicks, idleticks, totalticks;
	float		cpu_user, cpu_sys, cpu_idle;
	unsigned long long mem_wired, mem_active, mem_inactive, mem_used;
	unsigned long long mem_free;
	unsigned long long n_ipackets, n_opackets, n_ibytes, n_obytes;
	unsigned long long d_rops, d_wops, d_rbytes, d_wbytes;
	unsigned long long purgeable_mem;
	/* Make big enough for 100 years: " (HHHHHH:MM:SS)". */
	char		time_acc[16],strf_out[30];
	/* Make strings large enough for ", NNNNN <MAXLEN>\0". */
	char		sstr[LIBTOP_NSTATES*(LIBTOP_STATE_MAXLEN + 9)];
	char		sstr_buf[LIBTOP_STATE_MAXLEN + 9];
	char		fw_code[6], fw_data[6], fw_linkedit[6];
	char		vprvt[6], fw_private[6], rshrd[6];
	char		wired[6], active[6], inactive[6], used[6], free[6];
	char		vsize[6], fw_vsize[6];
	natural_t	pageins, pageouts;
	char		net_ibytes[6], net_obytes[6];
	char		disk_rbytes[6], disk_wbytes[6];
	char		xsu_total[6], xsu_used[6], xsu_avail[6];
	char		purgeable[6];


	/* Print time since start. */
	timersub(&samp_tsamp->time, &samp_tsamp->b_time, &tval);
	sec = tval.tv_sec;
	min = sec / 60;
	hour = min / 60;
	snprintf(time_acc, sizeof(time_acc), "%u:%02u:%02u", hour, min % 60, sec % 60);

	gettimeofday(&tval, NULL);
	time = tval.tv_sec;
	localtime_r(&(tval.tv_sec), &tm);
	strftime(strf_out,sizeof(strf_out)-1,"%Y/%m/%d %T",&tm);

	sstr[0]='\0';
	/* Render state count strings. */
	for (i = 0; i < LIBTOP_NSTATES; i++) 
	  if (samp_tsamp->state_breakdown[i] != 0) {
	    snprintf(sstr_buf, sizeof(sstr_buf),", %d %s", 
		     samp_tsamp->state_breakdown[i], libtop_state_str(i));
	    strlcat(sstr, sstr_buf, sizeof(sstr));
	  }

	/* Calculate CPU usage. */
	userticks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_USER]
	          + samp_tsamp->cpu.cpu_ticks[CPU_STATE_NICE];
	systicks =  samp_tsamp->cpu.cpu_ticks[CPU_STATE_SYSTEM];
	idleticks = samp_tsamp->cpu.cpu_ticks[CPU_STATE_IDLE];

	if(top_opt_c=='a') {
	  userticks -= (samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_USER]
		       +samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_NICE]);
	  systicks  -=  samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_SYSTEM];
	  idleticks -=  samp_tsamp->b_cpu.cpu_ticks[CPU_STATE_IDLE];
	}

	if(top_opt_c=='d' || top_opt_c=='n') {
	  userticks -= (samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_USER]
		       +samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_NICE]);
	  systicks  -=  samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_SYSTEM];
	  idleticks -=  samp_tsamp->p_cpu.cpu_ticks[CPU_STATE_IDLE];
	}

	if(userticks < 0) userticks = 0;
	if(systicks < 0)   systicks = 0;
	if(idleticks < 0) idleticks = 0;
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
	mem_free = (unsigned long long) samp_tsamp->vm_stat.free_count * samp_tsamp->pagesize;
	mem_used = (unsigned long long) samp_tsamp->memsize - mem_free;

	pageins = samp_tsamp->vm_stat.pageins;
	pageouts = samp_tsamp->vm_stat.pageouts;
	if(top_opt_c=='a') {
	  pageins -= samp_tsamp->b_vm_stat.pageins;
	  pageouts -= samp_tsamp->b_vm_stat.pageouts;
	}
	if(top_opt_c=='d') {
	  pageins -= samp_tsamp->p_vm_stat.pageins;
	  pageouts -= samp_tsamp->p_vm_stat.pageouts;
	}
	  
	if(top_opt_x) {
	  strftime(disp_lbuf, disp_bufsize-1,"%T",&tm);
	  if(top_opt_c=='a') {
	    strlcat(disp_lbuf, "     ", disp_bufsize);
	    strlcat(disp_lbuf,time_acc, disp_bufsize);
	  }
	  if(disp_bufsize>40) pad(disp_lbuf, disp_bufsize-1, '$', TRUE);
	  else                pad(disp_lbuf, 80, '$', TRUE);
	  
	  snprintf(disp_lbuf, disp_bufsize, "Processes:  %u total%s... %u threads",samp_tsamp->nprocs, sstr,
		   samp_tsamp->threads);
	  disp_lbuf[strlen(disp_lbuf)]=' ';
	  
	  if (samp_println("%s\n", disp_lbuf)
	      || samp_println("\
Load Avg: %5.2f, %5.2f, %5.2f    CPU usage: %5.2f%% user, %5.2f%% sys, %5.2f%% idle",
			      samp_tsamp->loadavg[0], samp_tsamp->loadavg[1], samp_tsamp->loadavg[2],
			      cpu_user, cpu_sys, cpu_idle)) 
	    return TRUE;
	} else {
	  if(top_opt_c=='a') {
	    if (samp_println("\
Time: %s (%s).  Threads: %u.  Procs: %u%s.", strf_out, time_acc, 
			     samp_tsamp->threads, samp_tsamp->nprocs,sstr)) return TRUE;
	  } else {
	    if (samp_println("\
Time: %s.  Threads: %u.  Procs: %u%s.", strf_out, 
			     samp_tsamp->threads, samp_tsamp->nprocs,sstr)) return TRUE;
	  }
	  if(samp_println("\
LoadAvg: %5.2f, %5.2f, %5.2f.  CPU: %5.2f%% user, %5.2f%% sys, %5.2f%% idle.",
			  samp_tsamp->loadavg[0], samp_tsamp->loadavg[1], samp_tsamp->loadavg[2],
			  cpu_user, cpu_sys, cpu_idle)) 
	    return TRUE;
	}

	if(!top_opt_x || (top_opt_c!='a' && top_opt_c!='d')) {
	  if (top_opt_f && samp_println("\
SharedLibs: num = %4u, resident = %5s code, %5s data, %5s linkedit.",
					samp_tsamp->fw_count,
					samp_p_vm_size_render(samp_tsamp->fw_code, fw_code, sizeof(fw_code)),
					samp_p_vm_size_render(samp_tsamp->fw_data, fw_data, sizeof(fw_data)),
					samp_p_vm_size_render(samp_tsamp->fw_linkedit, fw_linkedit, sizeof(fw_linkedit))))
	    return TRUE;

	  if (top_opt_r && samp_println("\
MemRegions: num = %5u, resident = %5s + %5s private, %5s shared.",
					samp_tsamp->reg,
					samp_p_vm_size_render(samp_tsamp->rprvt, vprvt, sizeof(vprvt)),
					samp_p_vm_size_render(samp_tsamp->fw_private, fw_private, sizeof(fw_private)),
					samp_p_vm_size_render(samp_tsamp->rshrd, rshrd, sizeof(rshrd))))
	    return TRUE;

	  if (samp_println("\
PhysMem: %5s wired, %5s active, %5s inactive, %5s used, %5s free.",
			   samp_p_vm_size_render(mem_wired, wired, sizeof(wired)),
			   samp_p_vm_size_render(mem_active, active, sizeof(active)),
			   samp_p_vm_size_render(mem_inactive, inactive, sizeof(inactive)),
			   samp_p_vm_size_render(mem_used, used, sizeof(used)),
			   samp_p_vm_size_render(mem_free, free, sizeof(free)))) 
	    return TRUE;
	  
	if (top_opt_f)  samp_p_vm_size_render(samp_tsamp->fw_vsize, fw_vsize, sizeof(fw_vsize));
	else fw_vsize[0]='\0';
	
	purgeable_mem = (samp_tsamp->vm_stat.purgeable_count * samp_tsamp->pagesize);

	if(!top_opt_x) {
	  if (samp_println("\
VirtMem: %5s%s%5s, %10u pagein%s, %10u pageout%s.",
			   samp_p_vm_size_render(samp_tsamp->vsize, vsize,sizeof(vsize)),
			   fw_vsize[0]?" + ":"", fw_vsize, pageins, pageins == 1 ? "" : "s",
			   pageouts, pageouts == 1 ? "" : "s")) return TRUE;
	} else {
	  if (top_opt_c!='d' && samp_println("VM: %s + %s   %u(%u) pageins, %u(%u) pageouts",
			   samp_p_vm_size_render(samp_tsamp->vsize, vsize, sizeof(vsize)),
			   samp_p_vm_size_render(samp_tsamp->fw_vsize, fw_vsize, sizeof(fw_vsize)),
			   samp_tsamp->vm_stat.pageins,
			   samp_tsamp->vm_stat.pageins - samp_tsamp->p_vm_stat.pageins,
			   samp_tsamp->vm_stat.pageouts, 
			   (samp_tsamp->vm_stat.pageouts
			    - samp_tsamp->p_vm_stat.pageouts))) return TRUE;
	}	  
	 
	purgeable_mem = (samp_tsamp->vm_stat.purgeable_count * samp_tsamp->pagesize);
	  
	if(!top_opt_x && top_opt_S && samp_println("Swap: %5s total, %5s used, %5s free.  Purgeable: %5s  %10u purges",
					(samp_tsamp->xsu_is_valid ?
					 samp_p_vm_size_render(samp_tsamp->xsu.xsu_total, xsu_total,sizeof(xsu_total)) : "n/a"),
					(samp_tsamp->xsu_is_valid ?
					 samp_p_vm_size_render(samp_tsamp->xsu.xsu_used, xsu_used,sizeof (xsu_used)) : "n/a"),
					(samp_tsamp->xsu_is_valid ?
					 samp_p_vm_size_render(samp_tsamp->xsu.xsu_avail, xsu_avail, sizeof (xsu_avail)) : "n/a"),
					(samp_tsamp->purgeable_is_valid ?
					 samp_p_vm_size_render(purgeable_mem,purgeable, sizeof (purgeable)) : "n/a"),
					samp_tsamp->vm_stat.purges)) return TRUE;

	if (top_opt_x && top_opt_S && samp_println("Swap: %s + %s free       Purgeable: %s  %u(%u) pages purged",
					 (samp_tsamp->xsu_is_valid ?
					  samp_p_vm_size_render(samp_tsamp->xsu.xsu_used, xsu_used, sizeof(xsu_used)) : "n/a"),
					 (samp_tsamp->xsu_is_valid ?
					  samp_p_vm_size_render(samp_tsamp->xsu.xsu_avail, xsu_avail, sizeof (xsu_avail)) : "n/a"),
					 (samp_tsamp->purgeable_is_valid ?
					  samp_p_vm_size_render(purgeable_mem, purgeable, sizeof (purgeable)) : "n/a"),
					 samp_tsamp->vm_stat.purges,
					 (samp_tsamp->vm_stat.purges
					  - samp_tsamp->p_vm_stat.purges))) return TRUE;
	}

	if(top_opt_c=='n') return FALSE; // skip network stats

	n_ipackets = samp_tsamp->net_ipackets;
	n_opackets = samp_tsamp->net_opackets;
	n_ibytes   = samp_tsamp->net_ibytes;
	n_obytes   = samp_tsamp->net_obytes;
	
	d_rops     = samp_tsamp->disk_rops;
	d_wops     = samp_tsamp->disk_wops;
	d_rbytes   = samp_tsamp->disk_rbytes;
	d_wbytes   = samp_tsamp->disk_wbytes;
	
	if(top_opt_c=='a') {
	  n_ipackets -= samp_tsamp->b_net_ipackets;
	  n_opackets -= samp_tsamp->b_net_opackets;
	  n_ibytes   -= samp_tsamp->b_net_ibytes;
	  n_obytes   -= samp_tsamp->b_net_obytes;
	  
	  d_rops     -= samp_tsamp->b_disk_rops;
	  d_wops     -= samp_tsamp->b_disk_wops;
	  d_rbytes   -= samp_tsamp->b_disk_rbytes;
	  d_wbytes   -= samp_tsamp->b_disk_wbytes;
	}

	if(top_opt_c=='d') {
	  n_ipackets -= samp_tsamp->p_net_ipackets;
	  n_opackets -= samp_tsamp->p_net_opackets;
	  n_ibytes   -= samp_tsamp->p_net_ibytes;
	  n_obytes   -= samp_tsamp->p_net_obytes;
	  
	  d_rops     -= samp_tsamp->p_disk_rops;
	  d_wops     -= samp_tsamp->p_disk_wops;
	  d_rbytes   -= samp_tsamp->p_disk_rbytes;
	  d_wbytes   -= samp_tsamp->p_disk_wbytes;
	}

	if(top_opt_x) {
	  if (samp_println("Networks: %10llu ipkts/%6lluK   %10llu opkts /%lluK", 
			   n_ipackets, n_ibytes/1024, n_opackets, n_obytes/1024) 
	      || samp_println("Disks:    %10llu reads/%6lluK   %10llu writes/%lluK",
			      d_rops, d_rbytes/1024, d_wops, d_wbytes/1024) 
	      || 	  samp_println("VM:       %10u pageins                  %u pageouts",
				       pageins, pageouts)) return TRUE;
	} else {
	  if (samp_println("\
Networks: packets = %10llu in, %10llu out, data = %5s in, %5s out.", n_ipackets, n_opackets,
			   samp_p_vm_size_render(n_ibytes, net_ibytes, sizeof(net_ibytes)),
			   samp_p_vm_size_render(n_obytes, net_obytes, sizeof(net_obytes)))
	      || samp_println("\
Disks: operations = %10llu in, %10llu out, data = %5s in, %5s out.", d_rops, d_wops,
			      samp_p_vm_size_render(d_rbytes, disk_rbytes, sizeof(disk_rbytes)),
			      samp_p_vm_size_render(d_wbytes, disk_wbytes, sizeof(disk_wbytes)))
	      ) return TRUE;
	}
	return FALSE;
}

/* Print legend for process information. */
boolean_t
samp_p_legend_print(void)
{
  char *legend;

  if(top_opt_P_legend) legend=top_opt_P_legend;
  else switch(top_opt_c) {
  case 'a': legend=top_opt_x?LEGEND_CA:LEGEND_XCA; break;
  case 'e': legend=top_opt_x?LEGEND_CE:LEGEND_XCE; break;
  case 'd': legend=top_opt_x?LEGEND_CD:LEGEND_XCD; break;
  case 'n':
    if(top_opt_w) legend=top_opt_x?LEGEND_CNW:LEGEND_XCNW;
      else          legend=top_opt_x?LEGEND_CN:LEGEND_XCN;
  break;
  default:  assert(0);
  }

  return samp_println("%s",legend);
}

size_t str_char_spn(const char *s, char c) {
  char search[2]={c,0};
  return strspn(s,search);
}

size_t str_char_cspn(const char *s, char c) {
  char search[2]={c,0};
  return strcspn(s,search);
}

void delete_char(char *s) {
  while(s[0]) {
    s[0]=s[1];
    s++;
  }
}

/* Takes in a string and pads it to the desired width with whitespace.
   The buffer pointed to by *string must be of size >= width if
   terminate is false, or > width if terminate is true.
   If just is '^', the string will be left-justfied.
   If just is '$', the string will be right-justified.
   If terminate is true, the string will have '\0' added at the end. */

char * pad(char *string, unsigned int width, char just, boolean_t terminate) {
  unsigned int len;

  len=strlen(string);
  assert(len <= width);
  switch(just) {
  case '^':   // left-justification: add spaces after string
    memset(string+len, ' ', width-len);
    break;
  case '$':
    memmove(string+width-len, string, len);
    memset(string, ' ', width-len);
    break;
  default: assert(0);
  }
  
  if(terminate) string[width]='\0';
  return string;
}

/* Format string parser helper function
   Given a string, looks to see if it begins with a valid format field.
   If so, it will return the field width (minimum: 2), otherwise it 
   will return 0.

   A valid format field:
   - Begins with FORMAT_LEFT ('^') or FORMAT_RIGHT ('$'), indicating
     the type of justification requested
   - follows with a character out of VALID_FORMATS
   - repeats that character as many times as necessary to specify the
     field width
   - is terminated by any character other than the format character,
     including the escape '\' and the delta specifier '-'

   For example:
   ^aaaa    valid, field type is 'a', width is 5
   $bb      valid, field type is 'b', width is 3
   ^c       valid, field type is 'c', width is 2
   $d\d     valid, field type is 'd', width is 2
   $e+      valid, field type is 'e', width is 2 (but has an extra
                                                  delta char tacked on)
*/

static unsigned int
parse_field(const char *format)
{
  int length;

  if(format==NULL || strlen(format)<2) return 0;
  if(format[0]!=FORMAT_LEFT && format[0]!=FORMAT_RIGHT) return 0;
  if(strchr(VALID_FORMATS,format[1])==NULL) return 0;
  length=str_char_spn(format+1,format[1])+1;
  if(length<2) return 0;
  return length;
}

char *
render_format_string(char *string,const libtop_psamp_t *a_psamp)
{
  char *ptr=string,temp_char,just,type;
  int length;
  task_events_info_data_t events;
  
  if(ptr == NULL || a_psamp==NULL) return NULL;

  /* Calculate events structure for later use */
  events=a_psamp->events;
  switch(top_opt_c) {
  case 'a':
    events.faults-=a_psamp->b_events.faults; 
    events.pageins-=a_psamp->b_events.pageins;
    events.cow_faults-=a_psamp->b_events.cow_faults;     
    events.messages_sent-=a_psamp->b_events.messages_sent;
    events.messages_received-=a_psamp->b_events.messages_received;
    events.syscalls_mach-=a_psamp->b_events.syscalls_mach;
    events.syscalls_unix-=a_psamp->b_events.syscalls_unix;
    events.csw-=a_psamp->b_events.csw;
    break;
  case 'd':
    events.faults-=a_psamp->p_events.faults; 
    events.pageins-=a_psamp->p_events.pageins;
    events.cow_faults-=a_psamp->p_events.cow_faults;     
    events.messages_sent-=a_psamp->p_events.messages_sent;
    events.messages_received-=a_psamp->p_events.messages_received;
    events.syscalls_mach-=a_psamp->p_events.syscalls_mach;
    events.syscalls_unix-=a_psamp->p_events.syscalls_unix;
    events.csw-=a_psamp->p_events.csw;
    break;
  case 'e':
    break;  /* do nothing */
  default:
    bzero((void *)&events, sizeof(events));
  }
  
  while(strlen(ptr)>1) { /* if the string is less than the minimum
			    field length, we are done substituting */
    length=parse_field(ptr);

    if(length==0) {      /* This is a literal with length at least 2 */
      ptr++;             /* Skip the first char (might be an escape) */
      length=strcspn(ptr,VALID_ESCAPES);  /* find the next escape */
      if(length==0 && ptr[0]==0) return string; /* no more escapes? we're done */
      ptr+=length;       /* otherwise, skip to the next escape */
      continue;
    } else {
      assert(length<=strlen(ptr));
      temp_char=ptr[length];
      just=ptr[0]; type=ptr[1];

      //      printf("ptr[%d]='%c'\n",length,temp_char);

      /* Note: we have to pass length+1 to all of these functions because our
	 definition of length doesn't include the trailing NULL, whereas
	 these functions do include it. */

      switch(type) {
      case FORMAT_PID:           samp_p_unsigned_render(a_psamp->pid, ptr, length+1); break;
      case FORMAT_THREADS:       samp_p_unsigned_render(a_psamp->th, ptr, length+1); break;
      case FORMAT_PORTS:         samp_p_unsigned_render(a_psamp->prt, ptr, length+1); break;
      case FORMAT_REGIONS:       samp_p_unsigned_render(a_psamp->reg, ptr, length+1); break;
      case FORMAT_UID:	         samp_p_unsigned_render(a_psamp->uid, ptr, length+1); break;
      case FORMAT_COMMAND:       strncpy(ptr,a_psamp->command,length); ptr[length]='\0'; break;
      case FORMAT_PERCENT_CPU:   samp_p_usage_render(a_psamp, ptr, length+1); break;
      case FORMAT_TIME:          samp_p_time_render(a_psamp, ptr, length+1, FALSE); break;
      case FORMAT_RPRVT:         samp_p_vm_size_render(a_psamp->rprvt, ptr, length+1); break;
      case FORMAT_RSHRD:         samp_p_vm_size_render(a_psamp->rshrd, ptr, length+1); break;
      case FORMAT_RSIZE:         samp_p_vm_size_render(a_psamp->rsize, ptr, length+1); break;
      case FORMAT_VPRVT:         samp_p_vm_size_render(a_psamp->vprvt, ptr, length+1); break;
      case FORMAT_VSIZE:         samp_p_vm_size_render(a_psamp->vsize, ptr, length+1); break;
      case FORMAT_USERNAME:      samp_p_username_render(a_psamp->uid, ptr, length+1); break;
      case FORMAT_FAULTS:        samp_p_int_render(events.faults, ptr, length+1); break;
      case FORMAT_PAGEINS:       samp_p_int_render(events.pageins, ptr, length+1); break;
      case FORMAT_COW_FAULTS:    samp_p_int_render(events.cow_faults, ptr, length+1); break;
      case FORMAT_MSGS_SENT:     samp_p_int_render(events.messages_sent, ptr, length+1); break;
      case FORMAT_MSGS_RECEIVED: samp_p_int_render(events.messages_received, ptr, length+1); break;
      case FORMAT_BSYSCALL:      samp_p_int_render(events.syscalls_unix, ptr, length+1); break;
      case FORMAT_MSYSCALL:      samp_p_int_render(events.syscalls_mach, ptr, length+1); break;
      case FORMAT_CSWITCH:       samp_p_int_render(events.csw, ptr, length+1); break;
      case FORMAT_TIME_HHMMSS:   samp_p_time_render(a_psamp, ptr, length+1, TRUE); break;
      default:
	printf("Error, invalid format char %c\n",type);
	exit(0);
      }
      pad(ptr, length, just, FALSE);
      ptr[length]=temp_char;
      ptr += length;

      if(ptr[0] == FORMAT_DELTA) {
	length=str_char_spn(ptr, FORMAT_DELTA);
	switch(type) {
	case FORMAT_PORTS:         samp_p_unsigned_delta(a_psamp->prt, a_psamp->p_prt, ptr, length+1); break;
	case FORMAT_RPRVT:         samp_p_vm_size_delta(a_psamp->rprvt, a_psamp->p_rprvt, ptr, length+1); break;
	case FORMAT_RSHRD:         samp_p_vm_size_delta(a_psamp->rshrd, a_psamp->p_rshrd, ptr, length+1); break;
	case FORMAT_RSIZE:         samp_p_vm_size_delta(a_psamp->rsize, a_psamp->p_rsize, ptr, length+1); break;
	case FORMAT_VPRVT:         samp_p_vm_size_delta(a_psamp->vprvt, a_psamp->p_vprvt, ptr, length+1); break;
	case FORMAT_VSIZE:         samp_p_vm_size_delta(a_psamp->vsize, a_psamp->p_vsize, ptr, length+1); break;
      default:
	printf("Error, delta not supported with format char %c\n",type);
	exit(0);
	}
	ptr += length;
      }	
    }
  }
  return string;
}

/* Print information for a psamp. */
static boolean_t
samp_p_proc_print(const libtop_psamp_t *a_psamp)
{
  char *format, *render_buf;
  
  if(top_opt_p_format) format=top_opt_p_format;
  else switch(top_opt_c) {
    case 'a': format=top_opt_x?FORMAT_CA:FORMAT_XCA; break;
    case 'e': format=top_opt_x?FORMAT_CE:FORMAT_XCE; break;
    case 'd': format=top_opt_x?FORMAT_CD:FORMAT_XCD; break;
    case 'n':
      if(top_opt_w) format=top_opt_x?FORMAT_CNW:FORMAT_XCNW;
      else          format=top_opt_x?FORMAT_CN:FORMAT_XCN;
    break;
    default:  assert(0);
    }

  render_buf=malloc(strlen(format)+1);
  if(render_buf==NULL) return TRUE;
  strcpy(render_buf,format);
	
  if(samp_println("%s",render_format_string(render_buf,a_psamp))) return TRUE;
  free(render_buf);
  return FALSE;
}

/*
 * Render a VM size delta.  If a_size and a_prev differ, the delta is rendered
 * in parentheses.  Otherwise a zero-length string is rendered.
 */
static char *
samp_p_vm_size_delta(unsigned long long a_size, unsigned long long a_prev,
    char *a_buf, unsigned a_bufsize)
{
	char  *p=a_buf;

	assert(a_bufsize > 1);

	if(a_bufsize==2) {
	  if(a_size == a_prev) p[0]=' ';
	  if(a_size < a_prev) p[0]='-';
	  if(a_size > a_prev) p[0]='+';
	  return a_buf;
	}

	if (a_size == a_prev) {
	  a_buf[0] = '\0';
	  pad(a_buf,a_bufsize-1,'^',FALSE);
	  return a_buf;
	}

	*p++='(';

	if (a_size > a_prev)  /* Growth. */
	  samp_p_vm_size_render(a_size - a_prev, p, a_bufsize - 2);
	else { 	  /* Shrinkage.  Prepend a nestled '-' to the value. */
	  *p='-';
	  samp_p_vm_size_render(a_prev - a_size, p+1, a_bufsize - 3);
	}

	pad(p,a_bufsize-3,'$',FALSE);
	  
	p[a_bufsize-3]=')';
	return a_buf;
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
	char  *p=a_buf;

	assert(a_bufsize > 1);

	if(a_bufsize==2) {
	  if(a_size == a_prev) p[0]=' ';
	  if(a_size < a_prev) p[0]='-';
	  if(a_size > a_prev) p[0]='+';
	  return a_buf;
	}

	if (a_size == a_prev) {
	  a_buf[0] = '\0';
	  pad(a_buf,a_bufsize-1,'^', FALSE);
	  return a_buf;
	}

	*p++='(';

	samp_p_int_render(a_size - a_prev, p, a_bufsize - 2);
	pad(p,a_bufsize-3,'$',FALSE);
	  
	p[a_bufsize-3]=')';
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
 * Render an unsigned integer, or render an overflow string if the value won't fit.
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
	if (user != NULL) strlcpy(a_buf, user, a_bufsize);
	else              strlcpy(a_buf, "???", a_bufsize);

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
  humanize_number(a_buf, a_bufsize, a_size, "", HN_AUTOSCALE, HN_NOSPACE);
  return a_buf;
}

/* Render the percent of CPU usage for a_psamp. */
static char *
samp_p_usage_render(const libtop_psamp_t *a_psamp, char *a_buf,
    unsigned a_bufsize)
{
  struct timeval	elapsed_tv, used_tv;
  unsigned long long	elapsed_us, used_us;
  int	                whole=0, part=0;

  if (a_psamp->p_seq == 0) {
    strcpy(a_buf,"  0.0");
    return a_buf;
  }
  
  switch (top_opt_c) {
  case 'a':
    timersub(&samp_tsamp->time, &samp_tsamp->b_time, &elapsed_tv);
    timersub(&a_psamp->total_time, &a_psamp->b_total_time, &used_tv);
    break;
  case 'e': case 'd': case 'n':
    timersub(&samp_tsamp->time, &samp_tsamp->p_time, &elapsed_tv);
    timersub(&a_psamp->total_time, &a_psamp->p_total_time, &used_tv);
    break;
  default: assert(0);
  }
  
  elapsed_us = (unsigned long long)elapsed_tv.tv_sec * 1000000ULL
             + (unsigned long long)elapsed_tv.tv_usec;
  used_us = (unsigned long long)used_tv.tv_sec * 1000000ULL
          + (unsigned long long)used_tv.tv_usec;
  
  whole = (used_us * 100ULL) / elapsed_us;
  part = (((used_us * 100ULL) - (whole * elapsed_us)) * 10ULL) / elapsed_us;

  snprintf(a_buf, a_bufsize, "%d.%01d", whole, part);
  
  return a_buf;
}

/* Render the time consumed by a_psamp. */
static char *
samp_p_time_render(const libtop_psamp_t *a_psamp, char *a_buf, unsigned a_bufsize, boolean_t deprecated)
{
	struct timeval	tv;
	unsigned	usec, sec, min, hour, day;

	if(top_opt_c=='a') timersub(&a_psamp->total_time, &a_psamp->b_total_time, &tv);
	else               tv = a_psamp->total_time;

	usec = tv.tv_usec;
	sec = tv.tv_sec;
	min = sec / 60;
	hour = min / 60;
	day = hour / 24;

	if(deprecated) {
	  if (min < 100) snprintf(a_buf, a_bufsize, "%u:%02u.%02u", min, sec % 60, usec / 10000);
	  else if (hour < 100) snprintf(a_buf, a_bufsize, "%u:%02u:%02u", hour, min % 60, sec % 60);
	  else  snprintf(a_buf, a_bufsize, "%u hrs", hour);
	} else {
	  if (sec < 60) snprintf(a_buf, a_bufsize, "%u.%02us", sec, usec / 10000);
	  else if (min < 60) snprintf(a_buf, a_bufsize, "%um%02us", min, sec - min * 60);
	  else if (hour < 24) snprintf(a_buf, a_bufsize, "%uh%02um", hour, min - hour * 60);
	  else if (day < 100) snprintf(a_buf, a_bufsize, "%ud%02uh", day, hour - day * 24);
	  else snprintf(a_buf, a_bufsize, "%ud", day);
	}

	return a_buf;
}
