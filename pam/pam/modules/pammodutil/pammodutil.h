#ifndef PAMMODUTIL_H
#define PAMMODUTIL_H

/*
 * $Id: pammodutil.h,v 1.3 2002/03/27 19:20:25 bbraun Exp $
 *
 * Copyright (c) 2001 Andrew Morgan <morgan@kernel.org>
 */

#include <pam/_pam_aconf.h>
#include <pam/_pam_macros.h>
#include <pam/pam_modules.h>
#include <pam/_pam_modutil.h>

#define PWD_INITIAL_LENGTH     0x100
#define PWD_ABSURD_PWD_LENGTH  0x1000

/* This is a simple cleanup, it just free()s the 'data' memory */
extern void _pammodutil_cleanup(pam_handle_t *pamh, void *data,
				int error_status);

#endif /* PAMMODUTIL_H */
