/*
 * @OSF_COPYRIGHT@
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <mach/boolean.h>
#include <mach/error.h>
#include <mach/mach_error.h>

#include "errorlib.h"
#include "externs.h"

static void do_compat(mach_error_t *);

static void
do_compat(mach_error_t *org_err)
{
	mach_error_t		err = *org_err;

	/* 
	 * map old error numbers to 
	 * to new error sys & subsystem 
	 */

	if ((-200 < err) && (err <= -100))
		err = -(err + 100) | IPC_SEND_MOD;
	else if ((-300 < err) && (err <= -200))
		err = -(err + 200) | IPC_RCV_MOD;
	else if ((-400 < err) && (err <= -300))
		err = -(err + 300) | MACH_IPC_MIG_MOD;
	else if ((1000 <= err) && (err < 1100))
		err = (err - 1000) | SERV_NETNAME_MOD;
	else if ((1600 <= err) && (err < 1700))
		err = (err - 1600) | SERV_ENV_MOD;
	else if ((27600 <= err) && (err < 27700))
		err = (err - 27600) | SERV_EXECD_MOD;

	*org_err = err;
}

char *
mach_error_type(mach_error_t err)
{
	int sub, system;

	do_compat( &err );

	sub = err_get_sub(err);
	system = err_get_system(err);

	if (system > err_max_system || sub >= _mach_errors[system].max_sub)
	    return((char *)"(?/?)");
	return(_mach_errors[system].subsystem[sub].subsys_name);
}

boolean_t mach_error_full_diag = FALSE;

__private_extern__ char *
mach_error_string_int(mach_error_t err, boolean_t *diag)
{
	int sub, system, code;

	do_compat( &err );

	sub = err_get_sub(err);
	system = err_get_system(err);
	code = err_get_code(err);

	*diag = TRUE;

	if (system > err_max_system)
	    return((char *)"(?/?) unknown error system");
	if (sub >= _mach_errors[system].max_sub)
	    return(_mach_errors[system].bad_sub);
	if (code >= _mach_errors[system].subsystem[sub].max_code)
	    return ((char *)NO_SUCH_ERROR);

	*diag = mach_error_full_diag;
	return( _mach_errors[system].subsystem[sub].codes[code] );
}

char *
mach_error_string(mach_error_t err)
{
	boolean_t diag;

	return mach_error_string_int( err, &diag );

}
