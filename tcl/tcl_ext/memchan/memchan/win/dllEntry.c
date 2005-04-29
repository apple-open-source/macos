/* 
 * dllEntry.c --
 *
 *    This procedure provides the entry point for the dll
 *
 */

#include <windows.h>

#if defined(_MSC_VER) || defined(__GNUC__)
#define DllEntryPoint DllMain
#endif

/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This routine is called by gcc, VC++ or Borland to invoke the
 *	initialization code for Tcl.
 *
 * Results:
 *	TRUE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllEntryPoint (
    HINSTANCE hInst,		/* Library instance handle. */
    DWORD reason,		/* Reason this function is being called. */
    LPVOID reserved)		/* Not used. */
{
    return TRUE; 
}
