/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

/* this is to get the following win32 DLL junk to work.
 * if ever tempted to remove this, first please read:
 * http://joel.editthispage.com/stories/storyReader$47
 */
 
#ifdef _UWIN
#ifndef _POSIX_		/* ncc doesn't define _POSIX_ */
/* i.e. if this is the win32 build using nmake with CC=ncc (native C) */
/* this was the easiest way to get some simple libc interfaces. */
#include "C:\Program Files\UWIN\usr\include\astwin32.h"
#undef _UWIN		/* don't assume ANY _UWIN features in the execution environment */
#endif /* _POSIX_ */
#endif /* _UWIN */

#ifndef __CYGWIN__
#if defined(_BLD_dotneato) && defined(_DLL)
#   define external __EXPORT__
#endif
#if !defined(_BLD_dotneato) && defined(__IMPORT__)
#   define external __IMPORT__
#endif
#endif

#ifndef external
#   define external   extern
#endif
#ifndef EXTERN
#define EXTERN extern
#endif

EXTERN		char		*Version;
EXTERN		char		**Lib;
EXTERN		char		**Files;			/* from command line */
EXTERN		char		**Lib;				/* from command line */
EXTERN		char		*CmdName;
EXTERN      char*       specificFlags;
EXTERN      char*       specificItems;
external	char		*Gvfilepath;		/* Path of files allowed in 'shapefile' attrib  (also ps libs)*/
external	int		y_invert;			/* invert y in dot & plain output */

#if ENABLE_CODEGENS
external	int		Output_lang;			/* POSTSCRIPT, DOT, etc. */
external	FILE		*Output_file;
external	int		Obj;
#endif

EXTERN		boolean		Verbose,Reduce,UseRankdir;
EXTERN		char		*HTTPServerEnVar;
EXTERN		char		*Output_file_name;
EXTERN		int			graphviz_errors;
EXTERN		int			Nop;
EXTERN		double		PSinputscale;
EXTERN		int			Syntax_errors;
EXTERN		int			Show_boxes;         /* emit code for correct box coordinates */
EXTERN		int			CL_type;            /* NONE, LOCAL, GLOBAL */
EXTERN		boolean		Concentrate;        /* if parallel edges should be merged */
EXTERN		double		Epsilon;    /* defined in input_graph */
EXTERN		double		Nodesep;
EXTERN		double		Nodefactor;
EXTERN		int		MaxIter;
EXTERN		int		Ndim;
EXTERN		int		State;      /* last finished phase */
EXTERN		double		Initial_dist;
EXTERN		double		Damping;
external	char*		BaseLineStyle[];  /* solid, width = 1 */

extern codegen_t FIG_CodeGen, GD_CodeGen, memGD_CodeGen, HPGL_CodeGen,
	ISMAP_CodeGen, IMAP_CodeGen, CMAP_CodeGen, CMAPX_CodeGen, MIF_CodeGen,
	XDot_CodeGen, MP_CodeGen, PIC_CodeGen, PS_CodeGen,
	DIA_CodeGen, SVG_CodeGen, VRML_CodeGen, VTX_CodeGen; 

EXTERN attrsym_t	
		*N_height, *N_width, *N_shape, *N_color, *N_fillcolor,
		*N_fontsize, *N_fontname, *N_fontcolor,
		*N_label, *N_style, *N_showboxes,
		*N_sides,*N_peripheries,*N_orientation,
		*N_skew,*N_distortion,*N_fixed,*N_layer,
		*N_group,*N_comment,*N_vertices,*N_z,*N_pin;

EXTERN attrsym_t	*E_weight, *E_minlen, *E_color,
					*E_fontsize, *E_fontname, *E_fontcolor,
					*E_label, *E_dir, *E_style, *E_decorate,
					*E_showboxes,*E_arrowsz,*E_constr,*E_layer,
					*E_comment,*E_label_float;
/* vladimir */
EXTERN attrsym_t	*E_samehead, *E_sametail,
					*E_arrowhead, *E_arrowtail,
					*E_headlabel, *E_taillabel,
					*E_labelfontsize, *E_labelfontname, *E_labelfontcolor,
					*E_labeldistance, *E_labelangle;

/* north */
EXTERN attrsym_t    *E_tailclip, *E_headclip;
#undef external

