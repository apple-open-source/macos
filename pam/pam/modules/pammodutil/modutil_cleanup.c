/*
 * $Id: modutil_cleanup.c,v 1.2 2002/03/27 02:36:46 bbraun Exp $
 *
 * This function provides a common pam_set_data() friendly version of free().
 */

#include "pammodutil.h"

void _pammodutil_cleanup(pam_handle_t *pamh, void *data, int error_status)
{
    if (data) {
	/* junk it */
	(void) free(data);
    }
}

