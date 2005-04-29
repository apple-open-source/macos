/*
 *  Transport.c
 *  ifd-CCID
 *
 *  Created by JL on Sat Jun 14 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */
#include "pcscdefines.h"

#include "Transport.h"
#include "usbserial.h"


// MAKE SURE VALUES AND ORDER MATCH ENUM IN Transport.h
TrFunctions TrFunctionTable[] =
{
  {
      OpenUSB,
      GetConfigDescNumberUSB,
      GetVendorAndProductIDUSB,
      GetClassDescUSB,
      SetupConnectionsUSB,
      WriteUSB,
      ReadUSB,
      CloseUSB
  }
    
};


