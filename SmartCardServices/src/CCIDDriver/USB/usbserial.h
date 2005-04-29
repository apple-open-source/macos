/*
 *  usbserial.h
 *  ifd-CCID
 *
 *  Created by JL on Mon Feb 10 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */

#ifndef _USBSERIAL_H_
#define _USBSERIAL_H_

#include "Transport.h"

// Open the base connection
TrRv OpenUSB( DWORD lun, DWORD channel );
// returns the number of conifguration descriptors
TrRv GetConfigDescNumberUSB( DWORD lun, BYTE* pcconfigDescNb );
// returns product and vendor ID for the USB device connected on LUN lun
TrRv GetVendorAndProductIDUSB( DWORD lun, DWORD *vendorID, DWORD *productID );
// Reads Class descriptor  once the device has been successfulle opened
// Can be called with pcdesc == NULL to find out the length
TrRv GetClassDescUSB( DWORD lun, BYTE configDescNb, BYTE bdescType,
                      BYTE *pcdesc, BYTE *pcdescLength);
// Sets connection to pipes
TrRv SetupConnectionsUSB( DWORD lun, BYTE ConfigDescNb, BYTE interruptPipe);
TrRv SetupUSB( DWORD lun,  void* pBuffer, DWORD * pLength);
TrRv WriteUSB( DWORD lun, DWORD length, BYTE *Buffer );
TrRv ReadUSB( DWORD lun, DWORD *length, BYTE *Buffer );
TrRv CloseUSB( DWORD lun );

#endif

