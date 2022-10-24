//
//  stubdata.h
//  stubdata
//
//  Created by Cyndy Ishida on 3/24/22.
//  Copyright © 2022 Apple. All rights reserved.
//

#ifndef __STUBDATA_H__
#define __STUBDATA_H__

#include "unicode/utypes.h"
#include "unicode/udata.h"
#include "unicode/uversion.h"

typedef struct {
    uint16_t headerSize;
    uint8_t magic1, magic2;
    UDataInfo info;
    char padding[8];
    uint32_t count, reserved;
    /*
    const struct {
    const char *const name;
    const void *const data;
    } toc[1];
    */
   int   fakeNameAndData[4];       /* TODO:  Change this header type from */
                                   /*        pointerTOC to OffsetTOC.     */
} ICU_Data_Header;

extern "C" U_EXPORT const ICU_Data_Header U_ICUDATA_ENTRY_POINT;

#endif /* __STUBDATA_H__ */
