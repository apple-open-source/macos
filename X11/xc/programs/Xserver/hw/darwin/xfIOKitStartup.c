/**************************************************************
 *
 * Startup code for the IOKit Darwin X Server
 *
 **************************************************************/
/*
 * Copyright (c) 2001-2002 Torrey T. Lyons. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/xfIOKitStartup.c,v 1.12 2003/01/15 02:34:04 torrey Exp $ */

#include "quartz/quartz.h"
#include "micmap.h"

void GlxExtensionInit(void);
void GlxWrapInitVisuals(miInitVisualsProcPtr *procPtr);


/*
 * DarwinHandleGUI
 *  This function is called first from main().
 *  It does nothing for the IOKit X server.
 */
void DarwinHandleGUI(
    int         argc,
    char        *argv[],
    char        *envp[] )
{
}


/*
 * DarwinGlxExtensionInit
 *  Initialize the GLX extension.
 *  Mesa is linked into the IOKit mode X server so we just call directly.
 */
void DarwinGlxExtensionInit(void)
{
    GlxExtensionInit();
}


/*
 * DarwinGlxWrapInitVisuals
 */
void DarwinGlxWrapInitVisuals(
    miInitVisualsProcPtr *procPtr)
{
    GlxWrapInitVisuals(procPtr);
}


// The IOKit X server does not accept any Quartz command line options.
int QuartzProcessArgument( int argc, char *argv[], int i )
{
    if (!strcmp( argv[i], "-fullscreen" ) ||
        !strcmp( argv[i], "-rootless" ) ||
        !strcmp( argv[i], "-quartz" ))
    {
        FatalError("Command line option %s is not available without Quartz "
                   "support.\nInstall the optional Xquartz.tgz tarball for "
                   "Quartz support.\n", argv[i]);
    }

    return 0;
}


// No Quartz support. All Quartz functions are no-ops.

Bool QuartzAddScreen(int index, ScreenPtr pScreen) {
    FatalError("QuartzAddScreen called without Quartz support.\n");
}

Bool QuartzSetupScreen(int index, ScreenPtr pScreen) {
    FatalError("QuartzInitCursor called without Quartz support.\n");
}

void QuartzInitOutput(int argc, char **argv) {
    FatalError("QuartzInitOutput called without Quartz support.\n");
}

void QuartzInitInput(int argc, char **argv) {
    FatalError("QuartzInitInput called without Quartz support.\n");
}

void QuartzGiveUp(void) {
    return;	// no message, we are quitting anyway
}

void QuartzProcessEvent(xEvent *xe) {
    FatalError("QuartzProcessEvent called without Quartz support.\n");
}

void QuartzBell(void) {
    FatalError("QuartzBell called without Quartz support.\n");
}
