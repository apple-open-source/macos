/*
 * mach_client_utilities.h
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosIPC/Headers/Kerberos/mach_client_utilities.h,v 1.5 2003/04/23 21:41:49 lxs Exp $
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

#ifndef MACH_CLIENT_UTILITIES_H
#define MACH_CLIENT_UTILITIES_H

#include <mach/mach.h>
#include <mach/message.h>
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
mach_client_lookup_server (const char *inServiceNamePrefix,
                           mach_port_t *outServicePort);
                        
kern_return_t
mach_client_lookup_and_launch_server (const char *inServiceNamePrefix,
                                      const char *inBundleIdentifier,
                                      const char *inPath,
                                      const char *inServerBundleName,
                                      mach_port_t *outServicePort);
                                            
boolean_t
mach_client_allow_server (security_token_t inToken);

#if __cplusplus
}
#endif

#if __cplusplus
class MachServerPort {
    public:
    
        MachServerPort (const char *inServiceNamePrefix,
                        const char *inBundleIdentifier, 
                        const char *inPath, 
                        const char *inServerBundleName) : mPort (MACH_PORT_NULL)  
            { 
                kern_return_t err;
                
                err = mach_client_lookup_and_launch_server (inServiceNamePrefix, 
                                                            inBundleIdentifier, 
                                                            inPath, 
                                                            inServerBundleName,
                                                            &mPort);
                
                // Clean up after errors
                if (err != BOOTSTRAP_SUCCESS) {
                    if (mPort != MACH_PORT_NULL) {
                        mach_port_deallocate (mach_task_self(), mPort);
                    }
                    mPort = MACH_PORT_NULL;
                }
            }
            
        MachServerPort (const char *inServiceNamePrefix) : mPort (MACH_PORT_NULL)  
            { 
                kern_return_t err;
                
                err = mach_client_lookup_server (inServiceNamePrefix, &mPort);
                
                // Clean up after errors
                if (err != BOOTSTRAP_SUCCESS) {
                    if (mPort != MACH_PORT_NULL) {
                        mach_port_deallocate (mach_task_self(), mPort);
                    }
                    mPort = MACH_PORT_NULL;
                }
            }

        ~MachServerPort () 
            { 
                if (mPort != MACH_PORT_NULL) 
                    mach_port_deallocate (mach_task_self(), mPort); 
            }
        
        mach_port_t Get () const 
            { 
                return mPort; 
            } 
        
    private:
        mach_port_t	mPort;        
};
#endif /* __cplusplus */

#endif /* MACH_CLIENT_UTILITIES_H */