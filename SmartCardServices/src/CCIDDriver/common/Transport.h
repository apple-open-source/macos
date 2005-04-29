/*
 *  Transport.h
 *  ifd-CCID
 *
 *  Created by JL on Sat Jun 14 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */

#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__
#ifdef __cplusplus
extern "C"
{
#endif

#include "wintypes.h"


typedef enum {
  TrRv_OK                     = 0x00,
  TrRv_ERR                    = 0x01,
} TrRv;


typedef struct {
    TrRv (*Open)( DWORD lun, DWORD channel );
    // returns the number of conifguration descriptors once the device has been successfully opened
    TrRv (*GetConfigDescNumber)( DWORD lun, BYTE* pcconfigDescNb );
    TrRv (*GetVendorAndProductID)( DWORD lun, DWORD *vendorID, DWORD *productID );
    // Reads the class descriptor once the device has been successfully opened
    TrRv (*GetClassDesc)(  DWORD lun, BYTE configDescNb, BYTE bdescType,
                           BYTE *pcdesc, BYTE *pcdescLength);
    TrRv (*SetupConnections)( DWORD lun, BYTE ConfigDescNb, BYTE interruptPipe);
    TrRv (*Write)( DWORD lun, DWORD length, BYTE *Buffer );
    TrRv (*Read)( DWORD lun, DWORD *length, BYTE *Buffer );
    TrRv (*Close)( DWORD lun );
} TrFunctions;

// MAKE SURE VALUES AND ORDER MATCH TABLE IN Transport.c
typedef enum {
  TrType_USB                     = 0x00,
  TrType_SERIAL                  = 0x01,
} TrType;

extern TrFunctions TrFunctionTable[];
#ifdef __cplusplus
}
#endif


#endif