#include "tkTreeCtrl.h"

typedef struct Column Column;
typedef struct Item Item;

struct Column
{
	int neededWidth;
	int neededHeight;
	TreeStyle style;
	Column *next;
};

struct Item
{
	int id; /* unique id */
	int depth; /* tree depth (-1 for the unique root item) */
	int neededHeight; /* miniumum height of this item (max of all columns) */
	int fixedHeight; /* desired height of this item (zero for no-such-value) */
	int numChildren;
	int index; /* "row" in flattened tree */
	int indexVis; /* visible "row" in flattened tree, or -1 if not visible */
	int state; /* STATE_xxx */
	int isVisible;
	int hasButton;
	Item *parent;
	Item *firstChild;
	Item *lastChild;
	Item *prevSibling;
	Item *nextSibling;
	TreeItemDInfo dInfo; /* display info, or NULL */
	TreeItemRInfo rInfo; /* range info, or NULL */
	Column *columns;
};

#define ISROOT(i) ((i)->depth == -1)

/*****/

static Column *Column_Alloc(void)
{
	Column *column = (Column *) ckalloc(sizeof(Column));
	memset(column, '\0', sizeof(Column));
	return column;
}

void TreeItemColumn_InvalidateSize(TreeCtrl *tree, TreeItemColumn column_)
{
	Column *column = (Column *) column_;

	column->neededWidth = column->neededHeight = -1;
}

#if 0

int Column_NeededHeight(TreeCtrl *tree, Item *item, Column *self)
{
	int width, height;

	if (self->style != NULL)
		TreeStyle_GetNeededSize(tree, self->style, item->state, &width, &height);
	else
		width = height = 0;
	return height;
}

#endif

int TreeItemColumn_NeededWidth(TreeCtrl *tree, TreeItem item_, TreeItemColumn column_)
{
	Item *item = (Item *) item_;
	Column *self = (Column *) column_;

	if (self->style != NULL)
		return TreeStyle_NeededWidth(tree, self->style, item->state);
	return 0;
}

TreeStyle TreeItemColumn_GetStyle(TreeCtrl *tree, TreeItemColumn column)
{
	return ((Column *) column)->style;
}

void TreeItemColumn_ForgetStyle(TreeCtrl *tree, TreeItemColumn column_)
{
	Column *self = (Column *) column_;

	if (self->style != NULL)
	{
		TreeStyle_FreeResources(tree, self->style);
		self->style = NULL;
		self->neededWidth = self->neededHeight = 0;
	}
}

TreeItemColumn TreeItemColumn_GetNext(TreeCtrl *tree, TreeItemColumn column)
{
	return (TreeItemColumn) ((Column *) column)->next;
}

Column *Column_FreeResources(TreeCtrl *tree, Column *self)
{
	Column *next = self->next;

	if (self->style != NULL)
		TreeStyle_FreeResources(tree, self->style);
	WFREE(self, Column);
	return next;
}

/*****/

static void Item_UpdateIndex(TreeCtrl *tree, Item *item, int *index, int *indexVis)
{
	Item *child;
	int parentVis, parentOpen;

	/* Also track max depth */
	if (item->parent != NULL)
		item->depth = item->parent->depth + 1;
	else
		item->depth = 0;
	if (item->depth > tree->depth)
		tree->depth = item->depth;

	item->index = (*index)++;
	item->indexVis = -1;
	if (item->parent != NULL)
	{
		parentOpen = (item->parent->state & STATE_OPEN) != 0;
		parentVis = item->parent->indexVis != -1;
		if (ISROOT(item->parent) && !tree->showRoot)
		{
			parentOpen = TRUE;
			parentVis = item->parent->isVisible;
		}
		if (parentVis && parentOpen && item->isVisible)
			item->indexVis = (*indexVis)++;
	}
	child = item->firstChild;
	while (child != NULL)
	{
		Item_UpdateIndex(tree, child, index, indexVis);
		child = child->nextSibling;
	}
}

void Tree_UpdateItemIndex(TreeCtrl *tree)
{
	Item *item = (Item *) tree->root;
	int index = 1, indexVis = 0;

if (tree->debug.enable && tree->debug.data)
	dbwin("Tree_UpdateItemIndex %s\n", Tk_PathName(tree->tkwin));

	/* Also track max depth */
	tree->depth = -1;

	item->index = 0;
	item->indexVis = -1;
	if (tree->showRoot && item->isVisible)
		item->indexVis = indexVis++;
	item = item->firstChild;
	while (item != NULL)
	{
		Item_UpdateIndex(tree, item, &index, &indexVis);
		item = item->nextSibling;
	}
	tree->itemVisCount = indexVis;
	tree->updateIndex = 0;
}

TreeItem TreeItem_Alloc(TreeCtrl *tree)
{
	Item *item = (Item *) ckalloc(sizeof(Item));
	memset(item, '\0', sizeof(Item));
	item->isVisible = TRUE;
	item->state =
		STATE_OPEN |
		STATE_ENABLED;
	if (tree->gotFocus)
		item->state |= STATE_FOCUS;
	item->indexVis = -1;
	Tree_AddItem(tree, (TreeItem) item);
	return (TreeItem) item;
}

TreeItem TreeItem_AllocRoot(TreeCtrl *tree)
{
	Item *item;

	item = (Item *) TreeItem_Alloc(tree);
	item->depth = -1;
	item->state |= STATE_ACTIVE;
	return (TreeItem) item;
}

TreeItemColumn TreeItem_GetFirstColumn(TreeCtrl *tree, TreeItem item)
{
	return (TreeItemColumn) ((Item *) item)->columns;
}

int TreeItem_GetState(TreeCtrl *tree, TreeItem item_)
{
	return ((Item *) item_)->state;
}

int TreeItem_ChangeState(TreeCtrl *tree, TreeItem item_, int stateOff, int stateOn)
{
	Item *item = (Item *) item_;
	Column *column;
	int columnIndex = 0, state;
	int sMask, iMask = 0;

	state = item->state;
	state &= ~stateOff;
	state |= stateOn;

	if (state == item->state)
		return 0;

	column = item->columns;
	while (column != NULL)
	{
		if (column->style != NULL)
		{
			sMask = TreeStyle_ChangeState(tree, column->style, item->state, state);
			if (sMask)
			{
				if (sMask & CS_LAYOUT)
					Tree_InvalidateColumnWidth(tree, columnIndex);
				iMask |= sMask;
			}
		}
		columnIndex++;
		column = column->next;
	}

	/* OPEN -> CLOSED or vice versa */
	if ((stateOff | stateOn) & STATE_OPEN)
	{
		/* This item has a button */
		if (item->hasButton && tree->showButtons && (!ISROOT(item) || tree->showRootButton))
		{
			/* Image or bitmap is used */
			if ((tree->openButtonWidth != tree->closedButtonWidth) ||
				(tree->openButtonHeight != tree->closedButtonHeight))
			{
				iMask |= CS_LAYOUT;
			}
			iMask |= CS_DISPLAY;
		}
	}

	if (iMask & CS_LAYOUT)
	{
		TreeItem_InvalidateHeight(tree, item_);
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
	}
	if (iMask & CS_DISPLAY)
		Tree_InvalidateItemDInfo(tree, item_, NULL);

	item->state = state;

	return iMask;
}

void TreeItem_Undefine(TreeCtrl *tree, TreeItem item_, int state)
{
	Item *item = (Item *) item_;
	item->state &= ~state;
}

int TreeItem_GetButton(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	return item->hasButton;
}

int TreeItem_SetButton(TreeCtrl *tree, TreeItem item_, int hasButton)
{
	Item *item = (Item *) item_;
	return item->hasButton = hasButton;
}

int TreeItem_GetDepth(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
#if 0
	if (tree->updateIndex)
		Tree_UpdateItemIndex(tree);
#endif
	return item->depth;
}

int TreeItem_SetDepth(TreeCtrl *tree, TreeItem item_, int depth)
{
	Item *item = (Item *) item_;
	return item->depth = depth;
}

int TreeItem_GetID(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	return item->id;
}

int TreeItem_SetID(TreeCtrl *tree, TreeItem item_, int id)
{
	Item *item = (Item *) item_;
	return item->id = id;
}

int TreeItem_GetOpen(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	return (item->state & STATE_OPEN) != 0;
}

int TreeItem_SetOpen(TreeCtrl *tree, TreeItem item_, int isOpen)
{
	Item *item = (Item *) item_;
	if (isOpen)
		item->state |= STATE_OPEN;
	else
		item->state &= ~STATE_OPEN;
	return isOpen;
}

int TreeItem_GetSelected(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	return (item->state & STATE_SELECTED) != 0;
}

int TreeItem_SetSelected(TreeCtrl *tree, TreeItem item_, int isSelected)
{
	Item *item = (Item *) item_;
	if (isSelected)
		item->state |= STATE_SELECTED;
	else
		item->state &= ~STATE_SELECTED;
	return isSelected;
}

TreeItem TreeItem_SetFirstChild(TreeCtrl *tree, TreeItem item_, TreeItem firstChild)
{
	Item *item = (Item *) item_;
	return (TreeItem) (item->firstChild = (Item *) firstChild);
}

TreeItem TreeItem_GetFirstChild(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	return (TreeItem) item->firstChild;
}

TreeItem TreeItem_SetLastChild(TreeCtrl *tree, TreeItem item_, TreeItem lastChild)
{
	Item *item = (Item *) item_;
	return (TreeItem) (item->lastChild = (Item *) lastChild);
}

TreeItem TreeItem_GetLastChild(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	return (TreeItem) item->lastChild;
}

TreeItem TreeItem_SetParent(TreeCtrl *tree, TreeItem item_, TreeItem parent)
{
	Item *item = (Item *) item_;
	return (TreeItem) (item->parent = (Item *) parent);
}

TreeItem TreeItem_GetParent(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	return (TreeItem) item->parent;
}

TreeItem TreeItem_SetNextSibling(TreeCtrl *tree, TreeItem item_, TreeItem nextSibling)
{
	Item *item = (Item *) item_;
	return (TreeItem) (item->nextSibling = (Item *) nextSibling);
}

TreeItem TreeItem_GetNextSibling(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	return (TreeItem) item->nextSibling;
}

TreeItem TreeItem_SetPrevSibling(TreeCtrl *tree, TreeItem item_, TreeItem prevSibling)
{
	Item *item = (Item *) item_;
	return (TreeItem) (item->prevSibling = (Item *) prevSibling);
}

TreeItem TreeItem_GetPrevSibling(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	return (TreeItem) item->prevSibling;
}

void TreeItem_SetDInfo(TreeCtrl *tree, TreeItem item_, TreeItemDInfo dInfo)
{
	Item *item = (Item *) item_;
	item->dInfo = dInfo;
}

TreeItemDInfo TreeItem_GetDInfo(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	return item->dInfo;
}

void TreeItem_SetRInfo(TreeCtrl *tree, TreeItem item_, TreeItemRInfo rInfo)
{
	Item *item = (Item *) item_;
	item->rInfo = rInfo;
}

TreeItemRInfo TreeItem_GetRInfo(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	return item->rInfo;
}

TreeItem TreeItem_Next(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;

	if (item->firstChild != NULL)
		return (TreeItem) item->firstChild;
	if (item->nextSibling != NULL)
		return (TreeItem) item->nextSibling;
	while (1)
	{
		item = item->parent;
		if (item == NULL)
			break;
		if (item->nextSibling != NULL)
			return (TreeItem) item->nextSibling;
	}
	return NULL;
}

TreeItem TreeItem_NextVisible(TreeCtrl *tree, TreeItem item)
{
	item = TreeItem_Next(tree, item);
	while (item != NULL)
	{
		if (TreeItem_ReallyVisible(tree, item))
			return item;
		item = TreeItem_Next(tree, item);
	}
	return NULL;
}

TreeItem TreeItem_Prev(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	Item *walk;

	if (item->parent == NULL) /* root */
		return NULL;
	walk = item->parent;
	if (item->prevSibling)
	{
		walk = item->prevSibling;
		while (walk->lastChild != NULL)
			walk = walk->lastChild;
	}
	return (TreeItem) walk;
}

TreeItem TreeItem_PrevVisible(TreeCtrl *tree, TreeItem item)
{
	item = TreeItem_Prev(tree, item);
	while (item != NULL)
	{
		if (TreeItem_ReallyVisible(tree, item))
			return item;
		item = TreeItem_Prev(tree, item);
	}
	return NULL;
}

void TreeItem_ToIndex(TreeCtrl *tree, TreeItem item_, int *index, int *indexVis)
{
	Item *item = (Item *) item_;

	if (tree->updateIndex)
		Tree_UpdateItemIndex(tree);
	if (index != NULL) (*index) = item->index;
	if (indexVis != NULL) (*indexVis) = item->indexVis;
}

static int IndexFromList(int listIndex, int objc, Tcl_Obj **objv, CONST char **indexNames)
{
	Tcl_Obj *elemPtr;
	int index;

	if (listIndex >= objc)
		return -1;
	elemPtr = objv[listIndex];
	if (Tcl_GetIndexFromObj(NULL, elemPtr, indexNames, NULL, 0, &index) != TCL_OK)
		return -1;
	return index;
}

/*
%W index all
%W index "active MODIFIERS"
%W index "anchor MODIFIERS"
%W index "nearest x y MODIFIERS"
%W index "root MODIFIERS"
%W index "first ?visible? MODIFIERS"
%W index "last ?visible? MODIFIERS"
%W index "rnc row col MODIFIERS"
%W index "ID MODIFIERS"
MODIFIERS:
	above
	below
	left
	right
	top
	bottom
	leftmost
	rightmost
	next ?visible?
	prev ?visible?
	parent
	firstchild ?visible?
	lastchild ?visible?
	child N ?visible?
	nextsibling ?visible?
	prevsibling ?visible?
	sibling N ?visible?

Examples:
	%W index "first visible firstchild"
	%W index "first visible firstchild visible"
	%W index "nearest x y nextsibling visible"
*/

int TreeItem_FromObj(TreeCtrl *tree, Tcl_Obj *objPtr, TreeItem *itemPtr, int flags)
{
	Tcl_Interp *interp = tree->interp;
	int objc;
	int index, listIndex, id;
	Tcl_HashEntry *hPtr;
	Tcl_Obj **objv, *elemPtr;
	Item *item = NULL;
	static CONST char *indexName[] = { "active", "all", "anchor", "first", "last",
		"nearest", "rnc", "root", (char *) NULL };
	enum indexEnum {
		INDEX_ACTIVE, INDEX_ALL, INDEX_ANCHOR, INDEX_FIRST, INDEX_LAST,
		INDEX_NEAREST, INDEX_RNC, INDEX_ROOT
	} ;
	static CONST char *modifiers[] = { "above", "below", "bottom", "child",
		"firstchild", "lastchild", "left", "leftmost", "next", "nextsibling",
		"parent", "prev", "prevsibling", "right", "rightmost", "sibling", "top",
		"visible", (char *) NULL };
	enum modEnum {
		MOD_ABOVE, MOD_BELOW, MOD_BOTTOM, MOD_CHILD, MOD_FIRSTCHILD, MOD_LASTCHILD,
		MOD_LEFT, MOD_LEFTMOST, MOD_NEXT, MOD_NEXTSIBLING, MOD_PARENT, MOD_PREV, MOD_PREVSIBLING,
		MOD_RIGHT, MOD_RIGHTMOST, MOD_SIBLING, MOD_TOP, MOD_VISIBLE
	};
	static int modArgs[] = { 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1 };

	if (Tcl_ListObjGetElements(NULL, objPtr, &objc, &objv) != TCL_OK)
		goto baditem;
	if (objc == 0)
		goto baditem;

	listIndex = 0;
	elemPtr = objv[listIndex++];
	if (Tcl_GetIndexFromObj(NULL, elemPtr, indexName, NULL, 0, &index) == TCL_OK)
	{
		switch ((enum indexEnum) index)
		{
			case INDEX_ACTIVE:
			{
				item = (Item *) tree->activeItem;
				break;
			}
			case INDEX_ALL:
			{
				if (!(flags & IFO_ALLOK))
				{
					Tcl_AppendResult(interp,
						"can't specify \"all\" for this command", NULL);
					return TCL_ERROR;
				}
				if (objc > 1)
					goto baditem;
				(*itemPtr) = ITEM_ALL;
				return TCL_OK;
			}
			case INDEX_ANCHOR:
			{
				item = (Item *) tree->anchorItem;
				break;
			}
			case INDEX_FIRST:
			{
				item = (Item *) tree->root;
				if (IndexFromList(listIndex, objc, objv, modifiers) == MOD_VISIBLE)
				{
					if (!item->isVisible)
						item = NULL;
					else if (!tree->showRoot)
						item = (Item *) TreeItem_NextVisible(tree, (TreeItem) item);
					listIndex++;
				}
				break;
			}
			case INDEX_LAST:
			{
				item = (Item *) tree->root;
				while (item->lastChild)
				{
					item = item->lastChild;
				}
				if (IndexFromList(listIndex, objc, objv, modifiers) == MOD_VISIBLE)
				{
					if (!((Item *) tree->root)->isVisible)
						item = NULL; /* nothing is visible */
					else if (item == (Item *) tree->root && !tree->showRoot)
						item = NULL; /* no item but root, not visible */
					else if (!TreeItem_ReallyVisible(tree, (TreeItem) item))
						item = (Item *) TreeItem_PrevVisible(tree, (TreeItem) item);
					listIndex++;
				}
				break;
			}
			case INDEX_NEAREST:
			{
				int x, y;

				if (objc < 3)
					goto baditem;
				if (Tcl_GetIntFromObj(NULL, objv[listIndex++], &x) != TCL_OK)
					goto baditem;
				if (Tcl_GetIntFromObj(NULL, objv[listIndex++], &y) != TCL_OK)
					goto baditem;
				item = (Item *) Tree_ItemUnderPoint(tree, &x, &y, TRUE);
				break;
			}
			case INDEX_RNC:
			{
				int row, col;

				if (objc < 3)
					goto baditem;
				if (Tcl_GetIntFromObj(NULL, objv[listIndex++], &row) != TCL_OK)
					goto baditem;
				if (Tcl_GetIntFromObj(NULL, objv[listIndex++], &col) != TCL_OK)
					goto baditem;
				item = (Item *) Tree_RNCToItem(tree, row, col);
				break;
			}
			case INDEX_ROOT:
			{
				item = (Item *) tree->root;
				break;
			}
		}
	}
	else if (Tcl_GetIntFromObj(NULL, elemPtr, &id) == TCL_OK)
	{
		hPtr = Tcl_FindHashEntry(&tree->itemHash, (char *) id);
		if (!hPtr)
		{
			if (!(flags & IFO_NULLOK))
				goto noitem;
			(*itemPtr) = NULL;
			return TCL_OK;
		}
		item = (Item *) Tcl_GetHashValue(hPtr);
	}
	else
	{
		goto baditem;
	}
	/* This means a valid specification was given, but there is no such item */
	if (item == NULL)
	{
		if (!(flags & IFO_NULLOK))
			goto noitem;
		(*itemPtr) = (TreeItem) item;
		return TCL_OK;
	}
	for (; listIndex < objc; /* nothing */)
	{
		int nextIsVisible = FALSE;

		elemPtr = objv[listIndex];
		if (Tcl_GetIndexFromObj(interp, elemPtr, modifiers, "modifier", 0, &index) != TCL_OK)
			return TCL_ERROR;
		if (objc - listIndex < modArgs[index])
			goto baditem;
		if (IndexFromList(listIndex + modArgs[index], objc, objv, modifiers) == MOD_VISIBLE)
			nextIsVisible = TRUE;
		switch ((enum modEnum) index)
		{
			case MOD_ABOVE:
			{
				item = (Item *) Tree_ItemAbove(tree, (TreeItem) item);
				nextIsVisible = FALSE;
				break;
			}
			case MOD_BELOW:
			{
				item = (Item *) Tree_ItemBelow(tree, (TreeItem) item);
				nextIsVisible = FALSE;
				break;
			}
			case MOD_BOTTOM:
			{
				item = (Item *) Tree_ItemBottom(tree, (TreeItem) item);
				nextIsVisible = FALSE;
				break;
			}
			case MOD_CHILD:
			{
				int n;

				if (Tcl_GetIntFromObj(interp, objv[listIndex + 1], &n) != TCL_OK)
					return TCL_ERROR;
				item = item->firstChild;
				if (nextIsVisible)
				{
					while (item != NULL)
					{
						if (TreeItem_ReallyVisible(tree, (TreeItem) item))
							if (n-- <= 0)
								break;
						item = item->nextSibling;
					}
				}
				else
				{
					while ((n-- > 0) && (item != NULL))
						item = item->nextSibling;
				}
				break;
			}
			case MOD_FIRSTCHILD:
			{
				item = item->firstChild;
				if (nextIsVisible)
				{
					while ((item != NULL) && !TreeItem_ReallyVisible(tree, (TreeItem) item))
						item = item->nextSibling;
				}
				break;
			}
			case MOD_LASTCHILD:
			{
				item = item->lastChild;
				if (nextIsVisible)
				{
					while ((item != NULL) && !TreeItem_ReallyVisible(tree, (TreeItem) item))
						item = item->prevSibling;
				}
				break;
			}
			case MOD_LEFT:
			{
				item = (Item *) Tree_ItemLeft(tree, (TreeItem) item);
				nextIsVisible = FALSE;
				break;
			}
			case MOD_LEFTMOST:
			{
				item = (Item *) Tree_ItemLeftMost(tree, (TreeItem) item);
				nextIsVisible = FALSE;
				break;
			}
			case MOD_NEXT:
			{
				if (nextIsVisible)
					item = (Item *) TreeItem_NextVisible(tree, (TreeItem) item);
				else
					item = (Item *) TreeItem_Next(tree, (TreeItem) item);
				break;
			}
			case MOD_NEXTSIBLING:
			{
				item = item->nextSibling;
				if (nextIsVisible)
				{
					while ((item != NULL) && !TreeItem_ReallyVisible(tree, (TreeItem) item)) {
						item = item->nextSibling;
					}
				}
				break;
			}
			case MOD_PARENT:
			{
				item = item->parent;
				nextIsVisible = FALSE;
				break;
			}
			case MOD_PREV:
			{
				if (nextIsVisible)
					item = (Item *) TreeItem_PrevVisible(tree, (TreeItem) item);
				else
					item = (Item *) TreeItem_Prev(tree, (TreeItem) item);
				break;
			}
			case MOD_PREVSIBLING:
			{
				item = item->prevSibling;
				if (nextIsVisible)
				{
					while ((item != NULL) && !TreeItem_ReallyVisible(tree, (TreeItem) item)) {
						item = item->prevSibling;
					}
				}
				break;
			}
			case MOD_RIGHT:
			{
				item = (Item *) Tree_ItemRight(tree, (TreeItem) item);
				nextIsVisible = FALSE;
				break;
			}
			case MOD_RIGHTMOST:
			{
				item = (Item *) Tree_ItemRightMost(tree, (TreeItem) item);
				nextIsVisible = FALSE;
				break;
			}
			case MOD_SIBLING:
			{
				int n;

				if (Tcl_GetIntFromObj(interp, objv[listIndex + 1], &n) != TCL_OK)
					return TCL_ERROR;
				item = item->parent;
				if (item == NULL)
					break;
				item = item->firstChild;
				if (nextIsVisible)
				{
					while (item != NULL)
					{
						if (TreeItem_ReallyVisible(tree, (TreeItem) item))
							if (n-- <= 0)
								break;
						item = item->nextSibling;
					}
				}
				else
				{
					while ((n-- > 0) && (item != NULL))
						item = item->nextSibling;
				}
				break;
			}
			case MOD_TOP:
			{
				item = (Item *) Tree_ItemTop(tree, (TreeItem) item);
				nextIsVisible = FALSE;
				break;
			}
			case MOD_VISIBLE:
			{
				goto baditem;
			}
		}
		if (item == NULL)
		{
			if (!(flags & IFO_NULLOK))
				goto noitem;
			(*itemPtr) = (TreeItem) item;
			return TCL_OK;
		}
		listIndex += modArgs[index];
		if (nextIsVisible)
			listIndex++;
	}
	if (ISROOT(item))
	{
		if ((flags & IFO_NOTROOT))
		{
			Tcl_AppendResult(interp,
				"can't specify \"root\" for this command", NULL);
			return TCL_ERROR;
		}
	}
	(*itemPtr) = (TreeItem) item;
	return TCL_OK;
baditem:
	Tcl_AppendResult(interp, "bad item description \"", Tcl_GetString(objPtr),
		"\"", NULL);
	return TCL_ERROR;
noitem:
	Tcl_AppendResult(interp, "item \"", Tcl_GetString(objPtr),
		"\" doesn't exist", NULL);
	return TCL_ERROR;
}

static void Item_Toggle(TreeCtrl *tree, Item *item, int stateOff, int stateOn)
{
	int mask;

	mask = TreeItem_ChangeState(tree, (TreeItem) item, stateOff, stateOn);

	if (ISROOT(item) && !tree->showRoot)
		return;

#if 0
	/* Don't affect display if we weren't visible */
	if (!TreeItem_ReallyVisible(tree, (TreeItem) item))
		return;

	/* Invalidate display info for this item, so it is redrawn later. */
	Tree_InvalidateItemDInfo(tree, (TreeItem) item, NULL);
#endif

	if (item->numChildren > 0)
	{
		/* indexVis needs updating for all items after this one, if we
		 * have any visible children */
		tree->updateIndex = 1;
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

		/* Hiding/showing children may change the width of any column */
		Tree_InvalidateColumnWidth(tree, -1);
	}

	/* If this item was previously onscreen, this call is repetitive. */
	Tree_EventuallyRedraw(tree);
}

void TreeItem_OpenClose(TreeCtrl *tree, TreeItem item_, int mode, int recurse)
{
	Item *item = (Item *) item_;
	Item *child;
	int stateOff = 0, stateOn = 0;

	if (mode == -1)
	{
		if (item->state & STATE_OPEN)
			stateOff = STATE_OPEN;
		else
			stateOn = STATE_OPEN;
	}
	else if (!mode && (item->state & STATE_OPEN))
		stateOff = STATE_OPEN;
	else if (mode && !(item->state & STATE_OPEN))
		stateOn = STATE_OPEN;

	if (stateOff != stateOn)
	{
		TreeNotify_OpenClose(tree, item_, stateOn, TRUE);
		Item_Toggle(tree, item, stateOff, stateOn);
		TreeNotify_OpenClose(tree, item_, stateOn, FALSE);
	}
	if (recurse)
	{
		for (child = item->firstChild; child != NULL; child = child->nextSibling)
			TreeItem_OpenClose(tree, (TreeItem) child, mode, recurse);
	}
}

void TreeItem_Delete(TreeCtrl *tree, TreeItem item_)
{
	Item *self = (Item *) item_;

	if (TreeItem_ReallyVisible(tree, item_))
		Tree_InvalidateColumnWidth(tree, -1);

	while (self->numChildren > 0)
		TreeItem_Delete(tree, (TreeItem) self->firstChild);

	TreeItem_RemoveFromParent(tree, item_);
	Tree_RemoveItem(tree, item_);
	TreeItem_FreeResources(tree, item_);
	if (tree->activeItem == item_)
	{
		tree->activeItem = tree->root;
		TreeItem_ChangeState(tree, tree->activeItem, 0, STATE_ACTIVE);
	}
	if (tree->anchorItem == item_)
		tree->anchorItem = tree->root;
	if (tree->debug.enable && tree->debug.data)
		Tree_Debug(tree);
}

void TreeItem_UpdateDepth(TreeCtrl *tree, TreeItem item_)
{
	Item *self = (Item *) item_;
	Item *child;

	if (ISROOT(self))
		return;
	if (self->parent != NULL)
		self->depth = self->parent->depth + 1;
	else
		self->depth = 0;
	child = self->firstChild;
	while (child != NULL)
	{
		TreeItem_UpdateDepth(tree, (TreeItem) child);
		child = child->nextSibling;
	}
}

void TreeItem_AddToParent(TreeCtrl *tree, TreeItem item_)
{
	Item *self = (Item *) item_;
	Item *last;

	/* If this is the new last child, redraw the lines of the previous
	 * sibling and all of its descendants so the line from the previous
	 * sibling reaches this item */
	if ((self->prevSibling != NULL) &&
		(self->nextSibling == NULL) &&
		tree->showLines)
	{
		last = self->prevSibling;
		while (last->lastChild != NULL)
			last = last->lastChild;
		Tree_InvalidateItemDInfo(tree, (TreeItem) self->prevSibling,
			(TreeItem) last);
	}

	tree->updateIndex = 1;
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

	/* Tree_UpdateItemIndex() also recalcs depth, but in one of my demos
	 * I retrieve item depth during list creation. Since Tree_UpdateItemIndex()
	 * is slow I will keep depth up-to-date here. */
	TreeItem_UpdateDepth(tree, item_);

	Tree_InvalidateColumnWidth(tree, -1);
	if (tree->debug.enable && tree->debug.data)
		Tree_Debug(tree);
}

static void RemoveFromParentAux(TreeCtrl *tree, Item *self, int *index)
{
	Item *child;

	/* Invalidate display info. Don't free it because we may just be
	 * moving the item to a new parent. FIXME: if it is being moved,
	 * it might not actually need to be redrawn (just copied) */
	if (self->dInfo != NULL)
		Tree_InvalidateItemDInfo(tree, (TreeItem) self, NULL);

	if (self->parent != NULL)
		self->depth = self->parent->depth + 1;
	else
		self->depth = 0;
	self->index = (*index)++;
	self->indexVis = -1;
	child = self->firstChild;
	while (child != NULL)
	{
		RemoveFromParentAux(tree, child, index);
		child = child->nextSibling;
	}
}

void TreeItem_RemoveFromParent(TreeCtrl *tree, TreeItem item_)
{
	Item *self = (Item *) item_;
	Item *parent = self->parent;
	Item *last;
	int index = 0;

	if (parent == NULL)
		return;

	/* If this is the last child, redraw the lines of the previous
	 * sibling and all of its descendants because the line from
	 * the previous sibling to us is now gone */
	if ((self->prevSibling != NULL) &&
		(self->nextSibling == NULL) &&
		tree->showLines)
	{
		last = self->prevSibling;
		while (last->lastChild != NULL)
			last = last->lastChild;
		Tree_InvalidateItemDInfo(tree, (TreeItem) self->prevSibling,
			(TreeItem) last);
	}

	tree->updateIndex = 1;
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

	if (self->prevSibling)
		self->prevSibling->nextSibling = self->nextSibling;
	if (self->nextSibling)
		self->nextSibling->prevSibling = self->prevSibling;
	if (parent->firstChild == self)
	{
		parent->firstChild = self->nextSibling;
		if (!parent->firstChild)
			parent->lastChild = NULL;
	}
	if (parent->lastChild == self)
		parent->lastChild = self->prevSibling;
	self->prevSibling = self->nextSibling = NULL;
	self->parent = NULL;
	parent->numChildren--;

	/* Update depth, index and indexVis. Index is needed for some operations
	 * that use a range of items, such as delete. */
	RemoveFromParentAux(tree, self, &index);
}

void TreeItem_RemoveColumn(TreeCtrl *tree, TreeItem item_, TreeItemColumn column_)
{
	Item *self = (Item *) item_;
	Column *column, *prev;

	column = self->columns;
	prev = NULL;
	while (column != NULL)
	{
		if (column == (Column *) column_)
		{
			if (prev != NULL)
				prev->next = column->next;
			else
				self->columns = column->next;
			Column_FreeResources(tree, column);
			break;
		}
		prev = column;
		column = column->next;
	}
	if (column == NULL)
		panic("TreeItem_RemoveColumn: can't find column");
}

void TreeItem_MoveColumn(TreeCtrl *tree, TreeItem item, int columnIndex, int beforeIndex)
{
	Item *self = (Item *) item;
	Column *before = NULL, *move = NULL;
	Column *prevM = NULL, *prevB = NULL;
	Column *last = NULL, *prev, *walk;
	int index = 0;

	prev = NULL;
	walk = self->columns;
	while (walk != NULL)
	{
		if (index == columnIndex)
		{
			prevM = prev;
			move = walk;
		}
		if (index == beforeIndex)
		{
			prevB = prev;
			before = walk;
		}
		prev = walk;
		if (walk->next == NULL)
			last = walk;
		index++;
		walk = walk->next;
	}

	if (move == NULL)
		return;

	if (prevM == NULL)
		self->columns = move->next;
	else
		prevM->next = move->next;
	if (before == NULL)
	{
		last->next = move;
		move->next = NULL;
	}
	else
	{
		if (prevB == NULL)
			self->columns = move;
		else
			prevB->next = move;
		move->next = before;
	}
}

void TreeItem_FreeResources(TreeCtrl *tree, TreeItem item_)
{
	Item *self = (Item *) item_;
	Column *column;

	column = self->columns;
	while (column != NULL)
		column = Column_FreeResources(tree, column);
	if (self->dInfo != NULL)
		Tree_FreeItemDInfo(tree, item_, NULL);
	if (self->rInfo != NULL)
		Tree_FreeItemRInfo(tree, item_);
	WFREE(self, Item);
}

int TreeItem_NeededHeight(TreeCtrl *tree, TreeItem item_)
{
	Item *self = (Item *) item_;
	Column *column;

	self->neededHeight = 0;
	for (column = self->columns; column != NULL; column = column->next)
	{
		if (column->style != NULL)
		{
			self->neededHeight = MAX(self->neededHeight,
				TreeStyle_NeededHeight(tree, column->style, self->state));
		}
	}
	return self->neededHeight;
}

int TreeItem_UseHeight(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	Column *column = item->columns;
	TreeColumn treeColumn = tree->columns;
	StyleDrawArgs drawArgs;
	int height = 0;

	drawArgs.tree = tree;
	drawArgs.state = item->state;

	while (column != NULL)
	{
		if (TreeColumn_Visible(treeColumn) && (column->style != NULL))
		{
			drawArgs.style = column->style;
			if (TreeColumn_FixedWidth(treeColumn) != -1)
			{
				drawArgs.width = TreeColumn_UseWidth(treeColumn);
				if (TreeColumn_Index(treeColumn) == tree->columnTree)
					drawArgs.width -= TreeItem_Indent(tree, item_);
			}
			else
				drawArgs.width = -1;
			height = MAX(height, TreeStyle_UseHeight(&drawArgs));
		}
		treeColumn = TreeColumn_Next(treeColumn);
		column = column->next;
	}

	return height;
}

int TreeItem_Height(TreeCtrl *tree, TreeItem item_)
{
	Item *self = (Item *) item_;
	int buttonHeight = 0;
	int useHeight;

	if (!self->isVisible || (ISROOT(self) && !tree->showRoot))
		return 0;

	/* Update column + style + element sizes */
	useHeight = TreeItem_UseHeight(tree, item_);

	/* Can't have less height than our button */
	if (tree->showButtons && self->hasButton && (!ISROOT(self) || tree->showRootButton))
	{
		buttonHeight = (self->state & STATE_OPEN) ?
			tree->openButtonHeight : tree->closedButtonHeight;
	}

	/* User specified a fixed height for this item */
	if (self->fixedHeight > 0)
		return MAX(self->fixedHeight, buttonHeight);

	/* Fixed height of all items */
	if (tree->itemHeight > 0)
		return MAX(tree->itemHeight, buttonHeight);

	/* No fixed height specified */
	return MAX(useHeight, buttonHeight);
}

void TreeItem_InvalidateHeight(TreeCtrl *tree, TreeItem item_)
{
	Item *self = (Item *) item_;

	if (self->neededHeight < 0)
		return;
	self->neededHeight = -1;
}

static Column *Item_CreateColumn(TreeCtrl *tree, Item *self, int columnIndex, int *isNew)
{
	Column *column;
	int i;

	if (isNew != NULL) (*isNew) = FALSE;
	column = self->columns;
	if (column == NULL)
	{
		column = Column_Alloc();
		column->neededWidth = column->neededHeight = -1;
		self->columns = column;
		if (isNew != NULL) (*isNew) = TRUE;
	}
	for (i = 0; i < columnIndex; i++)
	{
		if (column->next == NULL)
		{
			column->next = Column_Alloc();
			column->next->neededWidth = column->next->neededHeight = -1;
			if (isNew != NULL) (*isNew) = TRUE;
		}
		column = column->next;
	}

	Tree_CreateColumn(tree, columnIndex, NULL);

	return column;
}

static Column *Item_FindColumn(TreeCtrl *tree, Item *self, int columnIndex)
{
	Column *column;
	int i = 0;

	column = self->columns;
	if (!column)
		return NULL;
	while (column != NULL && i < columnIndex)
	{
		column = column->next;
		i++;
	}
	return column;
}

static int Item_FindColumnFromObj(TreeCtrl *tree, Item *item, Tcl_Obj *obj,
	Column **column, int *indexPtr)
{
	int columnIndex;

	if (Tcl_GetIntFromObj(NULL, obj, &columnIndex) == TCL_OK)
	{
		if (columnIndex < 0)
		{
			FormatResult(tree->interp, "bad column index \"%d\": must be > 0",
				columnIndex);
			return TCL_ERROR;
		}
	}
	else
	{
		TreeColumn treeColumn;

		if (Tree_FindColumnByTag(tree, obj, &treeColumn, CFO_NOT_TAIL) != TCL_OK)
			return TCL_ERROR;
		columnIndex = TreeColumn_Index(treeColumn);
	}
	(*column) = Item_FindColumn(tree, item, columnIndex);
	if ((*column) == NULL)
	{
		FormatResult(tree->interp, "item %d doesn't have column %d",
			item->id, columnIndex);
		return TCL_ERROR;
	}
	if (indexPtr != NULL)
		(*indexPtr) = columnIndex;
	return TCL_OK;
}

TreeItemColumn TreeItem_FindColumn(TreeCtrl *tree, TreeItem item, int columnIndex)
{
	return (TreeItemColumn) Item_FindColumn(tree, (Item *) item, columnIndex);
}

int TreeItem_ColumnFromObj(TreeCtrl *tree, TreeItem item, Tcl_Obj *obj, TreeItemColumn *columnPtr, int *indexPtr)
{
	return Item_FindColumnFromObj(tree, (Item *) item, obj, (Column **) columnPtr, indexPtr);
}

static int Item_CreateColumnFromObj(TreeCtrl *tree, Item *item, Tcl_Obj *obj, Column **column, int *indexPtr)
{
	int columnIndex;

	if (Tcl_GetIntFromObj(NULL, obj, &columnIndex) == TCL_OK)
	{
		if (columnIndex < 0)
		{
			FormatResult(tree->interp,
				"bad column index \"%d\": must be >= 0",
				columnIndex);
			return TCL_ERROR;
		}
	}
	else
	{
		TreeColumn treeColumn;

		if (Tree_FindColumnByTag(tree, obj, &treeColumn, CFO_NOT_TAIL) != TCL_OK)
			return TCL_ERROR;
		columnIndex = TreeColumn_Index(treeColumn);
	}
	(*column) = Item_CreateColumn(tree, item, columnIndex, NULL);
	if (indexPtr != NULL)
		(*indexPtr) = columnIndex;
	return TCL_OK;
}

int TreeItem_Indent(TreeCtrl *tree, TreeItem item_)
{
	Item *self = (Item *) item_;
	int indent;

	if (ISROOT(self))
		return (tree->showRoot && tree->showButtons && tree->showRootButton) ? tree->useIndent : 0;

	if (tree->updateIndex)
		Tree_UpdateItemIndex(tree);

	indent = tree->useIndent * self->depth;
	if (tree->showRoot || tree->showButtons || tree->showLines)
		indent += tree->useIndent;
	if (tree->showRoot && tree->showButtons && tree->showRootButton)
		indent += tree->useIndent;
	return indent;
}

static void ItemDrawBackground(TreeCtrl *tree, TreeColumn treeColumn,
	Item *item, Column *column, Drawable drawable, int x, int y, int width,
	int height, int index)
{
	GC gc = None;

	gc = TreeColumn_BackgroundGC(treeColumn, index);
	if (gc == None)
		gc = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);
	XFillRectangle(tree->display, drawable, gc, x, y, width, height);
}

void TreeItem_Draw(TreeCtrl *tree, TreeItem item_, int x, int y,
	int width, int height, Drawable drawable, int minX, int maxX, int index)
{
	Item *self = (Item *) item_;
	int indent, columnWidth, totalWidth;
	Column *column;
	StyleDrawArgs drawArgs;
	TreeColumn treeColumn;

	drawArgs.tree = tree;
	drawArgs.drawable = drawable;
	drawArgs.state = self->state;

	totalWidth = 0;
	treeColumn = tree->columns;
	column = self->columns;
	while (treeColumn != NULL)
	{
		if (!TreeColumn_Visible(treeColumn))
			columnWidth = 0;
		else if (tree->columnCountVis == 1)
			columnWidth = width;
		else
			columnWidth = TreeColumn_UseWidth(treeColumn);
		if (columnWidth > 0)
		{
			if (TreeColumn_Index(treeColumn) == tree->columnTree)
			{
				indent = TreeItem_Indent(tree, item_);
#if 0
				/* This means the tree lines/buttons don't share the item background color */
				if ((x + totalWidth < maxX) &&
					(x + totalWidth + indent > minX))
				{
					GC gc = Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC);
					XFillRectangle(tree->display, drawable, gc,
						x + totalWidth, y, indent, height);
				}
#endif
			}
			else
				indent = 0;
			if ((x /* + indent */ + totalWidth < maxX) &&
				(x + totalWidth + columnWidth > minX))
			{
				ItemDrawBackground(tree, treeColumn, self, column, drawable,
					x + totalWidth /* + indent*/ , y,
					columnWidth /* - indent */, height,
					index);
				if ((column != NULL) && (column->style != NULL))
				{
					drawArgs.style = column->style;
					drawArgs.x = x + indent + totalWidth;
					drawArgs.y = y;
					drawArgs.width = columnWidth - indent;
					drawArgs.height = height;
					drawArgs.justify = TreeColumn_Justify(treeColumn);
					TreeStyle_Draw(&drawArgs);
				}
			}
			totalWidth += columnWidth;
		}
		treeColumn = TreeColumn_Next(treeColumn);
		if (column != NULL)
			column = column->next;
	}
}

void TreeItem_DrawLines(TreeCtrl *tree, TreeItem item_, int x, int y, int width, int height, Drawable drawable)
{
	Item *self = (Item *) item_;
	Item *item, *parent;
	int indent, left, lineLeft, lineTop;
	int hasPrev, hasNext;
	int i, vert = 0;

	indent = TreeItem_Indent(tree, item_);

	/* Left edge of button/line area */
	left = x + tree->columnTreeLeft + indent - tree->useIndent;

	/* Left edge of vertical line */
	lineLeft = left + (tree->useIndent - tree->lineThickness) / 2;

	/* Top edge of horizontal line */
	lineTop = y + (height - tree->lineThickness) / 2;

	/* NOTE: The next three checks do not call TreeItem_ReallyVisible()
	 * since 'self' is ReallyVisible */

	/* Check for ReallyVisible previous sibling */
	item = self->prevSibling;
	while ((item != NULL) && !item->isVisible)
		item = item->prevSibling;
	hasPrev = (item != NULL);

	/* Check for ReallyVisible parent */
	if ((self->parent != NULL) && (!ISROOT(self->parent) || tree->showRoot))
		hasPrev = TRUE;

	/* Check for ReallyVisible next sibling */
	item = self->nextSibling;
	while ((item != NULL) && !item->isVisible)
		item = item->nextSibling;
	hasNext = (item != NULL);

	/* Vertical line to parent and/or previous/next sibling */
	if (hasPrev || hasNext)
	{
		int top = y, bottom = y + height;

		if (!hasPrev)
			top = lineTop;
		if (!hasNext)
			bottom = lineTop + tree->lineThickness;

		if (tree->lineStyle == LINE_STYLE_DOT)
		{
			for (i = 0; i < tree->lineThickness; i++)
				VDotLine(tree, drawable, tree->lineGC,
					lineLeft + i,
					top,
					bottom);
		}
		else
			XFillRectangle(tree->display, drawable, tree->lineGC,
				lineLeft,
				top,
				tree->lineThickness,
				bottom - top);

		/* Don't overlap horizontal line */
		vert = tree->lineThickness;
	}

	/* Horizontal line to self */
	if (!ISROOT(self) || (tree->showRoot && tree->showButtons && tree->showRootButton))
	{
		if (tree->lineStyle == LINE_STYLE_DOT)
		{
			for (i = 0; i < tree->lineThickness; i++)
				HDotLine(tree, drawable, tree->lineGC,
					lineLeft + vert,
					lineTop + i,
					x + tree->columnTreeLeft + indent);
		}
		else
			XFillRectangle(tree->display, drawable, tree->lineGC,
				lineLeft + vert,
				lineTop,
				left + tree->useIndent - (lineLeft + vert),
				tree->lineThickness);
	}

	/* Vertical lines from ancestors to their next siblings */
	for (parent = self->parent;
		parent != NULL;
		parent = parent->parent)
	{
		lineLeft -= tree->useIndent;

		/* Check for ReallyVisible next sibling */
		item = parent->nextSibling;
		while ((item != NULL) && !item->isVisible)
			item = item->nextSibling;

		if (item != NULL)
		{
			if (tree->lineStyle == LINE_STYLE_DOT)
			{
				for (i = 0; i < tree->lineThickness; i++)
					VDotLine(tree, drawable, tree->lineGC,
						lineLeft + i,
						y,
						y + height);
			}
			else
				XFillRectangle(tree->display, drawable, tree->lineGC,
					lineLeft,
					y,
					tree->lineThickness,
					height);
		}
	}
}

void TreeItem_DrawButton(TreeCtrl *tree, TreeItem item_, int x, int y, int width, int height, Drawable drawable)
{
	Item *self = (Item *) item_;
	int indent, left, lineLeft, lineTop;
	Tk_Image image = NULL;
	Pixmap bitmap = None;
	int imgW, imgH;
	int buttonLeft, buttonTop, w1;
	int offset = 0;

#if defined(MAC_TCL) || defined(MAC_OSX_TK)
	/* QuickDraw on Mac is offset by one pixel in both x and y. */
	offset = 1;
#endif

	if (!self->hasButton)
		return;
	if (ISROOT(self) && !tree->showRootButton)
		return;

	indent = TreeItem_Indent(tree, item_);

	/* Left edge of button/line area */
	left = x + tree->columnTreeLeft + indent - tree->useIndent;

	if (self->state & STATE_OPEN)
	{
		imgW = tree->openButtonWidth;
		imgH = tree->openButtonHeight;
		if (tree->openButtonImage != NULL)
			image = tree->openButtonImage;
		else if (tree->openButtonBitmap != None)
			bitmap = tree->openButtonBitmap;
	}
	else
	{
		imgW = tree->closedButtonWidth;
		imgH = tree->closedButtonHeight;
		if (tree->closedButtonImage != NULL)
			image = tree->closedButtonImage;
		else if (tree->closedButtonBitmap != None)
			bitmap = tree->closedButtonBitmap;
	}
	if (image != NULL)
	{
		Tk_RedrawImage(image, 0, 0, imgW, imgH, drawable,
			left + (tree->useIndent - imgW) / 2,
			y + (height - imgH) / 2);
		return;
	}
	if (bitmap != None)
	{
		GC gc = (self->state & STATE_OPEN) ? tree->buttonOpenGC : tree->buttonClosedGC;
		int bx = left + (tree->useIndent - imgW) / 2;
		int by = y + (height - imgH) / 2;

		XSetClipOrigin(tree->display, gc, bx, by);
		XCopyPlane(tree->display, bitmap, drawable, gc,
			0, 0, imgW, imgH,
			bx, by, 1);
		XSetClipOrigin(tree->display, gc, 0, 0);
		return;
	}

	w1 = tree->buttonThickness / 2;

	/* Left edge of vertical line */
	/* Make sure this matches TreeItem_DrawLines() */
	lineLeft = left + (tree->useIndent - tree->buttonThickness) / 2;

	/* Top edge of horizontal line */
	/* Make sure this matches TreeItem_DrawLines() */
	lineTop = y + (height - tree->buttonThickness) / 2;

	buttonLeft = left + (tree->useIndent - tree->buttonSize) / 2;
	buttonTop = y + (height - tree->buttonSize) / 2;

	/* Erase button background */
	XFillRectangle(tree->display, drawable,
		Tk_3DBorderGC(tree->tkwin, tree->border, TK_3D_FLAT_GC),
		buttonLeft + tree->buttonThickness,
		buttonTop + tree->buttonThickness,
		tree->buttonSize - tree->buttonThickness,
		tree->buttonSize - tree->buttonThickness);

	/* Draw button outline */
	XDrawRectangle(tree->display, drawable, tree->buttonGC,
		buttonLeft + w1,
		buttonTop + w1,
		tree->buttonSize - tree->buttonThickness + offset,
		tree->buttonSize - tree->buttonThickness + offset);

	/* Horizontal '-' */
	XFillRectangle(tree->display, drawable, tree->buttonGC,
		buttonLeft + tree->buttonThickness * 2,
		lineTop,
		tree->buttonSize - tree->buttonThickness * 4,
		tree->buttonThickness);

	if (!(self->state & STATE_OPEN))
	{
		/* Finish '+' */
		XFillRectangle(tree->display, drawable, tree->buttonGC,
			lineLeft,
			buttonTop + tree->buttonThickness * 2,
			tree->buttonThickness,
			tree->buttonSize - tree->buttonThickness * 4);
	}
}

int TreeItem_ReallyVisible(TreeCtrl *tree, TreeItem item_)
{
	Item *self = (Item *) item_;

	if (!tree->updateIndex)
		return self->indexVis != -1;

	if (!self->isVisible)
		return 0;
	if (self->parent == NULL)
		return ISROOT(self) ? tree->showRoot : 0;
	if (ISROOT(self->parent))
	{
		if (!self->parent->isVisible)
			return 0;
		if (!tree->showRoot)
			return 1;
		if (!(self->parent->state & STATE_OPEN))
			return 0;
	}
	if (!self->parent->isVisible || !(self->parent->state & STATE_OPEN))
		return 0;
	return TreeItem_ReallyVisible(tree, (TreeItem) self->parent);
}

TreeItem TreeItem_RootAncestor(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;

	while (item->parent != NULL)
		item = item->parent;
	return (TreeItem) item;
}

int TreeItem_IsAncestor(TreeCtrl *tree, TreeItem item1, TreeItem item2)
{
	if (item1 == item2)
		return 0;
	while (item2 && item2 != item1)
		item2 = (TreeItem) ((Item *) item2)->parent;
	return item2 != NULL;
}

Tcl_Obj *TreeItem_ToObj(TreeCtrl *tree, TreeItem item_)
{
	return Tcl_NewIntObj(((Item *) item_)->id);
}

static int ItemElementCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	static CONST char *commandNames[] = { "actual", "cget", "configure", (char *) NULL };
	enum { COMMAND_ACTUAL, COMMAND_CGET, COMMAND_CONFIGURE };
	int index;
	int columnIndex;
	Column *column;
	Item *item;

	if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
		&index) != TCL_OK)
		return TCL_ERROR;

	if (TreeItem_FromObj(tree, objv[4], (TreeItem *) &item, 0) != TCL_OK)
		return TCL_ERROR;

	if (Item_FindColumnFromObj(tree, item, objv[5], &column, &columnIndex) != TCL_OK)
		return TCL_ERROR;

	if (column->style == NULL)
	{
		FormatResult(interp, "item %d column %d has no style",
			item->id, columnIndex);
		return TCL_ERROR;
	}

	switch (index)
	{
		/* T item element actual I C E option */
		case COMMAND_ACTUAL:
		{
			if (objc != 8)
			{
				Tcl_WrongNumArgs(tree->interp, 4, objv,
					"item column element option");
				return TCL_ERROR;
			}
			return TreeStyle_ElementActual(tree, column->style, item->state,
				objv[6], objv[7]);
		}

		/* T item element cget I C E option */
		case COMMAND_CGET:
		{
			if (objc != 8)
			{
				Tcl_WrongNumArgs(tree->interp, 4, objv,
					"item column element option");
				return TCL_ERROR;
			}
			return TreeStyle_ElementCget(tree, column->style, objv[6], objv[7]);
		}

		/* T item element configure I C E ... */
		case COMMAND_CONFIGURE:
		{
			int result, eMask;

			result = TreeStyle_ElementConfigure(tree, column->style, objv[6],
				objc - 7, (Tcl_Obj **) objv + 7, &eMask);
			if (eMask != 0)
			{
				if (eMask & CS_DISPLAY)
					Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
				if (eMask & CS_LAYOUT)
				{
					column->neededWidth = column->neededHeight = -1;
					Tree_InvalidateColumnWidth(tree, columnIndex);
					TreeItem_InvalidateHeight(tree, (TreeItem) item);
					Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
				}
			}
			return result;
		}
	}

	return TCL_OK;
}

static int ItemStyleCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	static CONST char *commandNames[] = { "elements", "map", "set", (char *) NULL };
	enum { COMMAND_ELEMENTS, COMMAND_MAP, COMMAND_SET };
	int index;
	Item *item;

	if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
		&index) != TCL_OK)
	{
		return TCL_ERROR;
	}

	if (TreeItem_FromObj(tree, objv[4], (TreeItem *) &item, 0) != TCL_OK)
	{
		return TCL_ERROR;
	}

	switch (index)
	{
		/* T item style elements I C */
		case COMMAND_ELEMENTS:
		{
			Column *column;
			int columnIndex;

			if (objc != 6)
			{
				Tcl_WrongNumArgs(interp, 4, objv, "item column");
				return TCL_ERROR;
			}
			if (Item_FindColumnFromObj(tree, item, objv[5], &column, &columnIndex) != TCL_OK)
			{
				return TCL_ERROR;
			}
			if (column->style == NULL)
			{
				FormatResult(interp, "item %d column %d has no style",
					item->id, columnIndex);
				return TCL_ERROR;
			}
			TreeStyle_ListElements(tree, column->style);
			break;
		}

		/* T item style map I C S map */
		case COMMAND_MAP:
		{
			TreeStyle style;
			Column *column;
			int columnIndex;
			int objcM;
			Tcl_Obj **objvM;

			if (objc != 8)
			{
				Tcl_WrongNumArgs(interp, 4, objv, "item column style map");
				return TCL_ERROR;
			}
			if (Item_CreateColumnFromObj(tree, item, objv[5], &column, &columnIndex) != TCL_OK)
				return TCL_ERROR;
			if (TreeStyle_FromObj(tree, objv[6], &style) != TCL_OK)
				return TCL_ERROR;
			if (column->style != NULL)
			{
				if (Tcl_ListObjGetElements(interp, objv[7], &objcM, &objvM) != TCL_OK)
					return TCL_ERROR;
				if (objcM & 1)
				{
					FormatResult(interp, "list must contain even number of elements");
					return TCL_ERROR;
				}
				if (TreeStyle_Remap(tree, column->style, style, objcM, objvM) != TCL_OK)
					return TCL_ERROR;
			}
			else
				column->style = TreeStyle_NewInstance(tree, style);
			Tree_InvalidateColumnWidth(tree, columnIndex);
			TreeItem_InvalidateHeight(tree, (TreeItem) item);
			Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
			break;
		}

		/* T item style set I ?C? ?S? ?C S ...?*/
		case COMMAND_SET:
		{
			TreeStyle style;
			Column *column;
			int i, columnIndex, length;

			if (objc < 5)
			{
				Tcl_WrongNumArgs(interp, 4, objv, "item ?column? ?style? ?column style ...?");
				return TCL_ERROR;
			}
			if (objc == 5)
			{
				Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
				column = item->columns;
				while (column != NULL)
				{
					if (column->style != NULL)
						Tcl_ListObjAppendElement(interp, listObj,
							TreeStyle_ToObj(column->style));
					else
						Tcl_ListObjAppendElement(interp, listObj,
							Tcl_NewObj());
					column = column->next;
				}
				Tcl_SetObjResult(interp, listObj);
				break;
			}
			if (objc == 6)
			{
				if (Item_FindColumnFromObj(tree, item, objv[5], &column, NULL) != TCL_OK)
					return TCL_ERROR;
				if (column->style != NULL)
					Tcl_SetObjResult(interp, TreeStyle_ToObj(column->style));
				break;
			}
			if ((objc - 5) & 1)
				return TCL_ERROR;
			for (i = 5; i < objc; i += 2)
			{
				if (Item_CreateColumnFromObj(tree, item, objv[i], &column, &columnIndex) != TCL_OK)
					return TCL_ERROR;
				(void) Tcl_GetStringFromObj(objv[i + 1], &length);
				if (length == 0)
				{
					if (column->style == NULL)
						continue;
					TreeItemColumn_ForgetStyle(tree, (TreeItemColumn) column);
				}
				else
				{
					if (TreeStyle_FromObj(tree, objv[i + 1], &style) != TCL_OK)
						return TCL_ERROR;
					TreeItemColumn_ForgetStyle(tree, (TreeItemColumn) column);
					column->style = TreeStyle_NewInstance(tree, style);
				}
				Tree_InvalidateColumnWidth(tree, columnIndex);
				TreeItem_InvalidateHeight(tree, (TreeItem) item);
				Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
			}
			break;
		}
	}

	return TCL_OK;
}

#if 0
T item sort I
	-first I (default firstchild)
	-last I (default lastchild)
	-command $cmd
	-dictionary
	-integer
	-real
	-increasing
	-decreasing
	-column C (default 0)
	-element E (default first "text")
#endif

/* one per column per SortItem */
struct SortItem1
{
	long longValue;
	double doubleValue;
	char *string;
};

/* one per Item */
struct SortItem
{
	Item *item;
	struct SortItem1 *item1;
	Tcl_Obj *obj; /* TreeItem_ToObj() */
};

typedef struct SortData SortData;

/* Used to process -element option */
struct SortElement
{
	TreeStyle style;
	TreeElement elem;
	int elemIndex;
};

/* One per TreeColumn */
struct SortColumn
{
	int (*proc)(SortData *, struct SortItem *, struct SortItem *, int); 
	int sortBy;
	int column;
	int order;
	Tcl_Obj *command;
	struct SortElement elems[20];
	int elemCount;
};

/* Data for sort as a whole */
struct SortData
{
	TreeCtrl *tree;
	struct SortItem *items;
	struct SortItem1 *item1s; /* SortItem.item1 points in here */
#define MAX_SORT_COLUMNS 40
	struct SortColumn columns[MAX_SORT_COLUMNS];
	int count; /* max number of columns to compare */
	int result;
};

/* from Tcl 8.4.0 */
static int DictionaryCompare(char *left, char *right)
{
	Tcl_UniChar uniLeft, uniRight, uniLeftLower, uniRightLower;
	int diff, zeros;
	int secondaryDiff = 0;

	while (1)
	{
		if (isdigit(UCHAR(*right))	/* INTL: digit */
			&& isdigit(UCHAR(*left)))
		{	/* INTL: digit */
			/*
			 * There are decimal numbers embedded in the two
			 * strings.  Compare them as numbers, rather than
			 * strings.  If one number has more leading zeros than
			 * the other, the number with more leading zeros sorts
			 * later, but only as a secondary choice.
			 */

			zeros = 0;
			while ((*right == '0') && (isdigit(UCHAR(right[1]))))
			{
				right++;
				zeros--;
			}
			while ((*left == '0') && (isdigit(UCHAR(left[1]))))
			{
				left++;
				zeros++;
			}
			if (secondaryDiff == 0)
			{
				secondaryDiff = zeros;
			}

			/*
			 * The code below compares the numbers in the two
			 * strings without ever converting them to integers.  It
			 * does this by first comparing the lengths of the
			 * numbers and then comparing the digit values.
			 */

			diff = 0;
			while (1)
			{
				if (diff == 0)
				{
					diff = UCHAR(*left) - UCHAR(*right);
				}
				right++;
				left++;
				if (!isdigit(UCHAR(*right)))
				{	/* INTL: digit */
					if (isdigit(UCHAR(*left)))
					{	/* INTL: digit */
						return 1;
					}
					else
					{
						/*
						 * The two numbers have the same length. See
						 * if their values are different.
						 */

						if (diff != 0)
						{
							return diff;
						}
						break;
					}
				}
				else if (!isdigit(UCHAR(*left)))
				{	/* INTL: digit */
					return -1;
				}
			}
			continue;
		}

		/*
		 * Convert character to Unicode for comparison purposes.  If either
		 * string is at the terminating null, do a byte-wise comparison and
		 * bail out immediately.
		 */

		if ((*left != '\0') && (*right != '\0'))
		{
			left += Tcl_UtfToUniChar(left, &uniLeft);
			right += Tcl_UtfToUniChar(right, &uniRight);
			/*
			 * Convert both chars to lower for the comparison, because
			 * dictionary sorts are case insensitve.  Covert to lower, not
			 * upper, so chars between Z and a will sort before A (where most
			 * other interesting punctuations occur)
			 */
			uniLeftLower = Tcl_UniCharToLower(uniLeft);
			uniRightLower = Tcl_UniCharToLower(uniRight);
		}
		else
		{
			diff = UCHAR(*left) - UCHAR(*right);
			break;
		}

		diff = uniLeftLower - uniRightLower;
		if (diff)
		{
			return diff;
		}
		else if (secondaryDiff == 0)
		{
			if (Tcl_UniCharIsUpper(uniLeft) &&
				Tcl_UniCharIsLower(uniRight))
			{
				secondaryDiff = -1;
			}
			else if (Tcl_UniCharIsUpper(uniRight) &&
				Tcl_UniCharIsLower(uniLeft))
			{
				secondaryDiff = 1;
			}
		}
	}
	if (diff == 0)
	{
		diff = secondaryDiff;
	}
	return diff;
}

static int CompareAscii(SortData *sortData, struct SortItem *a, struct SortItem *b, int n)
{
	return strcmp(a->item1[n].string, b->item1[n].string);
}

static int CompareDict(SortData *sortData, struct SortItem *a, struct SortItem *b, int n)
{
	return DictionaryCompare(a->item1[n].string, b->item1[n].string);
}

static int CompareDouble(SortData *sortData, struct SortItem *a, struct SortItem *b, int n)
{
	return (a->item1[n].doubleValue < b->item1[n].doubleValue) ? -1 :
		((a->item1[n].doubleValue == b->item1[n].doubleValue) ? 0 : 1);
}

static int CompareLong(SortData *sortData, struct SortItem *a, struct SortItem *b, int n)
{
	return (a->item1[n].longValue < b->item1[n].longValue) ? -1 :
		((a->item1[n].longValue == b->item1[n].longValue) ? 0 : 1);
}

static int CompareCmd(SortData *sortData, struct SortItem *a, struct SortItem *b, int n)
{
	Tcl_Interp *interp = sortData->tree->interp;
	Tcl_Obj **objv, *paramObjv[2];
	int objc, v;

	paramObjv[0] = a->obj;
	paramObjv[1] = b->obj;

	Tcl_ListObjLength(interp, sortData->columns[n].command, &objc);
	Tcl_ListObjReplace(interp, sortData->columns[n].command, objc - 2,
		2, 2, paramObjv);
   	Tcl_ListObjGetElements(interp, sortData->columns[n].command,
		&objc, &objv);

	sortData->result = Tcl_EvalObjv(interp, objc, objv, 0);
  
	if (sortData->result != TCL_OK)
	{
		Tcl_AddErrorInfo(interp, "\n    (evaluating item sort -command)");
		return 0;
	}

	sortData->result = Tcl_GetIntFromObj(interp, Tcl_GetObjResult(interp), &v);
	if (sortData->result != TCL_OK)
	{
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
			"-command returned non-numeric result", -1);
		return 0;
	}

	return v;
}

static int CompareProc(SortData *sortData, struct SortItem *a, struct SortItem *b)
{
	int i, v;

	for (i = 0; i < sortData->count; i++)
	{
		v = (*sortData->columns[i].proc)(sortData, a, b, i);

		/* -command returned error */
		if (sortData->result != TCL_OK)
			return 0;

		if (v != 0)
		{
			if (i && (sortData->columns[i].order != sortData->columns[0].order))
				v *= -1;
			return v;
		}
	}
	return 0;
}

/* BEGIN custom quicksort() */

static int find_pivot(SortData *sortData, struct SortItem *left, struct SortItem *right, struct SortItem *pivot)
{
	struct SortItem *a, *b, *c, *p;
	int v;

	a = left;
	b = (left + (right - left) / 2);
	c = right;

	v = CompareProc(sortData, a, b);
	if (sortData->result != TCL_OK)
		return 0;
	if (v < 0) { p = a; a = b; b = p; }

	v = CompareProc(sortData, a, c);
	if (sortData->result != TCL_OK)
		return 0;
	if (v < 0) { p = a; a = c; c = p; }

	v = CompareProc(sortData, b, c);
	if (sortData->result != TCL_OK)
		return 0;
	if (v < 0) { p = b; b = c; c = p; }

	v = CompareProc(sortData, a, b);
	if (sortData->result != TCL_OK)
		return 0;
	if (v < 0)
	{
		(*pivot) = *b;
		return 1;
	}

	v = CompareProc(sortData, b, c);
	if (sortData->result != TCL_OK)
		return 0;
	if (v < 0)
	{
		(*pivot) = *c;
		return 1;
	}

	for (p = left + 1; p <= right; p++)
	{
		int v = CompareProc(sortData, p, left);
		if (sortData->result != TCL_OK)
			return 0;
		if (v != 0)
		{
			(*pivot) = (v < 0) ? *left : *p;
			return 1;
		}
	}
	return 0;
}

/* If the user provides a -command which does not properly compare two
 * elements, quicksort may go into an infinite loop or access illegal memory.
 * This #define indicates parts of the code which are not part of a normal
 * quicksort, but are present to detect the aforementioned bugs. */
#define BUGGY_COMMAND

static struct SortItem *partition(SortData *sortData, struct SortItem *left, struct SortItem *right, struct SortItem *pivot)
{
	int v;
#ifdef BUGGY_COMMAND
	struct SortItem *min = left, *max = right;
#endif

	while (left <= right)
	{
		/*
			while (*left < *pivot)
				++left;
		*/
		while (1)
		{
			v = CompareProc(sortData, left, pivot);
			if (sortData->result != TCL_OK)
				return NULL;
			if (v >= 0)
				break;
#ifdef BUGGY_COMMAND
			/* If -command always returns < 0, 'left' becomes invalid */
			if (left == max)
				goto buggy;
#endif
			left++;
		}
		/*
			while (*right >= *pivot)
				--right;
		*/
		while (1)
		{
			v = CompareProc(sortData, right, pivot);
			if (sortData->result != TCL_OK)
				return NULL;
			if (v < 0)
				break;
#ifdef BUGGY_COMMAND
			/* If -command always returns >= 0, 'right' becomes invalid */
			if (right == min)
				goto buggy;
#endif
			right--;
		}
		if (left < right)
		{
			struct SortItem tmp = *left;
			*left = *right;
			*right = tmp;
			left++;
			right--;
		}
	}
	return left;
#ifdef BUGGY_COMMAND
buggy:
	FormatResult(sortData->tree->interp, "buggy item sort -command detected");
	sortData->result = TCL_ERROR;
	return NULL;
#endif
}

static void quicksort(SortData *sortData, struct SortItem *left, struct SortItem *right)
{
	struct SortItem *p, pivot;

	if (sortData->result != TCL_OK)
		return;

	if (find_pivot(sortData, left, right, &pivot) == 1)
	{
		p = partition(sortData, left, right, &pivot);
		if (sortData->result != TCL_OK)
			return;

		quicksort(sortData, left, p - 1);
		if (sortData->result != TCL_OK)
			return;

		quicksort(sortData, p, right);
	}
}

/* END custom quicksort() */

int ItemSortCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	Item *item, *first, *last, *walk, *lastChild;
	Column *column;
	int i, j, count, elemIndex, index, indexF = 0, indexL = 0;
	int sawColumn = FALSE, sawCmd = FALSE;
	static int (*sortProc[5])(SortData *, struct SortItem *, struct SortItem *, int) =
		{ CompareAscii, CompareDict, CompareDouble, CompareLong, CompareCmd };
	SortData sortData;
	TreeColumn treeColumn;
	struct SortElement *elemPtr;
	int notReally = FALSE;
	int result = TCL_OK;

	if (TreeItem_FromObj(tree, objv[3], (TreeItem *) &item, 0) != TCL_OK)
		return TCL_ERROR;

	/* If the item has no children, then nothing is done and no error
	 * is generated. */
	if (item->numChildren < 1)
		return TCL_OK;

	/* Defaults: sort ascii strings in column 0 only */
	sortData.tree = tree;
	sortData.count = 1;
	sortData.columns[0].column = 0;
	sortData.columns[0].sortBy = SORT_ASCII;
	sortData.columns[0].order = 1;
	sortData.columns[0].elemCount = 0;
	sortData.result = TCL_OK;

	first = item->firstChild;
	last = item->lastChild;

	for (i = 4; i < objc; )
	{
		static CONST char *optionName[] = { "-ascii", "-column", "-command",
			"-decreasing", "-dictionary", "-element", "-first", "-increasing",
			"-integer", "-last", "-notreally", "-real", NULL };
		int numArgs[] = { 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 1, 1 };
		enum { OPT_ASCII, OPT_COLUMN, OPT_COMMAND, OPT_DECREASING, OPT_DICT,
			OPT_ELEMENT, OPT_FIRST, OPT_INCREASING, OPT_INTEGER, OPT_LAST,
			OPT_NOT_REALLY, OPT_REAL };

		if (Tcl_GetIndexFromObj(interp, objv[i], optionName, "option", 0,
			&index) != TCL_OK)
			return TCL_ERROR;
		if (objc - i < numArgs[index])
		{
			FormatResult(interp, "missing value for \"%s\" option",
				optionName[index]);
			return TCL_ERROR;
		}
		switch (index)
		{
			case OPT_ASCII:
				sortData.columns[sortData.count - 1].sortBy = SORT_ASCII;
				break;
			case OPT_COLUMN:
				if (TreeColumn_FromObj(tree, objv[i + 1], &treeColumn, CFO_NOT_TAIL) != TCL_OK)
					return TCL_ERROR;
				/* The first -column we see is the first column we compare */
				if (sawColumn)
				{
					if (sortData.count + 1 > MAX_SORT_COLUMNS)
					{
						FormatResult(interp,
							"can't compare more than %d columns",
							MAX_SORT_COLUMNS);
						return TCL_ERROR;
					}
					sortData.count++;
					/* Defaults for this column */
					sortData.columns[sortData.count - 1].sortBy = SORT_ASCII;
					sortData.columns[sortData.count - 1].order = 1;
					sortData.columns[sortData.count - 1].elemCount = 0;
				}
				sortData.columns[sortData.count - 1].column = TreeColumn_Index(treeColumn);
				sawColumn = TRUE;
				break;
			case OPT_COMMAND:
				sortData.columns[sortData.count - 1].command = objv[i + 1];
				sortData.columns[sortData.count - 1].sortBy = SORT_COMMAND;
				sawCmd = TRUE;
				break;
			case OPT_DECREASING:
				sortData.columns[sortData.count - 1].order = 0;
				break;
			case OPT_DICT:
				sortData.columns[sortData.count - 1].sortBy = SORT_DICT;
				break;
			case OPT_ELEMENT:
			{
				int listObjc;
				Tcl_Obj **listObjv;

				if (Tcl_ListObjGetElements(interp, objv[i + 1], &listObjc,
					&listObjv) != TCL_OK)
					return TCL_ERROR;
				elemPtr = sortData.columns[sortData.count - 1].elems;
				sortData.columns[sortData.count - 1].elemCount = 0;
				if (listObjc == 0)
				{
				}
				else if (listObjc == 1)
				{
					if (TreeElement_FromObj(tree, listObjv[0], &elemPtr->elem)
						!= TCL_OK)
					{
						Tcl_AddErrorInfo(interp,
							"\n    (processing -element option)");
						return TCL_ERROR;
					}
					if (!TreeElement_IsType(tree, elemPtr->elem, "text"))
					{
						FormatResult(interp,
							"element %s is not of type \"text\"",
							Tcl_GetString(listObjv[0]));
						Tcl_AddErrorInfo(interp,
							"\n    (processing -element option)");
						return TCL_ERROR;
					}
					elemPtr->style = NULL;
					elemPtr->elemIndex = -1;
					sortData.columns[sortData.count - 1].elemCount++;
				}
				else
				{
					if (listObjc & 1)
					{
						FormatResult(interp,
							"list must have even number of elements");
						Tcl_AddErrorInfo(interp,
							"\n    (processing -element option)");
						return TCL_ERROR;
					}
					for (j = 0; j < listObjc; j += 2)
					{
						if ((TreeStyle_FromObj(tree, listObjv[j],
							&elemPtr->style) != TCL_OK) ||
							(TreeElement_FromObj(tree, listObjv[j + 1],
							&elemPtr->elem) != TCL_OK) ||
							(TreeStyle_FindElement(tree, elemPtr->style,
							elemPtr->elem, &elemPtr->elemIndex) != TCL_OK))
						{
							Tcl_AddErrorInfo(interp,
								"\n    (processing -element option)");
							return TCL_ERROR;
						}
						if (!TreeElement_IsType(tree, elemPtr->elem, "text"))
						{
							FormatResult(interp,
								"element %s is not of type \"text\"",
								Tcl_GetString(listObjv[j + 1]));
							Tcl_AddErrorInfo(interp,
								"\n    (processing -element option)");
							return TCL_ERROR;
						}
						sortData.columns[sortData.count - 1].elemCount++;
						elemPtr++;
					}
				}
				break;
			}
			case OPT_FIRST:
				if (TreeItem_FromObj(tree, objv[i + 1], (TreeItem *) &first, 0) != TCL_OK)
					return TCL_ERROR;
				if (first->parent != item)
				{
					FormatResult(interp, "item %d is not a child of item %d",
						first->id, item->id);
					return TCL_ERROR;
				}
				break;
			case OPT_INCREASING:
				sortData.columns[sortData.count - 1].order = 1;
				break;
			case OPT_INTEGER:
				sortData.columns[sortData.count - 1].sortBy = SORT_LONG;
				break;
			case OPT_LAST:
				if (TreeItem_FromObj(tree, objv[i + 1], (TreeItem *) &last, 0) != TCL_OK)
					return TCL_ERROR;
				if (last->parent != item)
				{
					FormatResult(interp, "item %d is not a child of item %d",
						last->id, item->id);
					return TCL_ERROR;
				}
				break;
			case OPT_NOT_REALLY:
				notReally = TRUE;
				break;
			case OPT_REAL:
				sortData.columns[sortData.count - 1].sortBy = SORT_DOUBLE;
				break;
		}
		i += numArgs[index];
	}

	/* If there are no columns, we cannot perform a sort unless -command
	 * is specified. */
	if ((tree->columnCount < 1) && (sortData.columns[0].sortBy != SORT_COMMAND))
	{
		FormatResult(interp, "there are no columns");
		return TCL_ERROR;
	}

	if (first == last)
	{
		if (notReally)
			Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) first));
		return TCL_OK;
	}

	for (i = 0; i < sortData.count; i++)
	{
		sortData.columns[i].proc = sortProc[sortData.columns[i].sortBy];

		if (sortData.columns[i].sortBy == SORT_COMMAND)
		{
			Tcl_Obj *obj = Tcl_DuplicateObj(sortData.columns[i].command);
			Tcl_Obj *obj2 = Tcl_NewObj();
			Tcl_IncrRefCount(obj);
			if (Tcl_ListObjAppendElement(interp, obj, obj2) != TCL_OK)
			{
				Tcl_DecrRefCount(obj);
				Tcl_IncrRefCount(obj2);
				Tcl_DecrRefCount(obj2);
				/* FIXME: free other .command[] */
				return TCL_ERROR;
			}
			(void) Tcl_ListObjAppendElement(interp, obj, obj2);
			sortData.columns[i].command = obj;
		}
	}

	index = 0;
	walk = item->firstChild;
	while (walk != NULL)
	{
		if (walk == first)
			indexF = index;
		if (walk == last)
			indexL = index;
		index++;
		walk = walk->nextSibling;
	}
	if (indexF > indexL)
	{
		walk = last;
		last = first;
		first = walk;

		index = indexL;
		indexL = indexF;
		indexF = index;
	}
	count = indexL - indexF + 1;

	sortData.item1s = (struct SortItem1 *) ckalloc(sizeof(struct SortItem1) * count * sortData.count);
	sortData.items = (struct SortItem *) ckalloc(sizeof(struct SortItem) * count);
	for (i = 0; i < count; i++)
	{
		sortData.items[i].item1 = sortData.item1s + i * sortData.count;
		sortData.items[i].obj = NULL;
	}

	index = 0;
	walk = first;
	while (walk != last->nextSibling)
	{
		struct SortItem *sortItem = &sortData.items[index];

		sortItem->item = walk;
		if (sawCmd)
		{
			Tcl_Obj *obj = TreeItem_ToObj(tree, (TreeItem) walk);
			Tcl_IncrRefCount(obj);
			sortData.items[index].obj = obj;
		}
		for (i = 0; i < sortData.count; i++)
		{
			struct SortItem1 *sortItem1 = sortItem->item1 + i;

			if (sortData.columns[i].sortBy == SORT_COMMAND)
				continue;

			column = Item_FindColumn(tree, walk, sortData.columns[i].column);
			if (column == NULL)
			{
				FormatResult(interp, "item %d doesn't have column %d",
					walk->id, sortData.columns[i].column);
				result = TCL_ERROR;
				goto done;
			}
			if (column->style == NULL)
			{
				FormatResult(interp, "item %d column %d has no style",
					walk->id, sortData.columns[i].column);
				result = TCL_ERROR;
				goto done;
			}

			/* -element was empty. Find the first text element in the style */
			if (sortData.columns[i].elemCount == 0)
				elemIndex = -1;

			/* -element was element name. Find the element in the style */
			else if ((sortData.columns[i].elemCount == 1) &&
				(sortData.columns[i].elems[0].style == NULL))
			{
				if (TreeStyle_FindElement(tree, column->style,
					sortData.columns[i].elems[0].elem, &elemIndex) != TCL_OK)
				{
					result = TCL_ERROR;
					goto done;
				}
			}

			/* -element was style/element pair list */
			else
			{
				TreeStyle masterStyle = TreeStyle_GetMaster(tree, column->style);

				/* If the item style does not match any in the -element list,
				 * we will use the first text element in the item style. */
				elemIndex = -1;

				/* Match a style from the -element list. Look in reverse order
				 * to handle duplicates. */
				for (j = sortData.columns[i].elemCount - 1; j >= 0; j--)
				{
					if (sortData.columns[i].elems[j].style == masterStyle)
					{
						elemIndex = sortData.columns[i].elems[j].elemIndex;
						break;
					}
				}
			}
			if (TreeStyle_GetSortData(tree, column->style, elemIndex,
				sortData.columns[i].sortBy,
				&sortItem1->longValue,
				&sortItem1->doubleValue,
				&sortItem1->string) != TCL_OK)
			{
				result = TCL_ERROR;
				goto done;
			}
		}
		index++;
		walk = walk->nextSibling;
	}

	quicksort(&sortData, sortData.items, sortData.items + count - 1);

	if (sortData.result != TCL_OK)
	{
		result = sortData.result;
		goto done;
	}

	if (sawCmd)
		Tcl_ResetResult(interp);

	if (notReally)
	{
		Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
		Tcl_Obj *itemObj;

		/* Smallest to largest */
		if (sortData.columns[0].order == 1)
		{
			for (i = 0; i < count; i++)
			{
				itemObj = sortData.items[i].obj;
				if (itemObj == NULL)
					itemObj = TreeItem_ToObj(tree,
						(TreeItem) sortData.items[i].item);
				Tcl_ListObjAppendElement(interp, listObj, itemObj);
			}
		}

		/* Largest to smallest */
		else
		{
			for (i = count - 1; i >= 0; i--)
			{
				itemObj = sortData.items[i].obj;
				if (itemObj == NULL)
					itemObj = TreeItem_ToObj(tree,
						(TreeItem) sortData.items[i].item);
				Tcl_ListObjAppendElement(interp, listObj, itemObj);
			}
		}

		Tcl_SetObjResult(interp, listObj);
		goto done;
	}

	first = first->prevSibling;
	last = last->nextSibling;

	/* Smallest to largest */
	if (sortData.columns[0].order == 1)
	{
		for (i = 0; i < count - 1; i++)
		{
			sortData.items[i].item->nextSibling = sortData.items[i + 1].item;
			sortData.items[i + 1].item->prevSibling = sortData.items[i].item;
		}
		indexF = 0;
		indexL = count - 1;
	}

	/* Largest to smallest */
	else
	{
		for (i = count - 1; i > 0; i--)
		{
			sortData.items[i].item->nextSibling = sortData.items[i - 1].item;
			sortData.items[i - 1].item->prevSibling = sortData.items[i].item;
		}
		indexF = count - 1;
		indexL = 0;
	}

	lastChild = item->lastChild;

	sortData.items[indexF].item->prevSibling = first;
	if (first)
		first->nextSibling = sortData.items[indexF].item;
	else
		item->firstChild = sortData.items[indexF].item;

	sortData.items[indexL].item->nextSibling = last;
	if (last)
		last->prevSibling = sortData.items[indexL].item;
	else
		item->lastChild = sortData.items[indexL].item;

	/* Redraw the lines of the old/new lastchild */
	if ((item->lastChild != lastChild) && tree->showLines)
	{
		if (lastChild->dInfo != NULL)
			Tree_InvalidateItemDInfo(tree, (TreeItem) lastChild,
				(TreeItem) NULL);
		if (item->lastChild->dInfo != NULL)
			Tree_InvalidateItemDInfo(tree, (TreeItem) item->lastChild,
				(TreeItem) NULL);
	}

	tree->updateIndex = 1;
	Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

done:
	for (i = 0; i < count; i++)
		if (sortData.items[i].obj != NULL)
			Tcl_DecrRefCount(sortData.items[i].obj);
	for (i = 0; i < sortData.count; i++)
		if (sortData.columns[i].sortBy == SORT_COMMAND)
			Tcl_DecrRefCount(sortData.columns[i].command);
	ckfree((char *) sortData.item1s);
	ckfree((char *) sortData.items);

	if (tree->debug.enable && tree->debug.data)
		Tree_Debug(tree);

	return result;
}

static int ItemStateCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	static CONST char *commandNames[] = { "get", "set", (char *) NULL };
	enum { COMMAND_GET, COMMAND_SET };
	int index;
	Item *item;

	if (Tcl_GetIndexFromObj(interp, objv[3], commandNames, "command", 0,
		&index) != TCL_OK)
		return TCL_ERROR;

	switch (index)
	{
		/* T item state get I ?state? */
		case COMMAND_GET:
		{
			Tcl_Obj *listObj;
			int i, states[3];

			if (objc > 6)
			{
				Tcl_WrongNumArgs(interp, 5, objv, "?state?");
				return TCL_ERROR;
			}
			if (TreeItem_FromObj(tree, objv[4], (TreeItem *) &item, 0) != TCL_OK)
				return TCL_ERROR;
			if (objc == 6)
			{
				states[STATE_OP_ON] = 0;
				if (StateFromObj(tree, objv[5], states, NULL,
					SFO_NOT_OFF | SFO_NOT_TOGGLE) != TCL_OK)
					return TCL_ERROR;
				Tcl_SetObjResult(interp,
					Tcl_NewBooleanObj((item->state & states[STATE_OP_ON]) != 0));
				break;
			}
			listObj = Tcl_NewListObj(0, NULL);
			for (i = 0; i < 32; i++)
			{
				if (tree->stateNames[i] == NULL)
					continue;
				if (item->state & (1L << i))
				{
					Tcl_ListObjAppendElement(interp, listObj,
						Tcl_NewStringObj(tree->stateNames[i], -1));
				}
			}
			Tcl_SetObjResult(interp, listObj);
			break;
		}

		/* T item state set I ?I? {state ...} */
		case COMMAND_SET:
		{
			TreeItem item, itemFirst, itemLast;
			int i, states[3], stateOn, stateOff;
			int listObjc;
			Tcl_Obj **listObjv;

			if (objc < 6 || objc > 7)
			{
				Tcl_WrongNumArgs(interp, 5, objv, "?last? stateList");
				return TCL_ERROR;
			}
			if (TreeItem_FromObj(tree, objv[4], &itemFirst, IFO_ALLOK) != TCL_OK)
				return TCL_ERROR;
			if (objc == 6)
			{
				itemLast = itemFirst;
			}
			if (objc == 7)
			{
				if (TreeItem_FromObj(tree, objv[5], &itemLast, IFO_ALLOK) != TCL_OK)
					return TCL_ERROR;
			}
			states[0] = states[1] = states[2] = 0;
			if (Tcl_ListObjGetElements(interp, objv[objc - 1],
				&listObjc, &listObjv) != TCL_OK)
				return TCL_ERROR;
			if (listObjc == 0)
				break;
			for (i = 0; i < listObjc; i++)
			{
				if (StateFromObj(tree, listObjv[i], states, NULL,
					SFO_NOT_STATIC) != TCL_OK)
					return TCL_ERROR;
			}
			if ((itemFirst == ITEM_ALL) || (itemLast == ITEM_ALL))
			{
				Tcl_HashEntry *hPtr;
				Tcl_HashSearch search;

				hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
				while (hPtr != NULL)
				{
					item = (TreeItem) Tcl_GetHashValue(hPtr);
					stateOn = states[STATE_OP_ON];
					stateOff = states[STATE_OP_OFF];
					stateOn |= ~((Item *) item)->state & states[STATE_OP_TOGGLE];
					stateOff |= ((Item *) item)->state & states[STATE_OP_TOGGLE];
					TreeItem_ChangeState(tree, item, stateOff, stateOn);
					hPtr = Tcl_NextHashEntry(&search);
				}
				break;
			}
			if (objc == 7)
			{
				int indexFirst, indexLast;

				if (TreeItem_RootAncestor(tree, itemFirst) !=
					TreeItem_RootAncestor(tree,itemLast))
				{
					FormatResult(interp,
						"item %d and item %d don't share a common ancestor",
						TreeItem_GetID(tree, itemFirst),
						TreeItem_GetID(tree, itemLast));
					return TCL_ERROR;
				}
				TreeItem_ToIndex(tree, itemFirst, &indexFirst, NULL);
				TreeItem_ToIndex(tree, itemLast, &indexLast, NULL);
				if (indexFirst > indexLast)
				{
					item = itemFirst;
					itemFirst = itemLast;
					itemLast = item;
				}
			}
			item = itemFirst;
			while (item != NULL)
			{
				stateOn = states[STATE_OP_ON];
				stateOff = states[STATE_OP_OFF];
				stateOn |= ~((Item *) item)->state & states[STATE_OP_TOGGLE];
				stateOff |= ((Item *) item)->state & states[STATE_OP_TOGGLE];
				TreeItem_ChangeState(tree, item, stateOff, stateOn);
				if (item == itemLast)
					break;
				item = TreeItem_Next(tree, item);
			}
			break;
		}
	}

	return TCL_OK;
}

int TreeItemCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	static CONST char *commandNames[] = {
		"ancestors",
		"children",
		"create",
		"delete",
		"firstchild",
		"lastchild",
		"nextsibling",
		"numchildren",
		"parent",
		"prevsibling",
		"remove",

		"bbox",
		"complex",
		"dump",
		"element",
		"hasbutton",
		"index",
		"isancestor",
		"isopen",
		"rnc",
		"sort",
		"state",
		"style",
		"text",
		"visible",
		(char *) NULL
	};
	enum {
		COMMAND_ANCESTORS,
		COMMAND_CHILDREN,
		COMMAND_CREATE,
		COMMAND_DELETE,
		COMMAND_FIRSTCHILD,
		COMMAND_LASTCHILD,
		COMMAND_NEXTSIBLING,
		COMMAND_NUMCHILDREN,
		COMMAND_PARENT,
		COMMAND_PREVSIBLING,
		COMMAND_REMOVE,

		COMMAND_BBOX,
		COMMAND_COMPLEX,
		COMMAND_DUMP,
		COMMAND_ELEMENT,
		COMMAND_HASBUTTON,
		COMMAND_INDEX,
		COMMAND_ISANCESTOR,
		COMMAND_ISOPEN,
		COMMAND_RNC,
		COMMAND_SORT,
		COMMAND_STATE,
		COMMAND_STYLE,
		COMMAND_TEXT,
		COMMAND_VISIBLE
	};
#define AF_NOTANCESTOR 0x00010000 /* item can't be ancestor of other item */
#define AF_PARENT 0x00020000 /* item must have a parent */
#define AF_EQUALOK 0x00040000 /* second item can be same as first */
#define AF_SAMEROOT 0x00080000 /* both items must be descendants of a common ancestor */
	struct {
		int minArgs;
		int maxArgs;
		int flags;
		int flags2;
		char *argString;
	} argInfo[] = {
		{ 1, 1, 0, 0, "item" }, /* ancestors */
		{ 1, 1, 0, 0, "item" }, /* children */
		{ 0, 0, 0, 0, NULL }, /* create */
		{ 1, 2, IFO_ALLOK, IFO_ALLOK | AF_EQUALOK | AF_SAMEROOT, "first ?last?" }, /* delete */
		{ 1, 2, 0, IFO_NOTROOT | AF_NOTANCESTOR, "item ?newFirstChild?" }, /* firstchild */
		{ 1, 2, 0, IFO_NOTROOT | AF_NOTANCESTOR, "item ?newLastChild?" }, /* lastchild */
		{ 1, 2, IFO_NOTROOT | AF_PARENT, IFO_NOTROOT | AF_NOTANCESTOR, "item ?newNextSibling?" }, /* nextsibling */
		{ 1, 1, 0, 0, "item" }, /* numchildren */
		{ 1, 1, 0, 0, "item" }, /* parent */
		{ 1, 2, IFO_NOTROOT | AF_PARENT, IFO_NOTROOT | AF_NOTANCESTOR, "item ?newPrevSibling?" }, /* prevsibling */
		{ 1, 1, IFO_NOTROOT, 0, "item" }, /* remove */
		
		{ 1, 3, 0, 0, "item ?column? ?element?" }, /* bbox */
		{ 2, 100000, 0, 0, "item list..." }, /* complex */
		{ 1, 1, 0, 0, "item" }, /* dump */
		{ 4, 100000, 0, 0, "command item column element ?arg ...?" }, /* element */
		{ 1, 2, 0, 0, "item ?boolean?" }, /* hasbutton */
		{ 1, 1, 0, 0, "item" }, /* index */
		{ 2, 2, 0, AF_EQUALOK, "item item2" }, /* isancestor */
		{ 1, 1, 0, 0, "item" }, /* isopen */
		{ 1, 1, 0, 0, "item" }, /* rnc */
		{ 1, 100000, 0, 0, "item ?option ...?" }, /* sort */
		{ 2, 100000, 0, 0, "command item ?arg ...?" }, /* state */
		{ 2, 100000, 0, 0, "command item ?arg ...?" }, /* style */
		{ 2, 100000, 0, 0, "item column ?text? ?column text ...?" }, /* text */
		{ 1, 2, 0, 0, "item ?boolean?" }, /* visible */
	};
	int index;
	int numArgs = objc - 3;
	Item *item, *item2 = NULL, *child;

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

	if ((numArgs < argInfo[index].minArgs) ||
		(numArgs > argInfo[index].maxArgs))
	{
		Tcl_WrongNumArgs(interp, 3, objv, argInfo[index].argString);
		return TCL_ERROR;
	}

	switch (index)
	{
		case COMMAND_ELEMENT:
		{
			return ItemElementCmd(clientData, interp, objc, objv);
		}
		case COMMAND_SORT:
		{
			return ItemSortCmd(clientData, interp, objc, objv);
		}
		case COMMAND_STYLE:
		{
			return ItemStyleCmd(clientData, interp, objc, objv);
		}
		case COMMAND_STATE:
		{
			return ItemStateCmd(clientData, interp, objc, objv);
		}
	}

	if (numArgs >= 1)
	{
		if (TreeItem_FromObj(tree, objv[3], (TreeItem *) &item,
			argInfo[index].flags & 0xFFFF) != TCL_OK)
		{
			return TCL_ERROR;
		}

		switch (index)
		{
			/* T item bbox I ?C? ?E? */
			case COMMAND_BBOX:
			{
				int x, y, w, h;
				int i, columnIndex, indent, totalWidth;
				TreeColumn treeColumn;
				TreeItemColumn itemColumn;
				StyleDrawArgs drawArgs;
				TreeItem item_ = (TreeItem) item;
				XRectangle rect;

				if (Tree_ItemBbox(tree, item_, &x, &y, &w, &h) < 0)
					return TCL_OK;
				if (objc > 4)
				{
					if (TreeItem_ColumnFromObj(tree, item_, objv[4],
						&itemColumn, &columnIndex) != TCL_OK)
						return TCL_ERROR;
					totalWidth = 0;
					treeColumn = tree->columns;
					for (i = 0; i < columnIndex; i++)
					{
						totalWidth += TreeColumn_UseWidth(treeColumn);
						treeColumn = TreeColumn_Next(treeColumn);
					}
					if (columnIndex == tree->columnTree)
						indent = TreeItem_Indent(tree, item_);
					else
						indent = 0;
					if (objc == 5)
					{
						FormatResult(interp, "%d %d %d %d",
							x + totalWidth + indent - tree->xOrigin,
							y - tree->yOrigin,
							x + totalWidth + TreeColumn_UseWidth(treeColumn) - tree->xOrigin,
							y + h - tree->yOrigin);
						return TCL_OK;
					}
					drawArgs.style = TreeItemColumn_GetStyle(tree, itemColumn);
					if (drawArgs.style == NULL)
					{
						FormatResult(interp, "item %d column %d has no style",
							TreeItem_GetID(tree, item_), columnIndex);
						return TCL_ERROR;
					}
					drawArgs.tree = tree;
					drawArgs.drawable = None;
					drawArgs.state = TreeItem_GetState(tree, item_);
					drawArgs.x = x + indent + totalWidth;
					drawArgs.y = y;
					drawArgs.width = TreeColumn_UseWidth(treeColumn) - indent;
					drawArgs.height = h;
					drawArgs.justify = TreeColumn_Justify(treeColumn);
					if (TreeStyle_GetElemRects(&drawArgs, objc - 5, objv + 5,
						&rect) == -1)
						return TCL_ERROR;
					x = rect.x;
					y = rect.y;
					w = rect.width;
					h = rect.height;
				}
				FormatResult(interp, "%d %d %d %d",
					x - tree->xOrigin,
					y - tree->yOrigin,
					x - tree->xOrigin + w,
					y - tree->yOrigin + h);
				return TCL_OK;
			}
			case COMMAND_COMPLEX:
			{
				int i, j, columnIndex;
				int objc1, objc2;
				Tcl_Obj **objv1, **objv2;
				Column *column;
				int eMask, cMask, iMask = 0;
				int result = TCL_OK;

				if (objc <= 4)
					break;
				columnIndex = 0;
				for (i = 4; i < objc; i++, columnIndex++)
				{
					column = Item_FindColumn(tree, item, columnIndex);
					if (column == NULL)
					{
						FormatResult(interp, "item %d doesn't have column %d",
							item->id, columnIndex);
						result = TCL_ERROR;
						goto doneComplex;
					}
					/* List of element-configs per column */
					if (Tcl_ListObjGetElements(interp, objv[i],
						&objc1, &objv1) != TCL_OK)
					{
						result = TCL_ERROR;
						goto doneComplex;
					}
					if (objc1 == 0)
						continue;
					if (column->style == NULL)
					{
						FormatResult(interp, "item %d column %d has no style",
							item->id, columnIndex);
						result = TCL_ERROR;
						goto doneComplex;
					}
					cMask = 0;
					for (j = 0; j < objc1; j++)
					{
						/* elem option value... */
						if (Tcl_ListObjGetElements(interp, objv1[j],
							&objc2, &objv2) != TCL_OK)
						{
							result = TCL_ERROR;
							goto doneComplex;
						}
						if (objc2 < 3)
						{
							FormatResult(interp,
								"wrong # args: should be \"element option value...\"");
							result = TCL_ERROR;
							goto doneComplex;
						}
						if (TreeStyle_ElementConfigure(tree, column->style,
							objv2[0], objc2 - 1, objv2 + 1, &eMask) != TCL_OK)
						{
							result = TCL_ERROR;
							goto doneComplex;
						}
						cMask |= eMask;
						iMask |= eMask;
					}
					if (cMask & CS_LAYOUT)
						column->neededWidth = column->neededHeight = -1;
				}
doneComplex:
				if (iMask & CS_DISPLAY)
					Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
				if (iMask & CS_LAYOUT)
				{
					Tree_InvalidateColumnWidth(tree, -1);
					TreeItem_InvalidateHeight(tree, (TreeItem) item);
					Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
				}
				return result;
			}
			case COMMAND_DUMP:
			{
				if (tree->updateIndex)
					Tree_UpdateItemIndex(tree);
				FormatResult(interp, "index %d indexVis %d neededHeight %d",
					item->index, item->indexVis, item->neededHeight);
				return TCL_OK;
			}
			case COMMAND_HASBUTTON:
			{
				int hasButton;

				if (objc == 5)
				{
					if (Tcl_GetBooleanFromObj(interp, objv[4], &hasButton) != TCL_OK)
						return TCL_ERROR;
					if (hasButton != item->hasButton)
					{
						item->hasButton = hasButton;
						Tree_InvalidateItemDInfo(tree, (TreeItem) item, NULL);
					}
				}
				Tcl_SetObjResult(interp, Tcl_NewBooleanObj(item->hasButton));
				return TCL_OK;
			}
#if 0
			/* T item state I ?state ...? */
			case COMMAND_STATE:
			{
				int i, j, negate, stateOn = 0, stateOff = 0;
				char *string;

				if (objc == 4)
				{
					Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
					for (i = 0; i < 32; i++)
					{
						if (tree->stateNames[i] == NULL)
							continue;
						if (item->state & (1L << i))
						{
							Tcl_ListObjAppendElement(interp, listObj,
								Tcl_NewStringObj(tree->stateNames[i], -1));
						}
					}
					Tcl_SetObjResult(interp, listObj);
					return TCL_OK;
				}
				for (i = 4; i < objc; i++)
				{
					negate = 0;
					string = Tcl_GetString(objv[i]);
					if (string[0] == '!')
						negate = 1;
					for (j = STATE_USER - 1; j < 32; j++)
					{
						if (tree->stateNames[j] == NULL)
							continue;
						if (strcmp(tree->stateNames[j], string + negate) == 0)
							break;
					}
					if (j == 32)
					{
						FormatResult(interp, "cannot change state \"%s\"", string);
						return TCL_ERROR;
					}
					if (negate)
						stateOff |= 1L << j;
					else
						stateOn |= 1L << j;
				}
				TreeItem_ChangeState(tree, (TreeItem) item, stateOff, stateOn);
				return TCL_OK;
			}
#endif
			case COMMAND_VISIBLE:
			{
				int visible;

				if (objc == 5)
				{
					if (Tcl_GetBooleanFromObj(interp, objv[4], &visible) != TCL_OK)
						return TCL_ERROR;
					if (visible != item->isVisible)
					{
						item->isVisible = visible;

						/* May change the width of any column */
						Tree_InvalidateColumnWidth(tree, -1);

						/* If this is the last child, redraw the lines of the previous
						 * sibling and all of its descendants because the line from
						 * the previous sibling to us is appearing/disappearing */
						if ((item->prevSibling != NULL) &&
							(item->nextSibling == NULL) &&
							tree->showLines)
						{
							Item *last = item->prevSibling;
							while (last->lastChild != NULL)
								last = last->lastChild;
							Tree_InvalidateItemDInfo(tree,
								(TreeItem) item->prevSibling,
								(TreeItem) last);
						}

						tree->updateIndex = 1;
						Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
					}
				}
				Tcl_SetObjResult(interp, Tcl_NewBooleanObj(item->isVisible));
				return TCL_OK;
			}
			case COMMAND_ISOPEN:
			{
				Tcl_SetObjResult(interp, Tcl_NewBooleanObj(item->state & STATE_OPEN));
				return TCL_OK;
			}
			case COMMAND_INDEX:
			{
				FormatResult(interp, "%d %d", item->index, item->indexVis);
				return TCL_OK;
			}
			case COMMAND_RNC:
			{
				int row,col;

				if (Tree_ItemToRNC(tree, (TreeItem) item, &row, &col) == TCL_OK)
					FormatResult(interp, "%d %d", row, col);
				return TCL_OK;
			}

			/* T item text I C ?text? ?C text ...? */
			case COMMAND_TEXT:
			{
				Column *column;
				Tcl_Obj *textObj;
				int i, columnIndex;

				if (objc == 5)
				{
					if (Item_FindColumnFromObj(tree, item, objv[4], &column, NULL) != TCL_OK)
					{
						return TCL_ERROR;
					}
					if (column->style != NULL)
					{
						textObj = TreeStyle_GetText(tree, column->style);
						if (textObj != NULL)
							Tcl_SetObjResult(interp, textObj);
					}
					return TCL_OK;
				}
				if ((objc - 4) & 1)
				{
					FormatResult(interp, "wrong # args: should be \"column text column text...\"");
					return TCL_ERROR;
				}
				TreeItem_InvalidateHeight(tree, (TreeItem) item);
				Tree_InvalidateItemDInfo(tree, (TreeItem) item, NULL);
				Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
				for (i = 4; i < objc; i += 2)
				{
					if (Item_CreateColumnFromObj(tree, item, objv[i], &column, &columnIndex) != TCL_OK)
						return TCL_ERROR;
					if (column->style == NULL)
					{
						FormatResult(interp, "item %d column %d has no style",
							item->id, columnIndex);
						return TCL_ERROR;
					}
					TreeStyle_SetText(tree, column->style, objv[i + 1]);
					column->neededWidth = column->neededHeight = -1;
					Tree_InvalidateColumnWidth(tree, columnIndex);
				}
				return TCL_OK;
			}
		}

		if ((argInfo[index].flags & AF_PARENT) && (item->parent == NULL))
		{
			FormatResult(interp, "item %d must have a parent", item->id);
			return TCL_ERROR;
		}
	}
	if (numArgs >= 2)
	{
		if (TreeItem_FromObj(tree, objv[4], (TreeItem *) &item2,
			argInfo[index].flags2 & 0xFFFF) != TCL_OK)
		{
			return TCL_ERROR;
		}
		if (!(argInfo[index].flags2 & AF_EQUALOK) && (item == item2))
		{
			FormatResult(interp, "item %d same as second item", item->id);
			return TCL_ERROR;
		}
		if ((argInfo[index].flags2 & AF_PARENT) && (item2->parent == NULL))
		{
			FormatResult(interp, "item %d must have a parent", item2->id);
			return TCL_ERROR;
		}
		if ((argInfo[index].flags & AF_NOTANCESTOR) &&
			TreeItem_IsAncestor(tree, (TreeItem) item, (TreeItem) item2))
		{
			FormatResult(interp, "item %d is ancestor of item %d",
				item->id, item2->id);
			return TCL_ERROR;
		}
		if ((argInfo[index].flags2 & AF_NOTANCESTOR) &&
			TreeItem_IsAncestor(tree, (TreeItem) item2, (TreeItem) item))
		{
			FormatResult(interp, "item %d is ancestor of item %d",
				item2->id, item->id);
			return TCL_ERROR;
		}
		if ((argInfo[index].flags2 & AF_SAMEROOT) &&
			TreeItem_RootAncestor(tree, (TreeItem) item) !=
			TreeItem_RootAncestor(tree, (TreeItem) item2))
		{
			FormatResult(interp,
				"item %d and item %d don't share a common ancestor",
				item->id, item2->id);
			return TCL_ERROR;
		}
	}

	switch (index)
	{
		case COMMAND_ANCESTORS:
		{
			Tcl_Obj *listObj;
			Item *parent = item->parent;

			listObj = Tcl_NewListObj(0, NULL);
			while (parent != NULL)
			{
				Tcl_ListObjAppendElement(interp, listObj,
					TreeItem_ToObj(tree, (TreeItem) parent));
				parent = parent->parent;
			}
			Tcl_SetObjResult(interp, listObj);
			break;
		}
		case COMMAND_CHILDREN:
		{
			if (item->numChildren != 0)
			{
				Tcl_Obj *listObj;

				listObj = Tcl_NewListObj(0, NULL);
				child = item->firstChild;
				while (child != NULL)
				{
					Tcl_ListObjAppendElement(interp, listObj,
						TreeItem_ToObj(tree, (TreeItem) child));
					child = child->nextSibling;
				}
				Tcl_SetObjResult(interp, listObj);
			}
			break;
		}
		case COMMAND_CREATE:
		{
			item = (Item *) TreeItem_Alloc(tree);
			Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) item));
			break;
		}
		case COMMAND_DELETE:
		{
			int index1, index2;

			if (item == (Item *) ITEM_ALL || item2 == (Item *) ITEM_ALL)
			{
				/* Do it this way so any detached items are deleted */
				while (1)
				{
					Tcl_HashEntry *hPtr;
					Tcl_HashSearch search;
					hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
					item = (Item *) Tcl_GetHashValue(hPtr);
					if (item == (Item *) tree->root)
						hPtr = Tcl_NextHashEntry(&search);
					if (hPtr == NULL)
						break;
					item = (Item *) Tcl_GetHashValue(hPtr);
					TreeItem_Delete(tree, (TreeItem) item);
				}
			}
			else if (item2 != NULL)
			{
				TreeItem_ToIndex(tree, (TreeItem) item, &index1, NULL);
				TreeItem_ToIndex(tree, (TreeItem) item2, &index2, NULL);
				if (index1 > index2)
				{
					Item *swap = item;
					item = item2;
					item2 = swap;
				}
				while (1)
				{
					Item *prev = (Item *) TreeItem_Prev(tree, (TreeItem) item2);
					if (!ISROOT(item2))
						TreeItem_Delete(tree, (TreeItem) item2);
					if (item2 == item)
						break;
					item2 = prev;
				}
			}
			else
			{
				if (!ISROOT(item))
					TreeItem_Delete(tree, (TreeItem) item);
			}
			break;
		}
		case COMMAND_FIRSTCHILD:
		{
			if (item2 != NULL && item2 != item->firstChild)
			{
				TreeItem_RemoveFromParent(tree, (TreeItem) item2);
				item2->nextSibling = item->firstChild;
				if (item->firstChild != NULL)
					item->firstChild->prevSibling = item2;
				else
					item->lastChild = item2;
				item->firstChild = item2;
				item2->parent = item;
				item->numChildren++;
				TreeItem_AddToParent(tree, (TreeItem) item2);
			}
			if (item->firstChild != NULL)
				Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) item->firstChild));
			break;
		}
		case COMMAND_ISANCESTOR:
		{
			Tcl_SetObjResult(interp, Tcl_NewBooleanObj(
				TreeItem_IsAncestor(tree, (TreeItem) item, (TreeItem) item2)));
			break;
		}
		case COMMAND_LASTCHILD:
		{
			if (item2 != NULL && item2 != item->lastChild)
			{
				TreeItem_RemoveFromParent(tree, (TreeItem) item2);
				item2->prevSibling = item->lastChild;
				if (item->lastChild != NULL)
					item->lastChild->nextSibling = item2;
				else
					item->firstChild = item2;
				item->lastChild = item2;
				item2->parent = item;
				item->numChildren++;
				TreeItem_AddToParent(tree, (TreeItem) item2);
			}
			if (item->lastChild != NULL)
				Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) item->lastChild));
			break;
		}
		case COMMAND_NEXTSIBLING:
		{
			if (item2 != NULL && item2 != item->nextSibling)
			{
				TreeItem_RemoveFromParent(tree, (TreeItem) item2);
				item2->prevSibling = item;
				if (item->nextSibling != NULL)
				{
					item->nextSibling->prevSibling = item2;
					item2->nextSibling = item->nextSibling;
				}
				else
					item->parent->lastChild = item2;
				item->nextSibling = item2;
				item2->parent = item->parent;
				item->parent->numChildren++;
				TreeItem_AddToParent(tree, (TreeItem) item2);
			}
			if (item->nextSibling != NULL)
				Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) item->nextSibling));
			break;
		}
		case COMMAND_NUMCHILDREN:
		{
			Tcl_SetObjResult(interp, Tcl_NewIntObj(item->numChildren));
			break;
		}
		case COMMAND_PARENT:
		{
			if (item->parent != NULL)
				Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) item->parent));
			break;
		}
		case COMMAND_PREVSIBLING:
		{
			if (item2 != NULL && item2 != item->prevSibling)
			{
				TreeItem_RemoveFromParent(tree, (TreeItem) item2);
				item2->nextSibling = item;
				if (item->prevSibling != NULL)
				{
					item->prevSibling->nextSibling = item2;
					item2->prevSibling = item->prevSibling;
				}
				else
					item->parent->firstChild = item2;
				item->prevSibling = item2;
				item2->parent = item->parent;
				item->parent->numChildren++;
				TreeItem_AddToParent(tree, (TreeItem) item2);
			}
			if (item->prevSibling != NULL)
				Tcl_SetObjResult(interp, TreeItem_ToObj(tree, (TreeItem) item->prevSibling));
			break;
		}
		case COMMAND_REMOVE:
		{
			if (item->parent == NULL)
				break;
			TreeItem_RemoveFromParent(tree, (TreeItem) item);
			if (tree->debug.enable && tree->debug.data)
				Tree_Debug(tree);
			Tree_InvalidateColumnWidth(tree, -1);
			Tree_FreeItemDInfo(tree, (TreeItem) item, NULL);
			break;
		}
	}

	return TCL_OK;
}

int TreeItem_Debug(TreeCtrl *tree, TreeItem item_)
{
	Item *item = (Item *) item_;
	Item *child;
	Tcl_Interp *interp = tree->interp;
	int count;

	if (item->parent == item)
	{
		FormatResult(interp,
			"parent of %d is itself", item->id);
		return TCL_ERROR;
	}

	if (item->parent == NULL)
	{
		if (item->prevSibling != NULL)
		{
			FormatResult(interp,
				"parent of %d is nil, prevSibling is not nil",
				item->id);
			return TCL_ERROR;
		}
		if (item->nextSibling != NULL)
		{
			FormatResult(interp,
				"parent of %d is nil, nextSibling is not nil",
				item->id);
			return TCL_ERROR;
		}
	}

	if (item->prevSibling != NULL)
	{
		if (item->prevSibling == item)
		{
			FormatResult(interp,
				"prevSibling of %d is itself",
				item->id);
			return TCL_ERROR;
		}
		if (item->prevSibling->nextSibling != item)
		{
			FormatResult(interp,
				"item%d.prevSibling.nextSibling is not it",
				item->id);
			return TCL_ERROR;
		}
	}

	if (item->nextSibling != NULL)
	{
		if (item->nextSibling == item)
		{
			FormatResult(interp,
				"nextSibling of %d is itself",
				item->id);
			return TCL_ERROR;
		}
		if (item->nextSibling->prevSibling != item)
		{
			FormatResult(interp,
				"item%d.nextSibling->prevSibling is not it",
				item->id);
			return TCL_ERROR;
		}
	}

	if (item->numChildren < 0)
	{
		FormatResult(interp,
			"numChildren of %d is %d",
			item->id, item->numChildren);
		return TCL_ERROR;
	}

	if (item->numChildren == 0)
	{
		if (item->firstChild != NULL)
		{
			FormatResult(interp,
				"item%d.numChildren is zero, firstChild is not nil",
				item->id);
			return TCL_ERROR;
		}
		if (item->lastChild != NULL)
		{
			FormatResult(interp,
				"item%d.numChildren is zero, lastChild is not nil",
				item->id);
			return TCL_ERROR;
		}
	}

	if (item->numChildren > 0)
	{
		if (item->firstChild == NULL)
		{
			FormatResult(interp,
				"item%d.firstChild is nil",
				item->id);
			return TCL_ERROR;
		}
		if (item->firstChild == item)
		{
			FormatResult(interp,
				"item%d.firstChild is itself",
				item->id);
			return TCL_ERROR;
		}
		if (item->firstChild->parent != item)
		{
			FormatResult(interp,
				"item%d.firstChild.parent is not it",
				item->id);
			return TCL_ERROR;
		}
		if (item->firstChild->prevSibling != NULL)
		{
			FormatResult(interp,
				"item%d.firstChild.prevSibling is not nil",
				item->id);
			return TCL_ERROR;
		}

		if (item->lastChild == NULL)
		{
			FormatResult(interp,
				"item%d.lastChild is nil",
				item->id);
			return TCL_ERROR;
		}
		if (item->lastChild == item)
		{
			FormatResult(interp,
				"item%d.lastChild is itself",
				item->id);
			return TCL_ERROR;
		}
		if (item->lastChild->parent != item)
		{
			FormatResult(interp,
				"item%d.lastChild.parent is not it",
				item->id);
			return TCL_ERROR;
		}
		if (item->lastChild->nextSibling != NULL)
		{
			FormatResult(interp,
				"item%d.lastChild.nextSibling is not nil",
				item->id);
			return TCL_ERROR;
		}

		/* Count number of children */
		count = 0;
		child = item->firstChild;
		while (child != NULL)
		{
			count++;
			child = child->nextSibling;
		}
		if (count != item->numChildren)
		{
			FormatResult(interp,
				"item%d.numChildren is %d, but counted %d",
				item->id, item->numChildren, count);
			return TCL_ERROR;
		}

		/* Debug each child recursively */
		child = item->firstChild;
		while (child != NULL)
		{
			if (child->parent != item)
			{
				FormatResult(interp,
					"child->parent of %d is not it",
					item->id);
				return TCL_ERROR;
			}
			if (TreeItem_Debug(tree, (TreeItem) child) != TCL_OK)
				return TCL_ERROR;
			child = child->nextSibling;
		}
	}
	return TCL_OK;
}

char *TreeItem_Identify(TreeCtrl *tree, TreeItem item_, int x, int y)
{
	Item *self = (Item *) item_;
	int left, top, width, height;
	int indent, columnWidth, totalWidth;
	Column *column;
	StyleDrawArgs drawArgs;
	TreeColumn treeColumn;

	if (Tree_ItemBbox(tree, item_, &left, &top, &width, &height) < 0)
		return NULL;
#if 0
	if (y >= Tk_Height(tree->tkwin) || y + height <= 0)
		return NULL;
#endif
	drawArgs.tree = tree;
	drawArgs.drawable = None;
	drawArgs.state = self->state;

	totalWidth = 0;
	treeColumn = tree->columns;
	column = self->columns;
	while (column != NULL)
	{
		if (!TreeColumn_Visible(treeColumn))
			columnWidth = 0;
		else if (tree->columnCountVis == 1)
			columnWidth = width;
		else
			columnWidth = TreeColumn_UseWidth(treeColumn);
		if (columnWidth > 0)
		{
			if (TreeColumn_Index(treeColumn) == tree->columnTree)
				indent = TreeItem_Indent(tree, item_);
			else
				indent = 0;
			if ((x >= totalWidth + indent) && (x < totalWidth + columnWidth))
			{
				if (column->style != NULL)
				{
					drawArgs.style = column->style;
					drawArgs.x = indent + totalWidth;
					drawArgs.y = 0;
					drawArgs.width = columnWidth - indent;
					drawArgs.height = height;
					drawArgs.justify = TreeColumn_Justify(treeColumn);
					return TreeStyle_Identify(&drawArgs, x, y);
				}
				return NULL;
			}
			totalWidth += columnWidth;
		}
		treeColumn = TreeColumn_Next(treeColumn);
		column = column->next;
	}
	return NULL;
}

void TreeItem_Identify2(TreeCtrl *tree, TreeItem item_,
	int x1, int y1, int x2, int y2, Tcl_Obj *listObj)
{
	Item *self = (Item *) item_;
	int indent, columnWidth, totalWidth;
	int x, y, w, h;
	Column *column;
	StyleDrawArgs drawArgs;
	TreeColumn treeColumn;

	if (Tree_ItemBbox(tree, item_, &x, &y, &w, &h) < 0)
		return;

	drawArgs.tree = tree;
	drawArgs.drawable = None;
	drawArgs.state = self->state;

	totalWidth = 0;
	treeColumn = tree->columns;
	column = self->columns;
	while (treeColumn != NULL)
	{
		if (!TreeColumn_Visible(treeColumn))
			columnWidth = 0;
		else if (tree->columnCountVis == 1)
			columnWidth = w;
		else
			columnWidth = TreeColumn_UseWidth(treeColumn);
		if (columnWidth > 0)
		{
			if (TreeColumn_Index(treeColumn) == tree->columnTree)
				indent = TreeItem_Indent(tree, item_);
			else
				indent = 0;
			if ((x2 >= x + totalWidth + indent) && (x1 < x + totalWidth + columnWidth))
			{
				Tcl_Obj *subListObj = Tcl_NewListObj(0, NULL);
				Tcl_ListObjAppendElement(tree->interp, subListObj,
					Tcl_NewIntObj(TreeColumn_Index(treeColumn)));
				if ((column != NULL) && (column->style != NULL))
				{
					drawArgs.style = column->style;
					drawArgs.x = x + totalWidth + indent;
					drawArgs.y = y;
					drawArgs.width = columnWidth - indent;
					drawArgs.height = h;
					drawArgs.justify = TreeColumn_Justify(treeColumn);
					TreeStyle_Identify2(&drawArgs, x1, y1, x2, y2, subListObj);
				}
				Tcl_ListObjAppendElement(tree->interp, listObj, subListObj);
			}
			totalWidth += columnWidth;
		}
		treeColumn = TreeColumn_Next(treeColumn);
		if (column != NULL)
			column = column->next;
	}
}

