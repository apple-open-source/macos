/*
 * KerberosAgent.h
 *
 * $Header$
 *
 * Copyright 2003 Massachusetts Institute of Technology.
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

#define KERBEROSLOGIN_DEPRECATED
#include <Kerberos/KerberosLogin.h>
#include <Kerberos/KerberosLoginPrivate.h>
#include <Kerberos/mach_server_utilities.h>
#include <Kerberos/KerberosDebug.h>

#include <mach/message.h>
#include <mach/mach_init.h>
#include <mach/mach_error.h>

#include <syslog.h>
#include <stdarg.h>

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#endif
