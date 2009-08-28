/*
 * Compatibility shims with RELENG_4
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved.
 */
#ifndef SMB_COMPAT4_H
#define SMB_COMPAT4_H

#include <sys/param.h>

mbuf_t smb_mbuf_getm(mbuf_t m, size_t len, int how, int type);

#endif	/* !SMB_COMPAT4_H */
