/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <stdlib.h>
#include <stdarg.h>

#include "xlog.h"
#include "impl.h"

extern struct xlog_ops __xlog_filelog_ops ;
#ifndef NO_SYSLOG
extern struct xlog_ops __xlog_syslog_ops ;
#endif

struct lookup_table
{
	struct xlog_ops	*ops ;
	xlog_e		type ;
} ;

static struct lookup_table ops_lookup_table[] =
{
	{ &__xlog_filelog_ops,	XLOG_FILELOG },
#ifndef NO_SYSLOG
	{ &__xlog_syslog_ops,	XLOG_SYSLOG },
#endif
	{ NULL,                 0           }
} ;


#define CALLBACK( xp, status ) 				\
	if ( (xp)->xl_callback ) 			\
		(*(xp)->xl_callback)( (xlog_h)(xp), status, (xp)->xl_callback_arg )


static void xlog_link( xlog_s *client, xlog_s *server ) ;
static void xlog_unlink( xlog_s *xp ) ;


static struct xlog_ops *xlog_ops_lookup( xlog_e type )
{
	struct lookup_table *ltp ;

	for ( ltp = &ops_lookup_table[ 0 ] ; ltp->ops ; ltp++ )
		if ( ltp->type == type )
			break ;
	return( ltp->ops ) ;
}


/* VARARGS3 */
xlog_h xlog_create( xlog_e type, const char *id, int flags, ... )
{
	xlog_s				*xp ;
	va_list				ap ;
	struct xlog_ops	*xops ;
	int				status ;

	if ( ( xp = NEW( xlog_s ) ) == NULL )
		return( NULL ) ;
	
	if ( id == NULL || ( xp->xl_id = __xlog_new_string( id ) ) == NULL )
	{
		free( xp ) ;
		return( NULL ) ;
	}

	xops = xlog_ops_lookup( type ) ;
	
	if ( xops != NULL )
	{
		va_start( ap, flags ) ;
		xp->xl_ops = xops ;
		status = XL_INIT( xp, ap ) ;
		va_end( ap ) ;

		if ( status == XLOG_ENOERROR )
		{
			xp->xl_flags = flags ;
			xp->xl_type = type ;
			xp->xl_clients = XLOG_NULL ;
			xp->xl_use = XLOG_NULL ;
			return( (xlog_h) xp ) ;
		}
	}

	free( xp->xl_id ) ;
	free( xp ) ;
	return( NULL ) ;
}



static void xlog_link( xlog_s *client, xlog_s *server )
{
	client->xl_use = server ;
	if ( server == NULL )
		return ;

	if ( server->xl_clients == XLOG_NULL )
	{
		INIT_LINKS( client, xl_other_users ) ;
		server->xl_clients = client ;
	}
	else
		LINK( server, client, xl_other_users ) ;
}


static void xlog_unlink( xlog_s *xp )
{
	xlog_s *server = xp->xl_use ;

	/*
	 * Step 1: remove from server chain
	 */
	if ( server != XLOG_NULL )
	{
		if ( server->xl_clients == xp )
			if ( NEXT( xp, xl_other_users ) == xp )
				server->xl_clients = XLOG_NULL ;
			else
				server->xl_clients = NEXT( xp, xl_other_users ) ;
		else
			UNLINK( xp, xl_other_users ) ;
	}

	/*
	 * Step 2: If we have users, clear their link to us.
	 */
	if ( xp->xl_clients != NULL )
	{
		xlog_s *xp2 = xp->xl_clients ;

		do
		{
			xp2->xl_use = XLOG_NULL ;
			xp2 = NEXT( xp2, xl_other_users ) ;
		}
		while ( xp2 != xp->xl_clients ) ;
	}
}


static void xlog_flags( xlog_s *xp, xlog_cmd_e cmd, ... )
{
	va_list ap;
	int	flag;
	int	old_value;
	int	*valp;

	va_start(ap, cmd);
	flag = va_arg( ap, int );
	old_value = ((xp->xl_flags & flag) != 0);
	valp = va_arg( ap, int * );
	va_end(ap);
	if ( cmd == XLOG_SETFLAG ) {
		if ( *valp )
			xp->xl_flags |= flag ;
		else
			xp->xl_flags &= ~flag ;
	}
	*valp = old_value ;
}


void xlog_destroy( xlog_h pxlog )
{
	xlog_s *xp = XP( pxlog ) ;

	xlog_unlink( xp ) ;
	XL_FINI( xp ) ;
	free( xp->xl_id ) ;
	free( xp ) ;
}


/* VARARGS4 */
void xlog_write( xlog_h pxlog, const char buf[], int len, int flags, ... )
{
	xlog_s	*xp = XP( pxlog ) ;
	va_list	ap ;
	int		status ;

	va_start( ap, flags ) ;
	status = XL_WRITE( xp, buf, len, flags, ap ) ;
	va_end( ap ) ;

	if ( status != XLOG_ENOERROR )
	{
		CALLBACK( xp, status ) ;
	}
}


/* VARARGS2 */
int xlog_control( xlog_h pxlog, xlog_cmd_e cmd, ... )
{
	va_list	ap ;
	xlog_s	*xp		= XP( pxlog ) ;
	int		status	= XLOG_ENOERROR ;

	va_start( ap, cmd ) ;

	switch ( cmd )
	{
		case XLOG_LINK:
			xlog_unlink( xp ) ;
			xlog_link( xp, va_arg( ap, xlog_s * ) ) ;
			xp->xl_callback_arg = va_arg( ap, void * ) ;
			break ;
		
		case XLOG_CALLBACK:
			xp->xl_callback = va_arg( ap, voidfunc ) ;
			break ;
			
		case XLOG_GETFLAG:
		case XLOG_SETFLAG:
			xlog_flags( xp, cmd, ap ) ;
			break ;

		default:
			status = XL_CONTROL( xp, cmd, ap ) ;
	}

	va_end( ap ) ;

	return( status ) ;
}


int xlog_parms( xlog_e type, ... )
{
	va_list	ap ;
	int		status ;

	va_start( ap, type ) ;
	switch ( type )
	{
#ifndef NO_SYSLOG
		case XLOG_SYSLOG:
			status = (*__xlog_syslog_ops.parms)( type, ap ) ;
			break ;
#endif
		case XLOG_FILELOG:
			status = (*__xlog_filelog_ops.parms)( type, ap ) ;
			break ;
		
		default:
			status = XLOG_ENOERROR ;
	}
	va_end( ap ) ;
	return( status ) ;
}

