/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86DoScanPci.c,v 1.16 2004/02/13 23:58:36 dawes Exp $ */
/*
 * Copyright (c) 1999-2002 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Finish setting up the server.
 * Call the functions from the scanpci module.
 */

#include <ctype.h>
#include <stdlib.h>
#include "X.h"
#include "Xmd.h"
#include "os.h"
#ifdef XFree86LOADER
#include "loaderProcs.h"
#endif
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86Pci.h"
#include "xf86ScanPci.h"


void DoScanPci(int argc, char **argv, int i)
{
  int j,skip,globalVerbose,scanpciVerbose;
  ScanPciSetupProcPtr PciSetup;
  ScanPciDisplayCardInfoProcPtr DisplayPCICardInfo;
#ifdef XFree86LOADER
  int errmaj, errmin;
#endif

  /*
   * first we need to finish setup of the OS so that we can call other
   * functions in the server
   */
  OsInit();

  /*
   * The old verbosity processing that was here isn't useful anymore, but
   * for compatibility purposes, ignore verbosity changes after the -scanpci
   * flag.
   */
  globalVerbose = xf86Verbose;

  /*
   * next we process the arguments that are remaining on the command line,
   * so that things like the module path can be set there
   */
  for ( j = i+1; j < argc; j++ ) {
    if ((skip = ddxProcessArgument(argc, argv, j)))
	j += (skip - 1);
  } 

  /*
   * Was the verbosity level increased?  If so, set it back.
   */
  if (xf86Verbose > globalVerbose)
    xf86SetVerbosity(globalVerbose);

  /*
   * Setting scanpciVerbose to 0 will ensure that the output will go to
   * stderr for all reasonable default stderr verbosity levels.
   */
  scanpciVerbose = 0;

  /*
   * now get the loader set up and load the scanpci module
   */
#ifdef XFree86LOADER
  /* Initialise the loader */
  LoaderInit();
  /* Tell the loader the default module search path */
  LoaderSetPath(xf86ModulePath);

  if (!LoadModule("scanpci", NULL, NULL, NULL, NULL, NULL,
                  &errmaj, &errmin)) {
    LoaderErrorMsg(NULL, "scanpci", errmaj, errmin);
    exit(1);
  }
  if (LoaderCheckUnresolved(LD_RESOLV_IFDONE)) {
      /* For now, just a warning */
      xf86Msg(X_WARNING, "Some symbols could not be resolved!\n");
  }
  PciSetup = (ScanPciSetupProcPtr)LoaderSymbol("ScanPciSetupPciIds");
  DisplayPCICardInfo =
    (ScanPciDisplayCardInfoProcPtr)LoaderSymbol("ScanPciDisplayPCICardInfo");
#else
  PciSetup = ScanPciSetupPciIds;
  DisplayPCICardInfo = ScanPciDisplayPCICardInfo;
#endif

  if (!(*PciSetup)())
    FatalError("ScanPciSetupPciIds() failed\n");
  (*DisplayPCICardInfo)(scanpciVerbose);

  /*
   * That's it; we really should clean things up, but a simple
   * exit seems to be all we need.
   */
  exit(0);
}
