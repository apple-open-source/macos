/*
 * LoginSessions.h
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

#ifndef LOGIN_SESSION_H
#define LOGIN_SESSION_H

// Session ID is a 32 bit quanitity, so the longest string is 4294967295U
#define kSecuritySessionStringMaxLength  16
#define kMachIPCMaxServicePrefixLength   256
#define kServiceNameMaxLength            (kMachIPCMaxServicePrefixLength + kSecuritySessionStringMaxLength + 3)

#define loginSessionHasGraphicsAccess  0x00000001
#define loginSessionCallerUsesGUI      0x00000002
#define loginSessionHasTerminalAccess  0x00000004

typedef u_int32_t LoginSessionAttributes;

#if __cplusplus
extern "C" {
#endif
    
const char *LoginSessionGetSecuritySessionName (void);

boolean_t LoginSessionIsRootSession (void);

LoginSessionAttributes LoginSessionGetSessionAttributes (void);

uid_t LoginSessionGetSessionUID (void);

uid_t LoginSessionGetSecurityAgentUID (void);

#if __cplusplus
}
#endif

#endif /* LOGIN_SESSION_H */
