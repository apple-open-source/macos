/* $XFree86: xc/programs/Xserver/PEX5/pexmodule.c,v 1.6 2000/01/25 18:37:36 dawes Exp $ */

#include "xf86Module.h"
#include "PEX.h"

static MODULESETUPPROTO(pex5Setup);

extern void PexExtensionInit(INITARGS);

static ExtensionModule pex5Ext = {
    PexExtensionInit,
    PEX_NAME_STRING,
    NULL,
    NULL,
    NULL
};

static XF86ModuleVersionInfo VersRec =
{
	"pex5",
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

XF86ModuleData pex5ModuleData = { &VersRec, pex5Setup, NULL };

static pointer
pex5Setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    LoadExtension(&pex5Ext, FALSE);

    /* Need a non-NULL return value to indicate success */
    return (pointer)1;
}
