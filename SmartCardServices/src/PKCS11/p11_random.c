/******************************************************************************
** 
**  $Id: p11_random.c,v 1.2 2003/02/13 20:06:39 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Random number generation
** 
******************************************************************************/

#include "cryptoki.h"

/* C_SeedRandom mixes additional seed material into the token's
 * random number generator. */
CK_DEFINE_FUNCTION(CK_RV, C_SeedRandom)
(
  CK_SESSION_HANDLE hSession,  /* the session's handle */
  CK_BYTE_PTR       pSeed,     /* the seed material */
  CK_ULONG          ulSeedLen  /* length of seed material */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_SeedRandom");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_SeedRandom");

    return rv;
}


/* C_GenerateRandom generates random data. */
CK_DEFINE_FUNCTION(CK_RV, C_GenerateRandom)
(
  CK_SESSION_HANDLE hSession,    /* the session's handle */
  CK_BYTE_PTR       RandomData,  /* receives the random data */
  CK_ULONG          ulRandomLen  /* # of bytes to generate */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_GenerateRandom");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GenerateRandom");

    return rv;
}

