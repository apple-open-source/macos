/* 
 * tkTreeColumn.c --
 *
 *	This module implements treectrl widget's columns.
 *
 * Copyright (c) 2002-2003 Christian Krone
 * Copyright (c) 2003 ActiveState Corporation
 *
 * RCS: @(#) $Id: tkTreeColumn.c,v 1.13 2004/02/10 07:40:57 hobbs2 Exp $
 */

#include "tkTreeCtrl.h"

typedef struct Column Column;

struct Column
{
    Tcl_Obj *textObj;		/* -text */
    char *text;			/* -text */
    int width;			/* -width */
    Tcl_Obj *widthObj;		/* -width */
    int minWidth;		/* -minwidth */
    Tcl_Obj *minWidthObj;	/* -minwidth */
    int stepWidth;		/* -stepwidth */
    Tcl_Obj *stepWidthObj;	/* -stepwidth */
    int widthHack;		/* -widthhack */
    Tk_Font tkfont;		/* -font */
    Tk_Justify justify;		/* -justify */
    Tk_3DBorder border;		/* -border */
    Tcl_Obj *borderWidthObj;	/* -borderwidth */
    int borderWidth;		/* -borderwidth */
    int relief;			/* -relief */
    XColor *textColor;		/* -textcolor */
    int expand;			/* -expand */
    int visible;		/* -visible */
    char *tag;			/* -tag */
    char *imageString;		/* -image */
    Pixmap bitmap;		/* -bitmap */
    int sunken;			/* -sunken */
    Tcl_Obj *itemBgObj;		/* -itembackground */
    int button;			/* -button */
    Tcl_Obj *textPadXObj;	/* -textpadx */
    int *textPadX;		/* -textpadx */
    Tcl_Obj *textPadYObj;	/* -textpady */
    int *textPadY;		/* -textpady */
    Tcl_Obj *imagePadXObj;	/* -imagepadx */
    int *imagePadX;		/* -imagepadx */
    Tcl_Obj *imagePadYObj;	/* -imagepady */
    int *imagePadY;		/* -imagepady */
    Tcl_Obj *arrowPadObj;	/* -arrowpad */
    int *arrowPad;		/* -arrowpad */

#define ARROW_NONE 0
#define ARROW_UP 1
#define ARROW_DOWN 2
    int arrow;			/* -arrow */

#define SIDE_LEFT 0
#define SIDE_RIGHT 1
    int arrowSide;		/* -arrowside */
    int arrowGravity;		/* -arrowgravity */

    TreeCtrl *tree;
    Tk_OptionTable optionTable;
    int index;			/* column number */
    int textLen;
    int textWidth;
    Tk_Image image;
    int neededWidth;		/* calculated from borders + image/bitmap +
				 * text + arrow */
    int neededHeight;		/* calculated from borders + image/bitmap +
				 * text */
    int useWidth;		/* -width, -minwidth, or required+expansion */
    int widthOfItems;		/* width of all TreeItemColumns */
    int itemBgCount;
    XColor **itemBgColor;
    GC bitmapGC;
    Column *next;
};

static char *arrowST[] = { "none", "up", "down", (char *) NULL };
static char *arrowSideST[] = { "left", "right", (char *) NULL };

#define COLU_CONF_IMAGE		0x0001
#define COLU_CONF_NWIDTH	0x0002	/* neededWidth */
#define COLU_CONF_NHEIGHT	0x0004	/* neededHeight */
#define COLU_CONF_TWIDTH	0x0008	/* totalWidth */
#define COLU_CONF_ITEMBG	0x0010
#define COLU_CONF_DISPLAY	0x0040
#define COLU_CONF_JUSTIFY	0x0080
#define COLU_CONF_TAG		0x0100
#define COLU_CONF_TEXT		0x0200
#define COLU_CONF_BITMAP	0x0400
#define COLU_CONF_RANGES	0x0800

static Tk_OptionSpec columnSpecs[] = {
    {TK_OPTION_STRING_TABLE, "-arrow", (char *) NULL, (char *) NULL,
     "none", -1, Tk_Offset(Column, arrow),
     0, (ClientData) arrowST, COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_STRING_TABLE, "-arrowside", (char *) NULL, (char *) NULL,
     "right", -1, Tk_Offset(Column, arrowSide),
     0, (ClientData) arrowSideST, COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_STRING_TABLE, "-arrowgravity", (char *) NULL, (char *) NULL,
     "left", -1, Tk_Offset(Column, arrowGravity),
     0, (ClientData) arrowSideST, COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-arrowpad", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(Column, arrowPadObj),
     Tk_Offset(Column, arrowPad), 0, (ClientData) &PadAmountOption,
     COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_BITMAP, "-bitmap", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(Column, bitmap),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_BITMAP | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_BORDER, "-background", (char *) NULL, (char *) NULL,
     DEF_BUTTON_BG_COLOR, -1, Tk_Offset(Column, border),
     0, (ClientData) DEF_BUTTON_BG_MONO, COLU_CONF_DISPLAY},
    {TK_OPTION_PIXELS, "-borderwidth", (char *) NULL, (char *) NULL,
     "2", Tk_Offset(Column, borderWidthObj), Tk_Offset(Column, borderWidth),
     0, (ClientData) NULL, COLU_CONF_TWIDTH | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_BOOLEAN, "-button", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(Column, button),
     0, (ClientData) NULL, 0},
    {TK_OPTION_BOOLEAN, "-expand", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(Column, expand),
     0, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_FONT, "-font", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(Column, tkfont),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_NWIDTH |
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY | COLU_CONF_TEXT},
    {TK_OPTION_STRING, "-image", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(Column, imageString),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_IMAGE | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-imagepadx", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(Column, imagePadXObj),
     Tk_Offset(Column, imagePadX), 0, (ClientData) &PadAmountOption,
     COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-imagepady", (char *) NULL, (char *) NULL,
     "0", Tk_Offset(Column, imagePadYObj),
     Tk_Offset(Column, imagePadY), 0, (ClientData) &PadAmountOption,
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_STRING, "-itembackground", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, itemBgObj), -1,
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_ITEMBG},
    {TK_OPTION_JUSTIFY, "-justify", (char *) NULL, (char *) NULL,
     "left", -1, Tk_Offset(Column, justify),
     0, (ClientData) NULL, COLU_CONF_DISPLAY | COLU_CONF_JUSTIFY},
    {TK_OPTION_PIXELS, "-minwidth", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, minWidthObj),
     Tk_Offset(Column, minWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_RELIEF, "-relief", (char *) NULL, (char *) NULL,
     "raised", -1, Tk_Offset(Column, relief),
     0, (ClientData) NULL, COLU_CONF_DISPLAY},
    {TK_OPTION_PIXELS, "-stepwidth", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, stepWidthObj),
     Tk_Offset(Column, stepWidth),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_RANGES},
    {TK_OPTION_BOOLEAN, "-sunken", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(Column, sunken),
     0, (ClientData) NULL, COLU_CONF_DISPLAY},
    {TK_OPTION_STRING, "-tag", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(Column, tag),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TAG},
    {TK_OPTION_STRING, "-text", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, textObj), Tk_Offset(Column, text),
     TK_OPTION_NULL_OK, (ClientData) NULL,
     COLU_CONF_TEXT | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_COLOR, "-textcolor", (char *) NULL, (char *) NULL,
     DEF_BUTTON_FG, -1, Tk_Offset(Column, textColor),
     0, (ClientData) NULL, COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-textpadx", (char *) NULL, (char *) NULL,
     "6", Tk_Offset(Column, textPadXObj),
     Tk_Offset(Column, textPadX), 0, (ClientData) &PadAmountOption,
     COLU_CONF_NWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_CUSTOM, "-textpady", (char *) NULL, (char *) NULL,
     "0", Tk_Offset(Column, textPadYObj),
     Tk_Offset(Column, textPadY), 0, (ClientData) &PadAmountOption,
     COLU_CONF_NHEIGHT | COLU_CONF_DISPLAY},
    {TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(Column, widthObj), Tk_Offset(Column, width),
     TK_OPTION_NULL_OK, (ClientData) NULL, COLU_CONF_TWIDTH},
    {TK_OPTION_BOOLEAN, "-visible", (char *) NULL, (char *) NULL,
     "1", -1, Tk_Offset(Column, visible),
     0, (ClientData) NULL, COLU_CONF_TWIDTH | COLU_CONF_DISPLAY},
    {TK_OPTION_BOOLEAN, "-widthhack", (char *) NULL, (char *) NULL,
     "0", -1, Tk_Offset(Column, widthHack),
     0, (ClientData) NULL, COLU_CONF_RANGES},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, 0, 0}
};

/* Called when Tk_Image is deleted or modified */
static void ImageChangedProc(
    ClientData clientData,
    int x, int y,
    int width, int height,
    int imageWidth, int imageHeight)
{
    /* I would like to know the image was deleted... */
    Column *column = (Column *) clientData;

    Tree_DInfoChanged(column->tree, DINFO_INVALIDATE | DINFO_OUT_OF_DATE);
}

int Tree_FindColumnByTag(TreeCtrl *tree, Tcl_Obj *obj, TreeColumn *columnPtr, int flags)
{
    Column *walk = (Column *) tree->columns;
    char *string = Tcl_GetString(obj);

    if (!strcmp(string, "tail")) {
	if (!(flags & CFO_NOT_TAIL)) {
	    (*columnPtr) = tree->columnTail;
	    return TCL_OK;
	}
	FormatResult(tree->interp, "can't specify \"tail\" for this command");
	return TCL_ERROR;
    }

    while (walk != NULL) {
	if ((walk->tag != NULL) && !strcmp(walk->tag, string)) {
	    (*columnPtr) = (TreeColumn) walk;
	    return TCL_OK;
	}
	walk = walk->next;
    }
    FormatResult(tree->interp, "column with tag \"%s\" doesn't exist",
	    string);
    return TCL_ERROR;
}

int TreeColumn_FromObj(TreeCtrl *tree, Tcl_Obj *obj, TreeColumn *columnPtr, int flags)
{
    int columnIndex;

    if (Tcl_GetIntFromObj(NULL, obj, &columnIndex) == TCL_OK) {
	if (columnIndex < 0 || columnIndex >= tree->columnCount) {
	    if (tree->columnCount > 0)
		FormatResult(tree->interp,
			"bad column index \"%d\": must be from 0 to %d",
			columnIndex, tree->columnCount - 1);
	    else
		FormatResult(tree->interp,
			"bad column index \"%d\": there are no columns",
			columnIndex);
	    return TCL_ERROR;
	}
	(*columnPtr) = Tree_FindColumn(tree, columnIndex);
	return TCL_OK;
    }

    return Tree_FindColumnByTag(tree, obj, columnPtr, flags);
}

TreeColumn Tree_FindColumn(TreeCtrl *tree, int columnIndex)
{
    Column *column = (Column *) tree->columns;

    while (column != NULL) {
	if (column->index == columnIndex)
	    break;
	column = column->next;
    }
    return (TreeColumn) column;
}

TreeColumn TreeColumn_Next(TreeColumn column_)
{
    return (TreeColumn) ((Column *) column_)->next;
}

static void Column_FreeColors(XColor **colors, int count)
{
    int i;

    if (colors == NULL) {
	return;
    }
    for (i = 0; i < count; i++) {
	if (colors[i] != NULL) {
	    Tk_FreeColor(colors[i]);
	}
    }
    wipefree((char *) colors, sizeof(XColor *) * count);
}

static int Column_Config(Column *column, int objc, Tcl_Obj *CONST objv[])
{
    TreeCtrl *tree = column->tree;
    Column saved, *walk;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;
    int mask;
    XGCValues gcValues;
    unsigned long gcMask;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) column,
			column->optionTable, objc, objv, tree->tkwin,
			&savedOptions, &mask) != TCL_OK) {
		mask = 0;
		continue;
	    }

	    if (mask & COLU_CONF_IMAGE)
		saved.image = column->image;
	    if (mask & COLU_CONF_ITEMBG) {
		saved.itemBgColor = column->itemBgColor;
		saved.itemBgCount = column->itemBgCount;
	    }

	    if ((mask & COLU_CONF_TAG) && (column->tag != NULL)) {
		if (column->index == -1) {
		    FormatResult(tree->interp,
			    "can't change tag of tail column");
		    continue;
		}
		/* Verify -tag is unique */
		walk = (Column *) tree->columns;
		if (!strcmp(column->tag, "tail")) {
		    FormatResult(tree->interp, "column tag \"%s\" is not unique",
			    column->tag);
		    continue;
		}
		while (walk != NULL) {
		    if ((walk != column) && (walk->tag != NULL) && !strcmp(walk->tag, column->tag)) {
			FormatResult(tree->interp, "column tag \"%s\" is not unique",
				column->tag);
			break;
		    }
		    walk = walk->next;
		}
		if (walk != NULL)
		    continue;
	    }

	    if (mask & COLU_CONF_IMAGE) {
		column->image = NULL;
		if (column->imageString != NULL) {
		    column->image = Tk_GetImage(tree->interp, tree->tkwin,
			    column->imageString, ImageChangedProc,
			    (ClientData) column);
		    if (column->image == NULL)
			continue;
		}
	    }

	    if (mask & COLU_CONF_ITEMBG) {
		column->itemBgColor = NULL;
		column->itemBgCount = 0;
		if (column->itemBgObj != NULL) {
		    int i, length, listObjc;
		    Tcl_Obj **listObjv;
		    XColor **colors;

		    if (Tcl_ListObjGetElements(tree->interp, column->itemBgObj,
				&listObjc, &listObjv) != TCL_OK)
			continue;
		    colors = (XColor **) ckalloc(sizeof(XColor *) * listObjc);
		    for (i = 0; i < listObjc; i++)
			colors[i] = NULL;
		    for (i = 0; i < listObjc; i++) {
			/* Can specify "" for tree background */
			(void) Tcl_GetStringFromObj(listObjv[i], &length);
			if (length != 0)
			    {
				colors[i] = Tk_AllocColorFromObj(tree->interp,
					tree->tkwin, listObjv[i]);
				if (colors[i] == NULL)
				    break;
			    }
		    }
		    if (i < listObjc) {
			Column_FreeColors(colors, listObjc);
			continue;
		    }
		    column->itemBgColor = colors;
		    column->itemBgCount = listObjc;
		}
	    }

	    if (mask & COLU_CONF_IMAGE) {
		if (saved.image != NULL)
		    Tk_FreeImage(saved.image);
	    }
	    if (mask & COLU_CONF_ITEMBG) {
		Column_FreeColors(saved.itemBgColor, saved.itemBgCount);

		/* Set max -itembackground */
		tree->columnBgCnt = 0;
		walk = (Column *) tree->columns;
		while (walk != NULL) {
		    if (walk->itemBgCount > tree->columnBgCnt)
			tree->columnBgCnt = walk->itemBgCount;
		    walk = walk->next;
		}
	    }
	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    if (mask & COLU_CONF_IMAGE) {
		if (column->image != NULL)
		    Tk_FreeImage(column->image);
		column->image = saved.image;
	    }
	    if (mask & COLU_CONF_ITEMBG) {
		Column_FreeColors(column->itemBgColor, column->itemBgCount);
		column->itemBgColor = saved.itemBgColor;
		column->itemBgCount = saved.itemBgCount;
	    }

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    if (mask & COLU_CONF_TEXT) {
	if (column->textObj != NULL)
	    (void) Tcl_GetStringFromObj(column->textObj, &column->textLen);
	else
	    column->textLen = 0;
	if (column->textLen) {
	    Tk_Font tkfont = column->tkfont ? column->tkfont : tree->tkfont;
	    column->textWidth = Tk_TextWidth(tkfont, column->text, column->textLen);
	} else
	    column->textWidth = 0;
    }

    if (mask & COLU_CONF_BITMAP) {
	if (column->bitmapGC != None) {
	    Tk_FreeGC(tree->display, column->bitmapGC);
	    column->bitmapGC = None;
	}
	if (column->bitmap != None) {
	    gcValues.clip_mask = column->bitmap;
	    gcValues.graphics_exposures = False;
	    gcMask = GCClipMask | GCGraphicsExposures;
	    column->bitmapGC = Tk_GetGC(tree->tkwin, gcMask, &gcValues);
	}
    }

    if (mask & COLU_CONF_ITEMBG)
	Tree_DInfoChanged(tree, DINFO_INVALIDATE | DINFO_OUT_OF_DATE);

    if (mask & COLU_CONF_NWIDTH)
	column->neededWidth = -1;
    if (mask & COLU_CONF_NHEIGHT) {
	column->neededHeight = -1;
	tree->headerHeight = -1;
    }

    if (mask & COLU_CONF_JUSTIFY)
	Tree_DInfoChanged(tree, DINFO_INVALIDATE | DINFO_OUT_OF_DATE);

    /* -stepwidth and  -widthHack */
    if (mask & COLU_CONF_RANGES)
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

    /* Redraw everything */
    if (mask & (COLU_CONF_TWIDTH | COLU_CONF_NWIDTH | COLU_CONF_NHEIGHT)) {
	tree->widthOfColumns = -1;
	Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH | DINFO_DRAW_HEADER);
    }

    /* Redraw header only */
    else if (mask & COLU_CONF_DISPLAY) {
	Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);
    }

    return TCL_OK;
}

static Column *Column_Alloc(TreeCtrl *tree)
{
    Column *column;

    column = (Column *) ckalloc(sizeof(Column));
    memset(column, '\0', sizeof(Column));
    column->tree = tree;
    column->optionTable = Tk_CreateOptionTable(tree->interp, columnSpecs);
    if (Tk_InitOptions(tree->interp, (char *) column, column->optionTable,
		tree->tkwin) != TCL_OK) {
	WFREE(column, Column);
	return NULL;
    }
#if 0
    if (Tk_SetOptions(header->tree->interp, (char *) column,
		column->optionTable, 0,
		NULL, header->tree->tkwin, &savedOptions,
		(int *) NULL) != TCL_OK) {
	WFREE(column, Column);
	return NULL;
    }
#endif
    column->neededWidth = column->neededHeight = -1;
    tree->headerHeight = tree->widthOfColumns = -1;
    tree->columnCount++;
    return column;
}

TreeColumn Tree_CreateColumn(TreeCtrl *tree, int columnIndex, int *isNew_)
{
    Column *column = (Column *) tree->columns;
    int i, isNew = FALSE;

    if (column == NULL) {
	column = Column_Alloc(tree);
	column->index = 0;
	tree->columns = (TreeColumn) column;
	isNew = TRUE;
    }
    for (i = 0; i < columnIndex; i++) {
	if (column->next == NULL) {
	    column->next = Column_Alloc(tree);
	    column->next->index = i + 1;
	    isNew = TRUE;
	}
	column = column->next;
    }
    if (isNew)
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
    if (isNew_ != NULL)
	(*isNew_) = isNew;
    return (TreeColumn) column;
}

static Column *Column_Free(Column *column)
{
    TreeCtrl *tree = column->tree;
    Column *next = column->next;

    Column_FreeColors(column->itemBgColor, column->itemBgCount);
    if (column->bitmapGC != None)
	Tk_FreeGC(tree->display, column->bitmapGC);
    if (column->image != NULL)
	Tk_FreeImage(column->image);
    Tk_FreeConfigOptions((char *) column, column->optionTable, tree->tkwin);
    WFREE(column, Column);
    tree->columnCount--;
    return next;
}

int TreeColumn_FixedWidth(TreeColumn column_)
{
    return ((Column *) column_)->widthObj ? ((Column *) column_)->width : -1;
}

int TreeColumn_MinWidth(TreeColumn column_)
{
    return ((Column *) column_)->minWidthObj ? ((Column *) column_)->minWidth : -1;
}

int TreeColumn_StepWidth(TreeColumn column_)
{
    return ((Column *) column_)->stepWidthObj ? ((Column *) column_)->stepWidth : -1;
}

int TreeColumn_NeededWidth(TreeColumn column_)
{
    Column *column = (Column *) column_;
    int i, widthList[3], padList[4], n = 0;
    int arrowWidth = 0;

    if (column->neededWidth >= 0)
	return column->neededWidth;

    for (i = 0; i < 3; i++) widthList[i] = 0;
    for (i = 0; i < 4; i++) padList[i] = 0;

    if (column->arrow != ARROW_NONE) {
	arrowWidth = Tree_HeaderHeight(column->tree) / 2;
	if (!(arrowWidth & 1))
	    arrowWidth--;
    }
    if ((column->arrow != ARROW_NONE) && (column->arrowSide == SIDE_LEFT)) {
	widthList[n] = arrowWidth;
	padList[n] = column->arrowPad[PAD_TOP_LEFT];
	padList[n + 1] = column->arrowPad[PAD_BOTTOM_RIGHT];
	n++;
    }
    if ((column->image != NULL) || (column->bitmap != None)) {
	int imgWidth, imgHeight;
	if (column->image != NULL)
	    Tk_SizeOfImage(column->image, &imgWidth, &imgHeight);
	else
	    Tk_SizeOfBitmap(column->tree->display, column->bitmap, &imgWidth, &imgHeight);
	padList[n] = MAX(column->imagePadX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = column->imagePadX[PAD_BOTTOM_RIGHT];
	widthList[n] = imgWidth;
	n++;
    }
    if (column->textLen > 0) {
	padList[n] = MAX(column->textPadX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = column->textPadX[PAD_BOTTOM_RIGHT];
	widthList[n] = column->textWidth;
	n++;
    }
    if ((column->arrow != ARROW_NONE) && (column->arrowSide == SIDE_RIGHT)) {
	widthList[n] = arrowWidth;
	padList[n] = column->arrowPad[PAD_TOP_LEFT];
	padList[n + 1] = column->arrowPad[PAD_BOTTOM_RIGHT];
	n++;
    }

    column->neededWidth = 0;
    for (i = 0; i < n; i++)
	column->neededWidth += widthList[i] + padList[i];
    column->neededWidth += padList[n];

    return column->neededWidth;
}

int TreeColumn_NeededHeight(TreeColumn column_)
{
    Column *column = (Column *) column_;

    if (column->neededHeight >= 0)
	return column->neededHeight;

    column->neededHeight = 0;
    if ((column->image != NULL) || (column->bitmap != None)) {
	int imgWidth, imgHeight;
	if (column->image != NULL)
	    Tk_SizeOfImage(column->image, &imgWidth, &imgHeight);
	else
	    Tk_SizeOfBitmap(column->tree->display, column->bitmap, &imgWidth, &imgHeight);
	imgHeight += column->imagePadY[PAD_TOP_LEFT]
	    + column->imagePadY[PAD_BOTTOM_RIGHT];
	column->neededHeight = MAX(column->neededHeight, imgHeight);
    }
    if (column->text != NULL) {
	Tk_Font tkfont = column->tkfont ? column->tkfont : column->tree->tkfont;
	Tk_FontMetrics fm;
	Tk_GetFontMetrics(tkfont, &fm);
	fm.linespace += column->textPadY[PAD_TOP_LEFT]
	    + column->textPadY[PAD_BOTTOM_RIGHT];
	column->neededHeight = MAX(column->neededHeight, fm.linespace);
    }
    column->neededHeight += column->borderWidth * 2;

    return column->neededHeight;
}

int TreeColumn_UseWidth(TreeColumn column_)
{
    /* Update layout if needed */
    (void) Tree_WidthOfColumns(((Column *) column_)->tree);

    return ((Column *) column_)->useWidth;
}

void TreeColumn_SetUseWidth(TreeColumn column_, int width)
{
    ((Column *) column_)->useWidth = width;
}

Tk_Justify TreeColumn_Justify(TreeColumn column_)
{
    return ((Column *) column_)->justify;
}

int TreeColumn_WidthHack(TreeColumn column_)
{
    return ((Column *) column_)->widthHack;
}

GC TreeColumn_BackgroundGC(TreeColumn column_, int index)
{
    Column *column = (Column *) column_;
    XColor *color;

    if ((index < 0) || (column->itemBgCount == 0))
	return None;
    color = column->itemBgColor[index % column->itemBgCount];
    if (color == NULL)
	return None;
    return Tk_GCForColor(color, Tk_WindowId(column->tree->tkwin));
}

int TreeColumn_Visible(TreeColumn column_)
{
    return ((Column *) column_)->visible;
}

int TreeColumn_Index(TreeColumn column_)
{
    return ((Column *) column_)->index;
}

int TreeColumnCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
    TreeCtrl *tree = (TreeCtrl *) clientData;
    static CONST char *commandNames[] = { "bbox", "cget", "configure",
					  "delete", "index", "move", "neededwidth", "width", (char *) NULL };
    enum { COMMAND_BBOX, COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_DELETE,
	   COMMAND_INDEX, COMMAND_MOVE, COMMAND_NEEDEDWIDTH, COMMAND_WIDTH };
    int index;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
		&index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	case COMMAND_BBOX:
	{
	    int x = 0;
	    Column *column, *walk;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column, CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    if (!tree->showHeader || !column->visible)
		break;
	    /* Update layout */
	    Tree_WidthOfColumns(tree);
	    walk = (Column *) tree->columns;
	    while (walk != column) {
		if (column->visible)
		    x += walk->useWidth;
		walk = walk->next;
	    }
	    FormatResult(interp, "%d %d %d %d",
		    x - tree->xOrigin,
		    tree->inset,
		    x - tree->xOrigin + column->useWidth,
		    tree->inset + Tree_HeaderHeight(tree));
	    break;
	}

	case COMMAND_CGET:
	{
	    TreeColumn column;
	    Tcl_Obj *resultObjPtr;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "column option");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], &column, 0) != TCL_OK)
		return TCL_ERROR;
	    resultObjPtr = Tk_GetOptionValue(interp, (char *) column,
		    ((Column *) column)->optionTable, objv[4], tree->tkwin);
	    if (resultObjPtr == NULL)
		return TCL_ERROR;
	    Tcl_SetObjResult(interp, resultObjPtr);
	    break;
	}

	case COMMAND_CONFIGURE:
	{
	    int columnIndex;
	    Column *column;

	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column ?option? ?value?");
		return TCL_ERROR;
	    }
	    if (objc <= 5) {
		Tcl_Obj *resultObjPtr;

		if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column,
			    0) != TCL_OK)
		    return TCL_ERROR;
		resultObjPtr = Tk_GetOptionInfo(interp, (char *) column,
			column->optionTable,
			(objc == 4) ? (Tcl_Obj *) NULL : objv[4],
			tree->tkwin);
		if (resultObjPtr == NULL)
		    return TCL_ERROR;
		Tcl_SetObjResult(interp, resultObjPtr);
		break;
	    }
	    /* If a positive index is specified, and the column doesn't exist,
	     * then create it. */
	    if (Tcl_GetIntFromObj(NULL, objv[3], &columnIndex) == TCL_OK) {
		if (columnIndex < 0) {
		    FormatResult(tree->interp,
			    "bad column index \"%d\": must be >= 0",
			    columnIndex);
		    return TCL_ERROR;
		}
		column = (Column *) Tree_CreateColumn(tree, columnIndex, NULL);
		if (column == NULL)
		    return TCL_ERROR;
	    } else if (Tree_FindColumnByTag(tree, objv[3], (TreeColumn *) &column, 0) != TCL_OK)
		return TCL_ERROR;
	    return Column_Config(column, objc - 4, objv + 4);
	}

	case COMMAND_DELETE:
	{
	    int columnIndex;
	    Column *column, *prev;
	    TreeItem item;
	    TreeItemColumn itemColumn;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column,
			CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    columnIndex = column->index;
	    if (columnIndex > 0) {
		prev = (Column *) Tree_FindColumn(tree, columnIndex - 1);
		prev->next = column->next;
	    } else {
		tree->columns = (TreeColumn) column->next;
	    }
	    Column_Free(column);

	    if (columnIndex == tree->columnTree)
		tree->columnTree = -1;

	    /* Delete all TreeItemColumns */
	    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		itemColumn = TreeItem_FindColumn(tree, item, columnIndex);
		if (itemColumn != NULL)
		    TreeItem_RemoveColumn(tree, item, itemColumn);
		hPtr = Tcl_NextHashEntry(&search);
	    }

	    /* Renumber columns */
	    column = (Column *) tree->columns;
	    columnIndex = 0;
	    while (column != NULL) {
		column->index = columnIndex++;
		column = column->next;
	    }

	    tree->widthOfColumns = tree->headerHeight = -1;
	    Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH);
	    break;
	}

	case COMMAND_WIDTH:
	{
	    Column *column;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column, 0) != TCL_OK)
		return TCL_ERROR;
	    /* Update layout if needed */
	    (void) Tree_TotalWidth(tree);
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(column->useWidth));
	    break;
	}

	case COMMAND_INDEX:
	{
	    Column *column;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column, 0) != TCL_OK)
		return TCL_ERROR;
	    if (column->index == -1)
		Tcl_SetObjResult(interp, Tcl_NewIntObj(tree->columnCount));
	    else
		Tcl_SetObjResult(interp, Tcl_NewIntObj(column->index));
	    break;
	}

	/* T column move C before */
	case COMMAND_MOVE:
	{
	    Column *move, *before;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch search;
	    TreeItem item;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "column before");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &move, CFO_NOT_TAIL) != TCL_OK)
		return TCL_ERROR;
	    if (TreeColumn_FromObj(tree, objv[4], (TreeColumn *) &before, 0) != TCL_OK)
		return TCL_ERROR;
	    if (move == before)
		break;
	    if ((move->next == before) || ((move->next == NULL) &&
			(before == (Column *) tree->columnTail)))
		break;

	    /* Move the column in every item */
	    hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	    while (hPtr != NULL) {
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		TreeItem_MoveColumn(tree, item, move->index, before->index);
		hPtr = Tcl_NextHashEntry(&search);
	    }
	    {
		Column *prevM = NULL, *prevB = NULL;
		Column *last = NULL, *prev, *walk;
		Column *columnTree = NULL;
		int index;

		prev = NULL;
		walk = (Column *) tree->columns;
		while (walk != NULL) {
		    if (walk == move)
			prevM = prev;
		    if (walk == before)
			prevB = prev;
		    if (walk->index == tree->columnTree)
			columnTree = walk;
		    prev = walk;
		    if (walk->next == NULL)
			last = walk;
		    walk = walk->next;
		}
		if (prevM == NULL)
		    tree->columns = (TreeColumn) move->next;
		else
		    prevM->next = move->next;
		if (before == (Column *) tree->columnTail) {
		    last->next = move;
		    move->next = NULL;
		} else {
		    if (prevB == NULL)
			tree->columns = (TreeColumn) move;
		    else
			prevB->next = move;
		    move->next = before;
		}

		/* Renumber columns */
		walk = (Column *) tree->columns;
		index = 0;
		while (walk != NULL) {
		    walk->index = index++;
		    walk = walk->next;
		}

		if (columnTree != NULL)
		    tree->columnTree = columnTree->index;
	    }
	    if (move->visible) {
		/* Must update column widths because of expansion. */
		/* Also update columnTreeLeft. */
		tree->widthOfColumns = -1;
		Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH |
			DINFO_INVALIDATE | DINFO_OUT_OF_DATE);
		/* BUG 784245 */
		Tree_DInfoChanged(tree, DINFO_DRAW_HEADER);
	    }
	    break;
	}

	case COMMAND_NEEDEDWIDTH:
	{
	    Column *column;
	    int width;

	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "column");
		return TCL_ERROR;
	    }
	    if (TreeColumn_FromObj(tree, objv[3], (TreeColumn *) &column, 0) != TCL_OK)
		return TCL_ERROR;
	    /* Update layout if needed */
	    (void) Tree_TotalWidth(tree);
	    width = TreeColumn_WidthOfItems((TreeColumn) column);
	    width = MAX(width, TreeColumn_NeededWidth((TreeColumn) column));
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(width));
	    break;
	}
    }

    return TCL_OK;
}

struct Layout
{
    Tk_Font tkfont;
    Tk_FontMetrics fm;
    int width; /* Provided by caller */
    int height; /* Provided by caller */
    int textLeft;
    int textWidth;
    int bytesThatFit;
    int imageLeft;
    int imageWidth;
    int arrowLeft;
    int arrowWidth;
    int arrowHeight;
};

static void Column_Layout(Column *column, struct Layout *layout)
{
    int i, leftList[3], widthList[3], padList[4], n = 0;
    int iArrow = -1, iImage = -1, iText = -1;
    int left, right;

    for (i = 0; i < 3; i++) leftList[i] = 0;
    for (i = 0; i < 3; i++) widthList[i] = 0;
    for (i = 0; i < 4; i++) padList[i] = 0;

    if (column->arrow != ARROW_NONE) {
	layout->arrowWidth = Tree_HeaderHeight(column->tree) / 2;
	if (!(layout->arrowWidth & 1))
	    layout->arrowWidth--;
	layout->arrowHeight = layout->arrowWidth;
    }
    if ((column->arrow != ARROW_NONE) && (column->arrowSide == SIDE_LEFT)) {
	widthList[n] = layout->arrowWidth;
	padList[n] = column->arrowPad[PAD_TOP_LEFT];
	padList[n + 1] = column->arrowPad[PAD_BOTTOM_RIGHT];
	iArrow = n++;
    }
    if ((column->image != NULL) || (column->bitmap != None)) {
	int imgWidth, imgHeight;
	if (column->image != NULL)
	    Tk_SizeOfImage(column->image, &imgWidth, &imgHeight);
	else
	    Tk_SizeOfBitmap(column->tree->display, column->bitmap, &imgWidth, &imgHeight);
	padList[n] = MAX(column->imagePadX[PAD_TOP_LEFT], padList[n]);
	padList[n + 1] = column->imagePadX[PAD_BOTTOM_RIGHT];
	widthList[n] = imgWidth;
	layout->imageWidth = imgWidth;
	iImage = n++;
    }
    if (column->textLen > 0) {
	layout->tkfont = column->tkfont ? column->tkfont : column->tree->tkfont;
	Tk_GetFontMetrics(layout->tkfont, &layout->fm);
	layout->bytesThatFit = 0;
	if (layout->width >= TreeColumn_NeededWidth((TreeColumn) column)) {
	    padList[n] = MAX(column->textPadX[PAD_TOP_LEFT],
		    padList[n]);
	    padList[n + 1] = column->textPadX[PAD_BOTTOM_RIGHT];
	    widthList[n] = column->textWidth;
	    iText = n++;
	    layout->bytesThatFit = column->textLen;
	    layout->textWidth = column->textWidth;
	} else {
	    int width = column->neededWidth - layout->width;
	    if (width < column->textWidth) {
		width = column->textWidth - width;
		layout->bytesThatFit = Ellipsis(layout->tkfont, column->text,
			column->textLen, &width, "...");
		padList[n] = MAX(column->textPadX[PAD_TOP_LEFT],
			padList[n]);
		padList[n + 1] = column->textPadX[PAD_BOTTOM_RIGHT];
		widthList[n] = width;
		iText = n++;
		layout->textWidth = width;
	    }
	}
    }
    if ((column->arrow != ARROW_NONE) && (column->arrowSide == SIDE_RIGHT)) {
	widthList[n] = layout->arrowWidth;
	padList[n] = column->arrowPad[PAD_TOP_LEFT];
	padList[n + 1] = column->arrowPad[PAD_BOTTOM_RIGHT];
	iArrow = n++;
    }

    if (n == 0)
	return;

    if (iText != -1) {
	switch (column->justify) {
	    case TK_JUSTIFY_LEFT:
		leftList[iText] = 0;
		break;
	    case TK_JUSTIFY_RIGHT:
		leftList[iText] = layout->width;
		break;
	    case TK_JUSTIFY_CENTER:
		if (iImage == -1)
		    leftList[iText] = (layout->width - widthList[iText]) / 2;
		else
		    leftList[iText] = (layout->width - widthList[iImage] -
			    padList[iText] - widthList[iText]) / 2 + widthList[iImage] +
			padList[iText];
		break;
	}
    }

    if (iImage != -1) {
	switch (column->justify) {
	    case TK_JUSTIFY_LEFT:
		leftList[iImage] = 0;
		break;
	    case TK_JUSTIFY_RIGHT:
		leftList[iImage] = layout->width;
		break;
	    case TK_JUSTIFY_CENTER:
		if (iText == -1)
		    leftList[iImage] = (layout->width - widthList[iImage]) / 2;
		else
		    leftList[iImage] = (layout->width - widthList[iImage] -
			    padList[iText] - widthList[iText]) / 2;
		break;
	}
    }

    if (iArrow == -1)
	goto finish;

    switch (column->justify) {
	case TK_JUSTIFY_LEFT:
	    switch (column->arrowSide) {
		case SIDE_LEFT:
		    leftList[iArrow] = 0;
		    break;
		case SIDE_RIGHT:
		    switch (column->arrowGravity) {
			case SIDE_LEFT:
			    leftList[iArrow] = 0;
			    break;
			case SIDE_RIGHT:
			    leftList[iArrow] = layout->width;
			    break;
		    }
		    break;
	    }
	    break;
	case TK_JUSTIFY_RIGHT:
	    switch (column->arrowSide) {
		case SIDE_LEFT:
		    switch (column->arrowGravity) {
			case SIDE_LEFT:
			    leftList[iArrow] = 0;
			    break;
			case SIDE_RIGHT:
			    leftList[iArrow] = layout->width;
			    break;
		    }
		    break;
		case SIDE_RIGHT:
		    leftList[iArrow] = layout->width;
		    break;
	    }
	    break;
	case TK_JUSTIFY_CENTER:
	    switch (column->arrowSide) {
		case SIDE_LEFT:
		    switch (column->arrowGravity) {
			case SIDE_LEFT:
			    leftList[iArrow] = 0;
			    break;
			case SIDE_RIGHT:
			    if (n == 3)
				leftList[iArrow] =
				    (layout->width - widthList[1] - padList[2] -
					    widthList[2]) / 2 - padList[1] - widthList[0];
			    else if (n == 2)
				leftList[iArrow] =
				    (layout->width - widthList[1]) / 2 -
				    padList[1] - widthList[0];
			    else
				leftList[iArrow] =
				    (layout->width - widthList[0]) / 2;
			    break;
		    }
		    break;
		case SIDE_RIGHT:
		    switch (column->arrowGravity) {
			case SIDE_LEFT:
			    if (n == 3)
				leftList[iArrow] =
				    (layout->width - widthList[0] - padList[1] -
					    widthList[1]) / 2 + widthList[0] + padList[1] +
				    widthList[1] + padList[2];
			    else if (n == 2)
				leftList[iArrow] =
				    (layout->width - widthList[0]) / 2 +
				    widthList[0] + padList[1];
			    else
				leftList[iArrow] =
				    (layout->width - widthList[0]) / 2;
			    break;
			case SIDE_RIGHT:
			    leftList[iArrow] = layout->width;
			    break;
		    }
		    break;
	    }
	    break;
    }

    finish:
    right = layout->width - padList[n];
    for (i = n - 1; i >= 0; i--) {
	if (leftList[i] + widthList[i] > right)
	    leftList[i] = right - widthList[i];
	right -= widthList[i] + padList[i];
    }
    left = padList[0];
    for (i = 0; i < n; i++) {
	if (leftList[i] < left)
	    leftList[i] = left;

	if (i == iArrow)
	    layout->arrowLeft = leftList[i];
	else if (i == iText)
	    layout->textLeft = leftList[i];
	else if (i == iImage)
	    layout->imageLeft = leftList[i];

	left += widthList[i] + padList[i + 1];
    }
}

void TreeColumn_Draw(TreeColumn column_, Drawable drawable, int x, int y)
{
    Column *column = (Column *) column_;
    TreeCtrl *tree = column->tree;
    int height = tree->headerHeight;
    struct Layout layout;
    int width = column->useWidth;
    int relief = column->sunken ? TK_RELIEF_SUNKEN : TK_RELIEF_RAISED;

    layout.width = width;
    layout.height = height;
    Column_Layout(column, &layout);

    Tk_Fill3DRectangle(tree->tkwin, drawable, column->border,
	    x, y, width, height, 0, TK_RELIEF_FLAT /* column->borderWidth, relief */);

    if (column->image != NULL) {
	int imgW, imgH, ix, iy, h;
	Tk_SizeOfImage(column->image, &imgW, &imgH);
	ix = x + layout.imageLeft + column->sunken;
	h = column->imagePadY[PAD_TOP_LEFT] + imgH
	    + column->imagePadY[PAD_BOTTOM_RIGHT];
	iy = y + (height - h) / 2 + column->sunken;
	iy += column->imagePadY[PAD_TOP_LEFT];
	Tk_RedrawImage(column->image, 0, 0, imgW, imgH, drawable, ix, iy);
    } else if (column->bitmap != None) {
	int imgW, imgH, bx, by, h;

	Tk_SizeOfBitmap(tree->display, column->bitmap, &imgW, &imgH);
	bx = x + layout.imageLeft + column->sunken;
	h = column->imagePadY[PAD_TOP_LEFT] + imgH
	    + column->imagePadY[PAD_BOTTOM_RIGHT];
	by = y + (height - h) / 2 + column->sunken;
	by += column->imagePadY[PAD_TOP_LEFT];
	XSetClipOrigin(tree->display, column->bitmapGC, bx, by);
	XCopyPlane(tree->display, column->bitmap, drawable, column->bitmapGC,
		0, 0, (unsigned int) imgW, (unsigned int) imgH,
		bx, by, 1);
	XSetClipOrigin(tree->display, column->bitmapGC, 0, 0);
    }

    if ((column->text != NULL) && (layout.bytesThatFit > 0)) {
	XGCValues gcValues;
	GC gc;
	unsigned long mask;
	char staticStr[256], *text = staticStr;
	int textLen = column->textLen;
	char *ellipsis = "...";
	int ellipsisLen = strlen(ellipsis);
	int tx, ty, h;

	if (textLen + ellipsisLen > sizeof(staticStr))
	    text = ckalloc(textLen + ellipsisLen);
	memcpy(text, column->text, textLen);
	if (layout.bytesThatFit != textLen) {
	    textLen = abs(layout.bytesThatFit);
	    if (layout.bytesThatFit > 0) {
		memcpy(text + layout.bytesThatFit, ellipsis, ellipsisLen);
		textLen += ellipsisLen;
	    }
	}

	gcValues.font = Tk_FontId(layout.tkfont);
	gcValues.foreground = column->textColor->pixel;
	gcValues.graphics_exposures = False;
	mask = GCFont | GCForeground | GCGraphicsExposures;
	gc = Tk_GetGC(tree->tkwin, mask, &gcValues);
	tx = x + layout.textLeft + column->sunken;
	h = column->textPadY[PAD_TOP_LEFT] + layout.fm.linespace
	    + column->textPadY[PAD_BOTTOM_RIGHT];
	ty = y + (height - h) / 2 + layout.fm.ascent + column->sunken;
	ty += column->textPadY[PAD_TOP_LEFT];
	Tk_DrawChars(tree->display, drawable, gc,
		layout.tkfont, text, textLen, tx, ty);
	Tk_FreeGC(tree->display, gc);
	if (text != staticStr)
	    ckfree(text);
    }

    if (column->arrow != ARROW_NONE) {
	int arrowWidth = layout.arrowWidth;
	int arrowHeight = layout.arrowHeight;
	int arrowTop = y + (height - arrowHeight) / 2;
	int arrowBottom = arrowTop + arrowHeight;
	XPoint points[5];
	int color1 = 0, color2 = 0;
	int i;

	switch (column->arrow) {
	    case ARROW_UP:
		points[0].x = x + layout.arrowLeft;
		points[0].y = arrowBottom - 1;
		points[1].x = x + layout.arrowLeft + arrowWidth / 2;
		points[1].y = arrowTop - 1;
		color1 = TK_3D_DARK_GC;
		points[4].x = x + layout.arrowLeft + arrowWidth / 2;
		points[4].y = arrowTop - 1;
		points[3].x = x + layout.arrowLeft + arrowWidth - 1;
		points[3].y = arrowBottom - 1;
		points[2].x = x + layout.arrowLeft;
		points[2].y = arrowBottom - 1;
		color2 = TK_3D_LIGHT_GC;
		break;
	    case ARROW_DOWN:
		points[0].x = x + layout.arrowLeft + arrowWidth - 1;
		points[0].y = arrowTop;
		points[1].x = x + layout.arrowLeft + arrowWidth / 2;
		points[1].y = arrowBottom;
		color1 = TK_3D_LIGHT_GC;
		points[2].x = x + layout.arrowLeft + arrowWidth - 1;
		points[2].y = arrowTop;
		points[3].x = x + layout.arrowLeft;
		points[3].y = arrowTop;
		points[4].x = x + layout.arrowLeft + arrowWidth / 2;
		points[4].y = arrowBottom;
		color2 = TK_3D_DARK_GC;
		break;
	}
	for (i = 0; i < 5; i++) {
	    points[i].x += column->sunken;
	    points[i].y += column->sunken;
	}
	XDrawLines(tree->display, drawable,
		Tk_3DBorderGC(tree->tkwin, column->border, color2),
		points + 2, 3, CoordModeOrigin);
	XDrawLines(tree->display, drawable,
		Tk_3DBorderGC(tree->tkwin, column->border, color1),
		points, 2, CoordModeOrigin);
    }

    Tk_Draw3DRectangle(tree->tkwin, drawable, column->border,
	    x, y, width, height, column->borderWidth, relief);
}

void Tree_DrawHeader(TreeCtrl *tree, Drawable drawable, int x, int y)
{
    Column *column = (Column *) tree->columns;
    Tk_Window tkwin = tree->tkwin;
    int minX, maxX, width, height;
    Drawable pixmap;

    /* Update layout if needed */
    (void) Tree_HeaderHeight(tree);
    (void) Tree_WidthOfColumns(tree);

    minX = tree->inset;
    maxX = Tk_Width(tkwin) - tree->inset;

    if (tree->doubleBuffer == DOUBLEBUFFER_ITEM)
	pixmap = Tk_GetPixmap(tree->display, Tk_WindowId(tkwin),
		Tk_Width(tkwin), tree->inset + tree->headerHeight, Tk_Depth(tkwin));
    else
	pixmap = drawable;

    while (column != NULL) {
	if (column->visible) {
	    if ((x < maxX) && (x + column->useWidth > minX))
		TreeColumn_Draw((TreeColumn) column, pixmap, x, y);
	    x += column->useWidth;
	}
	column = column->next;
    }

    /* Draw "tail" column */
    if (x < maxX) {
	column = (Column *) tree->columnTail;
	width = maxX - x + column->borderWidth;
	height = tree->headerHeight;
	Tk_Fill3DRectangle(tkwin, pixmap, column->border,
		x, y, width, height, column->borderWidth, column->relief);
	Tk_Draw3DRectangle(tkwin, pixmap, column->border,
		x, y, width, height, column->borderWidth, column->relief);
    }

    if (tree->doubleBuffer == DOUBLEBUFFER_ITEM) {
	XCopyArea(tree->display, pixmap, drawable,
		tree->copyGC, minX, y,
		maxX - minX, tree->headerHeight,
		tree->inset, y);

	Tk_FreePixmap(tree->display, pixmap);
    }
}

/* Calculate the maximum needed width of all ReallyVisible TreeItemColumns */
int TreeColumn_WidthOfItems(TreeColumn column_)
{
    Column *column = (Column *) column_;
    TreeCtrl *tree = column->tree;
    TreeItem item;
    TreeItemColumn itemColumn;
    int width;

    if (column->widthOfItems >= 0)
	return column->widthOfItems;

    column->widthOfItems = 0;
    item = tree->root;
    if (!TreeItem_ReallyVisible(tree, item))
	item = TreeItem_NextVisible(tree, item);
    while (item != NULL) {
	itemColumn = TreeItem_FindColumn(tree, item, column->index);
	if (itemColumn != NULL) {
	    width = TreeItemColumn_NeededWidth(tree, item, itemColumn);
	    if (column->index == tree->columnTree)
		width += TreeItem_Indent(tree, item);
	    column->widthOfItems = MAX(column->widthOfItems, width);
	}
	item = TreeItem_NextVisible(tree, item);
    }

    return column->widthOfItems;
}

/* Set useWidth for all columns */
void Tree_LayoutColumns(TreeCtrl *tree)
{
    Column *column = (Column *) tree->columns;
    int width, visWidth, totalWidth = 0;
    int numExpand = 0;

    while (column != NULL) {
	if (column->visible) {
	    if (column->widthObj != NULL)
		width = column->width;
	    else {
		width = TreeColumn_WidthOfItems((TreeColumn) column);
		width = MAX(width, TreeColumn_NeededWidth((TreeColumn) column));
		width = MAX(width, TreeColumn_MinWidth((TreeColumn) column));
		if (column->expand)
		    numExpand++;
	    }
	    column->useWidth = width;
	    totalWidth += width;
	} else
	    column->useWidth = 0;
	column = column->next;
    }

    visWidth = Tk_Width(tree->tkwin) - tree->inset * 2;
    if ((visWidth > totalWidth) && (numExpand > 0)) {
	int extraWidth = (visWidth - totalWidth) / numExpand;
	int fudge = (visWidth - totalWidth) - extraWidth * numExpand;
	int seen = 0;

	column = (Column *) tree->columns;
	while (column != NULL) {
	    if (column->visible && column->expand && (column->widthObj == NULL)) {
		column->useWidth += extraWidth;
		if (++seen == numExpand) {
		    column->useWidth += fudge;
		    break;
		}
	    }
	    column = column->next;
	}
    }
}

void Tree_InvalidateColumnWidth(TreeCtrl *tree, int columnIndex)
{
    Column *column;

    if (columnIndex == -1) {
	column = (Column *) tree->columns;
	while (column != NULL) {
	    column->widthOfItems = -1;
	    column = column->next;
	}
    } else {
	column = (Column *) Tree_FindColumn(tree, columnIndex);
	if (column != NULL)
	    column->widthOfItems = -1;
    }
    tree->widthOfColumns = -1;
    Tree_DInfoChanged(tree, DINFO_CHECK_COLUMN_WIDTH);
}

void TreeColumn_TreeChanged(TreeCtrl *tree, int flagT)
{
    Column *column;

    /* Column widths are invalidated elsewhere */
    if (flagT & TREE_CONF_FONT) {
	column = (Column *) tree->columns;
	while (column != NULL) {
	    if ((column->tkfont == NULL) && (column->textLen > 0)) {
		column->textWidth = Tk_TextWidth(tree->tkfont, column->text, column->textLen);
		column->neededHeight = -1;
	    }
	    column = column->next;
	}
	tree->headerHeight = -1;
    }
}

int Tree_HeaderHeight(TreeCtrl *tree)
{
    Column *column;
    int height;

    if (!tree->showHeader)
	return 0;

    if (tree->headerHeight >= 0)
	return tree->headerHeight;

    height = 0;
    column = (Column *) tree->columns;
    while (column != NULL) {
	if (column->visible)
	    height = MAX(height, TreeColumn_NeededHeight((TreeColumn) column));
	column = column->next;
    }
    return tree->headerHeight = height;
}

int Tree_WidthOfColumns(TreeCtrl *tree)
{
    Column *column;
    int width;

    if (tree->widthOfColumns >= 0)
	return tree->widthOfColumns;

    Tree_LayoutColumns(tree);

    tree->columnTreeLeft = 0;
    tree->columnTreeVis = FALSE;
    tree->columnVis = NULL;
    tree->columnCountVis = 0;
    width = 0;
    column = (Column *) tree->columns;
    while (column != NULL) {
	if (column->visible) {
	    if (tree->columnVis == NULL)
		tree->columnVis = (TreeColumn) column;
	    tree->columnCountVis++;
	    if (column->index == tree->columnTree) {
		tree->columnTreeLeft = width;
		tree->columnTreeVis = TRUE;
	    }
	    width += column->useWidth;
	}
	column = column->next;
    }

    return tree->widthOfColumns = width;
}

void Tree_InitColumns(TreeCtrl *tree)
{
    Column *column;

    column = Column_Alloc(tree);
    column->index = -1;
    column->tag = ckalloc(5);
    strcpy(column->tag, "tail");
    tree->columnTail = (TreeColumn) column;
    tree->columnCount = 0;
}

void Tree_FreeColumns(TreeCtrl *tree)
{
    Column *column = (Column *) tree->columns;

    while (column != NULL) {
	column = Column_Free(column);
    }

    Column_Free((Column *) tree->columnTail);
    tree->columnCount = 0;
}

