/* 
 * dllEntryPoint.c --
 * $Id: s.dllEntryPoint.c 1.2 01/03/23 21:55:32+01:00 neumann@somewhere.wu-wien.ac.at $ 
 *
 *	This file implements the Dll entry point as needed by Windows.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#if defined(_MSC_VER)
#   define DllEntryPoint DllMain
#endif

/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This wrapper function is used by Windows to invoke the
 *	initialization code for the DLL.  If we are compiling
 *	with Visual C++, this routine will be renamed to DllMain.
 *
 * Results:
 *	Returns TRUE;
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllEntryPoint(hInst, reason, reserved)
    HINSTANCE hInst;		/* Library instance handle. */
    DWORD reason;		/* Reason this function is being called. */
    LPVOID reserved;		/* Not used. */
{
    return TRUE;
}
