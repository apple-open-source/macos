/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/confwrite.c,v 1.10 2001/07/25 15:05:05 dawes Exp $ */
/*
 * Copyright 1999 by Joseph V. Moss <joe@XFree86.Org>
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

  This file contains Tcl bindings to the XF86Config file read/write routines

 */

#include "X.h"
#include "os.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86Parser.h"
#include "xf86tokens.h"

#include "tcl.h"
#include "xfsconf.h"

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef NEED_SNPRINTF
#include "snprintf.h"
#endif

static int putsection_files   (Tcl_Interp *interp, char *varpfx);
static int putsection_module  (Tcl_Interp *interp, char *varpfx);
static int putsection_flags   (Tcl_Interp *interp, char *varpfx);
#if 0
static int putsection_input   (Tcl_Interp *interp, char *varpfx);
static int putsection_dri     (Tcl_Interp *interp, char *varpfx);
#endif
static int putsection_vidadptr(Tcl_Interp *interp, char *varpfx);
static int putsection_modes   (Tcl_Interp *interp, char *varpfx);
static int putsection_monitor (Tcl_Interp *interp, char *varpfx);
static int putsection_device  (Tcl_Interp *interp, char *varpfx);
static int putsection_screen  (Tcl_Interp *interp, char *varpfx);
static int putsection_layout  (Tcl_Interp *interp, char *varpfx);
static int putsection_vendor  (Tcl_Interp *interp, char *varpfx);

static int write_options(
  Tcl_Interp *interp,
  char *section,
  char *idxname,
  XF86OptionPtr *ptr2optptr
);

static int write_modes(
  Tcl_Interp *interp,
  char *section,
  char *idxname,
  XF86ConfModeLinePtr *ptr2modeptr
);


/*
   Implements the xf86config_writefile command which writes
   values to the XF86Config file
*/

int
TCL_XF86WriteXF86Config(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	if (argc != 3) {
	    Tcl_SetResult(interp,
		"Usage: xf86config_writefile filename varprefix" , TCL_STATIC);
	    return TCL_ERROR;
	}

	config_list = (XF86ConfigPtr) XtMalloc(sizeof(XF86ConfigRec));
	memset(config_list, 0, sizeof(XF86ConfigRec));

	putsection_files   (interp, argv[2]);
	putsection_module  (interp, argv[2]);
	putsection_flags   (interp, argv[2]);
#if 0
	putsection_input   (interp, argv[2]);
	putsection_dri     (interp, argv[2]);
#endif
	putsection_vidadptr(interp, argv[2]);
	putsection_modes   (interp, argv[2]);
	putsection_monitor (interp, argv[2]);
	putsection_device  (interp, argv[2]);
	putsection_screen  (interp, argv[2]);
	putsection_layout  (interp, argv[2]);
	putsection_vendor  (interp, argv[2]);

	if (xf86WriteConfigFile(argv[1], config_list) == 0) {
		Tcl_SetResult(interp,
			"Unable to write file" , TCL_STATIC);
		return TCL_ERROR;
	}
	return TCL_OK;
}

#define SETSTR(ptr, idx) \
		{ char *p=Tcl_GetVar2(interp,section,idx,0); \
		  if (p && strlen(p)) { \
		    ptr=XtRealloc(ptr,strlen(p)+1); strcpy(ptr, p); \
		  } else ptr=NULL;   }
#define SETINT(var, idx) \
		{ char *p=Tcl_GetVar2(interp,section,idx,0); \
		  if (p && strlen(p)) Tcl_GetInt(interp, p, &(var));  }
#define SETBOOL(var, idx) \
		{ char *p=Tcl_GetVar2(interp,section,idx,0); \
		  var = 0; if (p && strlen(p)) var = 1;  }

#define SETnSTR(ptr, idx)	\
		{ char *p=Tcl_GetVar2(interp,namebuf,idx,0); \
		  if (p && strlen(p)) { \
		    ptr=XtRealloc(ptr,strlen(p)+1); strcpy(ptr, p); \
		  } else ptr=NULL;   }
#define SETnINT(var, idx) \
		{ char *p=Tcl_GetVar2(interp,namebuf,idx,0); \
		  if (p && strlen(p)) Tcl_GetInt(interp, p, &(var));  }
#define SETnBOOL(var, idx) \
		{ char *p=Tcl_GetVar2(interp,namebuf,idx,0); \
		  var = 0; if (p && strlen(p)) var = 1;  }

static int
putsection_files(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char section[128];
	XF86ConfFilesPtr fptr;

	fprintf(stderr, "putsection_files(%p, %s)\n", interp, varpfx);
	SECTION_NAME("Files");

	fptr = (XF86ConfFilesPtr) XtMalloc(sizeof(XF86ConfFilesRec));
	config_list->conf_files = fptr;
	memset(fptr, 0, sizeof(XF86ConfFilesRec));
	SETSTR(fptr->file_logfile,	"LogFile");
	SETSTR(fptr->file_fontpath,	"FontPath");
	SETSTR(fptr->file_rgbpath,	"RGBPath");
	SETSTR(fptr->file_modulepath,	"ModulePath");
	return TCL_OK;
}

static int
putsection_module(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char section[128], buf[160], *tmpbuf, *tmpptr, **idxv;
	int i, idxc;
	XF86LoadPtr lptr, prev;

	fprintf(stderr, "putsection_module(%p, %s)\n", interp, varpfx);
	SECTION_NAME("Module");

	config_list->conf_modules =
		(XF86ConfModulePtr) XtMalloc(sizeof(XF86ConfModuleRec));
	memset(config_list->conf_modules, 0, sizeof(XF86ConfModuleRec));
	lptr = config_list->conf_modules->mod_load_lst = NULL;

	strcpy(buf, "array names "); strcat(buf, section);
	if (Tcl_Eval(interp, buf) != TCL_OK)
		return TCL_ERROR;

	tmpbuf = XtMalloc(strlen(interp->result)+1);
	strcpy(tmpbuf, interp->result);
	if (Tcl_SplitList(interp, tmpbuf, &idxc, &idxv) != TCL_OK)
		return TCL_ERROR;
	
	prev = (XF86LoadPtr) 0;
	for (i = 0; i < idxc; i++) {
		if (strstr(idxv[i], "_Opt:") || strstr(idxv[i], ":Options"))
			continue;
		lptr = (XF86LoadPtr) XtMalloc(sizeof(XF86LoadRec));
		memset(lptr, 0, sizeof(XF86LoadRec));
		if (prev == (XF86LoadPtr) 0) {
			config_list->conf_modules->mod_load_lst = lptr;
		} else {
			prev->list.next = lptr;
		}
		lptr->load_name = idxv[i];
		tmpptr = Tcl_GetVar2(interp, section, idxv[i], 0);
		if (!NameCompare(tmpptr, "driver")) {
			lptr->load_type = 1;
		} else {
			lptr->load_type = 0;
		}
		write_options(interp, section, idxv[i], &(lptr->load_opt));
		prev = lptr;
	}
	XtFree(tmpbuf);
	return TCL_OK;
}

static int
putsection_flags(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char section[128];

	fprintf(stderr, "putsection_flags(%p, %s)\n", interp, varpfx);
	SECTION_NAME("Server");

	if (Tcl_GetVar2(interp, section, "Options", 0) != NULL) {
	    config_list->conf_flags =
		(XF86ConfFlagsPtr) XtMalloc(sizeof(XF86ConfFlagsRec));
	    memset(config_list->conf_flags, 0, sizeof(XF86ConfFlagsRec));
	    write_options(interp, section, NULL,
		&(config_list->conf_flags->flg_option_lst));
	}
	return TCL_OK;
}

static int
putsection_monitor(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char section[128], namebuf[128], *ptr, **idxv, **tmpv;
	int i, j, idxc, tmpc;
	XF86ConfMonitorPtr	mptr, prev;
	XF86ConfModesLinkPtr	lptr, lprev;

	fprintf(stderr, "putsection_monitor(%p, %s)\n", interp, varpfx);
	SECTION_NAME("Monitor");
	ptr = Tcl_GetVar2(interp, section, "Identifiers", 0);
	if (Tcl_SplitList(interp, ptr, &idxc, &idxv) != TCL_OK)
		return TCL_ERROR;
	
	prev = (XF86ConfMonitorPtr) 0;
	for (i = 0; i < idxc; i++) {
		mptr = (XF86ConfMonitorPtr) XtMalloc(sizeof(XF86ConfMonitorRec));
		memset(mptr, 0, sizeof(XF86ConfMonitorRec));
		snprintf(namebuf, 128, "%s_%s", section, idxv[i]);
		if (prev == (XF86ConfMonitorPtr) 0) {
			config_list->conf_monitor_lst = mptr;
		} else {
			prev->list.next = mptr;
		}
		mptr->mon_identifier = idxv[i];
		SETnSTR(mptr->mon_vendor,	"VendorName");
		SETnSTR(mptr->mon_modelname,	"ModelName");
		SETnINT(mptr->mon_width,	"Width");
		SETnINT(mptr->mon_height,	"Height");
		ptr = Tcl_GetVar2(interp, namebuf, "HorizSync", 0);
		tmpc=0;
		if (Tcl_SplitList(interp, ptr, &tmpc, &tmpv) != TCL_OK)
			return TCL_ERROR;
		fprintf(stderr, "HorizSync = '%s' tmpc=%d\n", ptr, tmpc);
		mptr->mon_n_hsync = tmpc;
		for (j = 0; j < tmpc; j++) {
			fprintf(stderr, "Scanning '%s'\n", tmpv[j]);
			sscanf(tmpv[j], "%g-%g",
				&(mptr->mon_hsync[j].lo),
				&(mptr->mon_hsync[j].hi));
		}
		ptr = Tcl_GetVar2(interp, namebuf, "VertRefresh", 0);
		if (Tcl_SplitList(interp, ptr, &tmpc, &tmpv) != TCL_OK)
			return TCL_ERROR;
		mptr->mon_n_vrefresh = tmpc;
		for (j = 0; j < tmpc; j++) {
			sscanf(tmpv[j], "%g-%g",
				&(mptr->mon_vrefresh[j].lo),
				&(mptr->mon_vrefresh[j].hi));
		}
		ptr = Tcl_GetVar2(interp, namebuf, "Gamma", 0);
		sscanf(ptr, "%g %g %g", &(mptr->mon_gamma_red),
			&(mptr->mon_gamma_green), &(mptr->mon_gamma_blue));
		write_modes  (interp, namebuf, NULL, &(mptr->mon_modeline_lst));
		write_options(interp, namebuf, NULL, &(mptr->mon_option_lst));
		ptr = Tcl_GetVar2(interp, namebuf, "ModeLinks", 0);
		if (Tcl_SplitList(interp, ptr, &tmpc, &tmpv) != TCL_OK)
			return TCL_ERROR;
		lprev = (XF86ConfModesLinkPtr) 0;
		for (j = 0; j < tmpc; j++) {
			lptr = (XF86ConfModesLinkPtr) XtMalloc(sizeof(XF86ConfModesLinkRec));
			memset(mptr, 0, sizeof(XF86ConfModesLinkRec));
			if (lprev == (XF86ConfModesLinkPtr) 0) {
				mptr->mon_modes_sect_lst = lptr;
			} else {
				lprev->list.next = lptr;
			}
			lptr->ml_modes_str = tmpv[j];
			lprev = lptr;
		}
		prev = mptr;
	}
	return TCL_OK;
}

static int
putsection_device(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char section[128], namebuf[128], *ptr, **idxv, **av;
	int i, j, idxc, ac, n;
	XF86ConfDevicePtr dptr, prev;
	double tmpdbl;

	fprintf(stderr, "putsection_device(%p, %s)\n", interp, varpfx);
	SECTION_NAME("Device");
	ptr = Tcl_GetVar2(interp, section, "Identifiers", 0);
	if (Tcl_SplitList(interp, ptr, &idxc, &idxv) != TCL_OK)
		return TCL_ERROR;
	
	prev = (XF86ConfDevicePtr) 0;
	for (i = 0; i < idxc; i++) {
		dptr = (XF86ConfDevicePtr) XtMalloc(sizeof(XF86ConfDeviceRec));
		memset(dptr, 0, sizeof(XF86ConfDeviceRec));
		snprintf(namebuf, 128, "%s_%s", section, idxv[i]);
		if (prev == (XF86ConfDevicePtr) 0) {
			config_list->conf_device_lst = dptr;
		} else {
			prev->list.next = dptr;
		}
		dptr->dev_identifier = idxv[i];
		fprintf(stderr, "identifier = %s\n", idxv[i]);
		SETnSTR(dptr->dev_vendor,	"VendorName");
		SETnSTR(dptr->dev_board,	"BoardName");
		SETnSTR(dptr->dev_chipset,	"Chipset");
		SETnSTR(dptr->dev_busid,	"BusID");
		SETnSTR(dptr->dev_card,		"Card");
		SETnSTR(dptr->dev_driver,	"Driver");
		SETnSTR(dptr->dev_ramdac,	"Ramdac");
		ptr = Tcl_GetVar2(interp, namebuf, "DacSpeed", 0);
		if (strlen(ptr)) {
		    if (Tcl_SplitList(interp, ptr, &ac, &av) != TCL_OK)
			return TCL_ERROR;
		    for (j = 0; j < ac; j++) {
			Tcl_GetInt(interp, av[j], &n);
			dptr->dev_dacSpeeds[j] = n * 1000;
		    }
		}
		SETnINT(dptr->dev_videoram,	"VideoRam");
		ptr = Tcl_GetVar2(interp, namebuf, "TextClockFreq", 0);
		if (ptr && strlen(ptr)) {
			Tcl_GetDouble(interp, ptr, &tmpdbl);
			dptr->dev_textclockfreq = tmpdbl*1000.0 + 0.5;
		}
		ptr = Tcl_GetVar2(interp, namebuf, "BIOSBase", 0);
		dptr->dev_bios_base = strtoul(ptr, NULL, 16);
		fprintf(stderr, "BIOSBase = %s\n", ptr);
		ptr = Tcl_GetVar2(interp, namebuf, "MemBase", 0);
		dptr->dev_mem_base = strtoul(ptr, NULL, 16);
		ptr = Tcl_GetVar2(interp, namebuf, "IOBase", 0);
		dptr->dev_io_base = strtoul(ptr, NULL, 16);
		SETnSTR(dptr->dev_clockchip,	"ClockChip");
		ptr = Tcl_GetVar2(interp, namebuf, "Clocks", 0);
		if (strlen(ptr)) {
		    if (Tcl_SplitList(interp, ptr, &ac, &av) != TCL_OK)
			return TCL_ERROR;
		    dptr->dev_clocks = ac;
		    for (j = 0; j < ac; j++) {
			Tcl_GetInt(interp, av[j], &n);
			dptr->dev_clock[j] = n * 1000;
		    }
		}
		ptr = Tcl_GetVar2(interp, namebuf, "ChipID", 0);
		dptr->dev_chipid = strtoul(ptr, NULL, 16);
		ptr = Tcl_GetVar2(interp, namebuf, "ChipRev", 0);
		dptr->dev_chiprev = strtoul(ptr, NULL, 16);
		write_options(interp, namebuf, NULL, &(dptr->dev_option_lst));
		prev = dptr;
	}
	return TCL_OK;
}

static int
putsection_screen(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char section[128], namebuf[128], tmpbuf2[128], *ptr, **idxv, **dv, **mv;
	int i, j, k, idxc, dc, depth, mc;
	XF86ConfScreenPtr	sptr, sprev;
	XF86ConfDisplayPtr	dptr, dprev;
	XF86ModePtr		mptr, mprev;

	fprintf(stderr, "putsection_screen(%p, %s)\n", interp, varpfx);
	SECTION_NAME("Screen");
	ptr = Tcl_GetVar2(interp, section, "Identifiers", 0);
	if (Tcl_SplitList(interp, ptr, &idxc, &idxv) != TCL_OK)
		return TCL_ERROR;
	
	sprev = (XF86ConfScreenPtr) 0;
	for (i = 0; i < idxc; i++) {
	    sptr = (XF86ConfScreenPtr) XtMalloc(sizeof(XF86ConfScreenRec));
	    memset(sptr, 0, sizeof(XF86ConfScreenRec));
	    snprintf(namebuf, 128, "%s_%s", section, idxv[i]);
	    fprintf(stderr, "namebuf = %s\n", namebuf);
	    if (sprev == (XF86ConfScreenPtr) 0) {
		    config_list->conf_screen_lst = sptr;
	    } else {
		    sprev->list.next = sptr;
	    }
	    sptr->scrn_identifier = idxv[i];
	    SETnSTR(sptr->scrn_obso_driver,	"Driver");
	    SETnINT(sptr->scrn_defaultdepth,	"DefaultDepth");
	    SETnINT(sptr->scrn_defaultbpp,	"DefaultBPP");
	    SETnINT(sptr->scrn_defaultfbbpp,	"DefaultFbBPP");
	    SETnSTR(sptr->scrn_monitor_str,	"Monitor");
	    fprintf(stderr, "dev = %p (%s)\n", sptr->scrn_device_str, sptr->scrn_device_str);
	    SETnSTR(sptr->scrn_device_str,	"Device");
	    fprintf(stderr, "dev' = %p (%s)\n", sptr->scrn_device_str, sptr->scrn_device_str);

	    dprev = (XF86ConfDisplayPtr) 0;
	    ptr = Tcl_GetVar2(interp, namebuf, "Depths", 0);
	    if (Tcl_SplitList(interp, ptr, &dc, &dv) != TCL_OK)
		    return TCL_ERROR;
	    fprintf(stderr, "Depths: %s\n", ptr);
	    for (j = 0; j < dc; j++) {
		dptr = (XF86ConfDisplayPtr) XtMalloc(sizeof(XF86ConfDisplayRec));
		memset(dptr, 0, sizeof(XF86ConfDisplayRec));
		Tcl_GetInt(interp, dv[j], &depth);
	        fprintf(stderr, "depth = %d\n", depth);
		if (dprev == (XF86ConfDisplayPtr) 0) {
			sptr->scrn_display_lst = dptr;
		} else {
			dprev->list.next = dptr;
		}
		dptr->disp_depth = depth;
	        fprintf(stderr, ">>> BPP\n");
		sprintf(tmpbuf2, "BPP,%d", depth);
		ptr = Tcl_GetVar2(interp, namebuf, tmpbuf2, 0);
		if (ptr)
		    Tcl_GetInt(interp, ptr, &(dptr->disp_bpp));
	        fprintf(stderr, ">>> ViewPort\n");
		sprintf(tmpbuf2, "ViewPort,%d", depth);
		ptr = Tcl_GetVar2(interp, namebuf, tmpbuf2, 0);
		if (ptr)
		    sscanf(ptr, "%d %d",
			&(dptr->disp_frameX0), &(dptr->disp_frameY0));
	        fprintf(stderr, ">>> Virtual\n");
		sprintf(tmpbuf2, "Virtual,%d", depth);
		ptr = Tcl_GetVar2(interp, namebuf, tmpbuf2, 0);
		if (ptr)
		    sscanf(ptr, "%d %d",
			&(dptr->disp_virtualX), &(dptr->disp_virtualY));
	        fprintf(stderr, ">>> Weight\n");
		sprintf(tmpbuf2, "Weight,%d", depth);
		ptr = Tcl_GetVar2(interp, namebuf, tmpbuf2, 0);
		if (ptr)
		    sscanf(ptr, "%1d%1d%1d", &(dptr->disp_weight.red),
			&(dptr->disp_weight.green), &(dptr->disp_weight.blue));
	        fprintf(stderr, ">>> Visual\n");
		sprintf(tmpbuf2, "Visual,%d", depth);
		dptr->disp_visual = Tcl_GetVar2(interp, namebuf, tmpbuf2, 0);
		sprintf(tmpbuf2, "Modes,%d", depth);
		ptr = Tcl_GetVar2(interp, namebuf, tmpbuf2, 0);
		if (Tcl_SplitList(interp, ptr, &mc, &mv) != TCL_OK)
		    return TCL_ERROR;
		fprintf(stderr, "%s '%s'\n", tmpbuf2, ptr);
		mprev = (XF86ModePtr) 0;
		for (k = 0; k < mc; k++) {
		    mptr = (XF86ModePtr) XtMalloc(sizeof(XF86ModeRec));
		    memset(mptr, 0, sizeof(XF86ModeRec));
		    if (mprev == (XF86ModePtr) 0) {
			    dptr->disp_mode_lst = mptr;
		    } else {
			    mprev->list.next = mptr;
		    }
		    fprintf(stderr, "Mode ID = '%s'  mptr=%p  dptr=%p\n", mv[k], mptr, dptr);
		    mptr->mode_name = ConfigStrdup(mv[k]);
		    mprev = mptr;
		}
	        fprintf(stderr, ">>> Options\n");
		write_options(interp, namebuf, dv[j], &(dptr->disp_option_lst));
		dprev = dptr;
	    }
	    write_options(interp, namebuf, NULL, &(sptr->scrn_option_lst));
	    sprev = sptr;
	}
	return TCL_OK;
}

static int
putsection_layout(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char section[128], namebuf[128], *ptr, **idxv, **av, **sv;
	int i, j, idxc, ac, sc;
	XF86ConfLayoutPtr	lptr, prev;
	XF86ConfAdjacencyPtr	aptr, aprev;

	fprintf(stderr, "putsection_layout(%p, %s)\n", interp, varpfx);
	SECTION_NAME("Layout");
	ptr = Tcl_GetVar2(interp, section, "Identifiers", 0);
	if (Tcl_SplitList(interp, ptr, &idxc, &idxv) != TCL_OK)
		return TCL_ERROR;
	
	fprintf(stderr, "Identifiers: %s\n", ptr);
	prev = (XF86ConfLayoutPtr) 0;
	for (i = 0; i < idxc; i++) {
	    lptr = (XF86ConfLayoutPtr) XtMalloc(sizeof(XF86ConfLayoutRec));
	    memset(lptr, 0, sizeof(XF86ConfLayoutRec));
	    snprintf(namebuf, 128, "%s_%s", section, idxv[i]);
	    if (prev == (XF86ConfLayoutPtr) 0) {
		    config_list->conf_layout_lst = lptr;
	    } else {
		    prev->list.next = lptr;
	    }
	    lptr->lay_identifier = ConfigStrdup(idxv[i]);
	    aprev = (XF86ConfAdjacencyPtr) 0;
	    ptr = Tcl_GetVar2(interp, namebuf, "Screens", 0);
	    if (Tcl_SplitList(interp, ptr, &sc, &sv) != TCL_OK)
		    return TCL_ERROR;
	    fprintf(stderr, "Screens: %s\n", ptr);
	    for (j = 0; j < sc; j++) {
		aptr = (XF86ConfAdjacencyPtr) XtMalloc(sizeof(XF86ConfAdjacencyRec));
		memset(aptr, 0, sizeof(XF86ConfAdjacencyRec));
	        fprintf(stderr, "screen = %s\n", sv[j]);
		if (aprev == (XF86ConfAdjacencyPtr) 0) {
			lptr->lay_adjacency_lst = aptr;
		} else {
			aprev->list.next = aptr;
		}
		aptr->adj_screen_str =	ConfigStrdup(sv[j]);
		ptr = Tcl_GetVar2(interp, namebuf, sv[j], 0);
		if (Tcl_SplitList(interp, ptr, &ac, &av) != TCL_OK)
			return TCL_ERROR;
		Tcl_GetInt(interp, av[0], &(aptr->adj_scrnum));
		aptr->adj_top_str =	ConfigStrdup(av[1]);
		aptr->adj_bottom_str =	ConfigStrdup(av[2]);
		aptr->adj_left_str =	ConfigStrdup(av[3]);
		aptr->adj_right_str =	ConfigStrdup(av[4]);
		aprev = aptr;
	    }
	    write_options(interp, namebuf, NULL, &(lptr->lay_option_lst));
	    prev = lptr;
	}
	return TCL_OK;
}

static int
putsection_vidadptr(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char section[128], namebuf[128], *ptr, **idxv;
	int i, idxc;
	XF86ConfVideoAdaptorPtr	aptr, prev;
	XF86ConfVideoPortPtr	pptr;

	fprintf(stderr, "putsection_vidadptr(%p, %s)\n", interp, varpfx);
	SECTION_NAME("VidAdaptr");
	ptr = Tcl_GetVar2(interp, section, "Identifiers", 0);
	if (Tcl_SplitList(interp, ptr, &idxc, &idxv) != TCL_OK)
		return TCL_ERROR;
	
	prev = (XF86ConfVideoAdaptorPtr) 0;
	for (i = 0; i < idxc; i++) {
		aptr = (XF86ConfVideoAdaptorPtr) XtMalloc(sizeof(XF86ConfVideoAdaptorRec));
		memset(aptr, 0, sizeof(XF86ConfVideoAdaptorRec));
		snprintf(namebuf, 128, "%s_%s", section, idxv[i]);
		if (prev == (XF86ConfVideoAdaptorPtr) 0) {
			config_list->conf_videoadaptor_lst = aptr;
		} else {
			prev->list.next = aptr;
		}
		aptr->va_identifier = idxv[i];
		SETnSTR(aptr->va_vendor,	"VendorName");
		SETnSTR(aptr->va_board,		"BoardName");
		SETnSTR(aptr->va_busid,		"BusID");
		SETnSTR(aptr->va_driver,	"Driver");
		/* XXX aptr->vp_port_lst */
		write_options(interp, namebuf, NULL, &(aptr->va_option_lst));
		/* XXX aptr->va_fwdref */
		prev = aptr;
	}
	return TCL_OK;
}

static int
putsection_modes(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char section[128], namebuf[128], *ptr, **idxv;
	int i, idxc;
	XF86ConfModesPtr	mptr, prev;

	fprintf(stderr, "putsection_modes(%p, %s)\n", interp, varpfx);
	SECTION_NAME("Modes");
	ptr = Tcl_GetVar2(interp, section, "Identifiers", 0);
	if (Tcl_SplitList(interp, ptr, &idxc, &idxv) != TCL_OK)
		return TCL_ERROR;
	
	prev = (XF86ConfModesPtr) 0;
	for (i = 0; i < idxc; i++) {
		mptr = (XF86ConfModesPtr) XtMalloc(sizeof(XF86ConfModesRec));
		memset(mptr, 0, sizeof(XF86ConfModesRec));
		snprintf(namebuf, 128, "%s_%s", section, idxv[i]);
		if (prev == (XF86ConfModesPtr) 0) {
			config_list->conf_modes_lst = mptr;
		} else {
			prev->list.next = mptr;
		}
		mptr->modes_identifier = idxv[i];
		write_modes(interp, namebuf, NULL, &(mptr->mon_modeline_lst));
		prev = mptr;
	}
	return TCL_OK;
}

static int
putsection_vendor(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char section[128], namebuf[128], *ptr, **idxv;
	int i, idxc;
	XF86ConfVendorPtr	vptr, prev;

	fprintf(stderr, "putsection_vendor(%p, %s)\n", interp, varpfx);
	SECTION_NAME("Vendor");
	ptr = Tcl_GetVar2(interp, section, "Identifiers", 0);
	if (Tcl_SplitList(interp, ptr, &idxc, &idxv) != TCL_OK)
		return TCL_ERROR;
	
	prev = (XF86ConfVendorPtr) 0;
	for (i = 0; i < idxc; i++) {
		vptr = (XF86ConfVendorPtr) XtMalloc(sizeof(XF86ConfVendorRec));
		memset(vptr, 0, sizeof(XF86ConfVendorRec));
		snprintf(namebuf, 128, "%s_%s", section, idxv[i]);
		if (prev == (XF86ConfVendorPtr) 0) {
			config_list->conf_vendor_lst = vptr;
		} else {
			prev->list.next = vptr;
		}
		vptr->vnd_identifier = idxv[i];
		write_options(interp, namebuf, NULL, &(vptr->vnd_option_lst));
		prev = vptr;
	}
	return TCL_OK;
}
	

static int
write_options(interp, section, idxname, ptr2optptr)
  Tcl_Interp *interp;
  char *section, *idxname;
  XF86OptionPtr *ptr2optptr;
{
	XF86OptionPtr optr, prev;
	char **optv, *tmpbuf, *optlist, *ptr, *ptr2;
	int  i, optc, idxlen;

	fprintf(stderr, "write_options(%p, %s, %s, %p)\n", interp, section, idxname, ptr2optptr);
	if (idxname) {
	    idxlen = strlen(idxname);
	    tmpbuf = XtMalloc(idxlen+9);
	    sprintf(tmpbuf, "%s:Options", idxname);
	} else {
	    idxlen = 0;
	    tmpbuf = XtMalloc(8);
	    strcpy(tmpbuf, "Options");
	}
	fprintf(stderr, "tmpbuf = '%s'\n", tmpbuf);
	optlist = Tcl_GetVar2(interp, section, tmpbuf, 0);
	fprintf(stderr, "options = %p\n", optlist);
	Tcl_SplitList(interp, optlist, &optc, &optv);
	fprintf(stderr, "options: %s\n", optlist);

	prev = (XF86OptionPtr) 0;
	for (i = 0; i < optc; i++) {
	    ptr = XtMalloc(idxlen+strlen(optv[i])+6);
	    sprintf(ptr, "%s%sOpt:%s", StrOrNull(idxname),
		(idxname? "_": ""), optv[i]);
	    optr = (XF86OptionPtr) XtMalloc(sizeof(XF86OptionRec));
	    memset(optr, 0, sizeof(XF86OptionRec));
	    fprintf(stderr, "optidx = %s\n", ptr);
	    if (prev == (XF86OptionPtr) 0) {
		*ptr2optptr = optr;
	    } else {
		prev->list.next = optr;
	    }
	    optr->opt_name= ConfigStrdup(optv[i]);
	    ptr2 = Tcl_GetVar2(interp, section, ptr, 0);
	    if (strlen(ptr2)) {
		    optr->opt_val = ConfigStrdup(ptr2);
	    }
	    optr->opt_used = 0;
	    prev = optr;
	    XtFree(ptr);
	}
	XtFree(tmpbuf);
	return TCL_OK;
}

static int
write_modes(interp, section, idxname, ptr2modeptr)
  Tcl_Interp *interp;
  char *section, *idxname;
  XF86ConfModeLinePtr *ptr2modeptr;
{
	XF86ConfModeLinePtr mptr, prev;
	char **modev, **mlv, *tmpbuf, *modelist, *ptr, *ptr2;
	int  i, j, modec, mlc, idxlen;
	double tmpclk;

	fprintf(stderr, "write_modes(%p, %s, %s, %p)\n", interp, section, idxname, ptr2modeptr);
	if (idxname) {
	    idxlen = strlen(idxname);
	    tmpbuf = XtMalloc(idxlen+9);
	    sprintf(tmpbuf, "%s:Modes", idxname);
	} else {
	    idxlen = 0;
	    tmpbuf = XtMalloc(8);
	    strcpy(tmpbuf, "Modes");
	}
	modelist = Tcl_GetVar2(interp, section, tmpbuf, 0);
	Tcl_SplitList(interp, modelist, &modec, &modev);

	prev = (XF86ConfModeLinePtr) 0;
	for (i = 0; i < modec; i++) {
	    ptr = XtMalloc(idxlen+strlen(modev[i])+6);
	    sprintf(ptr, "%s%sMode:%s", StrOrNull(idxname),
		(idxname? "_": ""), modev[i]);
	    mptr = (XF86ConfModeLinePtr) XtMalloc(sizeof(XF86ConfModeLineRec));
	    memset(mptr, 0, sizeof(XF86ConfModeLineRec));
	    if (prev == (XF86ConfModeLinePtr) 0) {
		*ptr2modeptr = mptr;
	    } else {
		prev->list.next = mptr;
	    }
	    ptr2 = Tcl_GetVar2(interp, section, ptr, 0);
	    Tcl_SplitList(interp, ptr2, &mlc, &mlv);
	    if (mlc < 10) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Invalid modeline: ", ptr2, NULL);
		return TCL_ERROR;
	    }
	    mptr->ml_identifier = ConfigStrdup(mlv[0]);
	    Tcl_GetDouble(interp, mlv[1],  &(tmpclk));
	    mptr->ml_clock = (int) (tmpclk * 1000.0 + 0.5);
	    Tcl_GetInt(interp, mlv[2], &(mptr->ml_hdisplay));
	    Tcl_GetInt(interp, mlv[3], &(mptr->ml_hsyncstart));
	    Tcl_GetInt(interp, mlv[4], &(mptr->ml_hsyncend));
	    Tcl_GetInt(interp, mlv[5], &(mptr->ml_htotal));
	    Tcl_GetInt(interp, mlv[6], &(mptr->ml_vdisplay));
	    Tcl_GetInt(interp, mlv[7], &(mptr->ml_vsyncstart));
	    Tcl_GetInt(interp, mlv[8], &(mptr->ml_vsyncend));
	    Tcl_GetInt(interp, mlv[9], &(mptr->ml_vtotal));

#define CHKFLAG(str,bit)	if (!NameCompare(mlv[j], (str))) { \
					mptr->ml_flags |= (bit); continue; }

	    for (j = 10; j < mlc; j++) {
		CHKFLAG("Interlace",	XF86CONF_INTERLACE);
		CHKFLAG("+HSync",	XF86CONF_PHSYNC);
		CHKFLAG("-HSync",	XF86CONF_NHSYNC);
		CHKFLAG("+VSync",	XF86CONF_PVSYNC);
		CHKFLAG("-VSync",	XF86CONF_NVSYNC);
		CHKFLAG("Composite",	XF86CONF_CSYNC);
		CHKFLAG("+CSync",	XF86CONF_PCSYNC);
		CHKFLAG("-CSync",	XF86CONF_NCSYNC);
		CHKFLAG("DoubleScan",	XF86CONF_DBLSCAN);
		if (strlen(mlv[j]) > 6 && mlv[j][5] == ' ') {
		    mlv[j][5] = '\0';
		    if (!NameCompare(mlv[j], "HSkew")) {
			mptr->ml_flags |= XF86CONF_HSKEW;
			Tcl_GetInt(interp, &(mlv[j][6]), &(mptr->ml_hskew));
		    }
		    if (!NameCompare(mlv[j], "VScan")) {
			Tcl_GetInt(interp, &(mlv[j][6]), &(mptr->ml_vscan));
		    }
		    mlv[j][5] = ' ';
		}
		/* Ignore unknown flags */
	    }
#undef CHKFLAG
	    prev = mptr;
	    XtFree(ptr);
	}
	XtFree(tmpbuf);
	return TCL_OK;
}

