/*
 *  AppleUSBXHCI_RootHub.h
 *
 *  Copyright © 2011 Apple Inc. All Rights Reserved. 
 *
 */

#ifndef _APPLEUSBXHCI_ROOTHUB_H_
#define _APPLEUSBXHCI_ROOTHUB_H_


struct XHCIRootHubResetParamsStruct
{
    IOReturn    status;
    UInt8       rhSpeed;
    UInt16      port;
};

typedef struct XHCIRootHubResetParamsStruct  XHCIRootHubResetParams, *XHCIRootHubResetParamPtr;



#endif
