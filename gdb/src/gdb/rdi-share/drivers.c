/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/* -*-C-*-
 *
 * 1.1.1.1
 *     1999/04/16 01:34:27
 *
 *
 * drivers.c - declares a NULL terminated list of device driver
 *             descriptors supported by the host.
 */
#include <stdio.h>

#include "drivers.h"

extern DeviceDescr angel_SerialDevice;
extern DeviceDescr angel_SerparDevice;
extern DeviceDescr angel_EthernetDevice;

DeviceDescr *devices[] =
{
    &angel_SerialDevice,
    &angel_SerparDevice,
    &angel_EthernetDevice,
    NULL
};

/* EOF drivers.c */
