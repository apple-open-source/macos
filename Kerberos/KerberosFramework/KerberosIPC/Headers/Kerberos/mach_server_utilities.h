/*
 * mach_server_utilities.h
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosIPC/Headers/Kerberos/mach_server_utilities.h,v 1.12 2003/04/23 21:41:52 lxs Exp $
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

#ifndef MACH_SERVER_UTILITIES_H
#define MACH_SERVER_UTILITIES_H

#include <mach/mach.h>
#include <mach/boolean.h>
#include <servers/bootstrap.h>
#include <sys/param.h>

#include <Kerberos/LoginSessions.h>

#define kMachIPCMaxMsgSize               2048 + MAX_TRAILER_SIZE
#define kMachIPCTimeout                  200

#if __cplusplus
extern "C" {
#endif
                                            
kern_return_t
mach_server_init (const char *inServiceNamePrefix,
                  boolean_t (*inDemuxProc)(mach_msg_header_t *, mach_msg_header_t *));

kern_return_t
mach_server_run_server ();

boolean_t 
mach_server_demux (mach_msg_header_t *request, mach_msg_header_t *reply);

mach_port_t
mach_server_get_server_port ();

boolean_t
mach_server_become_user (uid_t inNewServerUID);

boolean_t 
mach_server_quit_self ();

#if __cplusplus
}
#endif

#endif /* MACH_SERVER_UTILITIES_H */