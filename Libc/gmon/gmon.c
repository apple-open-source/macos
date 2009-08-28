/*
 * Copyright (c) 1999, 2003, 2004, 2007, 2008 Apple Inc. All rights reserved.
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
#if defined(PROFILE)
#error This module cannot be compiled with profiling
#endif

/*-
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * History
 *  2-Mar-90  Gregg Kellogg (gk) at NeXT
 *	Changed include of kern/mach.h to kern/mach_interface.h
 *
 *  1-May-90  Matthew Self (mself) at NeXT
 *	Added prototypes, and added casts to remove all warnings.
 *	Made all private data static.
 *	vm_deallocate old data defore vm_allocate'ing new data.
 *	Added new functions monoutput and monreset.
 *
 *  18-Dec-92 Development Environment Group at NeXT
 *	Added multiple profile areas, the ability to profile shlibs and the
 *	ability to profile rld loaded code.  Moved the machine dependent mcount
 *	routine out of this source file.
 *
 *  13-Dec-92 Development Environment Group at NeXT
 *	Added support for dynamic shared libraries.  Also removed the code that
 *	had been ifdef'ed out for profiling fixed shared libraries and
 *	objective-C.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)gmon.c	5.2 (Berkeley) 6/21/85";
#endif

/*
 * see profil(2) where this (SCALE_1_TO_1) is describe (incorrectly).
 *
 * The correct description:  scale is a fixed point value with
 * the binary point in the middle of the 32 bit value.  (Bit 16 is
 * 1, bit 15 is .5, etc.)
 *
 * Setting the scale to "1" (i.e. 0x10000), results in the kernel
 * choosing the profile bucket address 1 to 1 with the pc sampled.
 * Since buckets are shorts, if the profiling base were 0, then a pc
 * of 0 increments bucket 0, a pc of 2 increments bucket 1, and a pc
 * of 4 increments bucket 2.)  (Actually, this seems a little bogus,
 * 1 to 1 should map pc's to buckets -- that's probably what was
 * intended from the man page, but historically....
 */
#define		SCALE_1_TO_1	0x10000L

#define	MSG "No space for monitor buffer(s)\n"

#include <stdio.h>
#include <libc.h>
#include <monitor.h>
#include <sys/types.h>
#include <sys/gmon.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>

/*
 * These are defined in here and these declarations need to be moved to libc.h
 * where the other declarations for the monitor(3) routines are declared.
 */
extern void moninit(
    void);
extern void monaddition(
    char *lowpc,
    char *highpc);
extern void moncount(
    char *frompc,
    char *selfpc);
extern void monreset(
    void);
extern void monoutput(
    const char *filename);

static char profiling = -1;	/* tas (test and set) location for NeXT */
static char init = 0;		/* set while moninit() is being serviced */

static unsigned long order = 0;	/* call order */

typedef struct {
    /* the address range and size this mon struct refers to */
    char		*lowpc;
    char		*highpc;
    unsigned long	textsize;
    /* the data structures to support the arc's and their counts */
    unsigned short	*froms; /* froms is unsigned shorts indexing into tos */
    tostruct_t		*tos;
    long		tolimit;
    /* the pc-sample buffer, it's size and scale */
    char		*sbuf;
    long		ssiz;	/* includes the gmonhdr_t */
    long		scale;
} mon_t;
static mon_t *mon = NULL;
static unsigned long nmon = 0;

static void monsetup(
    mon_t *m,
    char *lowpc,
    char *highpc);
static long getprofhz(
    void);

void
moninit(
void)
{
#ifndef __LP64__
    const struct section *section;
#else
    const struct section_64 *section;
#endif
    char *lowpc, *highpc;
    unsigned long i;

	monreset();
	init = 1;

	section = getsectbyname("__TEXT", "__text");
	lowpc = (char *)section->addr,
	highpc = (char *)(section->addr + section->size);

	if(mon == NULL){
	    if((mon = malloc(sizeof(mon_t))) == NULL){
		write(2, MSG, sizeof(MSG) - 1);
		return;
	    }
	    nmon = 1;
	    memset(mon, '\0', sizeof(mon_t));
	}
	/*
	 * To continue to make monstartup() and the functions that existed
	 * before adding multiple profiling areas working correctly the new
	 * calls to get the dyld loaded code profiled are made after
	 * the first mon_t is allocated so that they will not use the 
	 * first mon_t and the old calls will always use the first mon_t
	 * in the list.
	 */
	monsetup(mon, lowpc, highpc);

	profil(mon->sbuf + sizeof(gmonhdr_t),
	       mon->ssiz - sizeof(gmonhdr_t),
	       (u_long)mon->lowpc, mon->scale);
	for(i = 1; i < nmon; i++)
	    add_profil(mon[i].sbuf + sizeof(gmonhdr_t),
		       mon[i].ssiz - sizeof(gmonhdr_t),
		       (u_long)mon[i].lowpc, mon[i].scale);
	init = 0;
	profiling = 0;

#if defined(__DYNAMIC__)
        /*
	 * Call _dyld_moninit() if the dyld is present.  This is done after the
	 * above calls so the dynamic libraries will be added after the
	 * executable.
	 */
	_dyld_moninit(monaddition);
#endif
}

void
monstartup(
char *lowpc,
char *highpc)
{
	monreset();
	if(mon == NULL){
	    if((mon = malloc(sizeof(mon_t))) == NULL){
		write(2, MSG, sizeof(MSG) - 1);
		return;
	    }
	    nmon = 1;
	    memset(mon, '\0', sizeof(mon_t));
	}
	monsetup(mon, lowpc, highpc);
}

/*
 * monaddtion() is used for adding additional pc ranges to profile.  This is
 * used for profiling dyld loaded code.
 */
void
monaddition(
char *lowpc,
char *highpc)
{
    char save_profiling;
    mon_t *m;

	if(mon == NULL){
	    monstartup(lowpc, highpc);
	    return;
	}
	save_profiling = profiling;
	profiling = -1;
	if((mon = realloc(mon, (nmon + 1) * sizeof(mon_t))) == NULL){
	    write(2, MSG, sizeof(MSG) - 1);
	    return;
	}
	m = mon + nmon;
	memset(m, '\0', sizeof(mon_t));
	nmon++;
	monsetup(m, lowpc, highpc);
	profiling = save_profiling;
}

static
void
monsetup(
mon_t *m,
char *lowpc,
char *highpc)
{
    long monsize;
    char *buffer;
    kern_return_t ret;
    gmonhdr_t *p;
    uintptr_t o;

	/*
	 * round lowpc and highpc to multiples of the density we're using
	 * so the rest of the scaling (here and in gprof) stays in longs.
	 */
	lowpc = (char *)ROUNDDOWN((uintptr_t)lowpc,
				  HISTFRACTION * sizeof(HISTCOUNTER));
	m->lowpc = lowpc;
	highpc = (char *)ROUNDUP((uintptr_t)highpc,
				 HISTFRACTION * sizeof(HISTCOUNTER));
	m->highpc = highpc;

	if(m->froms)
	    vm_deallocate(mach_task_self(),
			  (vm_address_t)m->froms,
			  (vm_size_t)(m->textsize / HASHFRACTION));
	m->textsize = highpc - lowpc;
	ret = vm_allocate(mach_task_self(),
			  (vm_address_t *)&m->froms,
			  (vm_size_t)(m->textsize / HASHFRACTION),
			   TRUE);
	if(ret != KERN_SUCCESS){
	    write(2, MSG, sizeof(MSG) - 1);
	    m->froms = 0;
	    return;
	}

	if(m->sbuf)
	    vm_deallocate(mach_task_self(),
			  (vm_address_t)m->sbuf,
			  (vm_size_t)m->ssiz);
	monsize = (m->textsize / HISTFRACTION) + sizeof(gmonhdr_t);
	ret = vm_allocate(mach_task_self(),
			  (vm_address_t *)&buffer,
			  (vm_size_t)monsize,
			   TRUE);
	if(ret != KERN_SUCCESS){
	    write(2, MSG, sizeof(MSG) - 1);
	    m->sbuf = 0;
	    return;
	}

	if(m->tos)
	    vm_deallocate(mach_task_self(),
			  (vm_address_t)m->tos,
			  (vm_size_t)(m->tolimit * sizeof(tostruct_t)));
	m->tolimit = m->textsize * ARCDENSITY / 100;
	if(m->tolimit < MINARCS){
	    m->tolimit = MINARCS;
	}
	else if(m->tolimit > 65534){
	    m->tolimit = 65534;
	}
	ret =  vm_allocate(mach_task_self(), 
			   (vm_address_t *)&m->tos,
			   (vm_size_t)(m->tolimit * sizeof(tostruct_t)),
			    TRUE);
	if(ret != KERN_SUCCESS){
	    write(2, MSG, sizeof(MSG) - 1);
	    m->tos = 0;
	    return;
	}
	m->tos[0].link = 0; /* a nop since tos was vm_allocated and is zero */

	/*
	 * If this is call to monsetup() was via monstartup() (m == mon) then
	 * it is using or reusing the first pc range and then the pc sample 
	 * buffer can be setup by the system call profil() via monitor() via
	 * a moncontrol(1) call.
	 *
	 * Otherwise this is call to monsetup() was via monaddition() and a
	 * new system call is needed to add an additional pc sample buffer in
	 * the kernel.
	 */
	if(m == mon && !init){
	    monitor(lowpc, highpc, buffer, monsize, m->tolimit);
	}
	else{
	    /* monitor() functionality */
	    m->sbuf = buffer;
	    m->ssiz = monsize;
	    p = (gmonhdr_t *)m->sbuf;
	    memset(p, '\0', sizeof(gmonhdr_t));
	    p->lpc = (uintptr_t)m->lowpc;
	    p->hpc = (uintptr_t)m->highpc;
	    p->ncnt = m->ssiz;
	    p->version = GMONVERSION;
	    p->profrate = getprofhz();
	    o = highpc - lowpc;
	    if((monsize - sizeof(gmonhdr_t)) < o)
/* POSSIBLE BUG, if "(float) (monsize - sizeof(gmonhdr_t))/ o)" is zero
 * then m->scale will be set to zero and the add_profil() call will disable
 * profiling */
		m->scale = ((float) (monsize - sizeof(gmonhdr_t))/ o) *
			   SCALE_1_TO_1;
	    else
		m->scale = SCALE_1_TO_1;

            /* moncontrol(mode == 1) functionality */
	    if(!init)
		add_profil(m->sbuf + sizeof(gmonhdr_t),
			   m->ssiz - sizeof(gmonhdr_t),
			   (long)m->lowpc, m->scale);
	}
}

void
monreset(
void)
{
    unsigned long i;
    mon_t *m;
    gmonhdr_t *p;

	moncontrol(0);
	if(mon == NULL)
	    return;
	for(i = 0; i < nmon; i++){
	    m = mon + i;
	    if(m->sbuf != NULL){
		memset(m->sbuf, '\0', m->ssiz);
		p = (gmonhdr_t *)m->sbuf;
		p->lpc = (uintptr_t)m->lowpc;
		p->hpc = (uintptr_t)m->highpc;
		p->ncnt = m->ssiz;
		p->version = GMONVERSION;
		p->profrate = getprofhz();
	    }
	    if(m->froms != NULL)
		memset(m->froms, '\0', m->textsize / HASHFRACTION);
	    if(m->tos != NULL)
		memset(m->tos, '\0', m->tolimit * sizeof(tostruct_t));
	}
	order = 0;
	moncontrol(1);
}

void
monoutput(
const char *filename)
{
    int fd;
    unsigned long i, fromindex, endfrom, toindex;
    uint32_t magic;
    gmon_data_t sample_data, arc_data, dyld_data;
    char *frompc;
    rawarc_order_t rawarc_order;
    mon_t *m;
    uint32_t image_count;
    intptr_t image_header;
    char *image_name;

	moncontrol(0);
	m = mon;
	if(m == NULL)
	    return;
	fd = creat(filename, 0666);
	if(fd < 0){
	    perror("mcount: gmon.out");
	    return;
	}

#ifndef __LP64__
	magic = GMON_MAGIC;
#else
	magic = GMON_MAGIC_64;
#endif
	write(fd, &magic, sizeof(uint32_t));

#if defined(__DYNAMIC__)
        {
	    image_count = _dyld_image_count();
	    if(image_count > 1){
#ifdef DYLD_DEBUG
		printf("image_count = %lu\n", image_count - 1);
		for(i = 1; i < image_count; i++){
		    image_header = _dyld_get_image_header(i);
		    printf("\timage_header %p\n", image_header);
		    image_name = _dyld_get_image_name(i);
		    printf("\timage_name %s\n", image_name);
		}
#endif
		/*
		 * Calculate the dyld_data.size.
		 */
		dyld_data.type = GMONTYPE_DYLD2_STATE;
		dyld_data.size = sizeof(uint32_t) +
		    sizeof(intptr_t) * (image_count - 1);
		for(i = 1; i < image_count; i++){
		    image_name = _dyld_get_image_name(i);
		    dyld_data.size += strlen(image_name) + 1;
		}

		/*
		 * Write the dyld_data.
		 */
		write(fd, &dyld_data, sizeof(gmon_data_t));
		image_count--;
		write(fd, &image_count, sizeof(uint32_t));
		image_count++;
		for(i = 1; i < image_count; i++){
		    image_header = (intptr_t)_dyld_get_image_header(i);
		    write(fd, &image_header, sizeof(intptr_t));
		    image_name = _dyld_get_image_name(i);
		    write(fd, image_name, strlen(image_name) + 1);
		}
	    }
	}
#endif
	for(i = 0; i < nmon; i++){
	    m = mon + i;
#ifdef DEBUG
	    fprintf(stderr, "[monoutput] sbuf %p ssiz %d\n", m->sbuf, m->ssiz);
#endif
	    sample_data.type = GMONTYPE_SAMPLES;
	    sample_data.size = m->ssiz;
	    write(fd, &sample_data, sizeof(gmon_data_t));
	    /*
	     * Write the gmonhdr_t and the pc-sample buffer.  Note the
	     * gmonhdr_t is in sbuf at the beginning of sbuf already
	     * filled in.
	     */
	    write(fd, m->sbuf, m->ssiz);

	    /*
	     * Now write out the raw arcs.
	     */
	    endfrom = m->textsize / (HASHFRACTION * sizeof(*m->froms));
	    arc_data.type = GMONTYPE_ARCS_ORDERS;
	    arc_data.size = 0;
#ifdef DEBUG
	    fprintf(stderr, "[monoutput] raw arcs, total %lu\n", endfrom);
#endif
	    for(fromindex = 0; fromindex < endfrom; fromindex++){
		if(m->froms[fromindex] == 0){
		    continue;
		}
#ifdef DEBUG
		fprintf(stderr, "[monoutput] raw arc count at index[%lu] %u\n",
			fromindex, m->froms[fromindex]);
#endif
		frompc = m->lowpc +
			 (fromindex * HASHFRACTION * sizeof(*m->froms));
		for(toindex = m->froms[fromindex];
		    toindex != 0;
		    toindex = m->tos[toindex].link){
		    arc_data.size += sizeof(rawarc_order_t);
		}
	    }
	    write(fd, &arc_data, sizeof(gmon_data_t));

	    for(fromindex = 0; fromindex < endfrom; fromindex++){
		if(m->froms[fromindex] == 0){
		    continue;
		}
		frompc = m->lowpc +
			 (fromindex * HASHFRACTION * sizeof(*m->froms));
		for(toindex = m->froms[fromindex];
		    toindex != 0;
		    toindex = m->tos[toindex].link){
#ifdef DEBUG
		    fprintf(stderr, "[monoutput] frompc %p selfpc %p "
			    "count %ld order %lu\n", frompc,
			    m->tos[toindex].selfpc,
			    m->tos[toindex].count, m->tos[toindex].order);
#endif
		    rawarc_order.raw_frompc = (uintptr_t)frompc;
		    rawarc_order.raw_selfpc = (uintptr_t)
					       m->tos[toindex].selfpc;
		    rawarc_order.raw_count = m->tos[toindex].count;
		    rawarc_order.raw_order = m->tos[toindex].order;
		    write(fd, &rawarc_order, sizeof(rawarc_order_t));
		}
	    }
	}
	close(fd);
}

void
monitor(
char *lowpc,
char *highpc,
char *buf,
int bufsiz,
int nfunc) /* nfunc is not used; available for compatability only. */
{
    intptr_t o;
    gmonhdr_t *p;
    mon_t *m;

	moncontrol(0);
	m = mon;
	if(m == NULL)
	    return;
	if(lowpc == 0){
	    moncontrol(0);
	    monoutput("gmon.out");
	    return;
	}
	m->sbuf = buf;
	m->ssiz = bufsiz;
	p = (gmonhdr_t *)buf;
	memset(p, '\0', sizeof(gmonhdr_t));
	p->lpc = (uintptr_t)lowpc;
	p->hpc = (uintptr_t)highpc;
	p->ncnt = m->ssiz;
	p->version = GMONVERSION;
	p->profrate = getprofhz();
	bufsiz -= sizeof(gmonhdr_t);
	if(bufsiz <= 0)
	    return;
	o = highpc - lowpc;
	if(bufsiz < o)
	    m->scale = ((float) bufsiz / o) * SCALE_1_TO_1;
	else
	    m->scale = SCALE_1_TO_1;
	moncontrol(1);
}

/*
 * Control profiling
 *	profiling is what mcount checks to see if
 *	all the data structures are ready.
 */
void
moncontrol(
int mode)
{
    mon_t *m;
    unsigned long i;

	if(mode){
	    /* start */
	    m = mon;
	    if(m != NULL){
		profil(m->sbuf + sizeof(gmonhdr_t),
		       m->ssiz - sizeof(gmonhdr_t),
		       (u_long)m->lowpc, m->scale);
		for(i = 1; i < nmon; i++)
		    add_profil(mon[i].sbuf + sizeof(gmonhdr_t),
			       mon[i].ssiz - sizeof(gmonhdr_t),
			       (u_long)mon[i].lowpc, mon[i].scale);
		profiling = 0;
	    }
	}
	else{
	    /* stop */
	    profil((char *)0, 0, 0, 0);
	    profiling = -1;
	}
}

void
moncount(
char *frompc,
char *selfpc)
{
    unsigned short *frompcindex;
    tostruct_t *top, *prevtop;
    unsigned long i, toindex;
    mon_t *m;

	m = mon;
	if(m == NULL)
	    return;
	/*
	 * Check that we are profiling and that we aren't recursively invoked.
	 * This should really be a test and set instruction in changing the
	 * value of profiling.
	 */
	if(profiling)
	    return;
	profiling++;


#ifdef DEBUG
	fprintf(stderr, "[moncount] frompc %p selfpc %p\n", frompc, selfpc);
#endif
	frompcindex = (unsigned short *)frompc;

	/*
	 * check that frompcindex is a reasonable pc value.
	 * for example:	signal catchers get called from the stack,
	 * 		not from text space.  too bad.
	 */
	for(i = 0; i < nmon; i++){
	    m = mon + i;
	    if((uintptr_t)frompcindex >= (uintptr_t)m->lowpc &&
	       (uintptr_t)frompcindex <  (uintptr_t)m->highpc)
		break;
	}
	if(i == nmon){
	    goto done;
	}
	else{
	    frompcindex = (unsigned short *)
		  ((uintptr_t)frompcindex - (uintptr_t)m->lowpc);
	}
	frompcindex =
	    &m->froms[((long)frompcindex) / (HASHFRACTION * sizeof(*m->froms))];
	toindex = *frompcindex;
	if(toindex == 0){
	    /*
	     *	first time traversing this arc
	     */
	    toindex = ++m->tos[0].link;
	    if(toindex >= m->tolimit){
		goto overflow;
	    }
	    *frompcindex = toindex;
	    top = &m->tos[toindex];
	    top->selfpc = (uintptr_t)selfpc;
	    top->count = 1;
	    top->link = 0;
	    top->order = ++order;
	    goto done;
	}
	top = &m->tos[toindex];
	if(top->selfpc == (uintptr_t)selfpc){
	    /*
	     * arc at front of chain; usual case.
	     */
	    top->count++;
	    goto done;
	}
	/*
	 * have to go looking down chain for it.
	 * top points to what we are looking at,
	 * prevtop points to previous top.
	 * we know it is not at the head of the chain.
	 */
	for(; /* goto done */; ){
	    if(top->link == 0){
		/*
		 * top is end of the chain and none of the chain
		 * had top->selfpc == selfpc.
		 * so we allocate a new tostruct_t
		 * and link it to the head of the chain.
		 */
		toindex = ++m->tos[0].link;
		if(toindex >= m->tolimit){
		    goto overflow;
		}
		top = &m->tos[toindex];
		top->selfpc = (uintptr_t)selfpc;
		top->count = 1;
		top->link = *frompcindex;
		top->order = ++order;
		*frompcindex = toindex;
		goto done;
	    }
	    /*
	     * otherwise, check the next arc on the chain.
	     */
	    prevtop = top;
	    top = &m->tos[top->link];
	    if(top->selfpc == (uintptr_t)selfpc){
		/*
		 * there it is.
		 * increment its count
		 * move it to the head of the chain.
		 */
		top->count++;
		toindex = prevtop->link;
		prevtop->link = top->link;
		top->link = *frompcindex;
		*frompcindex = toindex;
		goto done;
	    }
	}
done:
	profiling--;
	return;

overflow:
	profiling++; /* halt further profiling */
#define	TOLIMIT	"mcount: tos overflow\n"
	write(2, TOLIMIT, sizeof(TOLIMIT) - 1);
}

/*
 * Get the profiling rate.
 */
static
long
getprofhz(void)
{
    int mib[2];
    size_t size;
    struct clockinfo clockrate;

        mib[0] = CTL_KERN;
        mib[1] = KERN_CLOCKRATE;
        clockrate.profhz = 1;
        size = sizeof(clockrate);
        if(sysctl(mib, 2, &clockrate, &size, NULL, 0) < 0)
		;
        return(clockrate.profhz);
}
