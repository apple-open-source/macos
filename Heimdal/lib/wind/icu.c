/*
 * ICU glue
 */

#include "windlocl.h"
#include <unicode/usprep.h>

int
wind_stringprep(const uint32_t *in, size_t in_len,
		uint32_t *out, size_t *out_len,
		wind_profile_flags flags)
{
    UErrorCode status = 0;
    UStringPrepProfile *profile;
    UStringPrepProfileType type;
    UChar *uin, *dest;
    int32_t len;
    size_t n;

    if (in_len > UINT_MAX / sizeof(in[0]) || (*out_len) > UINT_MAX / sizeof(out[0]))
	return EINVAL;

    if (flags & WIND_PROFILE_SASL)
	type = USPREP_RFC4013_SASLPREP;
    else
	return EINVAL;

    /*
     * Should cache profile
     */

    profile = usprep_openByType(type, &status);
    if (profile == NULL)
	return ENOENT;

    uin = malloc(in_len * sizeof(uin[0]));
    dest = malloc(*out_len * sizeof(dest[0]));
    if (uin == NULL || dest == NULL) {
	free(uin);
	free(dest);
	usprep_close(profile);
	return ENOMEM;
    }
    
    /* ucs42ucs2 - don't care about surogates */
    for (n = 0; n < in_len; n++)
	uin[n] = in[n];

    status = 0;

    len = usprep_prepare(profile, uin, (int32_t)in_len, dest, (int32_t)*out_len,
			 USPREP_DEFAULT, NULL, &status);
    
    if (len < 0 || status) {
	free(dest);
	free(uin);
	return EINVAL;
    }

    for (n = 0; n < len; n++)
	out[n] = dest[n];

    *out_len = len;

    free(dest);
    free(uin);

    return 0;
}
