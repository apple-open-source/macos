/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/confread.c,v 1.10 2001/07/25 15:05:05 dawes Exp $ */
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

  Functions for converting a XF86ConfigRec to Tcl variables

 */

#include "X.h"
#include "os.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86Parser.h"
#include "xf86tokens.h"

#include "tcl.h"
#include "xfsconf.h"

char *defaultFontPath = COMPILEDDEFAULTFONTPATH;
char *rgbPath = RGB_DB;

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef NEED_SNPRINTF
#include "snprintf.h"
#endif

static int  getsection_files   (Tcl_Interp *interp, char *varpfx);
static int  getsection_module  (Tcl_Interp *interp, char *varpfx);
static int  getsection_flags   (Tcl_Interp *interp, char *varpfx);
#if 0
static int  getsection_input   (Tcl_Interp *interp, char *varpfx);
static int  getsection_dri     (Tcl_Interp *interp, char *varpfx);
#endif
static int  getsection_vidadptr(Tcl_Interp *interp, char *varpfx);
static int  getsection_modes   (Tcl_Interp *interp, char *varpfx);
static int  getsection_monitor (Tcl_Interp *interp, char *varpfx);
static int  getsection_device  (Tcl_Interp *interp, char *varpfx);
static int  getsection_screen  (Tcl_Interp *interp, char *varpfx);
static int  getsection_layout  (Tcl_Interp *interp, char *varpfx);
static int  getsection_vendor  (Tcl_Interp *interp, char *varpfx);

static void read_options(
  Tcl_Interp *interp,
  char *arrname,
  char *idxname,
  XF86OptionPtr opts
);

static void read_modes(
  Tcl_Interp *interp,
  char *arrname,
  char *idxname,
  XF86ConfModeLinePtr modes
);

#define APPEND_ELEMENT	(TCL_APPEND_VALUE|TCL_LIST_ELEMENT)

#define CONFPATH "%A,%R,/etc/X11/%R,%P/etc/X11/%R,%E,%F,/etc/X11/%F," \
		 "%P/etc/X11/%F,%D/%X,/etc/X11/%X,/etc/%X,%P/etc/X11/%X.%H," \
		 "%P/etc/X11/%X,%P/lib/X11/%X.%H,%P/lib/X11/%X"

/*
   Implements the xf86config_readfile command which locates and reads
   in the XF86Config file and set the values from it
*/

int
TCL_XF86ReadXF86Config(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	int	pfxarg = 1;
	char	*filename, *cmdline = NULL;

	if (argc > 3) {
		Tcl_SetResult(interp,
		    "Usage: xf86config_readfile [cmdline] <prefix>",
		    TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 3) {
		cmdline = argv[1];
		pfxarg = 2;
	}

	if ((filename = xf86OpenConfigFile(CONFPATH, cmdline, NULL)) == NULL) {
		Tcl_SetResult(interp,
			"Unable to open file" , TCL_STATIC);
		return TCL_ERROR;
	}
	Tcl_SetResult(interp, filename, TCL_VOLATILE);
	if ((config_list = xf86ReadConfigFile()) == NULL) {
		Tcl_SetResult(interp,
			"Error parsing config file" , TCL_STATIC);
		return TCL_ERROR;
	}
	xf86CloseConfigFile();
	getsection_files   (interp, argv[pfxarg]);
	getsection_module  (interp, argv[pfxarg]);
	getsection_flags   (interp, argv[pfxarg]);
#if 0
	getsection_input   (interp, argv[pfxarg]);
	getsection_dri     (interp, argv[pfxarg]);
#endif
	getsection_vidadptr(interp, argv[pfxarg]);
	getsection_modes   (interp, argv[pfxarg]);
	getsection_monitor (interp, argv[pfxarg]);
	getsection_device  (interp, argv[pfxarg]);
	getsection_screen  (interp, argv[pfxarg]);
	getsection_layout  (interp, argv[pfxarg]);
	getsection_vendor  (interp, argv[pfxarg]);
	return TCL_OK;
}

/*
  Set the Tcl variables for the config from the Files section
*/

static int
getsection_files(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char *f, section[128];

	SECTION_NAME("Files");
	fprintf(stderr, "getsection_%s(%p, %p(%s))\n", section, interp, varpfx, varpfx);

	Tcl_SetVar2(interp, section, "LogFile",
		StrOrNull(config_list->conf_files->file_logfile), 0);
	if (config_list->conf_files->file_fontpath) {
		f = validate_font_path(config_list->conf_files->file_fontpath);
		Tcl_SetVar2(interp, section, "FontPath", StrOrNull(f), 0);
	} else {
		Tcl_SetVar2(interp, section, "FontPath", defaultFontPath, 0);
	}
	if (config_list->conf_files->file_rgbpath)
		Tcl_SetVar2(interp, section, "RGBPath",
			config_list->conf_files->file_rgbpath, 0);
	else
		Tcl_SetVar2(interp, section, "RGBPath", rgbPath, 0);
	Tcl_SetVar2(interp, section, "ModulePath",
		StrOrNull(config_list->conf_files->file_modulepath), 0);
	return TCL_OK;
}

/*
  Set the Tcl variables for the config from the ServerFlags section
*/

static int
getsection_flags(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char section[128];

	SECTION_NAME("Server");
	fprintf(stderr, "getsection_%s(%p, %p(%s))\n", section, interp, varpfx, varpfx);

	if (!config_list->conf_flags)
		return TRUE;

	read_options(interp, section, NULL,
		config_list->conf_flags->flg_option_lst);
	return TCL_OK;
}

/*
  Set the Tcl variables for the config from the Monitor sections
*/

static int
getsection_monitor(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char		section[128];
	int		i;
	char		*namebuf, tmpbuf[32];
	XF86ConfMonitorPtr 	mptr;
	XF86ConfModesLinkPtr	lptr;

	SECTION_NAME("Monitor");
	fprintf(stderr, "getsection_%s(%p, %p(%s))\n", section, interp, varpfx, varpfx);
	Tcl_SetVar2(interp, section, "Identifiers", "", 0);
	for (mptr = config_list->conf_monitor_lst; mptr; mptr = mptr->list.next) {
		namebuf = (char *)XtMalloc(strlen(section)
					   + strlen(mptr->mon_identifier) + 2);
		sprintf(namebuf, "%s_%s", section, mptr->mon_identifier);

		Tcl_SetVar2(interp, section, "Identifiers", mptr->mon_identifier,
			APPEND_ELEMENT);
		Tcl_SetVar2(interp, namebuf, "VendorName",
			StrOrNull(mptr->mon_vendor), 0);
		Tcl_SetVar2(interp, namebuf, "ModelName",
			StrOrNull(mptr->mon_modelname), 0);
		sprintf(tmpbuf, "%d", mptr->mon_width);
		Tcl_SetVar2(interp, namebuf, "Width", tmpbuf, 0);
		sprintf(tmpbuf, "%d", mptr->mon_height);
		Tcl_SetVar2(interp, namebuf, "Height", tmpbuf, 0);

		Tcl_SetVar2(interp, namebuf, "HorizSync", "", 0);
		tmpbuf[0] = '\0';
		for (i = 0; i < mptr->mon_n_hsync; i++) {
		    sprintf(tmpbuf, "%.5g-%.5g", 
			mptr->mon_hsync[i].lo, mptr->mon_hsync[i].hi);
		    Tcl_SetVar2(interp, namebuf, "HorizSync", tmpbuf,
			APPEND_ELEMENT);
		}

		Tcl_SetVar2(interp, namebuf, "VertRefresh", "", 0);
		tmpbuf[0] = '\0';
		for (i = 0; i < mptr->mon_n_vrefresh; i++) {
		    sprintf(tmpbuf, "%.5g-%.5g", 
			mptr->mon_vrefresh[i].lo, mptr->mon_vrefresh[i].hi);
		    Tcl_SetVar2(interp, namebuf, "VertRefresh", tmpbuf,
			APPEND_ELEMENT);
		}

		sprintf(tmpbuf, "%.3g %.3g %.3g",
		    mptr->mon_gamma_red, 
		    mptr->mon_gamma_green, 
		    mptr->mon_gamma_blue);
		Tcl_SetVar2(interp, namebuf, "Gamma", tmpbuf, 0);

		read_modes  (interp, namebuf, NULL, mptr->mon_modeline_lst);
		read_options(interp, namebuf, NULL, mptr->mon_option_lst);
		Tcl_SetVar2 (interp, namebuf, "ModeLinks", "", 0);
		for (lptr = mptr->mon_modes_sect_lst; lptr; lptr = lptr->list.next) {
			Tcl_SetVar2 (interp, namebuf, "ModeLinks",
				lptr->ml_modes_str, APPEND_ELEMENT);
		}

		XtFree(namebuf);
	}
	return TCL_OK;
}

/*
  Set the Tcl variables for the config from the Device sections
*/

static int
getsection_device(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
	char	section[128];
	int	i;
	char	*namebuf, tmpbuf[128];
	XF86ConfDevicePtr	dptr;

	SECTION_NAME("Device");
	fprintf(stderr, "getsection_%s(%p, %p(%s))\n", section, interp, varpfx, varpfx);
	Tcl_SetVar2(interp, section, "Identifiers", "", 0);
	for (dptr = config_list->conf_device_lst; dptr; dptr = dptr->list.next) {
		namebuf = XtMalloc(strlen(section)
				   + strlen(dptr->dev_identifier) + 10);
		sprintf(namebuf, "%s_%s", section, dptr->dev_identifier);
		Tcl_SetVar2(interp, section, "Identifiers", dptr->dev_identifier,
			APPEND_ELEMENT);
		Tcl_SetVar2(interp, namebuf, "VendorName",
			StrOrNull(dptr->dev_vendor), 0);
		Tcl_SetVar2(interp, namebuf, "BoardName",
			StrOrNull(dptr->dev_board), 0);
		Tcl_SetVar2(interp, namebuf, "Chipset",
			StrOrNull(dptr->dev_chipset), 0);
		Tcl_SetVar2(interp, namebuf, "BusID",
			StrOrNull(dptr->dev_busid), 0);
		Tcl_SetVar2(interp, namebuf, "Card",
			StrOrNull(dptr->dev_card), 0);
		Tcl_SetVar2(interp, namebuf, "Driver",
			StrOrNull(dptr->dev_driver), 0);
		Tcl_SetVar2(interp, namebuf, "Ramdac",
			StrOrNull(dptr->dev_ramdac), 0);
		Tcl_SetVar2(interp, namebuf, "DacSpeed",
			NonZeroStr(dptr->dev_dacSpeeds[0]/1000,10), 0);
		if (dptr->dev_dacSpeeds[0]/1000 > 0)
		   for (i = 1; i < MAXDACSPEEDS; i++) {
		      if (dptr->dev_dacSpeeds[i]/1000 <= 0)
			 break;
		      sprintf(tmpbuf, "%d", dptr->dev_dacSpeeds[i]/1000);
		      Tcl_SetVar2(interp, namebuf, "DacSpeed",
				  tmpbuf, APPEND_ELEMENT);
		   }
		Tcl_SetVar2(interp, namebuf, "VideoRam",
			NonZeroStr(dptr->dev_videoram,10), 0);
		sprintf(tmpbuf, "%.5g", dptr->dev_textclockfreq/1000.0);
		Tcl_SetVar2(interp, namebuf, "TextClockFreq", tmpbuf, 0);
		Tcl_SetVar2(interp, namebuf, "BIOSBase",
			NonZeroStr(dptr->dev_bios_base,16), 0);
		Tcl_SetVar2(interp, namebuf, "MemBase",
			NonZeroStr(dptr->dev_mem_base,16), 0);
		Tcl_SetVar2(interp, namebuf, "IOBase",
			NonZeroStr(dptr->dev_io_base,16), 0);
		Tcl_SetVar2(interp, namebuf, "ClockChip",
			StrOrNull(dptr->dev_clockchip), 0);
		Tcl_SetVar2(interp, namebuf, "Clocks", "", 0);
		for (i = 0; i < dptr->dev_clocks; i++) {
			sprintf(tmpbuf, "%.5g ", dptr->dev_clock[i]/1000.0);
			Tcl_SetVar2(interp, namebuf, "Clocks",
				tmpbuf, APPEND_ELEMENT);
		}
		Tcl_SetVar2(interp, namebuf, "ChipID", (dptr->dev_chipid <1) ?
			"-1": NonZeroStr(dptr->dev_chipid,16), 0);
		Tcl_SetVar2(interp, namebuf, "ChipRev", (dptr->dev_chiprev <1) ?
			"-1": NonZeroStr(dptr->dev_chiprev,16), 0);

		read_options(interp, namebuf, NULL, dptr->dev_option_lst);
		XtFree(namebuf);
	}
	return TCL_OK;
}

/*
  Set the Tcl variables for the config from the Screen sections
*/

static int
getsection_screen(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
    char		section[128];
    XF86ConfScreenPtr	screen;
    XF86ConfDisplayPtr	disp;
    XF86ModePtr		mode;
    char		*namebuf, tmpbuf[128], tmpbuf2[32];
    int			depth;

    SECTION_NAME("Screen");
	fprintf(stderr, "getsection_%s(%p, %p(%s))\n", section, interp, varpfx, varpfx);
    Tcl_SetVar2(interp, section, "Identifiers", "", 0);
    for (screen = config_list->conf_screen_lst; screen; screen = screen->list.next) {
	namebuf = (char *) XtMalloc(strlen(section) 
		+ strlen(screen->scrn_identifier) + 2);
	sprintf(namebuf, "%s_%s", section, screen->scrn_identifier);
	Tcl_SetVar2(interp, section, "Identifiers", screen->scrn_identifier,
		APPEND_ELEMENT);
	Tcl_SetVar2(interp, namebuf, "Identifier",
		StrOrNull(screen->scrn_identifier), 0);
	Tcl_SetVar2(interp, namebuf, "Driver",
		StrOrNull(screen->scrn_obso_driver), 0);
	sprintf(tmpbuf, "%d", screen->scrn_defaultdepth);
	Tcl_SetVar2(interp, namebuf, "DefaultDepth", tmpbuf, 0);
	sprintf(tmpbuf, "%d", screen->scrn_defaultbpp);
	Tcl_SetVar2(interp, namebuf, "DefaultBPP", tmpbuf, 0);
	sprintf(tmpbuf, "%d", screen->scrn_defaultfbbpp);
	Tcl_SetVar2(interp, namebuf, "DefaultFbBPP", tmpbuf, 0);
	Tcl_SetVar2(interp, namebuf, "Monitor",
		StrOrNull(screen->scrn_monitor_str), 0);
	Tcl_SetVar2(interp, namebuf, "Device",
		StrOrNull(screen->scrn_device_str), 0);

	Tcl_SetVar2(interp, namebuf, "Depths", "", 0);
	for (disp = screen->scrn_display_lst; disp; disp = disp->list.next) {
	    depth = disp->disp_depth;
	    sprintf(tmpbuf, "%d", depth);
	    Tcl_SetVar2(interp, namebuf, "Depths", tmpbuf, APPEND_ELEMENT);
	    sprintf(tmpbuf2, "Depth,%d", depth);
	    Tcl_SetVar2(interp, namebuf, tmpbuf2, tmpbuf, 0);
	    sprintf(tmpbuf, "%d", disp->disp_bpp);
	    sprintf(tmpbuf2, "BPP,%d", depth);
	    Tcl_SetVar2(interp, namebuf, tmpbuf2, tmpbuf, 0);

	    if (disp->disp_frameX0 > 0 || disp->disp_frameX0 > 0) {
		sprintf(tmpbuf, "%d %d",
                        disp->disp_frameX0, disp->disp_frameY0);
		sprintf(tmpbuf2, "ViewPort,%d", depth);
		Tcl_SetVar2(interp, namebuf, tmpbuf2, tmpbuf, 0);
	    }

	    if (disp->disp_virtualX > 0 || disp->disp_virtualY > 0) {
		sprintf(tmpbuf, "%d %d",
			disp->disp_virtualX, disp->disp_virtualY);
		sprintf(tmpbuf2, "Virtual,%d", depth);
		Tcl_SetVar2(interp, namebuf, tmpbuf2, tmpbuf, 0);
	    }

	    if (disp->disp_depth == 16 && disp->disp_weight.red > 0) {
		sprintf(tmpbuf, "%d%d%d",
			disp->disp_weight.red,
			disp->disp_weight.green,
			disp->disp_weight.blue);
		sprintf(tmpbuf2, "Weight,%d", depth);
		Tcl_SetVar2(interp, namebuf, tmpbuf2, tmpbuf, 0);
	    }

	    if (disp->disp_visual) {
		if (!NameCompare(disp->disp_visual, "StaticGray") ||
		    !NameCompare(disp->disp_visual, "GrayScale") ||
		    !NameCompare(disp->disp_visual, "StaticColor") ||
		    !NameCompare(disp->disp_visual, "PseudoColor") ||
		    !NameCompare(disp->disp_visual, "TrueColor") ||
		    !NameCompare(disp->disp_visual, "DirectColor")) {
		    sprintf(tmpbuf2, "Visual,%d", depth);
		    Tcl_SetVar2(interp, namebuf, tmpbuf2, disp->disp_visual, 0);
		}
	    }

	    sprintf(tmpbuf2, "Modes,%d", depth);
	    Tcl_SetVar2(interp, namebuf, tmpbuf2, "", 0);
	    for (mode = disp->disp_mode_lst; mode; mode = mode->list.next) {
		Tcl_SetVar2(interp, namebuf, tmpbuf2, mode->mode_name,
			APPEND_ELEMENT);
	    }
	    sprintf(tmpbuf, "%d", depth);
	    read_options(interp, namebuf, tmpbuf, disp->disp_option_lst);

	}

	/* XXX: scrn_adaptor_lst */
	read_options(interp, namebuf, NULL, screen->scrn_option_lst);

	XtFree(namebuf);
    }

    return TCL_OK;
}

/*
  Set the Tcl variables for the config from the Module section
*/

static int
getsection_module(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
    char	section[128];
    XF86LoadPtr lptr;

    SECTION_NAME("Module");
	fprintf(stderr, "getsection_%s(%p, %p(%s))\n", section, interp, varpfx, varpfx);
    for (lptr = config_list->conf_modules->mod_load_lst; lptr; lptr = lptr->list.next) {
	    Tcl_SetVar2(interp, section, lptr->load_name,
			lptr->load_type? "driver": "module", 0);
	    read_options(interp, section, lptr->load_name, lptr->load_opt);
    }
    return TCL_OK;
}

/*
  Set the Tcl variables for the config from the Layout section
*/

static int
getsection_layout(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
    char			section[128], tmpbuf[16], *namebuf;
    XF86ConfLayoutPtr		lptr;
    XF86ConfAdjacencyPtr	aptr;
    XF86ConfInactivePtr		iptr;

    SECTION_NAME("Layout");
	fprintf(stderr, "getsection_%s(%p, %p(%s))\n", section, interp, varpfx, varpfx);
    Tcl_SetVar2(interp, section, "Identifiers", "", 0);
    for (lptr = config_list->conf_layout_lst; lptr; lptr = lptr->list.next) {
	namebuf = (char *)XtMalloc(strlen(section)
				   + strlen(lptr->lay_identifier) + 2);
	sprintf(namebuf, "%s_%s", section, lptr->lay_identifier);
	Tcl_SetVar2(interp, section, "Identifiers", lptr->lay_identifier,
		APPEND_ELEMENT);

	Tcl_SetVar2(interp, namebuf, "Screens", "", 0);
	for (aptr = lptr->lay_adjacency_lst; aptr; aptr = aptr->list.next) {
	    Tcl_SetVar2(interp, namebuf, "Screens",
		aptr->adj_screen_str, APPEND_ELEMENT);
	    sprintf(tmpbuf, "%d", aptr->adj_scrnum);
	    Tcl_SetVar2(interp, namebuf, aptr->adj_screen_str,
		tmpbuf, TCL_LIST_ELEMENT);
	    Tcl_SetVar2(interp, namebuf, aptr->adj_screen_str,
		StrOrNull(aptr->adj_top_str), APPEND_ELEMENT);
	    Tcl_SetVar2(interp, namebuf, aptr->adj_screen_str,
		StrOrNull(aptr->adj_bottom_str), APPEND_ELEMENT);
	    Tcl_SetVar2(interp, namebuf, aptr->adj_screen_str,
		StrOrNull(aptr->adj_left_str), APPEND_ELEMENT);
	    Tcl_SetVar2(interp, namebuf, aptr->adj_screen_str,
		StrOrNull(aptr->adj_right_str), APPEND_ELEMENT);
	}
	
	Tcl_SetVar2(interp, namebuf, "InactiveDevices", "", 0);
	for (iptr = lptr->lay_inactive_lst; iptr; iptr = iptr->list.next) {
	    Tcl_SetVar2(interp, namebuf, "InactiveDevices",
		StrOrNull(iptr->inactive_device_str), APPEND_ELEMENT);
	}
	read_options(interp, namebuf, NULL, lptr->lay_option_lst);
	XtFree(namebuf);
    }
    return TCL_OK;
}

/*
  Set the Tcl variables for the config from the VideoAdaptor section
*/

static int
getsection_vidadptr(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
    char			section[128];
    char			*namebuf;
    XF86ConfVideoAdaptorPtr	aptr;
    XF86ConfVideoPortPtr	pptr;

    SECTION_NAME("VidAdaptr");
	fprintf(stderr, "getsection_%s(%p, %p(%s))\n", section, interp, varpfx, varpfx);
    Tcl_SetVar2(interp, section, "Identifiers", "", 0);
    for (aptr= config_list->conf_videoadaptor_lst; aptr; aptr= aptr->list.next) {
	namebuf = (char *)XtMalloc(strlen(section)
				   + strlen(aptr->va_identifier) + 2);
	sprintf(namebuf, "%s_%s", section, aptr->va_identifier);
	Tcl_SetVar2(interp, section, "Identifiers", aptr->va_identifier,
		APPEND_ELEMENT);

	Tcl_SetVar2(interp, namebuf, "VendorName",
	    StrOrNull(aptr->va_vendor), 0);
	Tcl_SetVar2(interp, namebuf, "BoardName",
	    StrOrNull(aptr->va_board), 0);
	Tcl_SetVar2(interp, namebuf, "BusID",
	    StrOrNull(aptr->va_busid), 0);
	Tcl_SetVar2(interp, namebuf, "Driver",
	    StrOrNull(aptr->va_driver), 0);
	Tcl_SetVar2(interp, namebuf, "VideoPorts", "", 0);
	for (pptr = aptr->va_port_lst; pptr; pptr = pptr->list.next) {
	    Tcl_SetVar2(interp, namebuf, "VideoPorts",
		StrOrNull(pptr->vp_identifier), APPEND_ELEMENT);
	    read_options(interp, namebuf, pptr->vp_identifier, pptr->vp_option_lst);
	}
	read_options(interp, namebuf, NULL, aptr->va_option_lst);
	/* XXX what about pptr->va_fwdref??? */
    }
    return TCL_OK;
}

/*
  Set the Tcl variables for the config from the Modes section
*/

static int
getsection_modes(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
    char		section[128];
    char		*namebuf;
    XF86ConfModesPtr	mptr;

    SECTION_NAME("Modes");
	fprintf(stderr, "getsection_%s(%p, %p(%s))\n", section, interp, varpfx, varpfx);
    Tcl_SetVar2(interp, section, "Identifiers", "", 0);
    for (mptr = config_list->conf_modes_lst; mptr; mptr = mptr->list.next) {
	namebuf = (char *)XtMalloc(strlen(section)
				   + strlen(mptr->modes_identifier) + 2);
	sprintf(namebuf, "%s_%s", section, mptr->modes_identifier);
	Tcl_SetVar2(interp, section, "Identifiers", mptr->modes_identifier,
		APPEND_ELEMENT);
	read_modes(interp, namebuf, NULL, mptr->mon_modeline_lst);
    }
    return TCL_OK;
}

static int
getsection_vendor(interp, varpfx)
  Tcl_Interp *interp;
  char *varpfx;
{
    char		section[128];
    char		*namebuf;
    XF86ConfVendorPtr	vptr;

    SECTION_NAME("Vendor");
	fprintf(stderr, "getsection_%s(%p, %p(%s))\n", section, interp, varpfx, varpfx);
    Tcl_SetVar2(interp, section, "Identifiers", "", 0);
    for (vptr = config_list->conf_vendor_lst; vptr; vptr = vptr->list.next) {
	namebuf = (char *)XtMalloc(strlen(section)
				   + strlen(vptr->vnd_identifier) + 2);
	sprintf(namebuf, "%s_%s", section, vptr->vnd_identifier);
	Tcl_SetVar2(interp, section, "Identifiers", vptr->vnd_identifier,
		APPEND_ELEMENT);
	read_options(interp, namebuf, NULL, vptr->vnd_option_lst);
    }
    return TCL_OK;
}

static void
read_options(interp, arrname, idxname, opts)
  Tcl_Interp *interp;
  char *arrname, *idxname;
  XF86OptionPtr opts;
{
    char	idxbuf[128];
    char	optbuf[128];

    if (idxname) {
	snprintf(idxbuf, 128, "%s:Options", idxname);
    } else {
	strcpy(idxbuf, "Options");
    }
    Tcl_SetVar2(interp, arrname, idxbuf, "", 0);
    for ( ; opts; opts = opts->list.next) {
	snprintf(optbuf, 128, "%s%sOpt:%s", StrOrNull(idxname),
		(idxname? "_": ""), opts->opt_name);
	Tcl_SetVar2(interp, arrname, optbuf, opts->opt_val, 0);
	Tcl_SetVar2(interp, arrname, idxbuf, opts->opt_name, APPEND_ELEMENT);
    }
}

#define APPENDVAL(dfmt,var) { sprintf(tmpbuf2,dfmt,(var)); \
		 Tcl_SetVar2(interp,arrname,modebuf,tmpbuf2,APPEND_ELEMENT); }

static void
read_modes(interp, arrname, idxname, modes)
  Tcl_Interp *interp;
  char *arrname, *idxname;
  XF86ConfModeLinePtr modes;
{
    char	idxbuf[128], modebuf[128];
    char	tmpbuf2[16];
    static 	seqnum = 0;

    if (idxname) {
	snprintf(idxbuf, 128, "%s:Modes", idxname);
    } else {
	strcpy(idxbuf, "Modes");
    }
    Tcl_SetVar2(interp, arrname, idxbuf, "", 0);
    for ( ; modes; modes = modes->list.next) {
	snprintf(modebuf, 128, "%.110s%sMode:%d", StrOrNull(idxname),
		(idxname? "_": ""), ++seqnum);
	sprintf(tmpbuf2, "%d", seqnum);
	Tcl_SetVar2(interp, arrname, idxbuf, tmpbuf2, APPEND_ELEMENT);
	Tcl_SetVar2(interp, arrname, modebuf, modes->ml_identifier, 0);

	APPENDVAL("%.4g", modes->ml_clock/1000.0);
	APPENDVAL("%d",   modes->ml_hdisplay);
	APPENDVAL("%d",   modes->ml_hsyncstart);
	APPENDVAL("%d",   modes->ml_hsyncend);
	APPENDVAL("%d",   modes->ml_htotal);
	APPENDVAL("%d",   modes->ml_vdisplay);
	APPENDVAL("%d",   modes->ml_vsyncstart);
	APPENDVAL("%d",   modes->ml_vsyncend);
	APPENDVAL("%d",   modes->ml_vtotal);
	if (modes->ml_flags & XF86CONF_INTERLACE)
		Tcl_SetVar2(interp, arrname, modebuf, "Interlace", APPEND_ELEMENT);
	if (modes->ml_flags & XF86CONF_PHSYNC)
		Tcl_SetVar2(interp, arrname, modebuf, "+HSync", APPEND_ELEMENT);
	if (modes->ml_flags & XF86CONF_NHSYNC)
		Tcl_SetVar2(interp, arrname, modebuf, "-HSync", APPEND_ELEMENT);
	if (modes->ml_flags & XF86CONF_PVSYNC)
		Tcl_SetVar2(interp, arrname, modebuf, "+VSync", APPEND_ELEMENT);
	if (modes->ml_flags & XF86CONF_NVSYNC)
		Tcl_SetVar2(interp, arrname, modebuf, "-VSync", APPEND_ELEMENT);
	if (modes->ml_flags & XF86CONF_CSYNC)
		Tcl_SetVar2(interp, arrname, modebuf, "Composite", APPEND_ELEMENT);
	if (modes->ml_flags & XF86CONF_PCSYNC)
		Tcl_SetVar2(interp, arrname, modebuf, "+CSync", APPEND_ELEMENT);
	if (modes->ml_flags & XF86CONF_NCSYNC)
		Tcl_SetVar2(interp, arrname, modebuf, "-CSync", APPEND_ELEMENT);
	if (modes->ml_flags & XF86CONF_DBLSCAN)
		Tcl_SetVar2(interp, arrname, modebuf, "DoubleScan", APPEND_ELEMENT);
	if (modes->ml_flags & XF86CONF_HSKEW) {
		APPENDVAL("HSkew %d", modes->ml_hskew);
	}
	if (modes->ml_vscan) {
		APPENDVAL("VScan %d", modes->ml_vscan);
	}
    }
}

#undef APPENDVAL

