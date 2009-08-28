/* 
 * dllEntryPoint.c --
 * $Id: dllEntryPoint.c,v 1.1 2004/05/23 22:50:39 neumann Exp $ 
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
