/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/vmware/guest_os.h,v 1.1 2001/04/05 19:29:44 dawes Exp $ */
/* *********************************************************
 * Copyright (C) 1999-2001 VMware, Inc.
 * All Rights Reserved
 * Id: guest_os.h,v 1.5 2001/01/26 23:32:15 yoel Exp $
 * **********************************************************/

#ifndef _GUEST_OS_H_
#define _GUEST_OS_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#define GUEST_OS_BASE  0x5000

#define GUEST_OS_DOS        (GUEST_OS_BASE+1)
#define GUEST_OS_WIN31      (GUEST_OS_BASE+2)
#define GUEST_OS_WINDOWS95  (GUEST_OS_BASE+3)
#define GUEST_OS_WINDOWS98  (GUEST_OS_BASE+4)
#define GUEST_OS_WINDOWSME  (GUEST_OS_BASE+5)
#define GUEST_OS_NT         (GUEST_OS_BASE+6)
#define GUEST_OS_WIN2000    (GUEST_OS_BASE+7)
#define GUEST_OS_LINUX      (GUEST_OS_BASE+8)
#define GUEST_OS_OS2        (GUEST_OS_BASE+9)
#define GUEST_OS_OTHER      (GUEST_OS_BASE+10)
#define GUEST_OS_FREEBSD    (GUEST_OS_BASE+11)
#define GUEST_OS_WHISTLER   (GUEST_OS_BASE+12)


#endif 
