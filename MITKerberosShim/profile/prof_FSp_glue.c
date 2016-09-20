/*
 * prof_FSp_glue.c --- Deprecated FSSpec functions.  Mac-only.
 */

#include "prof_int.h"

#include <limits.h>

#include <CoreServices/CoreServices.h>

long KRB5_CALLCONV FSp_profile_init (const FSSpec* files, profile_t *ret_profile);

long KRB5_CALLCONV FSp_profile_init_path (const FSSpec* files, profile_t *ret_profile);

errcode_t KRB5_CALLCONV
FSp_profile_init(files, ret_profile)
	const FSSpec* files;
	profile_t *ret_profile;
{
    return memFullErr;
}

errcode_t KRB5_CALLCONV
FSp_profile_init_path(files, ret_profile)
	const FSSpec* files;
	profile_t *ret_profile;
{
    return FSp_profile_init (files, ret_profile);
}
