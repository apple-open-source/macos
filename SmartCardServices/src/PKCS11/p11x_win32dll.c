/******************************************************************************
** 
**  $Id: p11x_win32dll.c,v 1.2 2003/02/13 20:06:42 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Support for WIN32 DLL's
** 
******************************************************************************/
#ifdef WIN32

#include "cryptoki.h"

/******************************************************************************
** Function: DllMain
**
** Required entry point for WIN32 DLL's
**
** Parameters:
**  hModule            -
**  ul_reason_for_call -
**  lpReserved         -
**
** Returns:
**  TRUE
*******************************************************************************/
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            log_Log(LOG_LOW, "WIN32DLL: Process attach");
            break;
        case DLL_THREAD_ATTACH:
            log_Log(LOG_LOW, "WIN32DLL: Thread attach");
            break;
        case DLL_THREAD_DETACH:
            log_Log(LOG_LOW, "WIN32DLL: Thread detach");
            break;
        case DLL_PROCESS_DETACH:
            log_Log(LOG_LOW, "WIN32DLL: Process detach");
            C_Finalize(0);
            break;
    }
    return TRUE;
}

#endif /* WIN32 */
