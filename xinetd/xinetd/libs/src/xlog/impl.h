/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


/*
 * $Id: impl.h,v 1.1.1.3 2003/05/22 01:16:36 rbraun Exp $
 */

#ifndef __XLOG_IMPL_H
#define __XLOG_IMPL_H

#include <stdlib.h>  	/* For malloc() & free() prototypes */
#include <stdarg.h>
#define DEFINE_LINK_TYPE( type, name )		struct { type *next, *prev ; } name


#define NEXT( obj, field )		(obj)->field.next
#define PREV( obj, field )		(obj)->field.prev

#define INIT_LINKS( obj, field )			\
	{						\
		NEXT( obj, field ) = obj ;		\
		PREV( obj, field ) = obj ;		\
	}

/*
 * Link new object after object using the specified field
 */
#define LINK( obj, new_obj, field )			\
	{						\
		NEXT( new_obj, field ) = NEXT( obj, field ) ;	\
		PREV( new_obj, field ) = obj ;			\
		NEXT( obj, field ) = new_obj ;			\
		PREV( NEXT( obj, field ), field ) = new_obj ;	\
	}

#define UNLINK( obj, field )				\
	{						\
		NEXT( PREV( obj, field ), field ) = NEXT( obj, field ) ;	\
		PREV( NEXT( obj, field ), field ) = PREV( obj, field ) ;	\
	}


/*
 * xlog linking:
 * 	When xlog1 is linked to xlog2 (i.e. errors on xlog1 are reported to 
 *		xlog2) we use the xl_clients field on xlog2 and the xl_other_users 
 *		field on xlog1
 */
struct xlog
{
	xlog_e 			xl_type ;
	char 				*xl_id ;
	int 				xl_flags ;
	void 				(*xl_callback)() ;
	void 				*xl_callback_arg ;
	struct xlog 	*xl_use ;	/* xlog we report errors to 		*/
	struct xlog 	*xl_clients ;	/* linked list of xlogs that use */
													/* this xlog to report errors 	*/
	DEFINE_LINK_TYPE( struct xlog, xl_other_users ) ;
	struct xlog_ops
	{
		int 	(*init)		( struct xlog *, va_list ) ;
		void	(*fini) 	( struct xlog * ) ;
		int	(*write)	( struct xlog *, const char *, int, int, va_list ) ;
		int	(*control)	( struct xlog *, xlog_cmd_e, va_list ) ;
		int	(*parms) 	( xlog_e, va_list ) ;
	} 	*xl_ops ;
	void 	*xl_data ;
} ;

#define XL_INIT( xp, ap )		(*(xp)->xl_ops->init)( (xp), (ap) )
#define XL_FINI( xp )			(*(xp)->xl_ops->fini)( xp )
#define XL_WRITE( xp, buf, size, flags, ap ) \
		(*(xp)->xl_ops->write)( (xp), (buf), (size), (flags), (ap ) )
#define XL_CONTROL( xp, cmd, ap ) \
		(*(xp)->xl_ops->control)( (xp), (cmd), (ap) )

typedef struct xlog xlog_s ;

typedef void (*voidfunc)() ;
typedef int bool_int ;

#define XP( p )			((struct xlog *)(p))

#define XLOG_NULL		XP( NULL )

#ifndef FALSE
#define FALSE		0
#define TRUE		1
#endif

#ifndef NULL
#define NULL		0
#endif

#define NEW( type )		(type *) malloc( sizeof( type ) )

int __xlog_add_errno(const char *, int) ;
char *__xlog_explain_errno(char *, unsigned *) ;
char *__xlog_new_string(const char *) ;

#endif

