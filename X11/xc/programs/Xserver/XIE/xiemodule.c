/* $XFree86: xc/programs/Xserver/XIE/xiemodule.c,v 1.7 2000/01/25 18:37:37 dawes Exp $ */

#include "xf86Module.h"
#include "XIE.h"		

static MODULESETUPPROTO(xieSetup);

extern void XieInit(INITARGS);

ExtensionModule XieExt =
{
    XieInit,
    xieExtName,
    NULL,
    NULL,
    NULL
};

static XF86ModuleVersionInfo VersRec =
{
	"xie",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	1, 0, 0,
	ABI_CLASS_EXTENSION,
	ABI_EXTENSION_VERSION,
	MOD_CLASS_EXTENSION,
	{0,0,0,0}
};

XF86ModuleData xieModuleData = { &VersRec, xieSetup, NULL };

static pointer
xieSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    LoadExtension(&XieExt, FALSE);

    /* Need a non-NULL return value to indicate success */
    return (pointer)1;
}

