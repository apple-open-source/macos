#include "tkTreeCtrl.h"

typedef struct DragElem DragElem;
typedef struct DragImage DragImage;

struct DragElem
{
	int x, y, width, height;
	DragElem *next;
};

struct DragImage
{
	TreeCtrl *tree;
	Tk_OptionTable optionTable;
	int visible;
	int x, y; /* offset to draw at in canvas coords */
	int bounds[4]; /* bounds of all DragElems */
	DragElem *elem;
	int onScreen; /* TRUE if is displayed */
	int sx, sy; /* Window coords where displayed */
};

static Tk_OptionSpec optionSpecs[] = {
	{TK_OPTION_BOOLEAN, "-visible", (char *) NULL, (char *) NULL,
		"0", -1, Tk_Offset(DragImage, visible),
		0, (ClientData) NULL, 0},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, 0, 0}
};

static DragElem *DragElem_Alloc(DragImage *dragImage)
{
	DragElem *elem = (DragElem *) ckalloc(sizeof(DragElem));
	DragElem *walk = dragImage->elem;
	memset(elem, '\0', sizeof(DragElem));
	if (dragImage->elem == NULL)
		dragImage->elem = elem;
	else
	{
		while (walk->next != NULL)
			walk = walk->next;
		walk->next = elem;
	}
	return elem;
}

static DragElem *DragElem_Free(DragImage *dragImage, DragElem *elem)
{
	DragElem *next = elem->next;
	WFREE(elem, DragElem);
	return next;
}

int TreeDragImage_Init(TreeCtrl *tree)
{
	DragImage *dragImage;

	dragImage = (DragImage *) ckalloc(sizeof(DragImage));
	memset(dragImage, '\0', sizeof(DragImage));
	dragImage->tree = tree;
	dragImage->optionTable = Tk_CreateOptionTable(tree->interp, optionSpecs);
	if (Tk_InitOptions(tree->interp, (char *) dragImage, dragImage->optionTable,
		tree->tkwin) != TCL_OK)
	{
		WFREE(dragImage, DragImage);
		return TCL_ERROR;
	}
	tree->dragImage = (TreeDragImage) dragImage;
	return TCL_OK;
}

void TreeDragImage_Free(TreeDragImage dragImage_)
{
	DragImage *dragImage = (DragImage *) dragImage_;
	DragElem *elem = dragImage->elem;

	while (elem != NULL)
		elem = DragElem_Free(dragImage, elem);
	Tk_FreeConfigOptions((char *) dragImage, dragImage->optionTable,
		dragImage->tree->tkwin);
	WFREE(dragImage, DragImage);
}

void TreeDragImage_Display(TreeDragImage dragImage_)
{
	DragImage *dragImage = (DragImage *) dragImage_;
	TreeCtrl *tree = dragImage->tree;

	if (!dragImage->onScreen && dragImage->visible)
	{
		dragImage->sx = 0 - tree->xOrigin;
		dragImage->sy = 0 - tree->yOrigin;
		TreeDragImage_Draw(dragImage_, Tk_WindowId(tree->tkwin), dragImage->sx, dragImage->sy);
		dragImage->onScreen = TRUE;
	}
}

void TreeDragImage_Undisplay(TreeDragImage dragImage_)
{
	DragImage *dragImage = (DragImage *) dragImage_;
	TreeCtrl *tree = dragImage->tree;

	if (dragImage->onScreen)
	{
		TreeDragImage_Draw(dragImage_, Tk_WindowId(tree->tkwin), dragImage->sx, dragImage->sy);
		dragImage->onScreen = FALSE;
	}
}

static int DragImage_Config(DragImage *dragImage, int objc, Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = dragImage->tree;
	Tk_SavedOptions savedOptions;
	int mask, result;

	result = Tk_SetOptions(tree->interp, (char *) dragImage, dragImage->optionTable,
		objc, objv, tree->tkwin, &savedOptions, &mask);
	if (result != TCL_OK)
	{
		Tk_RestoreSavedOptions(&savedOptions);
		return TCL_ERROR;
	}
	Tk_FreeSavedOptions(&savedOptions);
#if 0
	if (mask & CONF_VISIBLE)
	{
		if (dragImage->visible)
			TreeDragImage_Display();
		else
			TreeDragImage_Unisplay();
	}
#endif
	return TCL_OK;
}

void TreeDragImage_Draw(TreeDragImage dragImage_, Drawable drawable, int x, int y)
{
	DragImage *dragImage = (DragImage *) dragImage_;
	TreeCtrl *tree = dragImage->tree;
	DragElem *elem = dragImage->elem;
	DotState dotState;

/*	if (!dragImage->visible)
		return; */
	if (elem == NULL)
		return;

	DotRect_Setup(tree, drawable, &dotState);

	while (elem != NULL)
	{
		DotRect_Draw(&dotState,
			x + dragImage->x + elem->x,
			y + dragImage->y + elem->y,
			elem->width, elem->height);
		elem = elem->next;
	}

	DotRect_Restore(&dotState);
}

int DragImageCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	DragImage *dragImage = (DragImage *) tree->dragImage;
	static CONST char *commandNames[] = { "add", "cget", "clear", "configure",
		"offset", "visible", (char *) NULL };
	enum { COMMAND_ADD, COMMAND_CGET, COMMAND_CLEAR, COMMAND_CONFIGURE,
		COMMAND_OFFSET, COMMAND_VISIBLE };
	int index;

	if (objc < 3)
	{
		Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg...?");
		return TCL_ERROR;
	}

	if (Tcl_GetIndexFromObj(interp, objv[2], commandNames, "command", 0,
		&index) != TCL_OK)
	{
		return TCL_ERROR;
	}

	switch (index)
	{
		/* T dragimage add I ?C? ?E ...? */
		case COMMAND_ADD:
		{
#define STATIC_SIZE 20
			XRectangle staticRects[STATIC_SIZE], *rects = staticRects;
			TreeItem item;
			TreeItemColumn itemColumn;
			TreeColumn treeColumn;
			int i, count, columnIndex;
			int indent, width, totalWidth;
			int x, y, w, h;
			DragElem *elem;
			StyleDrawArgs drawArgs;
			int result = TCL_OK;

			if (objc < 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "item ?column? ?element ...?");
				return TCL_ERROR;
			}

			if (TreeItem_FromObj(tree, objv[3], &item, 0) != TCL_OK)
				return TCL_ERROR;

			/* Validate all of the arguments, even if the command would exit
			 * early without needing to check those arguments. */
			if (objc > 4)
			{
				if (TreeItem_ColumnFromObj(tree, item, objv[4], &itemColumn, &columnIndex) != TCL_OK)
					return TCL_ERROR;
				if (objc > 5)
				{
					drawArgs.tree = tree;
					drawArgs.style = TreeItemColumn_GetStyle(tree, itemColumn);
					if (drawArgs.style != NULL)
					{
						if (TreeStyle_ValidateElements(&drawArgs,
							objc - 5, objv + 5) != TCL_OK)
							return TCL_ERROR;
					}
				}
			}

			if (Tree_ItemBbox(tree, item, &x, &y, &w, &h) < 0)
				return TCL_OK;
			if (w < 1 || h < 1)
				return TCL_OK;

			drawArgs.tree = tree;
			drawArgs.drawable = None;
			drawArgs.state = TreeItem_GetState(tree, item);
			drawArgs.y = y;
			drawArgs.height = h;

			TreeDragImage_Undisplay(tree->dragImage);

			if (objc > 4)
			{
				if (TreeItem_ColumnFromObj(tree, item, objv[4], &itemColumn, &columnIndex) != TCL_OK)
				{
					result = TCL_ERROR;
					goto doneAdd;
				}
				treeColumn = Tree_FindColumn(tree, columnIndex);
				if (!TreeColumn_Visible(treeColumn))
					goto doneAdd;
				drawArgs.style = TreeItemColumn_GetStyle(tree, itemColumn);
				if (drawArgs.style == NULL)
					goto doneAdd;
				totalWidth = 0;
				treeColumn = tree->columns;
				for (i = 0; i < columnIndex; i++)
				{
					totalWidth += TreeColumn_UseWidth(treeColumn);
					treeColumn = TreeColumn_Next(treeColumn);
				}
				if (TreeColumn_Index(treeColumn) == tree->columnTree)
					indent = TreeItem_Indent(tree, item);
				else
					indent = 0;
				drawArgs.x = x + indent + totalWidth;
				drawArgs.width = TreeColumn_UseWidth(treeColumn) - indent;
				drawArgs.justify = TreeColumn_Justify(treeColumn);
				if (objc - 5 > STATIC_SIZE)
					rects = (XRectangle *) ckalloc(sizeof(XRectangle) * (objc - 5));
				count = TreeStyle_GetElemRects(&drawArgs, objc - 5, objv + 5, rects);
				if (count == -1)
				{
					result = TCL_ERROR;
					goto doneAdd;
				}
				for (i = 0; i < count; i++)
				{
					elem = DragElem_Alloc(dragImage);
					elem->x = rects[i].x;
					elem->y = rects[i].y;
					elem->width = rects[i].width;
					elem->height = rects[i].height;
				}
			}
			else
			{
				totalWidth = 0;
				treeColumn = tree->columns;
				itemColumn = TreeItem_GetFirstColumn(tree, item);
				while (itemColumn != NULL)
				{
					if (!TreeColumn_Visible(treeColumn))
						goto nextColumn;
					width = TreeColumn_UseWidth(treeColumn);
					if (TreeColumn_Index(treeColumn) == tree->columnTree)
						indent = TreeItem_Indent(tree, item);
					else
						indent = 0;
					drawArgs.style = TreeItemColumn_GetStyle(tree, itemColumn);
					if (drawArgs.style != NULL)
					{
						drawArgs.x = x + indent + totalWidth;
						drawArgs.width = width - indent;
						drawArgs.justify = TreeColumn_Justify(treeColumn);
						count = TreeStyle_NumElements(tree, drawArgs.style);
						if (count > STATIC_SIZE)
							rects = (XRectangle *) ckalloc(sizeof(XRectangle) * count);
						count = TreeStyle_GetElemRects(&drawArgs, 0, NULL, rects);
						if (count == -1)
						{
							result = TCL_ERROR;
							goto doneAdd;
						}
						for (i = 0; i < count; i++)
						{
							elem = DragElem_Alloc(dragImage);
							elem->x = rects[i].x;
							elem->y = rects[i].y;
							elem->width = rects[i].width;
							elem->height = rects[i].height;
						}
						if (rects != staticRects)
						{
							ckfree((char *) rects);
							rects = staticRects;
						}
					}
					totalWidth += width;
nextColumn:
					treeColumn = TreeColumn_Next(treeColumn);
					itemColumn = TreeItemColumn_GetNext(tree, itemColumn);
				}
			}
			dragImage->bounds[0] = 100000;
			dragImage->bounds[1] = 100000;
			dragImage->bounds[2] = -100000;
			dragImage->bounds[3] = -100000;
			for (elem = dragImage->elem;
				elem != NULL;
				elem = elem->next)
			{
				if (elem->x < dragImage->bounds[0])
					dragImage->bounds[0] = elem->x;
				if (elem->y < dragImage->bounds[1])
					dragImage->bounds[1] = elem->y;
				if (elem->x + elem->width > dragImage->bounds[2])
					dragImage->bounds[2] = elem->x + elem->width;
				if (elem->y + elem->height > dragImage->bounds[3])
					dragImage->bounds[3] = elem->y + elem->height;
			}
doneAdd:
			if (rects != staticRects)
				ckfree((char *) rects);
			TreeDragImage_Display(tree->dragImage);
			return result;
		}

		/* T dragimage cget option */
		case COMMAND_CGET:
		{
			Tcl_Obj *resultObjPtr;

			if (objc != 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "option");
				return TCL_ERROR;
			}
			resultObjPtr = Tk_GetOptionValue(interp, (char *) dragImage,
				dragImage->optionTable, objv[3], tree->tkwin);
			if (resultObjPtr == NULL)
				return TCL_ERROR;
			Tcl_SetObjResult(interp, resultObjPtr);
			break;
		}

		/* T dragimage clear */
		case COMMAND_CLEAR:
		{
			if (objc != 3)
			{
				Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
				return TCL_ERROR;
			}
			if (dragImage->elem != NULL)
			{
				DragElem *elem = dragImage->elem;
				TreeDragImage_Undisplay(tree->dragImage);
/*				if (dragImage->visible)
					DragImage_Redraw(dragImage); */
				while (elem != NULL)
					elem = DragElem_Free(dragImage, elem);
				dragImage->elem = NULL;
			}
			break;
		}

		/* T dragimage configure ?option? ?value? ?option value ...? */
		case COMMAND_CONFIGURE:
		{
			Tcl_Obj *resultObjPtr;

			if (objc < 3)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "?option? ?value?");
				return TCL_ERROR;
			}
			if (objc <= 4)
			{
				resultObjPtr = Tk_GetOptionInfo(interp, (char *) dragImage,
					dragImage->optionTable,
					(objc == 3) ? (Tcl_Obj *) NULL : objv[3],
					tree->tkwin);
				if (resultObjPtr == NULL)
					return TCL_ERROR;
				Tcl_SetObjResult(interp, resultObjPtr);
				break;
			}
			return DragImage_Config(dragImage, objc - 3, objv + 3);
		}

		/* T dragimage offset ?x y? */
		case COMMAND_OFFSET:
		{
			int x, y;

			if (objc != 3 && objc != 5)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "?x y?");
				return TCL_ERROR;
			}
			if (objc == 3)
			{
				FormatResult(interp, "%d %d", dragImage->x, dragImage->y);
				break;
			}
			if (Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK)
				return TCL_ERROR;
			if (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)
				return TCL_ERROR;
			TreeDragImage_Undisplay(tree->dragImage);
/*			if (dragImage->visible)
				DragImage_Redraw(dragImage); */
			dragImage->x = x;
			dragImage->y = y;
			TreeDragImage_Display(tree->dragImage);
			break;
		}

		/* T dragimage visible ?boolean? */
		case COMMAND_VISIBLE:
		{
			int visible;

			if (objc != 3 && objc != 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "?boolean?");
				return TCL_ERROR;
			}
			if (objc == 4)
			{
				if (Tcl_GetBooleanFromObj(interp, objv[3], &visible) != TCL_OK)
					return TCL_ERROR;
				if (visible != dragImage->visible)
				{
					dragImage->visible = visible;
					TreeDragImage_Undisplay(tree->dragImage);
					TreeDragImage_Display(tree->dragImage);
				}
			}
			Tcl_SetObjResult(interp, Tcl_NewBooleanObj(dragImage->visible));
			break;
		}
	}

	return TCL_OK;
}
