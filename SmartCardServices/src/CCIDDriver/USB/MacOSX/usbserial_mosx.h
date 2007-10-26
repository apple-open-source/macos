/*
 *  usbserial.h
 *  ifd-CCID
 *
 *  Created by JL on Mon Feb 10 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */

#ifndef _USBSERIAL_MOSX_H_
#define _USBSERIAL_MOSX_H_

typedef struct _intFace {
    IOUSBInterfaceInterface245  **iface;
    IOUSBDeviceInterface245     **dev;
    UInt32 						usbAddr;
    UInt8						inPipeRef;
    UInt8						outPipeRef;
    UInt8 						used;
    UInt8 						ready;
    UInt16	 					vendorID;
    UInt16                      productID;
    UInt8                       class;
    UInt8                       subClass;
    UInt8                       protocol;
} intrFace, *pIntrFace;

#endif

