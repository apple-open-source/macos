/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#ifndef NO_SYSLOG
#include <syslog.h>
#else
#define LOG_ALERT				0
#endif

#include "sio.h"
#include "str.h"
#include "xlog.h"
#include "filelog.h"

static int filelog_init(xlog_s *, va_list) ;
static void filelog_fini(xlog_s *) ;
static int filelog_control(xlog_s *, xlog_cmd_e, va_list) ;
static int filelog_write(xlog_s *, const char buf[], int, int, va_list) ;
static int filelog_parms(xlog_e, va_list) ;
static int limit_checks(const xlog_s *) ;

struct xlog_ops __xlog_filelog_ops = 
	{
		filelog_init,
		filelog_fini,
		filelog_write,
		filelog_control,
		filelog_parms
	} ;


static int filelog_init( xlog_s *xp, va_list ap )
{
	int				fd ;
	struct filelog_s		*flp ;
	char				*filename;
	int				flags;

	filename = va_arg(ap, char *);
	flags = va_arg(ap, int);

	flp = NEW( struct filelog_s ) ;
	if ( flp == NULL )
		return( XLOG_ENOMEM ) ;

	if ( flags & O_CREAT )
		fd = open( filename, flags, va_arg( ap, int ) ) ;
	else
		fd = open( filename, flags ) ;

	if ( fd == -1 )
	{
		free( flp ) ;
		return( XLOG_EOPEN ) ;
	}
	
	FILELOG_DISABLE_SIZE_CONTROL( flp ) ;
	(void) Sbuftype( fd, SIO_LINEBUF ) ;
	flp->fl_fd = fd ;
	flp->fl_state = FL_OPEN ;
	xp->xl_data = flp ;
	return( XLOG_ENOERROR ) ;
}


static void filelog_fini( xlog_s *xp )
{
	struct filelog_s *flp = FILELOG( xp ) ;

	if ( flp->fl_state != FL_CLOSED )
	{
		(void) Sclose( flp->fl_fd ) ;
		flp->fl_state = FL_CLOSED ;
	}
	free( flp ) ;
	xp->xl_data = NULL ;
}


static int filelog_control( xlog_s *xp, xlog_cmd_e cmd, va_list ap )
{
	struct stat		st ;
	struct filelog_s *flp	= FILELOG( xp ) ;
	int		status	= XLOG_ENOERROR ;

	if ( flp->fl_state == FL_ERROR )
		return( flp->fl_error ) ;

	switch ( cmd )
	{
		case XLOG_GETFD:
			if ( flp->fl_state == FL_OPEN )
				*va_arg( ap, int * ) = flp->fl_fd ;
			else
				status = XLOG_ENOERROR ;
			break ;

		case XLOG_LIMITS:
			flp->fl_soft_limit = va_arg( ap, unsigned ) ;
			flp->fl_hard_limit = va_arg( ap, unsigned ) ;
			flp->fl_issued_warning = FALSE ;
			FILELOG_ENABLE_SIZE_CONTROL( flp ) ;
			flp->fl_state = FL_OPEN ;
			/* FALL THROUGH */

		case XLOG_SIZECHECK:
			if ( ! FILELOG_SIZE_CONTROL( flp ) )
				break ;
			if ( fstat( flp->fl_fd, &st ) == -1 )
			{
				FILELOG_DISABLE_SIZE_CONTROL( flp ) ;
				flp->fl_state = FL_ERROR ;
				flp->fl_error = status = XLOG_EFSTAT ;
			}
			else
			{
				flp->fl_size = st.st_size ;
				if ( flp->fl_size > flp->fl_soft_limit )
					status = limit_checks( xp ) ;
			}
			break ;
		case XLOG_LINK:
		case XLOG_CALLBACK:
		case XLOG_GETFLAG:
		case XLOG_SETFLAG:
		case XLOG_LEVEL:
		case XLOG_FACILITY:
		case XLOG_PREEXEC:
		case XLOG_POSTEXEC:
			break;
	}
	return( status ) ;
}


static int limit_checks( const xlog_s *xp )
{
	struct filelog_s *flp = FILELOG( xp ) ;
	char buf[ 100 ] ;

	if ( ! flp->fl_issued_warning )
	{
		if ( xp->xl_use != NULL )
			xlog_write( (xlog_h) xp->xl_use, buf,
				strx_nprint( buf, sizeof( buf ),
				"soft limit exceeded on '%s'", xp->xl_id ),
					XLOG_NOFLAGS, LOG_ALERT ) ;
		flp->fl_issued_warning = TRUE ;
	}

	if ( flp->fl_size <= flp->fl_hard_limit )
		return( XLOG_ENOERROR ) ;
	
	if ( xp->xl_use != NULL )
		xlog_write( (xlog_h) xp->xl_use, buf,
			strx_nprint( buf, sizeof( buf ),
			"hard limit exceeded on '%s'; log closed", xp->xl_id ),
				XLOG_NOFLAGS, LOG_ALERT ) ;
	flp->fl_state = FL_ERROR ;
	return( XLOG_ESIZE ) ;
}


static int filelog_write( xlog_s *xp, const char buf[], int len, int flags, 
	va_list ap )
{
	struct filelog_s *flp	= FILELOG( xp ) ;
	int 	action_flags	= ( xp->xl_flags | flags ) ;
	int	msglen		= 0 ;
	int	percent_m_pos   = 0 ;
	int	cc ;
	int	status ;
	time_t 		current_time ;
	struct tm	*tmp ;

	if ( flp->fl_state != FL_OPEN )
		return( flp->fl_error ) ;

	(void) time( &current_time ) ;
	tmp = localtime( &current_time ) ;
	cc = Sprint( flp->fl_fd, "%02d/%d/%d@%02d:%02d:%02d",
		tmp->tm_year%100, tmp->tm_mon+1, tmp->tm_mday,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec ) ;
	if ( cc == SIO_ERR ) 
		return XLOG_EWRITE;
	
	msglen += cc ;

	if ( action_flags & XLOG_PRINT_ID )
	{
		cc = Sprint( flp->fl_fd, " %s", xp->xl_id ) ;
		if ( cc == SIO_ERR ) {
			flp->fl_size += msglen ;
			return XLOG_EWRITE;
		}
		msglen += cc ;
	}

	if ( action_flags & XLOG_PRINT_PID )
	{
		cc = Sprint( flp->fl_fd, "[%d]", getpid() ) ;
		if ( cc == SIO_ERR ) { 
			flp->fl_size += msglen ;
			return XLOG_EWRITE;
		}
		msglen += cc ;
	}

	cc = Sprint( flp->fl_fd, ": " ) ;
	if ( cc == SIO_ERR ) { 
		flp->fl_size += msglen ;
		return XLOG_EWRITE;
	}
	msglen += cc ;

	if ( ( action_flags & XLOG_NO_ERRNO ) ||
		( percent_m_pos = __xlog_add_errno( buf, len ) ) == -1 )
	{
		cc = Swrite( flp->fl_fd, buf, len ) ;
		if ( cc == SIO_ERR ) { 
			flp->fl_size += msglen ;
			return XLOG_EWRITE;
		}
		msglen += cc ;
	}
	else
	{
		char errno_buf[ 100 ] ;
		unsigned size = sizeof( errno_buf ) ;
		char *ep ;

		/*
		 * The reason for the repetition of "msglen += cc ;" is that 
		 * in the future we may want to check cc for SIO_ERR
		 */
		ep = __xlog_explain_errno( errno_buf, &size ) ;
		cc = Swrite( flp->fl_fd, buf, percent_m_pos ) ;
		if ( cc == SIO_ERR ) { 
			flp->fl_size += msglen ;
			return XLOG_EWRITE;
		}
		msglen += cc ;
		cc = Swrite( flp->fl_fd, ep, size ) ;
		if ( cc == SIO_ERR ) { 
			flp->fl_size += msglen ;
			return XLOG_EWRITE;
		}
		msglen += cc ;
		cc = Swrite( flp->fl_fd, buf+percent_m_pos+2, 
				len-percent_m_pos-2 ) ;
		if ( cc == SIO_ERR ) { 
			flp->fl_size += msglen ;
			return XLOG_EWRITE;
		}
		msglen += cc ;
	}
	/*
	 * Writing a newline will cause a buffer flush since we asked for
	 * line-buffered output
	 */
	if ( Sputchar( flp->fl_fd, '\n' ) != SIO_ERR )
		msglen++ ;

	/*
	 * NOTE: we don't check if XLOG_NO_SIZECHECK is set in xp->xl_flags
	 *	because size control is off by default and in order to
	 *	be enabled XLOG_LIMITS must be used which overrides xp->xl_flags
	 */
	if ( ! FILELOG_SIZE_CONTROL( flp ) || ( flags & XLOG_NO_SIZECHECK ) )
		return( XLOG_ENOERROR ) ;

	flp->fl_size += msglen ;
	if ( flp->fl_size <= flp->fl_soft_limit || 
		( status = limit_checks( xp ) ) == XLOG_ENOERROR )
		return( XLOG_ENOERROR ) ;
	
	flp->fl_state = FL_SIZE ;
	return( status ) ;
}


static int filelog_parms( xlog_e type, va_list ap)
{
	return( XLOG_ENOERROR ) ;
}

