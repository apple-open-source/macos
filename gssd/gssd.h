/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef maximum
#define	maximum(a, b) (((a)>(b))?(a):(b))
#endif

#ifndef minimum
#define	minimum(a, b) (((a)<(b))?(a):(b))
#endif

#define CAST(T,x) (T)(uintptr_t)(x)

#define str_to_buf(s, b) do { \
	(b)->value = (s); (b)->length = strlen(s) + 1; \
	} while (0)

#define Fatal(fmt, ...) fatal("%s: %d: " fmt, __func__, __LINE__,## __VA_ARGS__)
#define Debug(fmt, ...) gssd_log(ASL_LEVEL_DEBUG, "%s: %d: " fmt, __func__, __LINE__,## __VA_ARGS__)
#define DEBUG(level, ...) do {\
	if (get_debug_level() >= level) {\
		Debug(__VA_ARGS__); \
	}\
} while (0)

#define HEXDUMP(level, ...) do {\
	if (get_debug_level() >= level) {\
		HexDump(__VA_ARGS__); \
	}\
} while (0)

#define Info(fmt, ...) do {\
	if (get_debug_level() > 1) {\
		gssd_log(ASL_LEVEL_INFO, "%s: %d: " fmt, __func__, __LINE__,## __VA_ARGS__); \
	} else { \
		gssd_log(ASL_LEVEL_INFO, "%s: " fmt, __func__,## __VA_ARGS__); \
	}\
} while (0)

#define Log(fmt, ...) do {\
	if (get_debug_level()) {\
		gssd_log(ASL_LEVEL_ERR, "%s: %d: " fmt, __func__, __LINE__,## __VA_ARGS__); \
	} else { \
		gssd_log(ASL_LEVEL_ERR, fmt,## __VA_ARGS__); \
	}\
} while (0)


extern char *buf_to_str(gss_buffer_t);
extern void gssd_enter(void *);
extern void gssd_remove(void *);
extern int gssd_check(void *);
extern char *oid_name(gss_OID);
extern char *gss_strerror(gss_OID, uint32_t, uint32_t);
extern void __attribute__((noreturn)) fatal(const char *, ...);
extern void gssd_log(int, const char *, ...);
extern void HexDump(const char *, size_t);
extern int traced(void);
int in_foreground(int);
extern void set_debug_level(int);
extern void set_debug_level_init(void (*)(int));
extern int get_debug_level(void);
