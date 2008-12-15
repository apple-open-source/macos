/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
 */

#include <architecture/arm/sel.h>

/*
 * Task State segment.
 */

typedef struct tss {
    sel_t		oldtss;
    unsigned int		:0;
    unsigned int	esp0;
    sel_t		ss0;
    unsigned int		:0;
    unsigned int	esp1;
    sel_t		ss1;
    unsigned int		:0;
    unsigned int	esp2;
    sel_t		ss2;
    unsigned int		:0;
    unsigned int	cr3;
    unsigned int	eip;
    unsigned int	eflags;
    unsigned int	eax;
    unsigned int	ecx;
    unsigned int	edx;
    unsigned int	ebx;
    unsigned int	esp;
    unsigned int	ebp;
    unsigned int	esi;
    unsigned int	edi;
    sel_t		es;
    unsigned int		:0;
    sel_t		cs;
    unsigned int		:0;
    sel_t		ss;
    unsigned int		:0;
    sel_t		ds;
    unsigned int		:0;
    sel_t		fs;
    unsigned int		:0;
    sel_t		gs;
    unsigned int		:0;
    sel_t		ldt;
    unsigned int		:0;
    unsigned int	t	:1,
    				:15,
			io_bmap	:16;
} tss_t;

#define TSS_SIZE(n)	(sizeof (struct tss) + (n))

/*
 * Task State segment descriptor.
 */

typedef struct tss_desc {
    unsigned short	limit00;
    unsigned short	base00;
    unsigned char	base16;
    unsigned char	type	:5,
#define DESC_TSS	0x09
			dpl	:2,
			present	:1;
    unsigned char	limit16	:4,
				:3,
			granular:1;
    unsigned char	base24;
} tss_desc_t;

/*
 * Task gate descriptor.
 */
 
typedef struct task_gate {
    unsigned short		:16;
    sel_t		tss;
    unsigned int		:8,
    			type	:5,
#define DESC_TASK_GATE	0x05
			dpl	:2,
			present	:1,
				:0;
} task_gate_t;
