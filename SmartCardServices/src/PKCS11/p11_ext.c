/******************************************************************************
** 
**  $Id: p11_ext.c,v 1.2 2003/02/13 20:06:38 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Functions added in for Cryptoki Version 2.01 or later
** 
******************************************************************************/

#include "cryptoki.h"

/* C_WaitForSlotEvent waits for a slot event (token insertion,
 * removal, etc.) to occur. */
CK_DEFINE_FUNCTION(CK_RV, C_WaitForSlotEvent)
(
  CK_FLAGS flags,        /* blocking/nonblocking flag */
  CK_SLOT_ID_PTR pSlot,  /* location that receives the slot ID */
  CK_VOID_PTR pRserved   /* reserved.  Should be NULL_PTR */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_WaitForSlotEvent");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_WaitForSlotEvent");

    return rv;
}
