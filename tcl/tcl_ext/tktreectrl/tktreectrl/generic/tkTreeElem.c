#include "tkTreeCtrl.h"
#include "tkTreeElem.h"

static int ObjectIsEmpty(Tcl_Obj *obj)
{
	int length;

	if (obj == NULL)
		return 1;
	if (obj->bytes != NULL)
		return (obj->length == 0);
	Tcl_GetStringFromObj(obj, &length);
	return (length == 0);
}

/* BEGIN custom "boolean" option */

/* Just like TK_OPTION_BOOLEAN but supports TK_OPTION_NULL_OK */
/* Internal value is -1 for no-such-value */

static int BooleanSet(
	ClientData clientData,
	Tcl_Interp *interp,
	Tk_Window tkwin,
	Tcl_Obj **value,
	char *recordPtr,
	int internalOffset,
	char *saveInternalPtr,
	int flags)
{
	int objEmpty;
	int new, *internalPtr;

	objEmpty = 0;

	if (internalOffset >= 0)
		internalPtr = (int *) (recordPtr + internalOffset);
	else
		internalPtr = NULL;

	objEmpty = ObjectIsEmpty((*value));

	if ((flags & TK_OPTION_NULL_OK) && objEmpty)
		(*value) = NULL;
	else
	{
		if (Tcl_GetBooleanFromObj(interp, (*value), &new) != TCL_OK)
			return TCL_ERROR;
	}
	if (internalPtr != NULL)
	{
		if ((*value) == NULL)
			new = -1;
		*((int *) saveInternalPtr) = *((int *) internalPtr);
		*((int *) internalPtr) = new;
	}

	return TCL_OK;
}

static Tcl_Obj *BooleanGet(
	ClientData clientData,
	Tk_Window tkwin,
	char *recordPtr,
	int internalOffset)
{
	int value = *(int *) (recordPtr + internalOffset);
	if (value == -1)
		return NULL;
	return Tcl_NewBooleanObj(value);
}

static void BooleanRestore(
	ClientData clientData,
	Tk_Window tkwin,
	char *internalPtr,
	char *saveInternalPtr)
{
	*(int *) internalPtr = *(int *) saveInternalPtr;
}

static Tk_ObjCustomOption booleanCO =
{
	"boolean",
	BooleanSet,
	BooleanGet,
	BooleanRestore,
	NULL,
	(ClientData) NULL
};

/* END custom "boolean" option */

/* BEGIN custom "integer" option */

/* Just like TK_OPTION_INT but supports TK_OPTION_NULL_OK and bounds checking */

typedef struct IntegerClientData
{
	int min;
	int max;
	int empty; /* internal form if empty */
	int flags; /* 0x01 - use min, 0x02 - use max */
} IntegerClientData;

static int IntegerSet(
	ClientData clientData,
	Tcl_Interp *interp,
	Tk_Window tkwin,
	Tcl_Obj **value,
	char *recordPtr,
	int internalOffset,
	char *saveInternalPtr,
	int flags)
{
	IntegerClientData *info = (IntegerClientData *) clientData;
	int objEmpty;
	int new, *internalPtr;

	objEmpty = 0;

	if (internalOffset >= 0)
		internalPtr = (int *) (recordPtr + internalOffset);
	else
		internalPtr = NULL;

	objEmpty = ObjectIsEmpty((*value));

	if ((flags & TK_OPTION_NULL_OK) && objEmpty)
		(*value) = NULL;
	else
	{
		if (Tcl_GetIntFromObj(interp, (*value), &new) != TCL_OK)
			return TCL_ERROR;
		if ((info->flags & 0x01) && (new < info->min))
		{
			FormatResult(interp,
				"bad integer value \"%d\": must be >= %d",
				new, info->min);
			return TCL_ERROR;
		}
		if ((info->flags & 0x02) && (new > info->max))
		{
			FormatResult(interp,
				"bad integer value \"%d\": must be <= %d",
				new, info->max);
			return TCL_ERROR;
		}
	}
	if (internalPtr != NULL)
	{
		if ((*value) == NULL)
			new = info->empty;
		*((int *) saveInternalPtr) = *((int *) internalPtr);
		*((int *) internalPtr) = new;
	}

	return TCL_OK;
}

static Tcl_Obj *IntegerGet(
	ClientData clientData,
	Tk_Window tkwin,
	char *recordPtr,
	int internalOffset)
{
	IntegerClientData *info = (IntegerClientData *) clientData;
	int value = *(int *) (recordPtr + internalOffset);
	if (value == info->empty)
		return NULL;
	return Tcl_NewIntObj(value);
}

static void IntegerRestore(
	ClientData clientData,
	Tk_Window tkwin,
	char *internalPtr,
	char *saveInternalPtr)
{
	*(int *) internalPtr = *(int *) saveInternalPtr;
}

/* END custom "integer" option */

/*****/

/* BEGIN custom "stringtable" option */

/* Just like TK_OPTION_STRING_TABLE but supports TK_OPTION_NULL_OK */
/* The integer rep is -1 if empty string specified */

typedef struct StringTableClientData
{
	CONST char **tablePtr; /* NULL-termintated list of strings */
	CONST char *msg; /* Tcl_GetIndexFromObj() message */
} StringTableClientData;

static int StringTableSet(
	ClientData clientData,
	Tcl_Interp *interp,
	Tk_Window tkwin,
	Tcl_Obj **value,
	char *recordPtr,
	int internalOffset,
	char *saveInternalPtr,
	int flags)
{
	StringTableClientData *info = (StringTableClientData *) clientData;
	int objEmpty;
	int new, *internalPtr;

	objEmpty = 0;

	if (internalOffset >= 0)
		internalPtr = (int *) (recordPtr + internalOffset);
	else
		internalPtr = NULL;

	objEmpty = ObjectIsEmpty((*value));

	if ((flags & TK_OPTION_NULL_OK) && objEmpty)
		(*value) = NULL;
	else
	{
		if (Tcl_GetIndexFromObj(interp, (*value), info->tablePtr,
			info->msg, 0, &new) != TCL_OK)
			return TCL_ERROR;
	}
	if (internalPtr != NULL)
	{
		if ((*value) == NULL)
			new = -1;
		*((int *) saveInternalPtr) = *((int *) internalPtr);
		*((int *) internalPtr) = new;
	}

	return TCL_OK;
}

static Tcl_Obj *StringTableGet(
	ClientData clientData,
	Tk_Window tkwin,
	char *recordPtr,
	int internalOffset)
{
	StringTableClientData *info = (StringTableClientData *) clientData;
	int index = *(int *) (recordPtr + internalOffset);

	if (index == -1)
		return NULL;
	return Tcl_NewStringObj(info->tablePtr[index], -1);
}

static void StringTableRestore(
	ClientData clientData,
	Tk_Window tkwin,
	char *internalPtr,
	char *saveInternalPtr)
{
	*(int *) internalPtr = *(int *) saveInternalPtr;
}

/* END custom "stringtable" option */

/*****/

typedef struct PerStateData PerStateData;
typedef struct PerStateInfo PerStateInfo;
typedef struct PerStateType PerStateType;

/* There is one of these for each XColor, Tk_Font, Tk_Image etc */
struct PerStateData
{
	int stateOff;
	int stateOn;
	/* Type-specific fields go here */
};

#define DEBUG_PSI

struct PerStateInfo
{
#ifdef DEBUG_PSI
	PerStateType *type;
#endif
	Tcl_Obj *obj;
	int count;
	PerStateData *data;
};

typedef int (*PerStateType_FromObjProc)(TreeCtrl *, Tcl_Obj *, PerStateData *);
typedef void (*PerStateType_FreeProc)(TreeCtrl *, PerStateData *);

struct PerStateType
{
#ifdef DEBUG_PSI
	char *name;
#endif
	int size;
	PerStateType_FromObjProc fromObjProc;
	PerStateType_FreeProc freeProc;
};

#define MATCH_NONE 0
#define MATCH_ANY 1
#define MATCH_PARTIAL 2
#define MATCH_EXACT 3

static int StateFromObj2(TreeCtrl *tree, Tcl_Obj *obj, int *stateOff, int *stateOn)
{
	int states[3];

	states[0] = states[1] = states[2] = 0;
	if (StateFromObj(tree, obj, states, NULL, SFO_NOT_TOGGLE) != TCL_OK)
		return TCL_ERROR;

	(*stateOn) |= states[STATE_OP_ON];
	(*stateOff) |= states[STATE_OP_OFF];
	return TCL_OK;
}

static void PerStateInfo_Free(
	TreeCtrl *tree,
	PerStateType *typePtr,
	PerStateInfo *pInfo)
{
	PerStateData *pData = pInfo->data;
	int i;

	if (pInfo->data == NULL)
		return;
#ifdef DEBUG_PSI
	if (pInfo->type != typePtr)
		panic("PerStateInfo_Free type mismatch: got %s expected %s",
			pInfo->type ? pInfo->type->name : "NULL", typePtr->name);
#endif
	for (i = 0; i < pInfo->count; i++)
	{
		(*typePtr->freeProc)(tree, pData);
		pData = (PerStateData *) (((char *) pData) + typePtr->size);
	}
	wipefree((char *) pInfo->data, typePtr->size * pInfo->count);
	pInfo->data = NULL;
	pInfo->count = 0;
}

static int PerStateInfo_FromObj(
	TreeCtrl *tree,
	PerStateType *typePtr,
	PerStateInfo *pInfo)
{
	int i, j;
	int objc, objc2;
	Tcl_Obj **objv, **objv2;
	PerStateData *pData;

#ifdef DEBUG_PSI
	pInfo->type = typePtr;
#endif

	PerStateInfo_Free(tree, typePtr, pInfo);

	if (pInfo->obj == NULL)
		return TCL_OK;

	if (Tcl_ListObjGetElements(tree->interp, pInfo->obj, &objc, &objv) != TCL_OK)
		return TCL_ERROR;

	if (objc == 0)
		return TCL_OK;

	if (objc == 1)
	{
		pData = (PerStateData *) ckalloc(typePtr->size);
		pData->stateOff = pData->stateOn = 0; /* all states */
		if ((*typePtr->fromObjProc)(tree, objv[0], pData) != TCL_OK)
		{
			wipefree((char *) pData, typePtr->size);
			return TCL_ERROR;
		}
		pInfo->data = pData;
		pInfo->count = 1;
		return TCL_OK;
	}

	if (objc & 1)
	{
		FormatResult(tree->interp, "list must have even number of elements");
		return TCL_ERROR;
	}

	pData = (PerStateData *) ckalloc(typePtr->size * (objc / 2));
	pInfo->data = pData;
	for (i = 0; i < objc; i += 2)
	{
		if ((*typePtr->fromObjProc)(tree, objv[i], pData) != TCL_OK)
		{
			PerStateInfo_Free(tree, typePtr, pInfo);
			return TCL_ERROR;
		}
		pInfo->count++;
		if (Tcl_ListObjGetElements(tree->interp, objv[i + 1], &objc2, &objv2) != TCL_OK)
		{
			PerStateInfo_Free(tree, typePtr, pInfo);
			return TCL_ERROR;
		}
		pData->stateOff = pData->stateOn = 0; /* all states */
		for (j = 0; j < objc2; j++)
		{
			if (StateFromObj2(tree, objv2[j], &pData->stateOff, &pData->stateOn) != TCL_OK)
			{
				PerStateInfo_Free(tree, typePtr, pInfo);
				return TCL_ERROR;
			}
		}
		pData = (PerStateData *) (((char *) pData) + typePtr->size);
	}
	return TCL_OK;
}

static PerStateData *PerStateInfo_ForState(
	TreeCtrl *tree,
	PerStateType *typePtr,
	PerStateInfo *pInfo,
	int state,
	int *match)
{
	PerStateData *pData = pInfo->data;
	int stateOff = ~state, stateOn = state;
	int i;

#ifdef DEBUG_PSI
	if ((pInfo->data != NULL) && (pInfo->type != typePtr))
		panic("PerStateInfo_ForState type mismatch: got %s expected %s",
			pInfo->type ? pInfo->type->name : "NULL", typePtr->name);
#endif

	for (i = 0; i < pInfo->count; i++)
	{
		/* Any state */
		if ((pData->stateOff == 0) &&
			(pData->stateOn == 0))
		{
			(*match) = MATCH_ANY;
			return pData;
		}

		/* Exact match */
		if ((pData->stateOff == stateOff) &&
			(pData->stateOn == stateOn))
		{
			(*match) = MATCH_EXACT;
			return pData;
		}

		/* Partial match */
		if (((pData->stateOff & stateOff) == pData->stateOff) &&
			((pData->stateOn & stateOn) == pData->stateOn))
		{
			(*match) = MATCH_PARTIAL;
			return pData;
		}

		pData = (PerStateData *) (((char *) pData) + typePtr->size);
	}

	(*match) = MATCH_NONE;
	return NULL;
}

static Tcl_Obj *PerStateInfo_ObjForState(
	TreeCtrl *tree,
	PerStateType *typePtr,
	PerStateInfo *pInfo,
	int state,
	int *match)
{
	PerStateData *pData;
	Tcl_Obj *obj;
	int i;

#ifdef DEBUG_PSI
	if ((pInfo->data != NULL) && (pInfo->type != typePtr))
		panic("PerStateInfo_ObjForState type mismatch: got %s expected %s",
			pInfo->type ? pInfo->type->name : "NULL", typePtr->name);
#endif

	pData = PerStateInfo_ForState(tree, typePtr, pInfo, state, match);
	if (pData != NULL)
	{
		i = ((char *) pData - (char *) pInfo->data) / typePtr->size;
		Tcl_ListObjIndex(tree->interp, pInfo->obj, i * 2, &obj);
		return obj;
	}

	return NULL;
}

static void PerStateInfo_Undefine(
	TreeCtrl *tree,
	PerStateType *typePtr,
	PerStateInfo *pInfo,
	int state)
{
	PerStateData *pData = pInfo->data;
	int i, j, numStates, stateOff, stateOn;
	Tcl_Obj *configObj = pInfo->obj, *listObj, *stateObj;

#ifdef DEBUG_PSI
	if ((pInfo->data != NULL) && (pInfo->type != typePtr))
		panic("PerStateInfo_Undefine type mismatch: got %s expected %s",
			pInfo->type ? pInfo->type->name : "NULL", typePtr->name);
#endif

	for (i = 0; i < pInfo->count; i++)
	{
		if ((pData->stateOff | pData->stateOn) & state)
		{
			pData->stateOff &= ~state;
			pData->stateOn &= ~state;
			if (Tcl_IsShared(configObj))
			{
				configObj = Tcl_DuplicateObj(configObj);
				Tcl_DecrRefCount(pInfo->obj);
				Tcl_IncrRefCount(configObj);
				pInfo->obj = configObj;
			}
			Tcl_ListObjIndex(tree->interp, configObj, i * 2 + 1, &listObj);
			if (Tcl_IsShared(listObj))
			{
				listObj = Tcl_DuplicateObj(listObj);
				Tcl_ListObjReplace(tree->interp, configObj, i * 2 + 1, 1, 1, &listObj);
			}
			Tcl_ListObjLength(tree->interp, listObj, &numStates);
			for (j = 0; j < numStates; )
			{
				Tcl_ListObjIndex(tree->interp, listObj, j, &stateObj);
				stateOff = stateOn = 0;
				StateFromObj2(tree, stateObj, &stateOff, &stateOn);
				if ((stateOff | stateOn) & state)
				{
					Tcl_ListObjReplace(tree->interp, listObj, j, 1, 0, NULL);
					numStates--;
				}
				else
					j++;
			}
			/* Given {bitmap {state1 state2 state3}}, we just invalidated
			 * the string rep of the sublist {state1 ...}, but not
			 * the parent list */
			Tcl_InvalidateStringRep(configObj);
		}
		pData = (PerStateData *) (((char *) pData) + typePtr->size);
	}
}

/*****/

struct PerStateGC
{
	unsigned long mask;
	XGCValues gcValues;
	GC gc;
	struct PerStateGC *next;
};

void PerStateGC_Free(TreeCtrl *tree, struct PerStateGC **pGCPtr)
{
	struct PerStateGC *pGC = (*pGCPtr), *next;

	while (pGC != NULL)
	{
		next = pGC->next;
		Tk_FreeGC(tree->display, pGC->gc);
		WFREE(pGC, struct PerStateGC);
		pGC = next;
	}
	(*pGCPtr) = NULL;
}

GC PerStateGC_Get(TreeCtrl *tree, struct PerStateGC **pGCPtr, unsigned long mask, XGCValues *gcValues)
{
	struct PerStateGC *pGC;

	if ((mask | (GCFont | GCForeground | GCBackground | GCGraphicsExposures)) != 
		(GCFont | GCForeground | GCBackground | GCGraphicsExposures))
		panic("PerStateGC_Get: unsupported mask");

	for (pGC = (*pGCPtr); pGC != NULL; pGC = pGC->next)
	{
		if (mask != pGC->mask)
			continue;
		if ((mask & GCFont) &&
			(pGC->gcValues.font != gcValues->font))
			continue;
		if ((mask & GCForeground) &&
			(pGC->gcValues.foreground != gcValues->foreground))
			continue;
		if ((mask & GCBackground) &&
			(pGC->gcValues.background != gcValues->background))
			continue;
		if ((mask & GCGraphicsExposures) &&
			(pGC->gcValues.graphics_exposures != gcValues->graphics_exposures))
			continue;
		return pGC->gc;
	}

	pGC = (struct PerStateGC *) ckalloc(sizeof(*pGC));
	pGC->gcValues = (*gcValues);
	pGC->mask = mask;
	pGC->gc = Tk_GetGC(tree->tkwin, mask, gcValues);
	pGC->next = (*pGCPtr);
	(*pGCPtr) = pGC;

	return pGC->gc;
}

/*****/

typedef struct PerStateDataBitmap PerStateDataBitmap;
struct PerStateDataBitmap
{
	PerStateData header;
	Pixmap bitmap;
};

static int BitmapFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataBitmap *pBitmap)
{
	if (ObjectIsEmpty(obj))
	{
		/* Specify empty string to override masterX */
		pBitmap->bitmap = None;
	}
	else
	{
		pBitmap->bitmap = Tk_AllocBitmapFromObj(tree->interp, tree->tkwin, obj);
		if (pBitmap->bitmap == None)
			return TCL_ERROR;
	}
	return TCL_OK;
}

static void BitmapFree(TreeCtrl *tree, PerStateDataBitmap *pBitmap)
{
	if (pBitmap->bitmap != None)
		Tk_FreeBitmap(tree->display, pBitmap->bitmap);
}

PerStateType pstBitmap =
{
#ifdef DEBUG_PSI
	"Bitmap",
#endif
	sizeof(PerStateDataBitmap),
	(PerStateType_FromObjProc) BitmapFromObj,
	(PerStateType_FreeProc) BitmapFree
};

static Pixmap PerStateBitmap_ForState(
	TreeCtrl *tree,
	PerStateInfo *pInfo,
	int state,
	int *match)
{
	PerStateDataBitmap *pData;

	pData = (PerStateDataBitmap *) PerStateInfo_ForState(tree, &pstBitmap, pInfo, state, match);
	if (pData != NULL)
		return pData->bitmap;
	return None;
}

/*****/

typedef struct PerStateDataBorder PerStateDataBorder;
struct PerStateDataBorder
{
	PerStateData header;
	Tk_3DBorder border;
};

static int BorderFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataBorder *pBorder)
{
	if (ObjectIsEmpty(obj))
	{
		/* Specify empty string to override masterX */
		pBorder->border = NULL;
	}
	else
	{
		pBorder->border = Tk_Alloc3DBorderFromObj(tree->interp, tree->tkwin, obj);
		if (pBorder->border == NULL)
			return TCL_ERROR;
	}
	return TCL_OK;
}

static void BorderFree(TreeCtrl *tree, PerStateDataBorder *pBorder)
{
	if (pBorder->border != NULL)
		Tk_Free3DBorder(pBorder->border);
}

PerStateType pstBorder =
{
#ifdef DEBUG_PSI
	"Border",
#endif
	sizeof(PerStateDataBorder),
	(PerStateType_FromObjProc) BorderFromObj,
	(PerStateType_FreeProc) BorderFree
};

static Tk_3DBorder PerStateBorder_ForState(
	TreeCtrl *tree,
	PerStateInfo *pInfo,
	int state,
	int *match)
{
	PerStateDataBorder *pData;

	pData = (PerStateDataBorder *) PerStateInfo_ForState(tree, &pstBorder, pInfo, state, match);
	if (pData != NULL)
		return pData->border;
	return NULL;
}

/*****/

typedef struct PerStateDataColor PerStateDataColor;
struct PerStateDataColor
{
	PerStateData header;
	XColor *color;
};

static int ColorFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataColor *pColor)
{
	if (ObjectIsEmpty(obj))
	{
		/* Specify empty string to override masterX */
		pColor->color = NULL;
	}
	else
	{
		pColor->color = Tk_AllocColorFromObj(tree->interp, tree->tkwin, obj);
		if (pColor->color == NULL)
			return TCL_ERROR;
	}
	return TCL_OK;
}

static void ColorFree(TreeCtrl *tree, PerStateDataColor *pColor)
{
	if (pColor->color != NULL)
		Tk_FreeColor(pColor->color);
}

PerStateType pstColor =
{
#ifdef DEBUG_PSI
	"Color",
#endif
	sizeof(PerStateDataColor),
	(PerStateType_FromObjProc) ColorFromObj,
	(PerStateType_FreeProc) ColorFree
};

static XColor *PerStateColor_ForState(
	TreeCtrl *tree,
	PerStateInfo *pInfo,
	int state,
	int *match)
{
	PerStateDataColor *pData;

	pData = (PerStateDataColor *) PerStateInfo_ForState(tree, &pstColor, pInfo, state, match);
	if (pData != NULL)
		return pData->color;
	return NULL;
}

/*****/

typedef struct PerStateDataFont PerStateDataFont;
struct PerStateDataFont
{
	PerStateData header;
	Tk_Font tkfont;
};

static int FontFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataFont *pFont)
{
	if (ObjectIsEmpty(obj))
	{
		/* Specify empty string to override masterX */
		pFont->tkfont = NULL;
	}
	else
	{
		pFont->tkfont = Tk_AllocFontFromObj(tree->interp, tree->tkwin, obj);
		if (pFont->tkfont == NULL)
			return TCL_ERROR;
	}
	return TCL_OK;
}

static void FontFree(TreeCtrl *tree, PerStateDataFont *pFont)
{
	if (pFont->tkfont != NULL)
		Tk_FreeFont(pFont->tkfont);
}

PerStateType pstFont =
{
#ifdef DEBUG_PSI
	"Font",
#endif
	sizeof(PerStateDataFont),
	(PerStateType_FromObjProc) FontFromObj,
	(PerStateType_FreeProc) FontFree
};

static Tk_Font PerStateFont_ForState(
	TreeCtrl *tree,
	PerStateInfo *pInfo,
	int state,
	int *match)
{
	PerStateDataFont *pData;

	pData = (PerStateDataFont *) PerStateInfo_ForState(tree, &pstFont, pInfo, state, match);
	if (pData != NULL)
		return pData->tkfont;
	return NULL;
}

/*****/

typedef struct PerStateDataImage PerStateDataImage;
struct PerStateDataImage
{
	PerStateData header;
	Tk_Image image;
	char *string;
};

static int ImageFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataImage *pImage)
{
	int length;
	char *string;

	if (ObjectIsEmpty(obj))
	{
		/* Specify empty string to override masterX */
		pImage->image = NULL;
		pImage->string = NULL;
	}
	else
	{
		string = Tcl_GetStringFromObj(obj, &length);
		pImage->image = Tree_GetImage(tree, string);
		if (pImage->image == NULL)
			return TCL_ERROR;
		pImage->string = ckalloc(length + 1);
		strcpy(pImage->string, string);
	}
	return TCL_OK;
}

static void ImageFree(TreeCtrl *tree, PerStateDataImage *pImage)
{
	if (pImage->string != NULL)
		ckfree(pImage->string);
	/* don't free image */
}

PerStateType pstImage =
{
#ifdef DEBUG_PSI
	"Image",
#endif
	sizeof(PerStateDataImage),
	(PerStateType_FromObjProc) ImageFromObj,
	(PerStateType_FreeProc) ImageFree
};

static Tk_Image PerStateImage_ForState(
	TreeCtrl *tree,
	PerStateInfo *pInfo,
	int state,
	int *match)
{
	PerStateDataImage *pData;

	pData = (PerStateDataImage *) PerStateInfo_ForState(tree, &pstImage, pInfo, state, match);
	if (pData != NULL)
		return pData->image;
	return NULL;
}

/*****/

typedef struct PerStateDataRelief PerStateDataRelief;
struct PerStateDataRelief
{
	PerStateData header;
	int relief;
};

static int ReliefFromObj(TreeCtrl *tree, Tcl_Obj *obj, PerStateDataRelief *pRelief)
{
	if (ObjectIsEmpty(obj))
	{
		/* Specify empty string to override masterX */
		pRelief->relief = TK_RELIEF_NULL;
	}
	else
	{
		if (Tk_GetReliefFromObj(tree->interp, obj, &pRelief->relief) != TCL_OK)
			return TCL_ERROR;
	}
	return TCL_OK;
}

static void ReliefFree(TreeCtrl *tree, PerStateDataRelief *pRelief)
{
}

PerStateType pstRelief =
{
#ifdef DEBUG_PSI
	"Relief",
#endif
	sizeof(PerStateDataRelief),
	(PerStateType_FromObjProc) ReliefFromObj,
	(PerStateType_FreeProc) ReliefFree
};

static int PerStateRelief_ForState(
	TreeCtrl *tree,
	PerStateInfo *pInfo,
	int state,
	int *match)
{
	PerStateDataRelief *pData;

	pData = (PerStateDataRelief *) PerStateInfo_ForState(tree, &pstRelief, pInfo, state, match);
	if (pData != NULL)
		return pData->relief;
	return TK_RELIEF_NULL;
}

/*****/

void PSTSave(
	PerStateInfo *pInfo,
	PerStateInfo *pSave)
{
#ifdef DEBUG_PSI
	pSave->type = pInfo->type; /* could be NULL */
#endif
	pSave->data = pInfo->data;
	pSave->count = pInfo->count;
	pInfo->data = NULL;
	pInfo->count = 0;
}

void PSTRestore(
	TreeCtrl *tree,
	PerStateType *typePtr,
	PerStateInfo *pInfo,
	PerStateInfo *pSave)
{
	PerStateInfo_Free(tree, typePtr, pInfo);
	pInfo->data = pSave->data;
	pInfo->count = pSave->count;
}

/*****/

typedef struct ElementBitmap ElementBitmap;

struct ElementBitmap
{
	Element header;
	PerStateInfo bitmap;
	PerStateInfo fg;
	PerStateInfo bg;
};

#define BITMAP_CONF_BITMAP 0x0001
#define BITMAP_CONF_FG 0x0002
#define BITMAP_CONF_BG 0x0004

static Tk_OptionSpec bitmapOptionSpecs[] = {
	{TK_OPTION_STRING, "-background", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementBitmap, bg.obj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL, BITMAP_CONF_BG},
	{TK_OPTION_STRING, "-bitmap", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementBitmap, bitmap.obj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL, BITMAP_CONF_BITMAP},
	{TK_OPTION_STRING, "-foreground", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementBitmap, fg.obj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL, BITMAP_CONF_FG},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void DeleteBitmap(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementBitmap *elemX = (ElementBitmap *) elem;

	PerStateInfo_Free(tree, &pstBitmap, &elemX->bitmap);
	PerStateInfo_Free(tree, &pstColor, &elemX->fg);
	PerStateInfo_Free(tree, &pstColor, &elemX->bg);
}

static int WorldChangedBitmap(ElementArgs *args)
{
	int flagM = args->change.flagMaster;
	int flagS = args->change.flagSelf;
	int mask = 0;

	if ((flagS | flagM) & BITMAP_CONF_BITMAP)
		mask |= CS_DISPLAY | CS_LAYOUT;

	if ((flagS | flagM) & (BITMAP_CONF_FG | BITMAP_CONF_BG))
		mask |= CS_DISPLAY;

	return mask;
}

static int ConfigBitmap(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementBitmap *elemX = (ElementBitmap *) elem;
	ElementBitmap savedX;
	Tk_SavedOptions savedOptions;
	int error;
	Tcl_Obj *errorResult = NULL;

	for (error = 0; error <= 1; error++)
	{
		if (error == 0)
		{
			if (Tk_SetOptions(tree->interp, (char *) elemX,
				elem->typePtr->optionTable,
				args->config.objc, args->config.objv, tree->tkwin,
				&savedOptions, &args->config.flagSelf) != TCL_OK)
			{
				args->config.flagSelf = 0;
				continue;
			}

			if (args->config.flagSelf & BITMAP_CONF_BITMAP)
				PSTSave(&elemX->bitmap, &savedX.bitmap);
			if (args->config.flagSelf & BITMAP_CONF_FG)
				PSTSave(&elemX->fg, &savedX.fg);
			if (args->config.flagSelf & BITMAP_CONF_BG)
				PSTSave(&elemX->bg, &savedX.bg);

			if (args->config.flagSelf & BITMAP_CONF_BITMAP)
			{
				if (PerStateInfo_FromObj(tree, &pstBitmap, &elemX->bitmap) != TCL_OK)
					continue;
			}

			if (args->config.flagSelf & BITMAP_CONF_FG)
			{
				if (PerStateInfo_FromObj(tree, &pstColor, &elemX->fg) != TCL_OK)
					continue;
			}

			if (args->config.flagSelf & BITMAP_CONF_BG)
			{
				if (PerStateInfo_FromObj(tree, &pstColor, &elemX->bg) != TCL_OK)
					continue;
			}

			if (args->config.flagSelf & BITMAP_CONF_BITMAP)
				PerStateInfo_Free(tree, &pstBitmap, &savedX.bitmap);
			if (args->config.flagSelf & BITMAP_CONF_FG)
				PerStateInfo_Free(tree, &pstColor, &savedX.fg);
			if (args->config.flagSelf & BITMAP_CONF_BG)
				PerStateInfo_Free(tree, &pstColor, &savedX.bg);
			Tk_FreeSavedOptions(&savedOptions);
			break;
		}
		else
		{
			errorResult = Tcl_GetObjResult(tree->interp);
			Tcl_IncrRefCount(errorResult);
			Tk_RestoreSavedOptions(&savedOptions);

			if (args->config.flagSelf & BITMAP_CONF_BITMAP)
				PSTRestore(tree, &pstBitmap, &elemX->bitmap, &savedX.bitmap);

			if (args->config.flagSelf & BITMAP_CONF_FG)
				PSTRestore(tree, &pstColor, &elemX->fg, &savedX.fg);

			if (args->config.flagSelf & BITMAP_CONF_BG)
				PSTRestore(tree, &pstColor, &elemX->bg, &savedX.bg);

			Tcl_SetObjResult(tree->interp, errorResult);
			Tcl_DecrRefCount(errorResult);
			return TCL_ERROR;
		}
	}

	return TCL_OK;
}

static int CreateBitmap(ElementArgs *args)
{
	return TCL_OK;
}

static void DisplayBitmap(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementBitmap *elemX = (ElementBitmap *) elem;
	ElementBitmap *masterX = (ElementBitmap *) elem->master;
	int state = args->state;
	int match, match2;
	Pixmap bitmap;
	XColor *fg, *bg;
	int imgW, imgH;

	bitmap = PerStateBitmap_ForState(tree, &elemX->bitmap, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Pixmap bitmap2 = PerStateBitmap_ForState(tree, &masterX->bitmap,
			state, &match2);
		if (match2 > match)
			bitmap = bitmap2;
	}

	fg = PerStateColor_ForState(tree, &elemX->fg, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *fg2 = PerStateColor_ForState(tree, &masterX->fg,
			state, &match2);
		if (match2 > match)
			fg = fg2;
	}

	bg = PerStateColor_ForState(tree, &elemX->bg, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *bg2 = PerStateColor_ForState(tree, &masterX->bg,
			state, &match2);
		if (match2 > match)
			bg = bg2;
	}

	if (bitmap != None)
	{
		int bx = args->display.x /* + args->display.pad[LEFT] */;
		int by = args->display.y /* + args->display.pad[TOP] */;
		int dx = 0, dy = 0;
		XGCValues gcValues;
		GC gc;
		unsigned long mask = 0;

		Tk_SizeOfBitmap(tree->display, bitmap, &imgW, &imgH);
		if (imgW < args->display.width)
			dx = (args->display.width - imgW) / 2;
		else if (imgW > args->display.width)
			imgW = args->display.width;
		if (imgH < args->display.height)
			dy = (args->display.height - imgH) / 2;
		else if (imgH > args->display.height)
			imgH = args->display.height;

		bx += dx;
		by += dy;

		if (fg != NULL)
		{
			gcValues.foreground = fg->pixel;
			mask |= GCForeground;
		}
		if (bg != NULL)
		{
			gcValues.background = bg->pixel;
			mask |= GCBackground;
		}
		else
		{
			gcValues.clip_mask = bitmap;
			mask |= GCClipMask;
		}
		gcValues.graphics_exposures = False;
		mask |= GCGraphicsExposures;
		gc = Tk_GetGC(tree->tkwin, mask, &gcValues);
		XSetClipOrigin(tree->display, gc, bx, by);
		XCopyPlane(tree->display, bitmap, args->display.drawable, gc,
			0, 0, (unsigned int) imgW, (unsigned int) imgH,
			bx, by, 1);
		Tk_FreeGC(tree->display, gc);
	}
}

static void LayoutBitmap(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementBitmap *elemX = (ElementBitmap *) elem;
	ElementBitmap *masterX = (ElementBitmap *) elem->master;
	int state = args->state;
	int match, match2;
	Pixmap bitmap;

	bitmap = PerStateBitmap_ForState(tree, &elemX->bitmap, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Pixmap bitmap2 = PerStateBitmap_ForState(tree, &masterX->bitmap,
			state, &match2);
		if (match2 > match)
			bitmap = bitmap2;
	}

	if (bitmap != None)
		Tk_SizeOfBitmap(tree->display, bitmap,
			&args->layout.width, &args->layout.height);
	else
		args->layout.width = args->layout.height = 0;
}

static int StateProcBitmap(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementBitmap *elemX = (ElementBitmap *) elem;
	ElementBitmap *masterX = (ElementBitmap *) elem->master;
	int match, match2;
	Pixmap bitmap1, bitmap2;
	XColor *fg1, *fg2;
	XColor *bg1, *bg2;
	int mask = 0;

	bitmap1 = PerStateBitmap_ForState(tree, &elemX->bitmap,
		args->states.state1, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Pixmap bitmap = PerStateBitmap_ForState(tree, &masterX->bitmap,
			args->states.state1, &match2);
		if (match2 > match)
			bitmap1 = bitmap;
	}

	fg1 = PerStateColor_ForState(tree, &elemX->fg,
		args->states.state1, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *fg = PerStateColor_ForState(tree, &masterX->fg,
			args->states.state1, &match2);
		if (match2 > match)
			fg1 = fg;
	}

	bg1 = PerStateColor_ForState(tree, &elemX->bg,
		args->states.state1, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *bg = PerStateColor_ForState(tree, &masterX->bg,
			args->states.state1, &match2);
		if (match2 > match)
			bg1 = bg;
	}

	bitmap2 = PerStateBitmap_ForState(tree, &elemX->bitmap,
		args->states.state2, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Pixmap bitmap = PerStateBitmap_ForState(tree, &masterX->bitmap,
			args->states.state2, &match2);
		if (match2 > match)
			bitmap2 = bitmap;
	}

	fg2 = PerStateColor_ForState(tree, &elemX->fg,
		args->states.state2, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *fg = PerStateColor_ForState(tree, &masterX->fg,
			args->states.state2, &match2);
		if (match2 > match)
			fg2 = fg;
	}

	bg2 = PerStateColor_ForState(tree, &elemX->bg,
		args->states.state2, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *bg = PerStateColor_ForState(tree, &masterX->bg,
			args->states.state2, &match2);
		if (match2 > match)
			bg2 = bg;
	}

	if ((fg1 != fg2) || (bg1 != bg2) || (bitmap1 != bitmap2))
		mask |= CS_DISPLAY;

	if (bitmap1 != bitmap2)
	{
		if ((bitmap1 != None) && (bitmap2 != None))
		{
			int w1, h1, w2, h2;
			Tk_SizeOfBitmap(tree->display, bitmap1, &w1, &h1);
			Tk_SizeOfBitmap(tree->display, bitmap2, &w2, &h2);
			if ((w1 != w2) || (h1 != h2))
				mask |= CS_LAYOUT;
		}
		else
			mask |= CS_LAYOUT;
	}

	return mask;
}

static void UndefProcBitmap(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	ElementBitmap *elemX = (ElementBitmap *) args->elem;

	PerStateInfo_Undefine(tree, &pstColor, &elemX->fg, args->state);
	PerStateInfo_Undefine(tree, &pstColor, &elemX->bg, args->state);
	PerStateInfo_Undefine(tree, &pstBitmap, &elemX->bitmap, args->state);
}

static int ActualProcBitmap(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	ElementBitmap *elemX = (ElementBitmap *) args->elem;
	ElementBitmap *masterX = (ElementBitmap *) args->elem->master;
	static CONST char *optionName[] = {
		"-background", "-bitmap", "-foreground",
		(char *) NULL };
	int index, match, matchM;
	Tcl_Obj *obj = NULL, *objM;

	if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
		return TCL_ERROR;

	switch (index)
	{
		case 0:
		{
			obj = PerStateInfo_ObjForState(tree, &pstColor, &elemX->bg, args->state, &match);
			if ((match != MATCH_EXACT) && (masterX != NULL))
			{
				objM = PerStateInfo_ObjForState(tree, &pstColor, &masterX->bg, args->state, &matchM);
				if (matchM > match)
					obj = objM;
			}
			/* When -background isn't specified, GC default (white) is used */
			if (ObjectIsEmpty(obj))
				obj = Tcl_NewStringObj("white", -1);
			break;
		}
		case 1:
		{
			obj = PerStateInfo_ObjForState(tree, &pstBitmap, &elemX->bitmap, args->state, &match);
			if ((match != MATCH_EXACT) && (masterX != NULL))
			{
				objM = PerStateInfo_ObjForState(tree, &pstBitmap, &masterX->bitmap, args->state, &matchM);
				if (matchM > match)
					obj = objM;
			}
			break;
		}
		case 2:
		{
			obj = PerStateInfo_ObjForState(tree, &pstColor, &elemX->fg, args->state, &match);
			if ((match != MATCH_EXACT) && (masterX != NULL))
			{
				objM = PerStateInfo_ObjForState(tree, &pstColor, &masterX->fg, args->state, &matchM);
				if (matchM > match)
					obj = objM;
			}
			/* When -foreground isn't specified, GC default (black) is used */
			if (ObjectIsEmpty(obj))
				obj = Tcl_NewStringObj("black", -1);
			break;
		}
	}
	if (obj != NULL)
		Tcl_SetObjResult(tree->interp, obj);
	return TCL_OK;
}

ElementType elemTypeBitmap = {
	"bitmap",
	sizeof(ElementBitmap),
	bitmapOptionSpecs,
	NULL,
	CreateBitmap,
	DeleteBitmap,
	ConfigBitmap,
	DisplayBitmap,
	LayoutBitmap,
	WorldChangedBitmap,
	StateProcBitmap,
	UndefProcBitmap,
	ActualProcBitmap
};

/*****/

typedef struct ElementBorder ElementBorder;

struct ElementBorder
{
	Element header; /* Must be first */
	PerStateInfo border;
	PerStateInfo relief;
	int thickness;
	Tcl_Obj *thicknessObj;
	int width;
	Tcl_Obj *widthObj;
	int height;
	Tcl_Obj *heightObj;
	int filled;
};

#define BORDER_CONF_BG 0x0001
#define BORDER_CONF_RELIEF 0x0002
#define BORDER_CONF_SIZE 0x0004
#define BORDER_CONF_THICKNESS 0x0008
#define BORDER_CONF_FILLED 0x0010

static Tk_OptionSpec borderOptionSpecs[] = {
	{TK_OPTION_STRING, "-background", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementBorder, border.obj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL, BORDER_CONF_BG},
	{TK_OPTION_CUSTOM, "-filled", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(ElementBorder, filled),
		TK_OPTION_NULL_OK, (ClientData) &booleanCO, BORDER_CONF_FILLED},
	{TK_OPTION_PIXELS, "-height", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementBorder, heightObj),
		Tk_Offset(ElementBorder, height),
		TK_OPTION_NULL_OK, (ClientData) NULL, BORDER_CONF_SIZE},
	{TK_OPTION_STRING, "-relief", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementBorder, relief.obj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL, BORDER_CONF_RELIEF},
	{TK_OPTION_PIXELS, "-thickness", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementBorder, thicknessObj),
		Tk_Offset(ElementBorder, thickness),
		TK_OPTION_NULL_OK, (ClientData) NULL, BORDER_CONF_THICKNESS},
	{TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementBorder, widthObj),
		Tk_Offset(ElementBorder, width),
		TK_OPTION_NULL_OK, (ClientData) NULL, BORDER_CONF_SIZE},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void DeleteBorder(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementBorder *elemX = (ElementBorder *) elem;

	PerStateInfo_Free(tree, &pstBorder, &elemX->border);
	PerStateInfo_Free(tree, &pstRelief, &elemX->relief);
}

static int WorldChangedBorder(ElementArgs *args)
{
	int flagM = args->change.flagMaster;
	int flagS = args->change.flagSelf;
	int mask = 0;

	if ((flagS | flagM) & BORDER_CONF_SIZE)
		mask |= CS_DISPLAY | CS_LAYOUT;

	if ((flagS | flagM) & (BORDER_CONF_BG | BORDER_CONF_RELIEF |
		BORDER_CONF_THICKNESS | BORDER_CONF_FILLED))
		mask |= CS_DISPLAY;

	return mask;
}

static int ConfigBorder(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementBorder *elemX = (ElementBorder *) elem;
	ElementBorder savedX;
	Tk_SavedOptions savedOptions;
	int error;
	Tcl_Obj *errorResult = NULL;

	for (error = 0; error <= 1; error++)
	{
		if (error == 0)
		{
			if (Tk_SetOptions(tree->interp, (char *) elemX,
				elem->typePtr->optionTable,
				args->config.objc, args->config.objv, tree->tkwin,
				&savedOptions, &args->config.flagSelf) != TCL_OK)
			{
				args->config.flagSelf = 0;
				continue;
			}

			if (args->config.flagSelf & BORDER_CONF_BG)
				PSTSave(&elemX->border, &savedX.border);
			if (args->config.flagSelf & BORDER_CONF_RELIEF)
				PSTSave(&elemX->relief, &savedX.relief);

			if (args->config.flagSelf & BORDER_CONF_BG)
			{
				if (PerStateInfo_FromObj(tree, &pstBorder, &elemX->border) != TCL_OK)
					continue;
			}

			if (args->config.flagSelf & BORDER_CONF_RELIEF)
			{
				if (PerStateInfo_FromObj(tree, &pstRelief, &elemX->relief) != TCL_OK)
					continue;
			}

			if (args->config.flagSelf & BORDER_CONF_BG)
				PerStateInfo_Free(tree, &pstBorder, &savedX.border);
			if (args->config.flagSelf & BORDER_CONF_RELIEF)
				PerStateInfo_Free(tree, &pstRelief, &savedX.relief);
			Tk_FreeSavedOptions(&savedOptions);
			break;
		}
		else
		{
			errorResult = Tcl_GetObjResult(tree->interp);
			Tcl_IncrRefCount(errorResult);
			Tk_RestoreSavedOptions(&savedOptions);

			if (args->config.flagSelf & BORDER_CONF_BG)
				PSTRestore(tree, &pstBorder, &elemX->border, &savedX.border);

			if (args->config.flagSelf & BORDER_CONF_RELIEF)
				PSTRestore(tree, &pstRelief, &elemX->relief, &savedX.relief);

			Tcl_SetObjResult(tree->interp, errorResult);
			Tcl_DecrRefCount(errorResult);
			return TCL_ERROR;
		}
	}

	return TCL_OK;
}

static int CreateBorder(ElementArgs *args)
{
	Element *elem = args->elem;
	ElementBorder *elemX = (ElementBorder *) elem;

	elemX->filled = -1;
	return TCL_OK;
}

static void DisplayBorder(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementBorder *elemX = (ElementBorder *) elem;
	ElementBorder *masterX = (ElementBorder *) elem->master;
	int state = args->state;
	int match, match2;
	Tk_3DBorder border;
	int relief, filled = FALSE;
	int thickness = 0;

	border = PerStateBorder_ForState(tree, &elemX->border, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Tk_3DBorder border2 = PerStateBorder_ForState(tree, &masterX->border, state, &match2);
		if (match2 > match)
			border = border2;
	}

	relief = PerStateRelief_ForState(tree, &elemX->relief, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		int relief2 = PerStateRelief_ForState(tree, &masterX->relief, state, &match2);
		if (match2 > match)
			relief = relief2;
	}

	if (elemX->thicknessObj)
		thickness = elemX->thickness;
	else if ((masterX != NULL) && (masterX->thicknessObj != NULL))
		thickness = masterX->thickness;

	if (elemX->filled != -1)
		filled = elemX->filled;
	else if ((masterX != NULL) && (masterX->filled != -1))
		filled = masterX->filled;

	if (border != NULL)
	{
		if (relief == TK_RELIEF_NULL)
			relief = TK_RELIEF_FLAT;
		if (filled)
		{
			Tk_Fill3DRectangle(tree->tkwin, args->display.drawable, border,
				args->display.x, 
				args->display.y,
				args->display.width, args->display.height,
				thickness, relief);
		}
		else if (thickness > 0)
		{
			Tk_Draw3DRectangle(tree->tkwin, args->display.drawable, border,
				args->display.x, 
				args->display.y,
				args->display.width, args->display.height,
				thickness, relief);
		}
	}
}

static void LayoutBorder(ElementArgs *args)
{
	Element *elem = args->elem;
	ElementBorder *elemX = (ElementBorder *) elem;
	ElementBorder *masterX = (ElementBorder *) elem->master;
	int width, height;

	if (elemX->widthObj != NULL)
		width = elemX->width;
	else if ((masterX != NULL) && (masterX->widthObj != NULL))
		width = masterX->width;
	else
		width = 0;
	args->layout.width = width;

	if (elemX->heightObj != NULL)
		height = elemX->height;
	else if ((masterX != NULL) && (masterX->heightObj != NULL))
		height = masterX->height;
	else
		height = 0;
	args->layout.height = height;
}

static int StateProcBorder(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementBorder *elemX = (ElementBorder *) elem;
	ElementBorder *masterX = (ElementBorder *) elem->master;
	int match, match2;
	Tk_3DBorder border1, border2;
	int relief1, relief2;
	int mask = 0;

	border1 = PerStateBorder_ForState(tree, &elemX->border, args->states.state1, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Tk_3DBorder border = PerStateBorder_ForState(tree, &masterX->border, args->states.state1, &match2);
		if (match2 > match)
			border1 = border;
	}

	relief1 = PerStateRelief_ForState(tree, &elemX->relief, args->states.state1, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		int relief = PerStateRelief_ForState(tree, &masterX->relief, args->states.state1, &match2);
		if (match2 > match)
			relief1 = relief;
	}

	border2 = PerStateBorder_ForState(tree, &elemX->border, args->states.state2, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Tk_3DBorder border = PerStateBorder_ForState(tree, &masterX->border, args->states.state2, &match2);
		if (match2 > match)
			border2 = border;
	}

	relief2 = PerStateRelief_ForState(tree, &elemX->relief, args->states.state2, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		int relief = PerStateRelief_ForState(tree, &masterX->relief, args->states.state2, &match2);
		if (match2 > match)
			relief2 = relief;
	}

	if ((border1 != border2) || (relief1 != relief2))
		mask |= CS_DISPLAY;

	return mask;
}

static void UndefProcBorder(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	ElementBorder *elemX = (ElementBorder *) args->elem;

	PerStateInfo_Undefine(tree, &pstBorder, &elemX->border, args->state);
	PerStateInfo_Undefine(tree, &pstRelief, &elemX->relief, args->state);
}

static int ActualProcBorder(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	ElementBorder *elemX = (ElementBorder *) args->elem;
	ElementBorder *masterX = (ElementBorder *) args->elem->master;
	static CONST char *optionName[] = {
		"-background", "-relief",
		(char *) NULL };
	int index, match, matchM;
	Tcl_Obj *obj = NULL, *objM;

	if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
		return TCL_ERROR;

	switch (index)
	{
		case 0:
		{
			obj = PerStateInfo_ObjForState(tree, &pstBorder, &elemX->border, args->state, &match);
			if ((match != MATCH_EXACT) && (masterX != NULL))
			{
				objM = PerStateInfo_ObjForState(tree, &pstBorder, &masterX->border, args->state, &matchM);
				if (matchM > match)
					obj = objM;
			}
			break;
		}
		case 1:
		{
			obj = PerStateInfo_ObjForState(tree, &pstRelief, &elemX->relief, args->state, &match);
			if ((match != MATCH_EXACT) && (masterX != NULL))
			{
				objM = PerStateInfo_ObjForState(tree, &pstRelief, &masterX->relief, args->state, &matchM);
				if (matchM > match)
					obj = objM;
			}
			if (ObjectIsEmpty(obj))
				obj = Tcl_NewStringObj("flat", -1);
			break;
		}
	}
	if (obj != NULL)
		Tcl_SetObjResult(tree->interp, obj);
	return TCL_OK;
}

ElementType elemTypeBorder = {
	"border",
	sizeof(ElementBorder),
	borderOptionSpecs,
	NULL,
	CreateBorder,
	DeleteBorder,
	ConfigBorder,
	DisplayBorder,
	LayoutBorder,
	WorldChangedBorder,
	StateProcBorder,
	UndefProcBorder,
	ActualProcBorder
};

/*****/

typedef struct ElementImage ElementImage;

struct ElementImage
{
	Element header;
	PerStateInfo image;
	int width;
	Tcl_Obj *widthObj;
	int height;
	Tcl_Obj *heightObj;
};

#define IMAGE_CONF_IMAGE 0x0001
#define IMAGE_CONF_SIZE 0x0002

static Tk_OptionSpec imageOptionSpecs[] = {
	{TK_OPTION_PIXELS, "-height", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementImage, heightObj),
		Tk_Offset(ElementImage, height),
		TK_OPTION_NULL_OK, (ClientData) NULL, IMAGE_CONF_SIZE},
	{TK_OPTION_STRING, "-image", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementImage, image.obj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL, IMAGE_CONF_IMAGE},
	{TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementImage, widthObj),
		Tk_Offset(ElementImage, width),
		TK_OPTION_NULL_OK, (ClientData) NULL, IMAGE_CONF_SIZE},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void DeleteImage(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementImage *elemX = (ElementImage *) elem;

	PerStateInfo_Free(tree, &pstImage, &elemX->image);
}

static int WorldChangedImage(ElementArgs *args)
{
	int flagM = args->change.flagMaster;
	int flagS = args->change.flagSelf;
	int mask = 0;

	if ((flagS | flagM) & (IMAGE_CONF_IMAGE | IMAGE_CONF_SIZE))
		mask |= CS_DISPLAY | CS_LAYOUT;

	return mask;
}

static int ConfigImage(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementImage *elemX = (ElementImage *) elem;
	ElementImage savedX;
	Tk_SavedOptions savedOptions;
	int error;
	Tcl_Obj *errorResult = NULL;

	for (error = 0; error <= 1; error++)
	{
		if (error == 0)
		{
			if (Tk_SetOptions(tree->interp, (char *) elemX,
				elem->typePtr->optionTable,
				args->config.objc, args->config.objv, tree->tkwin,
				&savedOptions, &args->config.flagSelf) != TCL_OK)
			{
				args->config.flagSelf = 0;
				continue;
			}

			if (args->config.flagSelf & IMAGE_CONF_IMAGE)
				PSTSave(&elemX->image, &savedX.image);

			if (args->config.flagSelf & IMAGE_CONF_IMAGE)
			{
				if (PerStateInfo_FromObj(tree, &pstImage, &elemX->image) != TCL_OK)
					continue;
			}

			if (args->config.flagSelf & IMAGE_CONF_IMAGE)
				PerStateInfo_Free(tree, &pstImage, &savedX.image);
			Tk_FreeSavedOptions(&savedOptions);
			break;
		}
		else
		{
			errorResult = Tcl_GetObjResult(tree->interp);
			Tcl_IncrRefCount(errorResult);
			Tk_RestoreSavedOptions(&savedOptions);

			if (args->config.flagSelf & IMAGE_CONF_IMAGE)
				PSTRestore(tree, &pstImage, &elemX->image, &savedX.image);

			Tcl_SetObjResult(tree->interp, errorResult);
			Tcl_DecrRefCount(errorResult);
			return TCL_ERROR;
		}
	}

	return TCL_OK;
}

static int CreateImage(ElementArgs *args)
{
	return TCL_OK;
}

static void DisplayImage(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementImage *elemX = (ElementImage *) elem;
	ElementImage *masterX = (ElementImage *) elem->master;
	int state = args->state;
	int match, matchM;
	Tk_Image image;
	int imgW, imgH;
	int dx = 0, dy = 0;

	image = PerStateImage_ForState(tree, &elemX->image, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Tk_Image imageM = PerStateImage_ForState(tree, &masterX->image, state, &matchM);
		if (matchM > match)
			image = imageM;
	}

	if (image != NULL)
	{
		Tk_SizeOfImage(image, &imgW, &imgH);
		if (imgW < args->display.width)
			dx = (args->display.width - imgW) / 2;
		else if (imgW > args->display.width)
			imgW = args->display.width;
		if (imgH < args->display.height)
			dy = (args->display.height - imgH) / 2;
		else if (imgH > args->display.height)
			imgH = args->display.height;
		Tk_RedrawImage(image, 0, 0, imgW, imgH, args->display.drawable,
			args->display.x /* + args->display.pad[LEFT] */ + dx,
			args->display.y /* + args->display.pad[TOP] */ + dy);
	}
}

static void LayoutImage(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementImage *elemX = (ElementImage *) elem;
	ElementImage *masterX = (ElementImage *) elem->master;
	int state = args->state;
	int match, match2;
	Tk_Image image;
	int width = 0, height = 0;

	image = PerStateImage_ForState(tree, &elemX->image, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Tk_Image image2 = PerStateImage_ForState(tree, &masterX->image, state, &match2);
		if (match2 > match)
			image = image2;
	}

	if (image != NULL)
		Tk_SizeOfImage(image, &width, &height);

	if (elemX->widthObj != NULL)
		width = elemX->width;
	else if ((masterX != NULL) && (masterX->widthObj != NULL))
		width = masterX->width;
	args->layout.width = width;

	if (elemX->heightObj != NULL)
		height = elemX->height;
	else if ((masterX != NULL) && (masterX->heightObj != NULL))
		height = masterX->height;
	args->layout.height = height;
}

static int StateProcImage(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementImage *elemX = (ElementImage *) elem;
	ElementImage *masterX = (ElementImage *) elem->master;
	int match, match2;
	Tk_Image image1, image2;
	int mask = 0;

	image1 = PerStateImage_ForState(tree, &elemX->image, args->states.state1, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Tk_Image image = PerStateImage_ForState(tree, &masterX->image, args->states.state1, &match2);
		if (match2 > match)
			image1 = image;
	}

	image2 = PerStateImage_ForState(tree, &elemX->image, args->states.state2, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Tk_Image image = PerStateImage_ForState(tree, &masterX->image, args->states.state2, &match2);
		if (match2 > match)
			image2 = image;
	}

	if (image1 != image2)
	{
		mask |= CS_DISPLAY;
		if ((image1 != NULL) && (image2 != NULL))
		{
			int w1, h1, w2, h2;
			Tk_SizeOfImage(image1, &w1, &h1);
			Tk_SizeOfImage(image2, &w2, &h2);
			if ((w1 != w2) || (h1 != h2))
				mask |= CS_LAYOUT;
		}
		else
			mask |= CS_LAYOUT;
	}

	return mask;
}

static void UndefProcImage(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	ElementImage *elemX = (ElementImage *) args->elem;

	PerStateInfo_Undefine(tree, &pstImage, &elemX->image, args->state);
}

static int ActualProcImage(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	ElementImage *elemX = (ElementImage *) args->elem;
	ElementImage *masterX = (ElementImage *) args->elem->master;
	static CONST char *optionName[] = {
		"-image",
		(char *) NULL };
	int index, match, matchM;
	Tcl_Obj *obj = NULL, *objM;

	if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
		return TCL_ERROR;

	switch (index)
	{
		case 0:
		{
			obj = PerStateInfo_ObjForState(tree, &pstImage, &elemX->image, args->state, &match);
			if ((match != MATCH_EXACT) && (masterX != NULL))
			{
				objM = PerStateInfo_ObjForState(tree, &pstImage, &masterX->image, args->state, &matchM);
				if (matchM > match)
					obj = objM;
			}
			break;
		}
	}
	if (obj != NULL)
		Tcl_SetObjResult(tree->interp, obj);
	return TCL_OK;
}

ElementType elemTypeImage = {
	"image",
	sizeof(ElementImage),
	imageOptionSpecs,
	NULL,
	CreateImage,
	DeleteImage,
	ConfigImage,
	DisplayImage,
	LayoutImage,
	WorldChangedImage,
	StateProcImage,
	UndefProcImage,
	ActualProcImage
};

/*****/

typedef struct ElementRect ElementRect;

struct ElementRect
{
	Element header;
	int width;
	Tcl_Obj *widthObj;
	int height;
	Tcl_Obj *heightObj;
	PerStateInfo fill;
	PerStateInfo outline;
	int outlineWidth;
	Tcl_Obj *outlineWidthObj;
	int open;
	char *openString;
	int showFocus;
};

#define RECT_CONF_FILL 0x0001
#define RECT_CONF_OUTLINE 0x0002
#define RECT_CONF_OUTWIDTH 0x0004
#define RECT_CONF_OPEN 0x0008
#define RECT_CONF_SIZE 0x0010
#define RECT_CONF_FOCUS 0x0020

static Tk_OptionSpec rectOptionSpecs[] = {
	{TK_OPTION_STRING, "-fill", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementRect, fill.obj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_FILL},
	{TK_OPTION_PIXELS, "-height", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementRect, heightObj),
		Tk_Offset(ElementRect, height),
		TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_SIZE},
	{TK_OPTION_STRING, "-open", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(ElementRect, openString),
		TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_OPEN},
	{TK_OPTION_STRING, "-outline", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementRect, outline.obj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_OUTLINE},
	{TK_OPTION_PIXELS, "-outlinewidth", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementRect, outlineWidthObj),
		Tk_Offset(ElementRect, outlineWidth),
		TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_OUTWIDTH},
	{TK_OPTION_CUSTOM, "-showfocus", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(ElementRect, showFocus),
		TK_OPTION_NULL_OK, (ClientData) &booleanCO, RECT_CONF_FOCUS},
	{TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementRect, widthObj),
		Tk_Offset(ElementRect, width),
		TK_OPTION_NULL_OK, (ClientData) NULL, RECT_CONF_SIZE},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void DeleteRect(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	ElementRect *elemX = (ElementRect *) args->elem;

	PerStateInfo_Free(tree, &pstColor, &elemX->fill);
	PerStateInfo_Free(tree, &pstColor, &elemX->outline);
}

static int WorldChangedRect(ElementArgs *args)
{
	int flagM = args->change.flagMaster;
	int flagS = args->change.flagSelf;
	int mask = 0;

	if ((flagS | flagM) & (RECT_CONF_SIZE | RECT_CONF_OUTWIDTH))
		mask |= CS_DISPLAY | CS_LAYOUT;

	if ((flagS | flagM) & (RECT_CONF_FILL | RECT_CONF_OUTLINE |
		RECT_CONF_OPEN | RECT_CONF_FOCUS))
		mask |= CS_DISPLAY;

	return mask;
}

static int ConfigRect(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementRect *elemX = (ElementRect *) elem;
	ElementRect savedX;
	Tk_SavedOptions savedOptions;
	int error;
	Tcl_Obj *errorResult = NULL;
	int i;

	for (error = 0; error <= 1; error++)
	{
		if (error == 0)
		{
			if (Tk_SetOptions(tree->interp, (char *) elemX,
				elem->typePtr->optionTable,
				args->config.objc, args->config.objv, tree->tkwin,
				&savedOptions, &args->config.flagSelf) != TCL_OK)
			{
				args->config.flagSelf = 0;
				continue;
			}

			if (args->config.flagSelf & RECT_CONF_FILL)
				PSTSave(&elemX->fill, &savedX.fill);
			if (args->config.flagSelf & RECT_CONF_OUTLINE)
				PSTSave(&elemX->outline, &savedX.outline);
			if (args->config.flagSelf & RECT_CONF_OPEN)
				savedX.open = elemX->open;

			if (args->config.flagSelf & RECT_CONF_FILL)
			{
				if (PerStateInfo_FromObj(tree, &pstColor, &elemX->fill) != TCL_OK)
					continue;
			}

			if (args->config.flagSelf & RECT_CONF_OUTLINE)
			{
				if (PerStateInfo_FromObj(tree, &pstColor, &elemX->outline) != TCL_OK)
					continue;
			}

			if (args->config.flagSelf & RECT_CONF_OPEN)
			{
				elemX->open = 0;
				if (elemX->openString != NULL)
				{
					int badChar = 0;

					for (i = 0; elemX->openString[i]; i++)
					{
						switch (elemX->openString[i])
						{
							case 'w': case 'W': elemX->open |= 0x01; break;
							case 'n': case 'N': elemX->open |= 0x02; break;
							case 'e': case 'E': elemX->open |= 0x04; break;
							case 's': case 'S': elemX->open |= 0x08; break;
							default:
							{
								Tcl_ResetResult(tree->interp);
								Tcl_AppendResult(tree->interp, "bad open value \"",
									elemX->openString, "\": must be a string ",
									"containing zero or more of n, e, s, and w",
									(char *) NULL);
								badChar = 1;
								break;
							}
						}
						if (badChar)
							break;
					}
					if (badChar)
						continue;
				}
			}

			if (args->config.flagSelf & RECT_CONF_FILL)
				PerStateInfo_Free(tree, &pstColor, &savedX.fill);
			if (args->config.flagSelf & RECT_CONF_OUTLINE)
				PerStateInfo_Free(tree, &pstColor, &savedX.outline);
			Tk_FreeSavedOptions(&savedOptions);
			break;
		}
		else
		{
			errorResult = Tcl_GetObjResult(tree->interp);
			Tcl_IncrRefCount(errorResult);
			Tk_RestoreSavedOptions(&savedOptions);

			if (args->config.flagSelf & RECT_CONF_FILL)
				PSTRestore(tree, &pstColor, &elemX->fill, &savedX.fill);

			if (args->config.flagSelf & RECT_CONF_OUTLINE)
				PSTRestore(tree, &pstColor, &elemX->outline, &savedX.outline);

			if (args->config.flagSelf & RECT_CONF_OPEN)
				elemX->open = savedX.open;

			Tcl_SetObjResult(tree->interp, errorResult);
			Tcl_DecrRefCount(errorResult);
			return TCL_ERROR;
		}
	}

	return TCL_OK;
}

static int CreateRect(ElementArgs *args)
{
	ElementRect *elemX = (ElementRect *) args->elem;

	elemX->showFocus = -1;
	return TCL_OK;
}

static void DisplayRect(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementRect *elemX = (ElementRect *) elem;
	ElementRect *masterX = (ElementRect *) elem->master;
	int state = args->state;
	int match, match2;
	XColor *color, *color2;
	int open = 0;
	int outlineWidth = 0;
	int showFocus = 0;

	if (elemX->outlineWidthObj != NULL)
		outlineWidth = elemX->outlineWidth;
	else if ((masterX != NULL) && (masterX->outlineWidthObj != NULL))
		outlineWidth = masterX->outlineWidth;

	if (elemX->openString != NULL)
		open = elemX->open;
	else if ((masterX != NULL) && (masterX->openString != NULL))
		open = masterX->open;

	if (elemX->showFocus != -1)
		showFocus = elemX->showFocus;
	else if ((masterX != NULL) && (masterX->showFocus != -1))
		showFocus = masterX->showFocus;

	color = PerStateColor_ForState(tree, &elemX->fill, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		color2 = PerStateColor_ForState(tree, &masterX->fill, state, &match2);
		if (match2 > match)
			color = color2;
	}
	if (color != NULL)
	{
		XFillRectangle(tree->display, args->display.drawable,
			Tk_GCForColor(color, Tk_WindowId(tree->tkwin)),
			args->display.x, args->display.y,
			args->display.width, args->display.height);
	}

	color = PerStateColor_ForState(tree, &elemX->outline, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		color2 = PerStateColor_ForState(tree, &masterX->outline, state, &match2);
		if (match2 > match)
			color = color2;
	}
	if ((color != NULL) && (outlineWidth > 0))
	{
		GC gc = Tk_GCForColor(color, Tk_WindowId(tree->tkwin));
		int w1, w2;

		w1 = outlineWidth / 2;
		w2 = outlineWidth - w1;
#if 0
		if (open == 0)
		{
			XDrawRectangle(tree->display, args->display.drawable, gc,
				args->display.x + w1, args->display.y + w1,
				args->display.width - outlineWidth,
				args->display.height - outlineWidth);
		}
		else
#endif
		{
			int x = args->display.x;
			int y = args->display.y;
			int w = args->display.width;
			int h = args->display.height;

			if (!(open & 0x01))
				XFillRectangle(tree->display, args->display.drawable, gc,
					x, y, outlineWidth, h);
			if (!(open & 0x02))
				XFillRectangle(tree->display, args->display.drawable, gc,
					x, y, w, outlineWidth);
			if (!(open & 0x04))
				XFillRectangle(tree->display, args->display.drawable, gc,
					x + w - outlineWidth, y, outlineWidth, h);
			if (!(open & 0x08))
				XFillRectangle(tree->display, args->display.drawable, gc,
					x, y + h - outlineWidth, w, outlineWidth);
		}
	}

	if (showFocus && (state & STATE_FOCUS) && (state & STATE_ACTIVE))
	{
		DrawActiveOutline(tree, args->display.drawable,
			args->display.x, args->display.y,
			args->display.width, args->display.height,
			open);
	}
}

static void LayoutRect(ElementArgs *args)
{
	Element *elem = args->elem;
	ElementRect *elemX = (ElementRect *) elem;
	ElementRect *masterX = (ElementRect *) elem->master;
	int width, height, outlineWidth;

	if (elemX->outlineWidthObj != NULL)
		outlineWidth = elemX->outlineWidth;
	else if ((masterX != NULL) && (masterX->outlineWidthObj != NULL))
		outlineWidth = masterX->outlineWidth;
	else
		outlineWidth = 0;

	if (elemX->widthObj != NULL)
		width = elemX->width;
	else if ((masterX != NULL) && (masterX->widthObj != NULL))
		width = masterX->width;
	else
		width = 0;
	args->layout.width = MAX(width, outlineWidth * 2);

	if (elemX->heightObj != NULL)
		height = elemX->height;
	else if ((masterX != NULL) && (masterX->heightObj != NULL))
		height = masterX->height;
	else
		height = 0;
	args->layout.height = MAX(height, outlineWidth * 2);
}

static int StateProcRect(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementRect *elemX = (ElementRect *) elem;
	ElementRect *masterX = (ElementRect *) elem->master;
	int match, match2;
	XColor *f1, *f2;
	XColor *o1, *o2;
	int s1, s2;
	int showFocus = 0;
	int mask = 0;

	if (elemX->showFocus != -1)
		showFocus = elemX->showFocus;
	else if ((masterX != NULL) && (masterX->showFocus != -1))
		showFocus = masterX->showFocus;

	f1 = PerStateColor_ForState(tree, &elemX->fill, args->states.state1, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *f = PerStateColor_ForState(tree, &masterX->fill, args->states.state1, &match2);
		if (match2 > match)
			f1 = f;
	}

	o1 = PerStateColor_ForState(tree, &elemX->outline, args->states.state1, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *o = PerStateColor_ForState(tree, &masterX->outline, args->states.state1, &match2);
		if (match2 > match)
			o1 = o;
	}

	s1 = showFocus &&
		(args->states.state1 & STATE_FOCUS) &&
		(args->states.state1 & STATE_ACTIVE);
 
	f2 = PerStateColor_ForState(tree, &elemX->fill, args->states.state2, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *f = PerStateColor_ForState(tree, &masterX->fill, args->states.state2, &match2);
		if (match2 > match)
			f2 = f;
	}

	o2 = PerStateColor_ForState(tree, &elemX->outline, args->states.state2, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *o = PerStateColor_ForState(tree, &masterX->outline, args->states.state2, &match2);
		if (match2 > match)
			o2 = o;
	}

	s2 = showFocus &&
		(args->states.state2 & STATE_FOCUS) &&
		(args->states.state2 & STATE_ACTIVE);

	if ((f1 != f2) || (o1 != o2) || (s1 != s2))
		mask |= CS_DISPLAY;

	return mask;
}

static void UndefProcRect(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	ElementRect *elemX = (ElementRect *) args->elem;

	PerStateInfo_Undefine(tree, &pstColor, &elemX->fill, args->state);
	PerStateInfo_Undefine(tree, &pstColor, &elemX->outline, args->state);
}

static int ActualProcRect(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	ElementRect *elemX = (ElementRect *) args->elem;
	ElementRect *masterX = (ElementRect *) args->elem->master;
	static CONST char *optionName[] = {
		"-fill", "-outline",
		(char *) NULL };
	int index, match, matchM;
	Tcl_Obj *obj = NULL, *objM;

	if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
		return TCL_ERROR;

	switch (index)
	{
		case 0:
		{
			obj = PerStateInfo_ObjForState(tree, &pstColor, &elemX->fill, args->state, &match);
			if ((match != MATCH_EXACT) && (masterX != NULL))
			{
				objM = PerStateInfo_ObjForState(tree, &pstColor, &masterX->fill, args->state, &matchM);
				if (matchM > match)
					obj = objM;
			}
			break;
		}
		case 1:
		{
			obj = PerStateInfo_ObjForState(tree, &pstColor, &elemX->outline, args->state, &match);
			if ((match != MATCH_EXACT) && (masterX != NULL))
			{
				objM = PerStateInfo_ObjForState(tree, &pstColor, &masterX->outline, args->state, &matchM);
				if (matchM > match)
					obj = objM;
			}
			break;
		}
	}
	if (obj != NULL)
		Tcl_SetObjResult(tree->interp, obj);
	return TCL_OK;
}

ElementType elemTypeRect = {
	"rect",
	sizeof(ElementRect),
	rectOptionSpecs,
	NULL,
	CreateRect,
	DeleteRect,
	ConfigRect,
	DisplayRect,
	LayoutRect,
	WorldChangedRect,
	StateProcRect,
	UndefProcRect,
	ActualProcRect
};

/*****/

typedef struct ElementText ElementText;

/* Compile-time option */
#define USE_TEXT_DATA 1

struct ElementText
{
	Element header;
	Tcl_Obj *textObj; /* -text */
	char *text;
	int textLen;
#ifdef USE_TEXT_DATA
	Tcl_Obj *dataObj;
#define TDT_NULL -1
#define TDT_DOUBLE 0
#define TDT_INTEGER 1
#define TDT_LONG 2
#define TDT_STRING 3
#define TDT_TIME 4
#define TEXT_CONF_DATA 0x0001000 /* for Tk_SetOptions() */
	int dataType; /* -datatype */
	Tcl_Obj *formatObj; /* -format */
	int stringRepInvalid;
#endif /* USE_TEXT_DATA */
	PerStateInfo font;
	PerStateInfo fill;
	struct PerStateGC *gc;
#define TK_JUSTIFY_NULL -1
	int justify; /* -justify */
	int lines; /* -lines */
	Tcl_Obj *widthObj; /* -width */
	int width; /* -width */
#define TEXT_WRAP_NULL -1
#define TEXT_WRAP_CHAR 0
#define TEXT_WRAP_WORD 1
	int wrap; /* -wrap */
	TextLayout layout;
	int layoutInvalid;
	int layoutWidth;
};

/* for Tk_SetOptions() */
#define TEXT_CONF_FONT 0x00000001
#define TEXT_CONF_FILL 0x00000002
#define TEXT_CONF_TEXTOBJ 0x00000010
#define TEXT_CONF_LAYOUT 0x00000020

static CONST char *textDataTypeST[] = { "double", "integer", "long", "string",
	"time", (char *) NULL };
static StringTableClientData textDataTypeCD =
{
	textDataTypeST,
	"datatype"
};
static Tk_ObjCustomOption textDataTypeCO =
{
	"datatype",
	StringTableSet,
	StringTableGet,
	StringTableRestore,
	NULL,
	(ClientData) &textDataTypeCD
};

static CONST char *textJustifyST[] = { "left", "right", "center", (char *) NULL };
static StringTableClientData textJustifyCD =
{
	textJustifyST,
	"justification"
};
static Tk_ObjCustomOption textJustifyCO =
{
	"justification",
	StringTableSet,
	StringTableGet,
	StringTableRestore,
	NULL,
	(ClientData) &textJustifyCD
};

static IntegerClientData textLinesCD =
{
	0, /* min */
	0, /* max (ignored) */
	-1, /* empty */
	0x01 /* flags: min */
};
static Tk_ObjCustomOption textLinesCO =
{
	"integer",
	IntegerSet,
	IntegerGet,
	IntegerRestore,
	NULL,
	(ClientData) &textLinesCD
};

static CONST char *textWrapST[] = { "char", "word", (char *) NULL };
static StringTableClientData textWrapCD =
{
	textWrapST,
	"wrap"
};
static Tk_ObjCustomOption textWrapCO =
{
	"wrap",
	StringTableSet,
	StringTableGet,
	StringTableRestore,
	NULL,
	(ClientData) &textWrapCD
};

static Tk_OptionSpec textOptionSpecs[] = {
#ifdef USE_TEXT_DATA
	{TK_OPTION_STRING, "-data", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementText, dataObj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_DATA},
	{TK_OPTION_CUSTOM, "-datatype", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(ElementText, dataType),
		TK_OPTION_NULL_OK, (ClientData) &textDataTypeCO, TEXT_CONF_DATA},
	{TK_OPTION_STRING, "-format", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementText, formatObj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_DATA},
#endif /* USE_TEXT_DATA */
	{TK_OPTION_STRING, "-fill", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementText, fill.obj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL,  TEXT_CONF_FILL},
	{TK_OPTION_STRING, "-font", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementText, font.obj), -1,
		TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_FONT},
	{TK_OPTION_CUSTOM, "-justify", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(ElementText, justify),
		TK_OPTION_NULL_OK, (ClientData) &textJustifyCO, TEXT_CONF_LAYOUT},
	{TK_OPTION_CUSTOM, "-lines", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(ElementText, lines),
		TK_OPTION_NULL_OK, (ClientData) &textLinesCO, TEXT_CONF_LAYOUT},
	{TK_OPTION_STRING, "-text", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementText, textObj),
		Tk_Offset(ElementText, text),
		TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_TEXTOBJ},
	{TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
		(char *) NULL, Tk_Offset(ElementText, widthObj),
		Tk_Offset(ElementText, width),
		TK_OPTION_NULL_OK, (ClientData) NULL, TEXT_CONF_LAYOUT},
	{TK_OPTION_CUSTOM, "-wrap", (char *) NULL, (char *) NULL,
		(char *) NULL, -1, Tk_Offset(ElementText, wrap),
		TK_OPTION_NULL_OK, (ClientData) &textWrapCO, TEXT_CONF_LAYOUT},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, 0, 0}
};

static int WorldChangedText(ElementArgs *args)
{
/*	TreeCtrl *tree = args->tree;*/
	Element *elem = args->elem;
	ElementText *elemX = (ElementText *) elem;
/*	ElementText *masterX = (ElementText *) elem->master;*/
	int flagT = args->change.flagTree;
	int flagM = args->change.flagMaster;
	int flagS = args->change.flagSelf;
	int mask = 0;

#ifdef USE_TEXT_DATA
	if ((flagS | flagM) & (TEXT_CONF_DATA | TEXT_CONF_TEXTOBJ))
	{
		elemX->stringRepInvalid = TRUE;
		mask |= CS_DISPLAY | CS_LAYOUT;
	}
#endif /* USE_TEXT_DATA */

	if (elemX->stringRepInvalid ||
		((flagS | flagM) & (TEXT_CONF_FONT | TEXT_CONF_LAYOUT)) ||
		/* Not always needed */
		(flagT & TREE_CONF_FONT))
	{
		elemX->layoutInvalid = TRUE;
		mask |= CS_DISPLAY | CS_LAYOUT;
	}

	if ((flagS | flagM) & TEXT_CONF_FILL)
		mask |= CS_DISPLAY;

	return mask;
}

#ifdef USE_TEXT_DATA

static void TextUpdateStringRep(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementText *elemX = (ElementText *) elem;
	ElementText *masterX = (ElementText *) elem->master;
	Tcl_Obj *dataObj, *formatObj, *textObj;
	int dataType;

	dataObj = elemX->dataObj;
	if ((dataObj == NULL) && (masterX != NULL))
		dataObj = masterX->dataObj; 

	dataType = elemX->dataType;
	if ((dataType == TDT_NULL) && (masterX != NULL))
		dataType = masterX->dataType; 

	formatObj = elemX->formatObj;
	if ((formatObj == NULL) && (masterX != NULL))
		formatObj = masterX->formatObj; 

	textObj = elemX->textObj;
	if ((textObj == NULL) && (masterX != NULL))
		textObj = masterX->textObj; 

	if ((elemX->textObj == NULL) && (elemX->text != NULL))
	{
		ckfree(elemX->text);
		elemX->text = NULL;
	}

	/* If -text is specified, -data, -datatype and -format are ignored */
	if (textObj != NULL)
	{
		(void) Tcl_GetStringFromObj(textObj, &elemX->textLen);
	}

	/* Only create a string rep if elemX (not masterX) has dataObj,
		dataType or formatObj. */
	else if ((dataObj != NULL) && (dataType != TDT_NULL) &&
		((elemX->dataObj != NULL) ||
		(elemX->dataType != TDT_NULL) ||
		(elemX->formatObj != NULL)))
	{
		int i, objc = 0;
		Tcl_Obj *objv[5], *resultObj = NULL;
		static Tcl_Obj *staticObj[3] = { NULL };
		static Tcl_Obj *staticFormat[4] = { NULL };
		Tcl_ObjCmdProc *clockObjCmd = NULL;
		Tcl_ObjCmdProc *formatObjCmd = NULL;

		if (staticFormat[0] == NULL)
		{
			staticFormat[0] = Tcl_NewStringObj("%g", -1);
			staticFormat[1] = Tcl_NewStringObj("%d", -1);
			staticFormat[2] = Tcl_NewStringObj("%ld", -1);
			staticFormat[3] = Tcl_NewStringObj("%s", -1);
			for (i = 0; i < 4; i++)
				Tcl_IncrRefCount(staticFormat[i]);
		}
		if (staticObj[0] == NULL)
		{
			staticObj[0] = Tcl_NewStringObj("clock", -1);
			staticObj[1] = Tcl_NewStringObj("format", -1);
			staticObj[2] = Tcl_NewStringObj("-format", -1);
			for (i = 0; i < 3; i++)
				Tcl_IncrRefCount(staticObj[i]);
		}
		if (clockObjCmd == NULL)
		{
			Tcl_CmdInfo cmdInfo;

			if (Tcl_GetCommandInfo(tree->interp, "::clock", &cmdInfo) == 1)
				clockObjCmd = cmdInfo.objProc;
		}
		if (formatObjCmd == NULL)
		{
			Tcl_CmdInfo cmdInfo;

			if (Tcl_GetCommandInfo(tree->interp, "::format", &cmdInfo) == 1)
				formatObjCmd = cmdInfo.objProc;
		}

		/* Important to remove any shared result object, otherwise
		 * calls like Tcl_SetStringObj(Tcl_GetObjResult()) fail. */
		Tcl_ResetResult(tree->interp);

		switch (dataType)
		{
			case TDT_DOUBLE:
				if (formatObjCmd == NULL)
					break;
				if (formatObj == NULL) formatObj = staticFormat[0];
				objv[objc++] = staticObj[1]; /* format */
				objv[objc++] = formatObj;
				objv[objc++] = dataObj;
				if (formatObjCmd(NULL, tree->interp, objc, objv) == TCL_OK)
					resultObj = Tcl_GetObjResult(tree->interp);
				break;
			case TDT_INTEGER:
				if (formatObjCmd == NULL)
					break;
				if (formatObj == NULL) formatObj = staticFormat[1];
				objv[objc++] = staticObj[1]; /* format */
				objv[objc++] = formatObj;
				objv[objc++] = dataObj;
				if (formatObjCmd(NULL, tree->interp, objc, objv) == TCL_OK)
					resultObj = Tcl_GetObjResult(tree->interp);
				break;
			case TDT_LONG:
				if (formatObjCmd == NULL)
					break;
				if (formatObj == NULL) formatObj = staticFormat[2];
				objv[objc++] = staticObj[1]; /* format */
				objv[objc++] = formatObj;
				objv[objc++] = dataObj;
				if (formatObjCmd(NULL, tree->interp, objc, objv) == TCL_OK)
					resultObj = Tcl_GetObjResult(tree->interp);
				break;
			case TDT_STRING:
				if (formatObjCmd == NULL)
					break;
				if (formatObj == NULL) formatObj = staticFormat[3];
				objv[objc++] = staticObj[1]; /* format */
				objv[objc++] = formatObj;
				objv[objc++] = dataObj;
				if (formatObjCmd(NULL, tree->interp, objc, objv) == TCL_OK)
					resultObj = Tcl_GetObjResult(tree->interp);
				break;
			case TDT_TIME:
				if (clockObjCmd == NULL)
					break;
				objv[objc++] = staticObj[0];
				objv[objc++] = staticObj[1];
				objv[objc++] = dataObj;
				if (formatObj != NULL)
				{
					objv[objc++] = staticObj[2];
					objv[objc++] = formatObj;
				}
				if (clockObjCmd(NULL, tree->interp, objc, objv) == TCL_OK)
					resultObj = Tcl_GetObjResult(tree->interp);
				break;
			default:
				panic("unknown ElementText dataType");
				break;
		}

		if (resultObj != NULL)
		{
			char *string = Tcl_GetStringFromObj(resultObj, &elemX->textLen);
			elemX->text = ckalloc(elemX->textLen);
			memcpy(elemX->text, string, elemX->textLen);
		}
	}
}

#endif /* USE_TEXT_DATA */

static void TextUpdateLayout(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementText *elemX = (ElementText *) elem;
	ElementText *masterX = (ElementText *) elem->master;
	int state = args->state;
	int match, match2;
	Tk_Font tkfont, tkfont2;
	char *text = NULL;
	int textLen = 0;
	int justify = TK_JUSTIFY_LEFT;
	int lines = 0;
	int wrap = TEXT_WRAP_WORD;
	int width = 0;
	int flags = 0;
	int i, multiLine = FALSE;

	if (elemX->layout != NULL)
	{
if (1 && tree->debug.enable && tree->debug.display)
	dbwin("TextUpdateLayout %s: free %p (%s)\n", Tk_PathName(tree->tkwin), elemX, masterX ? "instance" : "master");
		TextLayout_Free(elemX->layout);
		elemX->layout = NULL;
	}

	if (elemX->text != NULL)
	{
		text = elemX->text;
		textLen = elemX->textLen;
	}
	else if ((masterX != NULL) && (masterX->text != NULL))
	{
		text = masterX->text;
		textLen = masterX->textLen;
	}
	if ((text == NULL) || (textLen == 0))
		return;

	for (i = 0; i < textLen; i++)
	{
		if ((text[i] == '\n') || (text[i] == '\r'))
		{
			multiLine = TRUE;
			break;
		}
	}

	if (elemX->lines != -1)
		lines = elemX->lines;
	else if ((masterX != NULL) && (masterX->lines != -1))
		lines = masterX->lines;

	if (args->layout.width != -1)
		width = args->layout.width;
	else if (elemX->widthObj != NULL)
		width = elemX->width;
	else if ((masterX != NULL) && (masterX->widthObj != NULL))
		width = masterX->width;
if (0 && tree->debug.enable)
dbwin("lines %d multiLine %d width %d squeeze %d\n",
lines, multiLine, width, args->layout.squeeze);
	if ((lines == 1) || (!multiLine && (width == 0)))
		return;

	if (elemX->justify != TK_JUSTIFY_NULL)
		justify = elemX->justify;
	else if ((masterX != NULL) && (masterX->justify != TK_JUSTIFY_NULL))
		justify = masterX->justify;

	if (elemX->wrap != TEXT_WRAP_NULL)
		wrap = elemX->wrap;
	else if ((masterX != NULL) && (masterX->wrap != TEXT_WRAP_NULL))
		wrap = masterX->wrap;

	tkfont = PerStateFont_ForState(tree, &elemX->font, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		tkfont2 = PerStateFont_ForState(tree, &masterX->font, state, &match2);
		if (match2 > match)
			tkfont = tkfont2;
	}
	if (tkfont == NULL)
		tkfont = tree->tkfont;

	if (wrap == TEXT_WRAP_WORD)
		flags |= TK_WHOLE_WORDS;

	elemX->layout = TextLayout_Compute(tkfont, text,
		Tcl_NumUtfChars(text, textLen), width, justify, lines, flags);

if (1 && tree->debug.enable && tree->debug.display)
	dbwin("TextUpdateLayout %s: alloc %p (%s)\n", Tk_PathName(tree->tkwin), elemX, masterX ? "instance" : "master");
}

static void DeleteText(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementText *elemX = (ElementText *) elem;

	if (elemX->gc != NULL)
		PerStateGC_Free(tree, &elemX->gc);
	PerStateInfo_Free(tree, &pstColor, &elemX->fill);
	PerStateInfo_Free(tree, &pstFont, &elemX->font);
#ifdef USE_TEXT_DATA
	if ((elemX->textObj == NULL) && (elemX->text != NULL))
	{
		ckfree(elemX->text);
		elemX->text = NULL;
	}
#endif
	if (elemX->layout != NULL)
		TextLayout_Free(elemX->layout);
}

static int ConfigText(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementText *elemX = (ElementText *) elem;
	ElementText savedX;
	Tk_SavedOptions savedOptions;
	int error;
	Tcl_Obj *errorResult = NULL;

	for (error = 0; error <= 1; error++)
	{
		if (error == 0)
		{
			if (Tk_SetOptions(tree->interp, (char *) elemX,
				elem->typePtr->optionTable,
				args->config.objc, args->config.objv, tree->tkwin,
				&savedOptions, &args->config.flagSelf) != TCL_OK)
			{
				args->config.flagSelf = 0;
				continue;
			}

			if (args->config.flagSelf & TEXT_CONF_FILL)
				PSTSave(&elemX->fill, &savedX.fill);
			if (args->config.flagSelf & TEXT_CONF_FONT)
				PSTSave(&elemX->font, &savedX.font);

			if (args->config.flagSelf & TEXT_CONF_FILL)
			{
				if (PerStateInfo_FromObj(tree, &pstColor, &elemX->fill) != TCL_OK)
					continue;
			}

			if (args->config.flagSelf & TEXT_CONF_FONT)
			{
				if (PerStateInfo_FromObj(tree, &pstFont, &elemX->font) != TCL_OK)
					continue;
			}

			if (args->config.flagSelf & TEXT_CONF_FILL)
				PerStateInfo_Free(tree, &pstColor, &savedX.fill);
			if (args->config.flagSelf & TEXT_CONF_FONT)
				PerStateInfo_Free(tree, &pstFont, &savedX.font);
			Tk_FreeSavedOptions(&savedOptions);
			break;
		}
		else
		{
			errorResult = Tcl_GetObjResult(tree->interp);
			Tcl_IncrRefCount(errorResult);
			Tk_RestoreSavedOptions(&savedOptions);

			if (args->config.flagSelf & TEXT_CONF_FILL)
				PSTRestore(tree, &pstColor, &elemX->fill, &savedX.fill);

			if (args->config.flagSelf & TEXT_CONF_FONT)
				PSTRestore(tree, &pstFont, &elemX->font, &savedX.font);

			Tcl_SetObjResult(tree->interp, errorResult);
			Tcl_DecrRefCount(errorResult);
			return TCL_ERROR;
		}
	}

	return TCL_OK;
}

static int CreateText(ElementArgs *args)
{
	ElementText *elemX = (ElementText *) args->elem;

#ifdef USE_TEXT_DATA
	elemX->dataType = TDT_NULL;
#endif
	elemX->justify = TK_JUSTIFY_NULL;
	elemX->lines = -1;
	elemX->wrap = TEXT_WRAP_NULL;
	return TCL_OK;
}

static void DisplayText(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementText *elemX = (ElementText *) elem;
	ElementText *masterX = (ElementText *) elem->master;
	int state = args->state;
	int match, match2;
	XColor *color, *color2;
	char *text = elemX->text;
	int textLen = elemX->textLen;
	Tk_Font tkfont, tkfont2;
	TextLayout layout = NULL;
	Tk_FontMetrics fm;
	GC gc;
	int bytesThatFit, pixelsForText;
	char *ellipsis = "...";

	if ((text == NULL) && (masterX != NULL))
	{
		text = masterX->text;
		textLen = masterX->textLen;
	}

	if (text == NULL) /* always false (or layout sets height/width to zero) */
		return;

	color = PerStateColor_ForState(tree, &elemX->fill, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		color2 = PerStateColor_ForState(tree, &masterX->fill, state, &match2);
		if (match2 > match)
			color = color2;
	}

	tkfont = PerStateFont_ForState(tree, &elemX->font, state, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		tkfont2 = PerStateFont_ForState(tree, &masterX->font, state, &match2);
		if (match2 > match)
			tkfont = tkfont2;
	}

	/* FIXME: -font {"" {state...}}*/
	if ((color != NULL) || (tkfont != NULL))
	{
		XGCValues gcValues;
		unsigned long gcMask = 0;
		if (color == NULL)
			color = tree->fgColorPtr;
		gcValues.foreground = color->pixel;
		gcMask |= GCForeground;
		if (tkfont == NULL)
			tkfont = tree->tkfont;
		gcValues.font = Tk_FontId(tkfont);
		gcMask |= GCFont;
		gcValues.graphics_exposures = False;
		gcMask |= GCGraphicsExposures;
		gc = PerStateGC_Get(tree, (masterX != NULL) ? &masterX->gc :
			&elemX->gc, gcMask, &gcValues);
	}
	else
	{
		tkfont = tree->tkfont;
		gc = tree->textGC;
	}

	if (elemX->layout != NULL)
		layout = elemX->layout;

	if (layout != NULL)
	{
		TextLayout_Draw(tree->display, args->display.drawable, gc,
			layout,
			args->display.x /* + args->display.pad[LEFT] */,
			args->display.y /* + args->display.pad[TOP] */,
			0, -1);
		return;
	}

	Tk_GetFontMetrics(tkfont, &fm);

	pixelsForText = args->display.width /* - args->display.pad[LEFT] -
		args->display.pad[RIGHT] */;
	bytesThatFit = Ellipsis(tkfont, text, textLen, &pixelsForText, ellipsis);
	if (bytesThatFit != textLen)
	{
		char staticStr[256], *buf = staticStr;
		int bufLen = abs(bytesThatFit);
		int ellipsisLen = strlen(ellipsis);

		if (bufLen + ellipsisLen > sizeof(staticStr))
			buf = ckalloc(bufLen + ellipsisLen);
		memcpy(buf, text, bufLen);
		if (bytesThatFit > 0)
		{
			memcpy(buf + bufLen, ellipsis, ellipsisLen);
			bufLen += ellipsisLen;
		}
		Tk_DrawChars(tree->display, args->display.drawable, gc,
			tkfont, buf, bufLen, args->display.x /* + args->display.pad[LEFT] */,
			args->display.y /* + args->display.pad[TOP] */ + fm.ascent);
		if (buf != staticStr)
			ckfree(buf);
	}
	else
	{
		Tk_DrawChars(tree->display, args->display.drawable, gc,
			tkfont, text, textLen, args->display.x /* + args->display.pad[LEFT] */,
			args->display.y /* + args->display.pad[TOP] */ + fm.ascent);
	}
}

static void LayoutText(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementText *elemX = (ElementText *) elem;
	ElementText *masterX = (ElementText *) elem->master;
	int state = args->state;
	int match, match2;
	char *text = NULL;
	int textLen = 0;
	Tk_Font tkfont;
	Tk_FontMetrics fm;
	int width = 0;
	TextLayout layout = NULL;

#ifdef USE_TEXT_DATA
	if ((masterX != NULL) /* && (elemX != masterX) */ && masterX->stringRepInvalid)
	{
		args->elem = (Element *) masterX;
		TextUpdateStringRep(args);
		args->elem = elem;
		masterX->stringRepInvalid = FALSE;
	}
	if (elemX->stringRepInvalid)
	{
		TextUpdateStringRep(args);
		elemX->stringRepInvalid = FALSE;
	}
#endif

	if (elemX->layoutInvalid || (elemX->layoutWidth != args->layout.width))
	{
		TextUpdateLayout(args);
		elemX->layoutInvalid = FALSE;
		elemX->layoutWidth = args->layout.width;
	}

	if (elemX->layout != NULL)
		layout = elemX->layout;

	if (layout != NULL)
	{
		TextLayout_Size(layout, &args->layout.width, &args->layout.height);
		return;
	}

	if (elemX->text != NULL)
	{
		text = elemX->text;
		textLen = elemX->textLen;
	}
	else if ((masterX != NULL) && (masterX->text != NULL))
	{
		text = masterX->text;
		textLen = masterX->textLen;
	}

	if (text != NULL)
	{
		tkfont = PerStateFont_ForState(tree, &elemX->font, state, &match);
		if ((match != MATCH_EXACT) && (masterX != NULL))
		{
			Tk_Font tkfont2 = PerStateFont_ForState(tree, &masterX->font, state, &match2);
			if (match2 > match)
				tkfont = tkfont2;
		}
		if (tkfont == NULL)
			tkfont = tree->tkfont;

#if 0
		/* Weird bug with MS Sans Serif 8 bold */
		Tk_MeasureChars(font, text, textLen, -1, 0, &width);
		width2 = width;
		while (Tk_MeasureChars(font, text, textLen, width2, 0, &width) != textLen)
			width2++;
		args->layout.width = width2;
#else
		args->layout.width = Tk_TextWidth(tkfont, text, textLen);
#endif
		if (elemX->widthObj != NULL)
			width = elemX->width;
		else if ((masterX != NULL) && (masterX->widthObj != NULL))
			width = masterX->width;
		if ((width > 0) && (width < args->layout.width))
			args->layout.width = width;
		Tk_GetFontMetrics(tkfont, &fm);
		args->layout.height = fm.linespace; /* TODO: multi-line strings */
		return;
	}

	args->layout.width = args->layout.height = 0;
}

int Element_GetSortData(TreeCtrl *tree, Element *elem, int type, long *lv, double *dv, char **sv)
{
	ElementText *elemX = (ElementText *) elem;
	ElementText *masterX = (ElementText *) elem->master;
	Tcl_Obj *dataObj = elemX->dataObj;
	int dataType = elemX->dataType;
	Tcl_Obj *obj;

	if (dataType == TDT_NULL && masterX != NULL)
		dataType = masterX->dataType;

	switch (type)
	{
		case SORT_ASCII:
		case SORT_DICT:
			if (dataObj != NULL && dataType != TDT_NULL)
				(*sv) = Tcl_GetString(dataObj);
			else
				(*sv) = elemX->text;
			break;
		case SORT_DOUBLE:
			if (dataObj != NULL && dataType == TDT_DOUBLE)
				obj = dataObj;
			else
				obj = elemX->textObj;
			if (obj == NULL)
				return TCL_ERROR;
			if (Tcl_GetDoubleFromObj(tree->interp, obj, dv) != TCL_OK)
				return TCL_ERROR;
			break;
		case SORT_LONG:
			if (dataObj != NULL && dataType != TDT_NULL)
			{
				if (dataType == TDT_LONG || dataType == TDT_TIME)
				{
					if (Tcl_GetLongFromObj(tree->interp, dataObj, lv) != TCL_OK)
						return TCL_ERROR;
					break;
				}
				if (dataType == TDT_INTEGER)
				{
					int iv;
					if (Tcl_GetIntFromObj(tree->interp, dataObj, &iv) != TCL_OK)
						return TCL_ERROR;
					(*lv) = iv;
					break;
				}
			}
			if (elemX->textObj != NULL)
				if (Tcl_GetLongFromObj(tree->interp, elemX->textObj, lv) != TCL_OK)
					return TCL_ERROR;
			break;
	}
	return TCL_OK;
}

static int StateProcText(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	Element *elem = args->elem;
	ElementText *elemX = (ElementText *) elem;
	ElementText *masterX = (ElementText *) elem->master;
	int match, match2;
	XColor *f1, *f2;
	Tk_Font tkfont1, tkfont2;
	int mask = 0;

	f1 = PerStateColor_ForState(tree, &elemX->fill, args->states.state1, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *f = PerStateColor_ForState(tree, &masterX->fill, args->states.state1, &match2);
		if (match2 > match)
			f1 = f;
	}

	tkfont1 = PerStateFont_ForState(tree, &elemX->font, args->states.state1, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Tk_Font tkfont = PerStateFont_ForState(tree, &masterX->font, args->states.state1, &match2);
		if (match2 > match)
			tkfont1 = tkfont;
	}

	f2 = PerStateColor_ForState(tree, &elemX->fill, args->states.state2, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		XColor *f = PerStateColor_ForState(tree, &masterX->fill, args->states.state2, &match2);
		if (match2 > match)
			f2 = f;
	}

	tkfont2 = PerStateFont_ForState(tree, &elemX->font, args->states.state2, &match);
	if ((match != MATCH_EXACT) && (masterX != NULL))
	{
		Tk_Font tkfont = PerStateFont_ForState(tree, &masterX->font, args->states.state2, &match2);
		if (match2 > match)
			tkfont2 = tkfont;
	}

	if (tkfont1 != tkfont2)
		mask |= CS_DISPLAY | CS_LAYOUT;

	if (f1 != f2)
		mask |= CS_DISPLAY;

	return mask;
}

static void UndefProcText(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	ElementText *elemX = (ElementText *) args->elem;

	PerStateInfo_Undefine(tree, &pstColor, &elemX->fill, args->state);
	PerStateInfo_Undefine(tree, &pstFont, &elemX->font, args->state);
}

static int ActualProcText(ElementArgs *args)
{
	TreeCtrl *tree = args->tree;
	ElementText *elemX = (ElementText *) args->elem;
	ElementText *masterX = (ElementText *) args->elem->master;
	static CONST char *optionName[] = {
		"-fill", "-font",
		(char *) NULL };
	int index, match, matchM;
	Tcl_Obj *obj = NULL, *objM;

	if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
		return TCL_ERROR;

	switch (index)
	{
		case 0:
		{
			obj = PerStateInfo_ObjForState(tree, &pstColor, &elemX->fill, args->state, &match);
			if ((match != MATCH_EXACT) && (masterX != NULL))
			{
				objM = PerStateInfo_ObjForState(tree, &pstColor, &masterX->fill, args->state, &matchM);
				if (matchM > match)
					obj = objM;
			}
			if (ObjectIsEmpty(obj))
				obj = tree->fgObj;
			break;
		}
		case 1:
		{
			obj = PerStateInfo_ObjForState(tree, &pstFont, &elemX->font, args->state, &match);
			if ((match != MATCH_EXACT) && (masterX != NULL))
			{
				objM = PerStateInfo_ObjForState(tree, &pstFont, &masterX->font, args->state, &matchM);
				if (matchM > match)
					obj = objM;
			}
			if (ObjectIsEmpty(obj))
				obj = tree->fontObj;
			break;
		}
	}
	if (obj != NULL)
		Tcl_SetObjResult(tree->interp, obj);
	return TCL_OK;
}

ElementType elemTypeText = {
	"text",
	sizeof(ElementText),
	textOptionSpecs,
	NULL,
	CreateText,
	DeleteText,
	ConfigText,
	DisplayText,
	LayoutText,
	WorldChangedText,
	StateProcText,
	UndefProcText,
	ActualProcText
};

/*****/

