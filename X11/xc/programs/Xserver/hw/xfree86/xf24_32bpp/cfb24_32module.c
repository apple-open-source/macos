/* $XFree86: xc/programs/Xserver/hw/xfree86/xf24_32bpp/cfb24_32module.c,v 1.3 1999/01/26 05:54:20 dawes Exp $ */

#ifdef XFree86LOADER

#include "xf86Module.h"

static MODULESETUPPROTO(xf24_32bppSetup);

static XF86ModuleVersionInfo VersRec =
{
        "xf24_32bpp",
        MODULEVENDORSTRING,
        MODINFOSTRING1,
        MODINFOSTRING2,
        XF86_VERSION_CURRENT,
        1, 0, 0,
        ABI_CLASS_ANSIC,                /* Only need the ansic layer */
        ABI_ANSIC_VERSION,
	MOD_CLASS_NONE,
        {0,0,0,0}       /* signature, to be patched into the file by a tool */
};

XF86ModuleData xf24_32bppModuleData = { &VersRec, xf24_32bppSetup, NULL };

static pointer
xf24_32bppSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    if (!LoadSubModule(module, "cfb24", NULL, NULL, NULL, NULL,
			errmaj, errmin))
        return NULL;
    if (!LoadSubModule(module, "cfb32", NULL, NULL, NULL, NULL,
			errmaj, errmin))
        return NULL;
    return (pointer)1;  /* non-NULL required to indicate success */
}

#endif
