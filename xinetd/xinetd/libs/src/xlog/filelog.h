/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


/*
 * $Id: filelog.h,v 1.1.1.2 2003/05/22 01:16:36 rbraun Exp $
 */

#ifndef __FILELOG_H
#define __FILELOG_H

#include "impl.h"

/*
 * The file can be either open or closed.
 * When the file is closed, the state is always FL_CLOSED.
 * When the file is open, the state is:
 *		FL_OPEN:			if everything is ok
 *		FL_SIZE:			if the hard limit was exceeded
 *		FL_ERROR:		if an error occured
 */
typedef enum { FL_CLOSED = 0, FL_OPEN, FL_SIZE, FL_ERROR } filelog_state_e ;

struct filelog_s
{
	int					fl_fd ;
	filelog_state_e	fl_state ;
	int 					fl_error ;				/* error code when in FL_ERROR		*/
	bool_int				fl_size_control ;		/* enabled or not							*/
	bool_int				fl_issued_warning ;	/* when the soft limit was exceeded	*/
	unsigned 			fl_size ;            /* current size                   	*/
	unsigned				fl_soft_limit ;
	unsigned				fl_hard_limit ;
} ;

#define FILELOG_ENABLE_SIZE_CONTROL( flp )	(flp)->fl_size_control = TRUE
#define FILELOG_DISABLE_SIZE_CONTROL( flp )	(flp)->fl_size_control = FALSE
#define FILELOG_SIZE_CONTROL( flp )    		( (flp)->fl_size_control )

#define FILELOG( xp )			((struct filelog_s *)xp->xl_data)

#endif

