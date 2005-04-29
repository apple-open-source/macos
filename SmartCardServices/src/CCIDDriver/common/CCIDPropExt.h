/*
 *  CCIDPropExt.h
 *  ifd-CCID
 *
 *  Created by JL on Sun Jul 20 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */

// CCID Proprietary extension management

#ifndef __CCIDPROPEXT_H__
#define __CCIDPROPEXT_H__

#include "CCID.h"

typedef struct {
    CCIDRv (*OpenChannel)(DWORD lun);
    CCIDRv (*CloseChannel)(DWORD lun);
    CCIDRv (*PowerOn)(DWORD Lun, BYTE *abDataResp, DWORD *pdwDataRespLength,
                      BYTE bStatus, BYTE bError);
} CCIDPropExt;

// Looks-up for a proprietary extension structure according to vendorID/ProductID
CCIDRv CCIDPropExtLookupExt(DWORD dwVendorID, DWORD dwProductID, CCIDPropExt *pstCCIDPropExt);



#endif