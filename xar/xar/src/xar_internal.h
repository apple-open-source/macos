//
//  xar_internal.h
//  xarLib
//
//  Created by Jared Jones on 10/12/21.
//

#ifndef _XAR_INTERNAL_H_
#define _XAR_INTERNAL_H_

#ifdef XARSIG_BUILDING_WITH_XAR
#include "xar.h"
#else
#include <xar/xar.h>
#endif // XARSIG_BUILDING_WITH_XAR

// Undeprecate these for internal usage
xar_t xar_open(const char *file, int32_t flags) API_AVAILABLE(macos(10.4));
xar_t xar_open_digest_verify(const char *file, int32_t flags, void *expected_toc_digest, size_t expected_toc_digest_len) API_AVAILABLE(macos(10.14.4));
char *xar_get_path(xar_file_t f) API_AVAILABLE(macos(10.4));

#endif /* _XAR_INTERNAL_H_ */
