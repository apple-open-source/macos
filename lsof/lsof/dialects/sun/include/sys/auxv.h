/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_AUXV_H
#define	_SYS_AUXV_H

#pragma ident	"@(#)auxv.h	1.10	92/07/14 SMI"	/* SVr4.0 1.2	*/

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct
{
	int	a_type;
	union {
		long	a_val;
#ifdef __STDC__
		void	*a_ptr;
#else
		char	*a_ptr;
#endif
		void	(*a_fcn)();
	} a_un;
} auxv_t;

#define	AT_NULL		0
#define	AT_IGNORE	1
#define	AT_EXECFD	2
#define	AT_PHDR		3	/* &phdr[0] */
#define	AT_PHENT	4	/* sizeof(phdr[0]) */
#define	AT_PHNUM	5	/* # phdr entries */
#define	AT_PAGESZ	6	/* getpagesize(2) */
#define	AT_BASE		7	/* ld.so base addr */
#define	AT_FLAGS	8	/* processor flags */
#define	AT_ENTRY	9	/* a.out entry point */

#define	AT_SUN_UID	2000	/* effective user id */
#define	AT_SUN_RUID	2001	/* real user id */
#define	AT_SUN_GID	2002	/* effective group id */
#define	AT_SUN_RGID	2003	/* real group id */

#define	NUM_GEN_VECTORS	8	/* number of generic aux vectors */
#define	NUM_SUN_VECTORS	4	/* number of sun defined aux vectors */

#define	NUM_AUX_VECTORS	(NUM_GEN_VECTORS + NUM_SUN_VECTORS) /* aux vectors */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_AUXV_H */
