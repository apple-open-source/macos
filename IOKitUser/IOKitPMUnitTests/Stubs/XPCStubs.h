//
//  XPCStubs.h
//  IOKitUser
//
//  Created by Faramola Isiaka on 8/2/21.
//

#ifndef XPCStubs_h
#define XPCStubs_h

#include <xpc/xpc.h>

void addReplyForConnection(const char* connectionName, xpc_object_t reply);
void XPCStubsTeardown(void);

#endif /* XPCStubs_h */
