/* $XConsortium: tclkbd.c /main/2 1996/10/19 19:06:13 kaleb $ */





/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tclkbd.c,v 3.5 1996/12/27 06:54:16 dawes Exp $ */
/*
 * Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Joseph Moss not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Joseph Moss makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * JOSEPH MOSS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL JOSEPH MOSS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */


/*

  This file contains routines to add commands to the Tcl interpreter
     that interface with the XKEYBOARD server extension

 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Intrinsic.h>
#include <X11/Xproto.h>
#include <X11/Xfuncs.h>
#include <X11/Xatom.h>
#include <tcl.h>
#include <tk.h>

#include <X11/XKBlib.h>
#include <X11/extensions/XKBgeom.h>
#include <X11/extensions/XKM.h>
#include <X11/extensions/XKBfile.h>
#include <X11/extensions/XKBui.h>
#include <X11/extensions/XKBrules.h>

static int	TCL_XF86GetKBD(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86GetKBDComponents(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86FreeKBD(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86ListKBDComponents(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86LoadKBD(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86ResolveKBDComponents(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86ListKBDRules(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86GetKBDProp(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86SetKBDProp(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	init_xkb(
#if NeedNestedProtoTypes
    Tcl_Interp	*interp,
    Display	*dpy
#endif
);

extern XkbDescPtr	GetXkbDescPtr(
#if NeedNestedProtoTypes
    Tcl_Interp	*interp,
    char	*handle
#endif
);

extern void	GetXkbHandle(
#if NeedNestedProtoTypes
    char	*buf
#endif
);

static Tcl_HashTable	XkbDescTable;	/* Table of ptrs to XkbDescRecs */

/*
   Adds all the new commands to the Tcl interpreter
*/

int
XF86Kbd_Init(interp)
    Tcl_Interp	*interp;
{
	Tcl_InitHashTable(&XkbDescTable, TCL_STRING_KEYS);

	Tcl_CreateCommand(interp, "xkb_read",
		TCL_XF86GetKBD, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xkb_load",
		TCL_XF86LoadKBD, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xkb_free",
		TCL_XF86FreeKBD, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xkb_components",
		TCL_XF86GetKBDComponents, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xkb_listcomponents",
		TCL_XF86ListKBDComponents, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xkb_resolvecomponents",
		TCL_XF86ResolveKBDComponents, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xkb_listrules",
		TCL_XF86ListKBDRules, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xkb_getrulesprop",
		TCL_XF86GetKBDProp, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xkb_setrulesprop",
		TCL_XF86SetKBDProp, (ClientData) NULL,
		(void (*)()) NULL);

	return TCL_OK;
}

/*
   Check that the server supports the XKB extension
*/

static int
init_xkb(interp, dpy)
    Tcl_Interp	*interp;
    Display	*dpy;
{
	static Bool	been_here = False;
	int		major, minor, op, event, error;

	if (been_here == True)
		return TCL_OK;

	major = XkbMajorVersion;
	minor = XkbMinorVersion;
	if (!XkbQueryExtension(dpy, &op, &event, &error, &major, &minor)) {
		Tcl_SetResult(interp,
			"Unable to initialize XKEYBOARD extension",
			TCL_STATIC);
		return TCL_ERROR;
	}
	return TCL_OK;
}

/*
  Read an XKB description from the X server or from a .xkm file
*/

int
TCL_XF86GetKBD(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window	topwin;
	Display		*disp;
	XkbDescPtr	xkb;
	Tcl_HashEntry	*entry;

	if (argc != 2) {
		Tcl_SetResult(interp,
			"Usage: xkb_read from_server|<filename>", TCL_STATIC);
		return TCL_ERROR;
	}

	if (strcmp(argv[1], "from_server") == 0) {
		if ((topwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
			return TCL_ERROR;
		disp = Tk_Display(topwin);
		if (init_xkb(interp, disp) != TCL_OK)
			return TCL_ERROR;
		xkb=XkbGetKeyboard(disp,
			XkbGBN_AllComponentsMask,XkbUseCoreKbd);
	} else {
#if 0
		unsigned tmp;
		FILE *fd;
		int	major, minor;
		XkbFileInfo	result;

		XkbInitAtoms(NULL);
		major = XkbMajorVersion;
		minor = XkbMinorVersion;
		XkbLibraryVersion(&major, &minor);
		bzero((char *) &result, sizeof(result));
		if ((result.xkb=xkb=XkbAllocKeyboard()) == NULL) {
			Tcl_SetResult(interp,
				"Couldn't allocate keyboard", TCL_STATIC);
			return TCL_ERROR;
		}
		fd = fopen(argv[1], "r");
		tmp = XkmReadFile(fd,XkmGeometryMask,XkmKeymapLegal,&result);
		fclose(fd);
#else
		Tcl_SetResult(interp,
			"Reading from a file is not currently supported",
			TCL_STATIC);
		return TCL_ERROR;
#endif
	}
	

	if ((xkb==NULL)||(xkb->geom==NULL)) {
		Tcl_SetResult(interp, "Couldn't get keyboard", TCL_STATIC);
		return TCL_ERROR;
	}
	if (xkb->names->geometry == 0)
		xkb->names->geometry = xkb->geom->name;
	GetXkbHandle(interp->result);
	entry = Tcl_FindHashEntry(&XkbDescTable, interp->result);
	Tcl_SetHashValue(entry, xkb);
	return TCL_OK;
}

/*
  Return a Tcl list of the names of the components which make up
  the specified keyboard description
*/

int
TCL_XF86GetKBDComponents(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window	topwin;
	Display		*disp;
	XkbDescPtr	xkb;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: xkb_components <keyboard>",
				TCL_STATIC);
		return TCL_ERROR;
	}

	if ((topwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	disp = Tk_Display(topwin);

	xkb = GetXkbDescPtr(interp, argv[1]);
	if (xkb == NULL) {
		return TCL_ERROR;
	}
	Tcl_AppendElement(interp,
		XkbAtomText(disp, xkb->names->keycodes, XkbMessage));
	Tcl_AppendElement(interp,
		XkbAtomText(disp, xkb->names->types, XkbMessage));
	Tcl_AppendElement(interp,
		XkbAtomText(disp, xkb->names->compat, XkbMessage));
	Tcl_AppendElement(interp,
		XkbAtomText(disp, xkb->names->symbols, XkbMessage));
	Tcl_AppendElement(interp,
		XkbAtomText(disp, xkb->geom->name, XkbMessage));

	return TCL_OK;
}


/*
   Return a list of the components in the server's database

   Each component is prefixed by a character:
        # - Default
        + - Partial
        * - Partial & Default
     <sp> - None of the above

*/

#define MAX_COMPONENTS		400	/* Max # components of one type */
#define MAX_TTL_COMPONENTS	1000	/* Max # components of all types */
#define flag2char(flag)		((flag & XkbLC_Default)? \
				((flag & XkbLC_Partial)? '*': '#'): \
				((flag & XkbLC_Partial)? '+': ' '))

int
TCL_XF86ListKBDComponents(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window		topwin;
	Display			*disp;
	XkbComponentNamesRec	getcomps;
	XkbComponentListPtr	comps;
	char			*av[MAX_COMPONENTS], *names;
	char			bufs[MAX_COMPONENTS][32];
	int			max, i, ncomps;

	if (argc != 7) {
		Tcl_SetResult(interp,
			"Usage: xkb_listcomponents <keymap_pat> "
			"<keycodes_pat> <compat_pat> <types_pat> "
			"<symbols_pat> <geometry_pat>", TCL_STATIC);
		return TCL_ERROR;
	}

	if ((topwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	disp = Tk_Display(topwin);
	max = MAX_TTL_COMPONENTS;
	getcomps.keymap   = argv[1];
	getcomps.keycodes = argv[2];
	getcomps.compat   = argv[3];
	getcomps.types    = argv[4];
	getcomps.symbols  = argv[5];
	getcomps.geometry = argv[6];
	comps = XkbListComponents(disp, XkbUseCoreKbd, &getcomps, &max);

	for (i = 0; i < MAX_COMPONENTS; i++)
		av[i] = bufs[i];

	for (i = 0, ncomps = 0; i < comps->num_keymaps; i++) {
	    if (ncomps == MAX_COMPONENTS)
	        break;
	    if (comps->keymaps[i].flags & XkbLC_Hidden)
	        continue;
	    bufs[ncomps][0] = flag2char(comps->keymaps[i].flags);
	    strncpy(&(bufs[ncomps][1]), comps->keymaps[i].name, 30);
	    bufs[ncomps++][31] = '\0';
	}
	names = Tcl_Merge(ncomps, av);
	Tcl_AppendElement(interp, names);
	XtFree(names);

	for (i = 0, ncomps = 0; i < comps->num_keycodes; i++) {
	    if (ncomps == MAX_COMPONENTS)
	        break;
	    if (comps->keycodes[i].flags & XkbLC_Hidden)
	        continue;
	    bufs[ncomps][0] = flag2char(comps->keycodes[i].flags);
	    strncpy(&(bufs[ncomps][1]), comps->keycodes[i].name, 30);
	    bufs[ncomps++][31] = '\0';
	}
	names = Tcl_Merge(ncomps,av);
	Tcl_AppendElement(interp, names);
	XtFree(names);

	for (i = 0, ncomps = 0; i < comps->num_compat; i++) {
	    if (ncomps == MAX_COMPONENTS)
	        break;
	    if (comps->compat[i].flags & XkbLC_Hidden)
	        continue;
	    bufs[ncomps][0] = flag2char(comps->compat[i].flags);
	    strncpy(&(bufs[ncomps][1]), comps->compat[i].name, 30);
	    bufs[ncomps++][31] = '\0';
	}
	names = Tcl_Merge(ncomps,av);
	Tcl_AppendElement(interp, names);
	XtFree(names);

	for (i = 0, ncomps = 0; i < comps->num_types; i++) {
	    if (ncomps == MAX_COMPONENTS)
	        break;
	    if (comps->types[i].flags & XkbLC_Hidden)
	        continue;
	    bufs[ncomps][0] = flag2char(comps->types[i].flags);
	    strncpy(&(bufs[ncomps][1]), comps->types[i].name, 30);
	    bufs[ncomps++][31] = '\0';
	}
	names = Tcl_Merge(ncomps,av);
	Tcl_AppendElement(interp, names);
	XtFree(names);

	for (i = 0, ncomps = 0; i < comps->num_symbols; i++) {
	    if (ncomps == MAX_COMPONENTS)
	        break;
	    if (comps->symbols[i].flags & XkbLC_Hidden)
	        continue;
	    bufs[ncomps][0] = flag2char(comps->symbols[i].flags);
	    strncpy(&(bufs[ncomps][1]), comps->symbols[i].name, 30);
	    bufs[ncomps][31] = '\0';
	    ncomps++;
	}
	names = Tcl_Merge(ncomps,av);
	Tcl_AppendElement(interp, names);
	XtFree(names);

	for (i = 0, ncomps = 0; i < comps->num_geometry; i++) {
	    if (ncomps == MAX_COMPONENTS)
	        break;
	    if (comps->geometry[i].flags & XkbLC_Hidden)
	        continue;
	    bufs[ncomps][0] = flag2char(comps->geometry[i].flags);
	    strncpy(&(bufs[ncomps][1]), comps->geometry[i].name, 30);
	    bufs[ncomps++][31] = '\0';
	}
	names = Tcl_Merge(ncomps,av);
	Tcl_AppendElement(interp, names);
	XtFree(names);

	XkbFreeComponentList(comps);
	return TCL_OK;
}

/*
  Return a keyboard description, given the component names
  optionally load it into the server as the new keyboard
*/

static char *usage_LoadKBD = "Usage: xkb_load <keymap_pat> "
			"<keycodes_pat> <compat_pat> <types_pat> "
			"<symbols_pat> <geometry_pat> [load|noload]";

int
TCL_XF86LoadKBD(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window		topwin;
	Display			*disp;
	XkbComponentNamesRec	getcomps;
	XkbDescPtr		xkb;
	Bool			loadit = True;
	Tcl_HashEntry		*entry;

	if (argc < 7 || argc > 8) {
		Tcl_SetResult(interp, usage_LoadKBD, TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 8) {
		if (!strcmp(argv[7], "load"))
			loadit = True;
		else if (!strcmp(argv[7], "noload"))
			loadit = False;
		else {
			Tcl_SetResult(interp, usage_LoadKBD, TCL_STATIC);
			return TCL_ERROR;
		}
	}

	if ((topwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	disp = Tk_Display(topwin);

	getcomps.keymap   = argv[1];
	getcomps.keycodes = argv[2];
	getcomps.compat   = argv[3];
	getcomps.types    = argv[4];
	getcomps.symbols  = argv[5];
	getcomps.geometry = argv[6];
	xkb = XkbGetKeyboardByName(disp, XkbUseCoreKbd, &getcomps,
			XkbGBN_AllComponentsMask, 0, loadit);
	if (!xkb) {
		Tcl_SetResult(interp, "Load failed", TCL_STATIC);
		return TCL_ERROR;
	}
	if (xkb->names->geometry == 0)
		xkb->names->geometry = xkb->geom->name;

	GetXkbHandle(interp->result);
	entry = Tcl_FindHashEntry(&XkbDescTable, interp->result);
	Tcl_SetHashValue(entry, xkb);

	return TCL_OK;
}

/*
  Free the memory occupied by a keyboard description
*/

int
TCL_XF86FreeKBD(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	XkbDescPtr		xkb;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: xkb_free <keyboard>",
			TCL_STATIC);
		return TCL_ERROR;
	}

	xkb = GetXkbDescPtr(interp, argv[1]);
	XkbFreeKeyboard(xkb,0,True);
	return TCL_OK;
}

/*
  Use rules to determine the appropriate components for the given defs
*/

int
TCL_XF86ResolveKBDComponents(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	XkbRF_RulesPtr		rules;
	XkbRF_VarDefsRec	defs;
	XkbComponentNamesRec	comps;
	FILE			*fp;
	Bool			complete;

	if (argc != 6) {
		Tcl_SetResult(interp,
			"Usage: xkb_resolvecomponents <rulesfile>"
			    " <model> <layout> <variant> <options>",
			TCL_STATIC);
		return TCL_ERROR;
	}

	if ((fp = fopen(argv[1], "r")) == NULL) {
		Tcl_SetResult(interp, "Can't open rules file" , TCL_STATIC);
		return TCL_ERROR;
	}
	
	if ((rules= XkbRF_Create(0,0))==NULL) {
		fclose(fp);
		Tcl_SetResult(interp, "Can't create rules structure" , TCL_STATIC);
		return TCL_ERROR;
	}
	if (!XkbRF_LoadRules(fp,rules)) {
		fclose(fp);
		XkbRF_Free(rules,True);
		Tcl_SetResult(interp, "Can't load rules" , TCL_STATIC);
		return TCL_ERROR;
	}
	defs.model   = strlen(argv[2])? argv[2]: NULL;
	defs.layout  = strlen(argv[3])? argv[3]: NULL;
	defs.variant = strlen(argv[4])? argv[4]: NULL;
	defs.options = strlen(argv[5])? argv[5]: NULL;
	bzero((char *)&comps, sizeof(XkbComponentNamesRec));
	complete= XkbRF_GetComponents(rules, &defs, &comps);

	Tcl_AppendElement(interp,(comps.keymap?  comps.keymap:  ""));
	Tcl_AppendElement(interp,(comps.keycodes?comps.keycodes:""));
	Tcl_AppendElement(interp,(comps.compat?  comps.compat:  ""));
	Tcl_AppendElement(interp,(comps.types?   comps.types:   ""));
	Tcl_AppendElement(interp,(comps.symbols? comps.symbols: ""));
	Tcl_AppendElement(interp,(comps.geometry?comps.geometry:""));

	XtFree(comps.keymap);
	XtFree(comps.keycodes);
	XtFree(comps.compat);
	XtFree(comps.types);
	XtFree(comps.symbols);
	XtFree(comps.geometry);
	XkbRF_Free(rules,True);
	fclose(fp);

	return TCL_OK;
}

/*
  Return a list of rules defs and their descriptions
*/

#ifdef min
#undef min
#endif
#define min(a,b)	((a<b)?(a):(b))

int
TCL_XF86ListKBDRules(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	XkbRF_RulesPtr	list;
	Bool		result;
	int		i, maxcnt;
	char		*tmp;
	char		*av_names[MAX_COMPONENTS];
	char		*av_descs[MAX_COMPONENTS];

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: xkb_listrules <rulesfilename>",
			TCL_STATIC);
		return TCL_ERROR;
	}

	if ((list= XkbRF_Create(0,0))==NULL) {
		Tcl_SetResult(interp, "Can't create rules structure" , TCL_STATIC);
		return TCL_ERROR;
	}

	result = XkbRF_LoadDescriptionsByName(argv[1],NULL,list);

	if (result == False) {
		Tcl_SetResult(interp, "", TCL_STATIC);
		return TCL_OK;
	}

	maxcnt = min(list->models.num_desc,MAX_COMPONENTS);
	for (i=0; i<maxcnt; i++) {
		av_names[i] = list->models.desc[i].name;
		av_descs[i] = list->models.desc[i].desc;
	}

	if ((tmp = Tcl_Merge(maxcnt, av_names)) == NULL)
		return TCL_ERROR;
	Tcl_AppendElement(interp, tmp);
	XtFree(tmp);

	if ((tmp = Tcl_Merge(maxcnt, av_descs)) == NULL)
		return TCL_ERROR;
	Tcl_AppendElement(interp, tmp);
	XtFree(tmp);

	maxcnt = min(list->layouts.num_desc,MAX_COMPONENTS);
	for (i=0; i<maxcnt; i++) {
		av_names[i] = list->layouts.desc[i].name;
		av_descs[i] = list->layouts.desc[i].desc;
	}

	if ((tmp = Tcl_Merge(maxcnt, av_names)) == NULL)
		return TCL_ERROR;
	Tcl_AppendElement(interp, tmp);
	XtFree(tmp);

	if ((tmp = Tcl_Merge(maxcnt, av_descs)) == NULL)
		return TCL_ERROR;
	Tcl_AppendElement(interp, tmp);
	XtFree(tmp);

	maxcnt = min(list->variants.num_desc,MAX_COMPONENTS);
	for (i=0; i<maxcnt; i++) {
		av_names[i] = list->variants.desc[i].name;
		av_descs[i] = list->variants.desc[i].desc;
	}

	if ((tmp = Tcl_Merge(maxcnt, av_names)) == NULL)
		return TCL_ERROR;
	Tcl_AppendElement(interp, tmp);
	XtFree(tmp);

	if ((tmp = Tcl_Merge(maxcnt, av_descs)) == NULL)
		return TCL_ERROR;
	Tcl_AppendElement(interp, tmp);
	XtFree(tmp);

	maxcnt = min(list->options.num_desc,MAX_COMPONENTS);
	for (i=0; i<maxcnt; i++) {
		av_names[i] = list->options.desc[i].name;
		av_descs[i] = list->options.desc[i].desc;
	}

	if ((tmp = Tcl_Merge(maxcnt, av_names)) == NULL)
		return TCL_ERROR;
	Tcl_AppendElement(interp, tmp);
	XtFree(tmp);

	if ((tmp = Tcl_Merge(maxcnt, av_descs)) == NULL)
		return TCL_ERROR;
	Tcl_AppendElement(interp, tmp);
	XtFree(tmp);

	XkbRF_Free(list,True);

	return TCL_OK;
}

/*
  Find out what rules defs were used to generate the keyboard currently
  used by the server
*/

int
TCL_XF86GetKBDProp(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window		topwin;
	Display			*disp;
	char			*rulesfile;
	XkbRF_VarDefsRec	defs;

	if (argc != 1) {
		Tcl_SetResult(interp, "Usage: xkb_getrulesprop", TCL_STATIC);
		return TCL_ERROR;
	}

	if ((topwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	disp = Tk_Display(topwin);
	if (XkbRF_GetNamesProp(disp, &rulesfile, &defs) == False)
		return TCL_OK;
	Tcl_AppendElement(interp, rulesfile?    rulesfile:"");
	Tcl_AppendElement(interp, defs.model?   defs.model:"");
	Tcl_AppendElement(interp, defs.layout?  defs.layout:"");
	Tcl_AppendElement(interp, defs.variant? defs.variant:"");
	Tcl_AppendElement(interp, defs.options? defs.options:"");
	XtFree(rulesfile);
	XtFree(defs.model);
	XtFree(defs.layout);
	XtFree(defs.variant);
	XtFree(defs.options);

	return TCL_OK;
}

/*
  Set the _XKB_RULES_NAMES property to indicate what defs are
  being used for the keyboard
*/

int
TCL_XF86SetKBDProp(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window		topwin;
	Display			*disp;
	XkbRF_VarDefsRec	defs;
	char 			*rulesfile;

	if (argc != 6) {
		Tcl_SetResult(interp,
			"Usage: xkb_setrulesprop <rulesfile>"
			    " <model> <layout> <variant> <options>",
			TCL_STATIC);
		return TCL_ERROR;
	}

	if ((topwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	disp = Tk_Display(topwin);

	defs.model   = strlen(argv[2])? argv[2]: NULL;
	defs.layout  = strlen(argv[3])? argv[3]: NULL;
	defs.variant = strlen(argv[4])? argv[4]: NULL;
	defs.options = strlen(argv[5])? argv[5]: NULL;
	rulesfile = strrchr(argv[1], '/');
	if (rulesfile == NULL)
		rulesfile = argv[1];
	else
		rulesfile++;

	if (!XkbRF_SetNamesProp(disp, rulesfile, &defs)) {
		Tcl_SetResult(interp, "Unable to set rules property",
			TCL_STATIC);
		return TCL_ERROR;
	}
	return TCL_OK;
}

/*
  Given a string handle, return the corresponding pointer to a
  keyboard description
*/

XkbDescPtr
GetXkbDescPtr(interp, handle)
    Tcl_Interp	*interp;
    char	*handle;
{
	Tcl_HashEntry *entry;
	entry = Tcl_FindHashEntry(&XkbDescTable, handle);
	if (entry == NULL) {
		Tcl_AppendResult(interp, "No keyboard named \"",
			handle, "\"", (char *) NULL);
		return NULL;
	}
	return (XkbDescPtr) Tcl_GetHashValue(entry);
}

/*
  Get the next available handle
*/

void
GetXkbHandle(buf)
    char	*buf;
{
	static unsigned int id = 1;
	int new;

	do {
		sprintf(buf, "xkb%d", id++);
		Tcl_CreateHashEntry(&XkbDescTable, buf, &new);
	} while (!new);
}

