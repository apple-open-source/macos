/* 
 * win32main.c --
 *
 *

 *
 *	This file contains the DLL entry point.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 Scriptics Corporation.
 *
 * Copyright (c) 2000 Peter Farmer, Zveno Pty Ltd
 *
 * Zveno Pty Ltd makes this software and associated documentation
 * available free of charge for any purpose.  You may make copies
 * of the software but you must include all of this notice on any copy.
 *
 * Zveno Pty Ltd does not warrant that this software is error free
 * or fit for any purpose.  Zveno Pty Ltd disclaims any liability for
 * all claims, expenses, losses, damages and costs any user may incur
 * as a result of using, copying or modifying the software.
 *
 * $Id: win32main.c,v 1.1 2000/12/28 09:56:35 doss Exp $
 */

/* Exclude rarely-used stuff from Windows headers */
#define WIN32_LEAN_AND_MEAN		

#include <windows.h>

/*
 * The following declaration is for the VC++ DLL entry point.
 */

BOOL APIENTRY DllMain(
					  HANDLE module, 
					  DWORD reason, 
					  LPVOID reserved
					  );

#ifdef __WIN32__
#ifndef STATIC_BUILD


/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This wrapper function is used by Borland to invoke the
 *	initialization code for Tcl.  It simply calls the DllMain
 *	routine.
 *
 * Results:
 *	See DllMain.
 *
 * Side effects:
 *	See DllMain.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllEntryPoint(hInst, reason, reserved)
    HANDLE module,   	/* Library instance handle. */
    DWORD reason;		/* Reason this function is being called. */
    LPVOID reserved;	/* Not used. */
{
    return DllMain(hInst, reason, reserved);
}

/*
 *----------------------------------------------------------------------
 *
 * DllMain --
 *
 *	This routine is called by the VC++ C run time library init
 *	code, or the DllEntryPoint routine.  It is responsible for
 *	initializing various dynamically loaded libraries.
 *
 * Results:
 *	TRUE on sucess, FALSE on failure.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */
BOOL APIENTRY
DllMain(module, reason, reserved)
    HANDLE module;	    	/* Library instance handle. */
    DWORD reason;		    /* Reason this function is being called. */
    LPVOID reserved;		/* Not used. */
{
    switch (reason) {
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
	    break;
    }

    return TRUE; 
}

#endif /* !STATIC_BUILD */
#endif /* __WIN32__ */
