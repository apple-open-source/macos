/* $XConsortium: xfsconf.h /main/2 1996/10/19 19:06:59 kaleb $ */





/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/xfsconf.h,v 3.4 1999/04/05 07:13:03 dawes Exp $ */

extern char *XtMalloc(
    unsigned int	/* size */
);

extern char *XtCalloc(
    unsigned int	/* num */,
    unsigned int	/* size */
);

extern char *XtRealloc(
    char*		/* ptr */,
    unsigned int	/* num */
);

extern void XtFree(
    char*		/* ptr */
);

int XF86Config_Init(
    Tcl_Interp	*interp
);

int TCL_XF86ReadXF86Config(
    ClientData	clientData,
    Tcl_Interp	*interp,
    int		argc,
    char	**argv
);

int TCL_XF86WriteXF86Config(
    ClientData	clientData,
    Tcl_Interp	*interp,
    int		argc,
    char	**argv
);

char *NonZeroStr(
    unsigned long val,
    int base
);

char *get_path_elem(
     char **pnt
);

char *validate_font_path(
     char *path
);

char *token_to_string(
     SymTabPtr table,
     int token
);

int string_to_token(
     SymTabPtr table,
     char *string
);

extern char *rgbPath, *defaultFontPath;

extern XF86ConfigPtr config_list;

extern Tcl_Interp *errinterp;

extern Bool Must_have_memory;

extern SymTabRec xfsMouseTab[];

#define StrOrNull(xx)	((xx)==NULL? "": (xx))

#define SECTION_NAME(name) { int len = sizeof(section)-sizeof(name)-1; \
			strncpy(section, varpfx, len); \
			varpfx[len] = '\0'; strcat(section, name); \
			Tcl_AppendElement(interp, name); }

#define DIR_FILE	"/fonts.dir"

