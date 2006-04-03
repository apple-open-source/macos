/*
 * KerberosDebug.h
 *
 * $Header$
 *
 * Copyright 2004 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __KERBEROSDEBUG
#define __KERBEROSDEBUG

#include <sys/types.h>
#include <mach/mach.h>

#ifdef __cplusplus
extern "C" {
#endif
    
/* 
 * These symbols will be exported for use by Kerberos tools.
 * Give them names that won't collide with other applications
 * linking against the Kerberos framework.
 */

#define ddebuglevel      __KerberosDebug_ddebuglevel
#define dprintf          __KerberosDebug_dprintf
#define dprintmem        __KerberosDebug_dprintmem
#define dprintbootstrap  __KerberosDebug_dprintbootstrap
    
    
int ddebuglevel (void);
void dprintf (const char *format, ...) __attribute__ ((format (printf, 1, 2)));
void dprintmem (const void *data, size_t length);
void dprintbootstrap (task_t inTask);

#define SetSignalAction_(inAction)
#ifdef __PowerPlant__
#define GetSignalAction_() debugAction_Nothing
#else
#define GetSignalAction_() (0)
#endif

#ifdef __PowerPlant__
#	undef SignalPStr_
#	undef SignalCStr_
#	undef SignalIf_
#	undef SignalIfNot_
#endif /* __PowerPlant */

#define SignalPStr_(pstr)                                            \
    do {                                                             \
        dprintf ("%.*s in %s() (%s:%d)",                             \
                 (pstr) [0], (pstr) + 1,                             \
                 __FUNCTION__, __FILE__, __LINE__);                  \
    } while (0)

#define SignalCStr_(cstr)                                            \
    do {                                                             \
        dprintf ("%s in %s() (%s:%d)",                               \
                 cstr, __FUNCTION__, __FILE__, __LINE__);            \
    } while (0)

#define SignalIf_(test)                                              \
    do {                                                             \
        if (test) SignalCStr_("Assertion " #test " failed");         \
    } while (0)

#define SignalIfNot_(test) SignalIf_(!(test))

#define Assert_(test)      SignalIfNot_(test)

enum { errUncaughtException = 666 };

#define SafeTry_               try
#define SafeCatch_             catch (...)
#define SafeCatchOSErr_(error) catch (...) { SignalCStr_ ("Uncaught exception"); error = errUncaughtException; }

#define DebugThrow_(e)                                               \
    do {                                                             \
        dprintf ("Exception thrown from %s() (%s:%d)",               \
                 __FUNCTION__, __FILE__, __LINE__);                  \
        throw (e);                                                   \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* __KERBEROSDEBUG */
