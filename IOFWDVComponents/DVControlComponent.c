/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <Carbon/Carbon.h>

//#include <Resources.h>
//#include <OSUtils.h>

//#include <Components.h>

#include "DeviceControlPriv.h"
#include "IsochronousDataHandler.h"
#include "DVVers.h"

#include <stdio.h>
#include <stdlib.h>
//#include <syslog.h>	// Debug messages

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/avc/IOFireWireAVCLib.h>

#define DEBUG 0

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef struct ControlComponentInstance	ControlComponentInstance, *ControlComponentInstancePtr;

struct ControlComponentInstance
{
    // Instance variables in MacOS9 version
    ComponentInstance	self;
    Boolean		fDeviceEnable;
    // X Stuff
    IOFireWireAVCLibUnitInterface **fAVCInterface;
};							

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* QT defines */

#define CALLCOMPONENT_BASENAME() IDHDV
#define CALLCOMPONENT_GLOBALS()	ControlComponentInstancePtr storage

#define DEVICECONTROL_BASENAME()	FWDVC
#define DEVICECONTROL_GLOBALS()	ControlComponentInstancePtr storage
#include "DeviceControl.k.h"
#include "DeviceControlPriv.k.h"

/* Function prototypes */

pascal ComponentResult
FWDVCCodecComponentDispatch(ComponentParameters *params, char ** storage);


/* ---------------------- Debug Stuff -------------------------- */
#ifdef DEBUG
#define FailMessage(cond)		assert (!(cond))
#else
#define FailMessage(cond)		{}
#endif
#define FailWithVal(cond, handler,num)	\
        if (cond) {			\
                goto handler;		\
        }
#define FailWithAction(cond, action, handler)	\
        if (cond) {				\
                { action; }			\
                goto handler;			\
        }

#define FailIf(cond, handler)	\
        if (cond) {		\
            FailMessage(false);	\
                goto handler;	\
        }

#if DEBUG
static void print4(UInt32 val)
{
    char a, b, c, d;
    a = val>>24;
    b = val>>16;
    c = val>>8;
    d = val;

    if(a >= ' ' && b >= ' ' && c >= ' ' && d >= ' ')
        printf("%c%c%c%c", a, b, c, d);
    else
        printf(" 0x%x ", (int)val);
}

static void RecordEventLogger(UInt32 a, UInt32 b, UInt32 c, UInt32 d)
{
    if(a)
    print4(a);
    if(b)
    print4(b);
    if(c)
    print4(c);
    if(d)
    print4(d);
    printf("\n");
}
#else
#define RecordEventLogger(a, b, c, d)
#endif

//====================================================================================
//
// DoAVCTransaction()
//
//	ToDo:
//====================================================================================
static pascal ComponentResult
FWDVCDeviceControlDoAVCTransaction(ControlComponentInstancePtr dc,
                DVCTransactionParams* inTransaction)
{
    if ( dc->fAVCInterface == NULL )
            return(kIDHErrInvalidDeviceID);

    if ( !dc->fDeviceEnable )
            return(kIDHErrDeviceDisconnected);
    return (*dc->fAVCInterface)->AVCCommand(dc->fAVCInterface,
            inTransaction->commandBufferPtr, inTransaction->commandLength,
            inTransaction->responseBufferPtr, &inTransaction->responseBufferSize);

}

//====================================================================================
//
// EnableAVCTransactions()
//
//
//====================================================================================
static pascal ComponentResult
FWDVCDeviceControlEnableAVCTransactions(ControlComponentInstancePtr dc)
{
    ComponentResult	result = noErr;

    if ( dc->fAVCInterface != NULL )
            dc->fDeviceEnable = true;
    else
            result = kIDHErrDeviceNotOpened;

    return result;
}

//====================================================================================
//
// DisableAVCTransactions()
//
//
//====================================================================================
static pascal ComponentResult
FWDVCDeviceControlDisableAVCTransactions(ControlComponentInstancePtr dc)
{
    ComponentResult				result = noErr;

    dc->fDeviceEnable = false;

    return result;
}

//====================================================================================
//
// SetDeviceConnectionID()
//
//
//====================================================================================
static pascal ComponentResult
FWDVCDeviceControlSetDeviceConnectionID(
                            ControlComponentInstancePtr dc, DeviceConnectionID connectionID)
{
    ComponentResult		result = noErr;

    if ( dc->fDeviceEnable )
            result = kIDHErrDeviceInUse;
    else {
        if(dc->fAVCInterface != NULL)
            (*dc->fAVCInterface)->Release(dc->fAVCInterface);
        dc->fAVCInterface = (IOFireWireAVCLibUnitInterface **)(connectionID);
        if(dc->fAVCInterface != NULL)
            (*dc->fAVCInterface)->AddRef(dc->fAVCInterface);
    }

    return result;
}

//====================================================================================
//
// GetDeviceConnectionID()
//
//
//====================================================================================
static pascal ComponentResult
FWDVCDeviceControlGetDeviceConnectionID(
                            ControlComponentInstancePtr dc, DeviceConnectionID* connectionID)
{
    *connectionID = (DeviceConnectionID)dc->fAVCInterface;
    return noErr;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static pascal ComponentResult
FWDVCComponentOpen(ControlComponentInstancePtr storage, ComponentInstance self)
{
    kern_return_t err = noErr;

    RecordEventLogger( 'devc', 'open', 0, 0);
    storage = (ControlComponentInstancePtr)NewPtrClear(sizeof(ControlComponentInstance));
    if( nil == storage)
        return(MemError());
    RecordEventLogger( 'devc', 'ope2', (int)storage, 0);

    SetComponentInstanceStorage(self, (Handle) storage);

//Exit:

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static pascal ComponentResult
FWDVCComponentClose(ControlComponentInstancePtr storage, ComponentInstance self)
{
    RecordEventLogger( 'devc', 'clos', 0, (unsigned long) storage);
    if( !storage)
        return( noErr );

    if(storage->fAVCInterface != NULL)
        (*storage->fAVCInterface)->Release(storage->fAVCInterface);
    DisposePtr((Ptr) storage);

    SetComponentInstanceStorage(self, (Handle) nil );

    return( noErr );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static pascal ComponentResult
FWDVCComponentVersion(ControlComponentInstancePtr storage)
{
    RecordEventLogger( 'devc', 'vers', 0, 0);
    return 0x10001;
}

static pascal ComponentResult
FWDVCComponentRegister(ControlComponentInstancePtr storage)
{
    // need to re-register with each source type?
    RecordEventLogger( 'devc', 'reg ', 0, 0);
    return( noErr );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DoCDispatchWS(x,p,s)		\
    case k ## x ## Select:		\
        /* printf("FWDV" #x "{"); */	\
        { ComponentResult err;		\
            err = CallComponentFunctionWithStorageProcInfo( s, p, (ProcPtr) FWDVC ## x,	\
                                                uppCall ## x ## ProcInfo );		\
        /* printf("%ld}\n", err); */	\
        return err;	}

#define DoDispatchWS(x,p,s)		\
    case k ## x ## Select:		\
        /* printf("FWDV" #x "{"); */	\
        { ComponentResult err;		\
            err = CallComponentFunctionWithStorageProcInfo( s, p, (ProcPtr) FWDVC ## x,	\
                                                upp ## x ## ProcInfo );			\
        /* printf("%ld}\n", err); */ 	\
        return err;	}


static pascal ComponentResult
FWDVCComponentCanDo(ControlComponentInstancePtr storage, short selector)
{
    RecordEventLogger( 'devc', 'cand', 0, 0);

    switch(selector) {
        /* Standard selectors */
        case kComponentOpenSelect:
        case kComponentCloseSelect:
        case kComponentCanDoSelect:
        case kComponentVersionSelect:

        /* Device Control selectors */
        case kDeviceControlDoAVCTransactionSelect:
        case kDeviceControlEnableAVCTransactionsSelect:
        case kDeviceControlDisableAVCTransactionsSelect:
        case kDeviceControlSetDeviceConnectionIDSelect:
        case kDeviceControlGetDeviceConnectionIDSelect:

            return(true);

        default:
            RecordEventLogger( 'devc', 'cant', selector, 0);
            return (false);
    }
}


pascal ComponentResult
FWDVCCodecComponentDispatch(ComponentParameters *params, char ** storage)
{
    ComponentResult result;

    /*	If the selector is less than zero, it's a Component manager selector.	*/

    if ( params->what < 0  ) {
        switch ( params->what ) {
            DoCDispatchWS( ComponentOpen, params, storage );
            DoCDispatchWS( ComponentClose, params, storage );
            DoCDispatchWS( ComponentRegister, params, storage );
            DoCDispatchWS( ComponentCanDo, params, storage );
            DoCDispatchWS( ComponentVersion, params, storage );

            default :
                return (paramErr);
        }
    }

    /*
     *	Here we dispatch the rest of our calls. We use the magic thing manager routine which
     *	calls our subroutines with the proper parameters. The prototypes are in Image Codec.h.
     */
    switch ( params->what ) {
        DoDispatchWS( DeviceControlDoAVCTransaction, params, storage );
        DoDispatchWS( DeviceControlEnableAVCTransactions, params, storage );
        DoDispatchWS( DeviceControlDisableAVCTransactions, params, storage );
        DoDispatchWS( DeviceControlSetDeviceConnectionID, params, storage );
        DoDispatchWS( DeviceControlGetDeviceConnectionID, params, storage );
        
    default:
        {
            int len = params->paramSize/4;
            int i;
            printf("DVC unimp:%d %d ", params->what, params->paramSize);
            for(i=0; i<len; i++)
                printf("0x%lx ", params->params[i]);
            printf("\n");
            result = paramErr;
            return(result);
        }
    }
}


