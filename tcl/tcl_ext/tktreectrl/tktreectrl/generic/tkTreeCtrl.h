/* 
 * tkTreeCtrl.c --
 *
 *	This module is the header for treectrl widgets for the Tk toolkit.
 *
 * Copyright (c) 2002-2003 Christian Krone
 * Copyright (c) 2003 ActiveState Corporation
 *
 * RCS: @(#) $Id: tkTreeCtrl.h,v 1.12 2003/11/25 22:12:27 hobbs2 Exp $
 */

#include "tkPort.h"
#include "default.h"
#include "tkInt.h"
#include "qebind.h"

#ifdef HAVE_DBWIN_H
#include "dbwin.h"
#else /* HAVE_DBWIN_H */
#define dbwin printf
#endif /* HAVE_DBWIN_H */

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define INCREMENTS

typedef struct TreeCtrl TreeCtrl;
typedef struct TreeColumn_ *TreeColumn;
typedef struct TreeDInfo_ *TreeDInfo;
typedef struct TreeDragImage_ *TreeDragImage;
typedef struct TreeItem_ *TreeItem;
typedef struct TreeItemColumn_ *TreeItemColumn;
typedef struct TreeItemDInfo_ *TreeItemDInfo;
typedef struct TreeMarquee_ *TreeMarquee;
typedef struct TreeItemRInfo_ *TreeItemRInfo;
typedef struct TreeStyle_ *TreeStyle;
typedef struct TreeElement_ *TreeElement;

enum { LEFT, TOP, RIGHT, BOTTOM };

struct TreeCtrlDebug
{
    Tk_OptionTable optionTable;
    int enable;			/* Turn all debugging on/off */
    int data;			/* Debug data structures */
    int display;		/* Debug display routines */
    int displayDelay;		/* Delay between copy/draw operations */
    XColor *eraseColor;		/* Erase "invalidated" areas */
    GC gcErase;			/* for eraseColor */
};

struct TreeCtrl
{
    /* Standard stuff */
    Tk_Window tkwin;
    Display *display;
    Tcl_Interp *interp;
    Tcl_Command widgetCmd;
    Tk_OptionTable optionTable;

    /* Configuration options */
    Tcl_Obj *fgObj;		/* -foreground */
    XColor *fgColorPtr;		/* -foreground */
    int borderWidth;		/* -borderwidth */
    Tk_3DBorder border;		/* -background */
    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    int relief;			/* -relief */
    int highlightWidth;		/* -highlightthickness */
    XColor *highlightBgColorPtr; /* -highlightbackground */
    XColor *highlightColorPtr;	/* -highlightcolor */
    char *xScrollCmd;		/* -xscrollcommand */
    char *yScrollCmd;		/* -yscrollcommand */
    Tcl_Obj *xScrollDelay;	/* -xscrolldelay: used by scripts */
    Tcl_Obj *yScrollDelay;	/* -yscrolldelay: used by scripts */
    int xScrollIncrement;	/* -xscrollincrement */
    int yScrollIncrement;	/* -yscrollincrement */
    Tcl_Obj *scrollMargin;		/* -scrollmargin: used by scripts */
    char *takeFocus;		/* -takfocus */
    Tcl_Obj *fontObj;		/* -font */
    Tk_Font tkfont;		/* -font */
    int showButtons;		/* boolean: Draw expand/collapse buttons */
    int showLines;		/* boolean: Draw lines connecting parent to
				 * child */
    int showRoot;		/* boolean: Draw the unique root item */
    int showRootButton;		/* boolean: Draw expand/collapse button for
				 * root item */
    int showHeader;		/* boolean: show column titles */
    Tcl_Obj *indentObj;		/* pixels: offset of child relative to
				 * parent */
    int indent;			/* pixels: offset of child relative to
				 * parent */
    char *selectMode;		/* -selectmode: used by scripts only */
    Tcl_Obj *itemHeightObj;	/* -itemheight: Fixed height for all items
                                    (unless overridden) */
    int itemHeight;		/* -itemheight: Fixed height for all items
                                    (unless overridden) */
    int width;			/* -width */
    int height;			/* -height */
    int columnTree;		/* column where buttons/lines are drawn */
#define DOUBLEBUFFER_NONE 0
#define DOUBLEBUFFER_ITEM 1
#define DOUBLEBUFFER_WINDOW 2
    int doubleBuffer;		/* -doublebuffer */
    XColor *buttonColor;	/* -buttoncolor */
    Tcl_Obj *buttonSizeObj;	/* -buttonSize */
    int buttonSize;		/* -buttonsize */
    Tcl_Obj *buttonThicknessObj;/* -buttonthickness */
    int buttonThickness;	/* -buttonthickness */
    XColor *lineColor;		/* -linecolor */
    Tcl_Obj *lineThicknessObj;	/* -linethickness */
    int lineThickness;		/* -linethickness */
#define LINE_STYLE_DOT 0
#define LINE_STYLE_SOLID 1
    int lineStyle;		/* -linestyle */
    int vertical;		/* -orient */
    Tcl_Obj *wrapObj;		/* -wrap */
    char *openButtonString;	/* -openbuttonimage */
    char *closedButtonString;	/* -closedbuttonimage */
    Pixmap openButtonBitmap;	/* -openbuttonbitmap */
    Pixmap closedButtonBitmap;	/* -closedbuttonbitmap */
    int useIndent;		/* MAX of open/closed button width and
				 * indent */
#define BG_MODE_COLUMN 0
#define BG_MODE_INDEX 1
#define BG_MODE_ROW 2
#define BG_MODE_VISINDEX 3
    int backgroundMode;		/* -backgroundmode */
    int *itemPadX;		/* -itempadx */
    Tcl_Obj *itemPadXObj;	/* -itempadx */
    int *itemPadY;		/* -itempady */
    Tcl_Obj *itemPadYObj;	/* -itempady */

    struct TreeCtrlDebug debug;

    /* Other stuff */
    int gotFocus;		/* flag */
    int deleted;		/* flag */
    int updateIndex;		/* flag */
    int inset;			/* borderWidth + highlightWidth */
    int xOrigin;		/* offset from content x=0 to window x=0 */
    int yOrigin;		/* offset from content y=0 to window y=0 */
    GC copyGC;
    GC textGC;
    GC buttonGC;
    GC lineGC;
    Tk_Image openButtonImage;	/* -openbuttonimage */
    Tk_Image closedButtonImage;	/* -closedbuttonimage */
    GC buttonOpenGC;
    GC buttonClosedGC;
    int closedButtonWidth;
    int closedButtonHeight;
    int openButtonWidth;
    int openButtonHeight;
    int prevWidth;
    int prevHeight;
    int drawableXOrigin;
    int drawableYOrigin;

    TreeColumn columns;		/* List of columns */
    TreeColumn columnTail;	/* Last infinitely-wide column */
    TreeColumn columnVis;	/* First visible column */
    int columnCount;		/* Number of columns */
    int columnCountVis;		/* Number of visible columns */
    int headerHeight;		/* Height of column titles */
    int widthOfColumns;		/* Sum of column widths */
    int columnTreeLeft;		/* left of column where buttons/lines are
				 * drawn */
    int columnTreeVis;		/* TRUE if columnTree is visible */
    int columnBgCnt;		/* Max -itembackground colors */

    TreeItem root;
    TreeItem activeItem;
    TreeItem anchorItem;
    int nextItemId;
    Tcl_HashTable itemHash;	/* TreeItem.id -> TreeItem */
    Tcl_HashTable elementHash;	/* Element.name -> Element */
    Tcl_HashTable styleHash;	/* Style.name -> Style */
    Tcl_HashTable imageHash;	/* image name -> Tk_Image */
    int depth;			/* max depth of items under root */
    int itemCount;		/* Total number of items */
    int itemVisCount;		/* Total number of ReallyVisible() items */
    QE_BindingTable bindingTable;
    TreeDragImage dragImage;
    TreeMarquee marquee;
    TreeDInfo dInfo;
    int selectCount;		/* Number of selected items */

#define TREE_WRAP_NONE 0
#define TREE_WRAP_ITEMS 1
#define TREE_WRAP_PIXELS 2
#define TREE_WRAP_WINDOW 3
    int wrapMode;		/* TREE_WRAP_xxx */
    int wrapArg;		/* Number of items, number of pixels */

    int totalWidth;		/* Max/Sum of all TreeRanges */
    int totalHeight;		/* Max/Sum of all TreeRanges */

    struct {
	Tcl_Obj *xObj;
	int x;		/* Window coords */
	int sx;		/* Window coords */
	int onScreen;
    } columnProxy;

    char *stateNames[32];	/* Names of static and dynamic item states */

    int scanX;
    int scanY;
    int scanXOrigin;
    int scanYOrigin;
};

#define TREE_CONF_FONT 0x0001
#define TREE_CONF_ITEMHEIGHT 0x0002
#define TREE_CONF_INDENT 0x0004
#define TREE_CONF_WRAP 0x0008
#define TREE_CONF_BUTIMG_CLOSED 0x0010
#define TREE_CONF_BUTIMG_OPEN 0x0020
#define TREE_CONF_BUTBMP_CLOSED 0x0040
#define TREE_CONF_BUTBMP_OPEN 0x0080
#define TREE_CONF_RELAYOUT 0x0100
#define TREE_CONF_REDISPLAY 0x0200
#define TREE_CONF_FG 0x0400
#define TREE_CONF_PROXY 0x0800
#define TREE_CONF_BUTTON 0x1000
#define TREE_CONF_LINE 0x2000

extern void Tree_AddItem(TreeCtrl *tree, TreeItem item);
extern void Tree_RemoveItem(TreeCtrl *tree, TreeItem item);
extern Tk_Image Tree_GetImage(TreeCtrl *tree, char *imageName);
extern void Tree_UpdateScrollbarX(TreeCtrl *tree);
extern void Tree_UpdateScrollbarY(TreeCtrl *tree);
extern void Tree_AddToSelection(TreeCtrl *tree, TreeItem item);
extern void Tree_RemoveFromSelection(TreeCtrl *tree, TreeItem item);

#define STATE_OP_ON	0
#define STATE_OP_OFF	1
#define STATE_OP_TOGGLE	2
#define SFO_NOT_OFF	0x0001
#define SFO_NOT_TOGGLE	0x0002
#define SFO_NOT_STATIC	0x0004
extern int StateFromObj(TreeCtrl *tree, Tcl_Obj *obj, int states[3], int *indexPtr, int flags);

/* tkTreeItem.c */

#define ITEM_ALL ((TreeItem) -1)
#define IFO_ALLOK	0x0001	/* ItemFromObj flag: "all" is acceptable */
#define IFO_NULLOK	0x0002	/* ItemFromObj flag: can be NULL */
#define IFO_NOTROOT	0x0004	/* ItemFromObj flag: "root" is forbidden */
extern int TreeItem_FromObj(TreeCtrl *tree, Tcl_Obj *objPtr, TreeItem *itemPtr, int flags);

extern void FormatResult(Tcl_Interp *interp, char *fmt, ...);
extern void Tree_Debug(TreeCtrl *tree);

extern TreeItem TreeItem_Alloc(TreeCtrl *tree);
extern TreeItem TreeItem_AllocRoot(TreeCtrl *tree);
extern int TreeItem_Debug(TreeCtrl *tree, TreeItem item);
extern void TreeItem_OpenClose(TreeCtrl *tree, TreeItem item, int mode, int recurse);

#define STATE_OPEN	0x0001
#define STATE_SELECTED	0x0002
#define STATE_ENABLED	0x0004
#define STATE_ACTIVE	0x0008
#define STATE_FOCUS	0x0010
#define STATE_USER	6		/* first user bit */
extern int TreeItem_GetState(TreeCtrl *tree, TreeItem item_);

#define CS_DISPLAY 0x01
#define CS_LAYOUT 0x02
extern int TreeItem_ChangeState(TreeCtrl *tree, TreeItem item_, int stateOff, int stateOn);

extern void TreeItem_Undefine(TreeCtrl *tree, TreeItem item_, int state);

extern int TreeItem_GetButton(TreeCtrl *tree, TreeItem item_);
extern int TreeItem_SetButton(TreeCtrl *tree, TreeItem item, int hasButton);
extern int TreeItem_GetDepth(TreeCtrl *tree, TreeItem item_);
extern int TreeItem_SetDepth(TreeCtrl *tree, TreeItem item, int depth);
extern int TreeItem_GetID(TreeCtrl *tree, TreeItem item_);
extern int TreeItem_SetID(TreeCtrl *tree, TreeItem item_, int id);
extern int TreeItem_GetOpen(TreeCtrl *tree, TreeItem item_);
extern int TreeItem_SetOpen(TreeCtrl *tree, TreeItem item, int isOpen);
extern int TreeItem_GetSelected(TreeCtrl *tree, TreeItem item_);
extern int TreeItem_SetSelected(TreeCtrl *tree, TreeItem item, int isSelected);
extern TreeItem TreeItem_SetFirstChild(TreeCtrl *tree, TreeItem item, TreeItem firstChild);
extern TreeItem TreeItem_GetFirstChild(TreeCtrl *tree, TreeItem item);
extern TreeItem TreeItem_SetLastChild(TreeCtrl *tree, TreeItem item, TreeItem lastChild);
extern TreeItem TreeItem_GetLastChild(TreeCtrl *tree, TreeItem item);
extern TreeItem TreeItem_SetParent(TreeCtrl *tree, TreeItem item, TreeItem parent);
extern TreeItem TreeItem_GetParent(TreeCtrl *tree, TreeItem item);
extern TreeItem TreeItem_SetNextSibling(TreeCtrl *tree, TreeItem item, TreeItem nextSibling);
extern TreeItem TreeItem_GetNextSibling(TreeCtrl *tree, TreeItem item);
extern TreeItem TreeItem_SetPrevSibling(TreeCtrl *tree, TreeItem item, TreeItem prevSibling);
extern TreeItem TreeItem_GetPrevSibling(TreeCtrl *tree, TreeItem item);
extern void TreeItem_SetDInfo(TreeCtrl *tree, TreeItem item, TreeItemDInfo dInfo);
extern TreeItemDInfo TreeItem_GetDInfo(TreeCtrl *tree, TreeItem item);
extern void TreeItem_SetRInfo(TreeCtrl *tree, TreeItem item, TreeItemRInfo rInfo);
extern TreeItemRInfo TreeItem_GetRInfo(TreeCtrl *tree, TreeItem item);

extern void TreeItem_AppendChild(TreeCtrl *tree, TreeItem self, TreeItem child);
extern void TreeItem_RemoveFromParent(TreeCtrl *tree, TreeItem self);
extern int TreeItem_Height(TreeCtrl *tree, TreeItem self);
extern int TreeItem_TotalHeight(TreeCtrl *tree, TreeItem self);
extern void TreeItem_InvalidateHeight(TreeCtrl *tree, TreeItem self);
extern void TreeItem_Draw(TreeCtrl *tree, TreeItem self, int x, int y, int width, int height, Drawable drawable, int minX, int maxX, int index);
extern void TreeItem_DrawLines(TreeCtrl *tree, TreeItem self, int x, int y, int width, int height, Drawable drawable);
extern void TreeItem_DrawButton(TreeCtrl *tree, TreeItem self, int x, int y, int width, int height, Drawable drawable);
extern int TreeItem_ReallyVisible(TreeCtrl *tree, TreeItem self);
extern void TreeItem_FreeResources(TreeCtrl *tree, TreeItem self);
extern TreeItem TreeItem_RootAncestor(TreeCtrl *tree, TreeItem item_);
extern int TreeItem_IsAncestor(TreeCtrl *tree, TreeItem item1, TreeItem item2);
extern Tcl_Obj *TreeItem_ToObj(TreeCtrl *tree, TreeItem item);
extern void TreeItem_ToIndex(TreeCtrl *tree, TreeItem item, int *absolute, int *visible);
extern TreeItem TreeItem_Next(TreeCtrl *tree, TreeItem item);
extern TreeItem TreeItem_NextVisible(TreeCtrl *tree, TreeItem item);
extern TreeItem TreeItem_Prev(TreeCtrl *tree, TreeItem item);
extern TreeItem TreeItem_PrevVisible(TreeCtrl *tree, TreeItem item);
extern char *TreeItem_Identify(TreeCtrl *tree, TreeItem item_, int x, int y);
extern void TreeItem_Identify2(TreeCtrl *tree, TreeItem item_,
	int x1, int y1, int x2, int y2, Tcl_Obj *listObj);
extern int TreeItem_Indent(TreeCtrl *tree, TreeItem item_);
extern void Tree_UpdateItemIndex(TreeCtrl *tree);
extern int TreeItemCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);

extern TreeItemColumn TreeItem_GetFirstColumn(TreeCtrl *tree, TreeItem item);
extern TreeItemColumn TreeItemColumn_GetNext(TreeCtrl *tree, TreeItemColumn column);
extern void TreeItemColumn_InvalidateSize(TreeCtrl *tree, TreeItemColumn column);
extern TreeStyle TreeItemColumn_GetStyle(TreeCtrl *tree, TreeItemColumn column);
extern void TreeItemColumn_ForgetStyle(TreeCtrl *tree, TreeItemColumn column_);
extern int TreeItemColumn_NeededWidth(TreeCtrl *tree, TreeItem item_, TreeItemColumn column_);
extern TreeItemColumn TreeItem_FindColumn(TreeCtrl *tree, TreeItem item, int columnIndex);
extern int TreeItem_ColumnFromObj(TreeCtrl *tree, TreeItem item, Tcl_Obj *obj, TreeItemColumn *columnPtr, int *indexPtr);
extern void TreeItem_RemoveColumn(TreeCtrl *tree, TreeItem item_, TreeItemColumn column_);
extern void TreeItem_MoveColumn(TreeCtrl *tree, TreeItem item_, int columnIndex, int beforeIndex);

typedef struct StyleDrawArgs StyleDrawArgs;
struct StyleDrawArgs
{
    TreeCtrl *tree;
    TreeStyle style;
    int x;
    int y;
    int width;
    int height;
    Drawable drawable;
    int state;		/* STATE_xxx */
    Tk_Justify justify;
};

/* tkTreeStyle.c */
extern int TreeStyle_Init(Tcl_Interp *interp);
extern int TreeStyle_NeededWidth(TreeCtrl *tree, TreeStyle style_, int state);
extern int TreeStyle_NeededHeight(TreeCtrl *tree, TreeStyle style_, int state);
extern int TreeStyle_UseHeight(StyleDrawArgs *drawArgs);
extern void TreeStyle_Draw(StyleDrawArgs *args);
extern void TreeStyle_FreeResources(TreeCtrl *tree, TreeStyle style_);
extern void TreeStyle_Free(TreeCtrl *tree);
extern int TreeElement_FromObj(TreeCtrl *tree, Tcl_Obj *obj, TreeElement *elemPtr);
extern int TreeElement_IsType(TreeCtrl *tree, TreeElement elem_, CONST char *type);
extern int TreeStyle_FromObj(TreeCtrl *tree, Tcl_Obj *obj, TreeStyle *stylePtr);
extern Tcl_Obj *TreeStyle_ToObj(TreeStyle style_);
extern Tcl_Obj *TreeStyle_GetText(TreeCtrl *tree, TreeStyle style_);
extern void TreeStyle_SetText(TreeCtrl *tree, TreeStyle style_, Tcl_Obj *textObj);
extern int TreeStyle_FindElement(TreeCtrl *tree, TreeStyle style_, TreeElement elem_, int *index);
extern TreeStyle TreeStyle_NewInstance(TreeCtrl *tree, TreeStyle master);
extern int TreeStyle_ElementActual(TreeCtrl *tree, TreeStyle style_, int state, Tcl_Obj *elemObj, Tcl_Obj *obj);
extern int TreeStyle_ElementCget(TreeCtrl *tree, TreeStyle style_, Tcl_Obj *elemObj, Tcl_Obj *obj);
extern int TreeStyle_ElementConfigure(TreeCtrl *tree, TreeStyle style_, Tcl_Obj *elemObj, int objc, Tcl_Obj **objv, int *eMask);
extern void TreeStyle_ListElements(TreeCtrl *tree, TreeStyle style_);
extern TreeStyle TreeStyle_GetMaster(TreeCtrl *tree, TreeStyle style_);
extern char *TreeStyle_Identify(StyleDrawArgs *drawArgs, int x, int y);
extern void TreeStyle_Identify2(StyleDrawArgs *drawArgs,
	int x1, int y1, int x2, int y2, Tcl_Obj *listObj);
extern int TreeStyle_Remap(TreeCtrl *tree, TreeStyle styleFrom_, TreeStyle styleTo_, int objc, Tcl_Obj *CONST objv[]);
extern void TreeStyle_TreeChanged(TreeCtrl *tree, int flagT);
#define SORT_ASCII 0
#define SORT_DICT  1
#define SORT_DOUBLE 2
#define SORT_LONG 3
#define SORT_COMMAND 4
extern int TreeStyle_GetSortData(TreeCtrl *tree, TreeStyle style_, int elemIndex, int type, long *lv, double *dv, char **sv);
extern int TreeStyle_ValidateElements(StyleDrawArgs *drawArgs, int objc, Tcl_Obj *CONST objv[]);
extern int TreeStyle_GetElemRects(StyleDrawArgs *drawArgs, int objc, Tcl_Obj *CONST objv[], XRectangle rects[]);
extern int TreeElementCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int TreeStyleCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int TreeStyle_ChangeState(TreeCtrl *tree, TreeStyle style_, int state1, int state2);
extern void TreeStyle_UndefineState(TreeCtrl *tree, int state);
extern int TreeStyle_NumElements(TreeCtrl *tree, TreeStyle style_);

/* tkTreeNotify.c */
extern int TreeNotify_Init(TreeCtrl *tree);
extern void TreeNotify_OpenClose(TreeCtrl *tree, TreeItem item, int isOpen, int before);
extern void TreeNotify_Selection(TreeCtrl *tree, TreeItem select[], TreeItem deselect[]);
extern int TreeNotifyCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern void TreeNotify_ActiveItem(TreeCtrl *tree, TreeItem itemOld, TreeItem itemNew);
extern void TreeNotify_Scroll(TreeCtrl *tree, double fractions[2], int vertical);

/* tkTreeColumn.c */
extern void Tree_InitColumns(TreeCtrl *tree);
extern TreeColumn Tree_CreateColumn(TreeCtrl *tree, int columnIndex, int *isNew);
extern TreeColumn Tree_FindColumn(TreeCtrl *tree, int columnIndex);
#define CFO_NOT_TAIL 0x01
extern int Tree_FindColumnByTag(TreeCtrl *tree, Tcl_Obj *obj, TreeColumn *columnPtr, int flags);
extern int TreeColumn_FromObj(TreeCtrl *tree, Tcl_Obj *obj, TreeColumn *columnPtr, int flags);
extern int TreeColumnCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
extern int TreeColumn_Index(TreeColumn column_);
extern TreeColumn TreeColumn_Next(TreeColumn column_);
extern int TreeColumn_FixedWidth(TreeColumn column_);
extern int TreeColumn_MinWidth(TreeColumn column_);
extern int TreeColumn_StepWidth(TreeColumn column_);
extern int TreeColumn_NeededWidth(TreeColumn column_);
extern int TreeColumn_UseWidth(TreeColumn column_);
extern void TreeColumn_SetUseWidth(TreeColumn column_, int width);
extern Tk_Justify TreeColumn_Justify(TreeColumn column_);
extern int TreeColumn_WidthHack(TreeColumn column_);
extern int TreeColumn_Visible(TreeColumn column_);
extern GC TreeColumn_BackgroundGC(TreeColumn column_, int which);
extern void TreeColumn_Draw(TreeColumn column_, Drawable drawable, int x, int y);
extern void Tree_DrawHeader(TreeCtrl *tree, Drawable drawable, int x, int y);
extern int TreeColumn_WidthOfItems(TreeColumn column_);
extern void TreeColumn_InvalidateWidth(TreeColumn column_);
extern void TreeColumn_Init(TreeCtrl *tree);
extern void Tree_FreeColumns(TreeCtrl *tree);
extern void Tree_InvalidateColumnWidth(TreeCtrl *tree, int columnIndex);
extern int Tree_HeaderHeight(TreeCtrl *tree);
extern int Tree_WidthOfColumns(TreeCtrl *tree);
extern void TreeColumn_TreeChanged(TreeCtrl *tree, int flagT);

/* tkTreeDrag.c */
extern int TreeDragImage_Init(TreeCtrl *tree);
extern void TreeDragImage_Free(TreeDragImage dragImage_);
extern void TreeDragImage_Display(TreeDragImage dragImage_);
extern void TreeDragImage_Undisplay(TreeDragImage dragImage_);
extern void TreeDragImage_Draw(TreeDragImage dragImage_, Drawable drawable, int x, int y);
extern int DragImageCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);

/* tkTreeMarquee.c */
extern int TreeMarquee_Init(TreeCtrl *tree);
extern void TreeMarquee_Free(TreeMarquee marquee_);
extern void TreeMarquee_Draw(TreeMarquee marquee_, Drawable drawable, int x, int y);
extern void TreeMarquee_Display(TreeMarquee marquee_);
extern void TreeMarquee_Undisplay(TreeMarquee marquee_);
extern int TreeMarqueeCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);

/* tkTreeDisplay.c */
extern int Tree_TotalWidth(TreeCtrl *tree);
extern int Tree_TotalHeight(TreeCtrl *tree);
extern TreeItem Tree_ItemUnderPoint(TreeCtrl *tree, int *x, int *y, int nearest);
extern void Tree_FreeItemRInfo(TreeCtrl *tree, TreeItem item);
extern int Tree_ItemBbox(TreeCtrl *tree, TreeItem item, int *x, int *y, int *w, int *h);
extern TreeItem Tree_ItemAbove(TreeCtrl *tree, TreeItem item);
extern TreeItem Tree_ItemBelow(TreeCtrl *tree, TreeItem item);
extern TreeItem Tree_ItemLeft(TreeCtrl *tree, TreeItem item);
extern TreeItem Tree_ItemRight(TreeCtrl *tree, TreeItem item);
extern TreeItem Tree_ItemTop(TreeCtrl *tree, TreeItem item);
extern TreeItem Tree_ItemBottom(TreeCtrl *tree, TreeItem item);
extern TreeItem Tree_ItemLeftMost(TreeCtrl *tree, TreeItem item);
extern TreeItem Tree_ItemRightMost(TreeCtrl *tree, TreeItem item);
extern int Tree_ItemToRNC(TreeCtrl *tree, TreeItem item, int *row, int *col);
extern TreeItem Tree_RNCToItem(TreeCtrl *tree, int row, int col);

extern void TreeDInfo_Init(TreeCtrl *tree);
extern void TreeDInfo_Free(TreeCtrl *tree);
extern void Tree_EventuallyRedraw(TreeCtrl *tree);
extern void Tree_GetScrollFractionsX(TreeCtrl *tree, double fractions[2]);
extern void Tree_GetScrollFractionsY(TreeCtrl *tree, double fractions[2]);
extern int Increment_FindX(TreeCtrl *tree, int offset);
extern int Increment_FindY(TreeCtrl *tree, int offset);
extern int Increment_ToOffsetX(TreeCtrl *tree, int index);
extern int Increment_ToOffsetY(TreeCtrl *tree, int index);
extern int B_XviewCmd(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);
extern int B_YviewCmd(TreeCtrl *tree, int objc, Tcl_Obj *CONST objv[]);
extern void Tree_SetOriginX(TreeCtrl *tree, int xOrigin);
extern void Tree_SetOriginY(TreeCtrl *tree, int yOrigin);
extern void Tree_RelayoutWindow(TreeCtrl *tree);
extern void Tree_FreeItemDInfo(TreeCtrl *tree, TreeItem item1, TreeItem item2);
extern void Tree_InvalidateItemDInfo(TreeCtrl *tree, TreeItem item1, TreeItem item2);
extern void Tree_InvalidateArea(TreeCtrl *tree, int x1, int y1, int x2, int y2);
extern void Tree_InvalidateItemArea(TreeCtrl *tree, int x1, int y1, int x2, int y2);
extern void Tree_InvalidateRegion(TreeCtrl *tree, TkRegion region);
extern void Tree_InvalidateWindow(TreeCtrl *tree);
extern void Tree_RedrawArea(TreeCtrl *tree, int x1, int y1, int x2, int y2);
extern void Tree_RedrawItemArea(TreeCtrl *tree, int x1, int y1, int x2, int y2);
extern void Tree_FocusChanged(TreeCtrl *tree, int gotFocus);
extern TreeItem *Tree_ItemsInArea(TreeCtrl *tree, int minX, int minY, int maxX, int maxY);
extern void TreeColumnProxy_Undisplay(TreeCtrl *tree);
extern void TreeColumnProxy_Display(TreeCtrl *tree);

#define DINFO_OUT_OF_DATE 0x0001
#define DINFO_CHECK_COLUMN_WIDTH 0x0002
#define DINFO_DRAW_HEADER 0x0004
#define DINFO_SET_ORIGIN_X 0x0008
#define DINFO_UPDATE_SCROLLBAR_X 0x0010
#define DINFO_REDRAW_PENDING 0x00020
#define DINFO_INVALIDATE 0x0040
#define DINFO_DRAW_HIGHLIGHT 0x0080
#define DINFO_DRAW_BORDER 0x0100
#define DINFO_REDO_RANGES 0x0200
#define DINFO_SET_ORIGIN_Y 0x0400
#define DINFO_UPDATE_SCROLLBAR_Y 0x0800
#define DINFO_REDO_INCREMENTS 0x1000
extern void Tree_DInfoChanged(TreeCtrl *tree, int flags);

/* tkTreeUtils.c */
extern void wipefree(char *memPtr, int size);
#define WFREE(p,t) \
	wipefree((char *) p, sizeof(t))
extern int Ellipsis(Tk_Font tkfont, char *string, int numBytes, int *maxPixels, char *ellipsis);
extern void HDotLine(TreeCtrl *tree, Drawable drawable, GC gc, int x1, int y1, int x2);
extern void VDotLine(TreeCtrl *tree, Drawable drawable, GC gc, int x1, int y1, int y2);
extern void DotRect(TreeCtrl *tree, Drawable drawable, int x, int y, int width, int height);
extern void DrawActiveOutline(TreeCtrl *tree, Drawable drawable, int x, int y, int width, int height, int open);
typedef struct DotState
{
    int stuff[10];
} DotState;
extern void DotRect_Setup(TreeCtrl *tree, Drawable drawable, DotState *dotState);
extern void DotRect_Draw(DotState *dotState, int x, int y, int width, int height);
extern void DotRect_Restore(DotState *dotState);
typedef struct TextLayout_ *TextLayout;
extern TextLayout TextLayout_Compute(Tk_Font tkfont, CONST char *string,
	int numChars, int wrapLength, Tk_Justify justify, int maxLines, int flags);
extern void TextLayout_Free(TextLayout textLayout);
extern void TextLayout_Size(TextLayout textLayout, int *widthPtr, int *heightPtr);
extern void TextLayout_Draw(Display *display, Drawable drawable, GC gc,
	TextLayout layout, int x, int y, int firstChar, int lastChar);
extern void Tk_FillRegion(Display *display, Drawable drawable, GC gc, TkRegion rgn);


#define PAD_TOP_LEFT     0
#define PAD_BOTTOM_RIGHT 1
extern Tk_ObjCustomOption PadAmountOption;

extern int       TreeCtrl_GetPadAmountFromObj(Tcl_Interp *interp,
	Tk_Window tkwin, Tcl_Obj *padObj,
	int *topLeftPtr, int *bottomRightPtr);
extern Tcl_Obj * TreeCtrl_NewPadAmountObj(int *padAmounts);
