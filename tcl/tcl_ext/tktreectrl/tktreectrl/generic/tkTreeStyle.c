#include "tkTreeCtrl.h"
#include "tkTreeElem.h"

#define STATIC_SIZE 20
#define STATIC_ALLOC(P,T,C) \
	if (C > STATIC_SIZE) \
		P = (T *) ckalloc(sizeof(T) * (C))
#define STATIC_FREE(P,T,C) \
	memset((char *) P, 0xAA, sizeof(T) * (C)); \
	if (C > STATIC_SIZE) \
		ckfree((char *) P)

typedef struct Style Style;
typedef struct ElementLink ElementLink;

struct Style
{
	Tk_OptionTable optionTable;
	Tk_Uid name;
	int numElements;
	ElementLink *elements;
	int neededWidth;
	int neededHeight;
	int minWidth;
	int minHeight;
	int layoutWidth;
	int layoutHeight;
	Style *master;
	int vertical;
};

#define ELF_eEXPAND_W 0x0001 /* external expansion */
#define ELF_eEXPAND_N 0x0002
#define ELF_eEXPAND_E 0x0004
#define ELF_eEXPAND_S 0x0008
#define ELF_iEXPAND_W 0x0010 /* internal expansion */
#define ELF_iEXPAND_N 0x0020
#define ELF_iEXPAND_E 0x0040
#define ELF_iEXPAND_S 0x0080
#define ELF_SQUEEZE_X 0x0100 /* shrink if needed */
#define ELF_SQUEEZE_Y 0x0200
#define ELF_DETACH 0x0400

#define ELF_eEXPAND_WE (ELF_eEXPAND_W | ELF_eEXPAND_E)
#define ELF_eEXPAND_NS (ELF_eEXPAND_N | ELF_eEXPAND_S)
#define ELF_eEXPAND (ELF_eEXPAND_WE | ELF_eEXPAND_NS)
#define ELF_iEXPAND_WE (ELF_iEXPAND_W | ELF_iEXPAND_E)
#define ELF_iEXPAND_NS (ELF_iEXPAND_N | ELF_iEXPAND_S)
#define ELF_iEXPAND (ELF_iEXPAND_WE | ELF_iEXPAND_NS)
#define ELF_EXPAND_WE (ELF_eEXPAND_WE | ELF_iEXPAND_WE)
#define ELF_EXPAND_NS (ELF_eEXPAND_NS | ELF_iEXPAND_NS)
#define ELF_EXPAND_W (ELF_eEXPAND_W | ELF_iEXPAND_W)
#define ELF_EXPAND_N (ELF_eEXPAND_N | ELF_iEXPAND_N)
#define ELF_EXPAND_E (ELF_eEXPAND_E | ELF_iEXPAND_E)
#define ELF_EXPAND_S (ELF_eEXPAND_S | ELF_iEXPAND_S)

struct ElementLink
{
	Element *elem;
	int neededWidth;
	int neededHeight;
	int layoutWidth;
	int layoutHeight;
	int ePadX[2]; /* external horizontal padding */
	int ePadY[2]; /* external vertical padding */
	int iPadX[2]; /* internal horizontal padding */
	int iPadY[2]; /* internal vertical padding */
	int flags; /* ELF_xxx */
	int *onion, onionCount; /* -union option info */
};

static ElementType *elementTypeList = NULL;

static char *orientStringTable[] = { "horizontal", "vertical", (char *) NULL };

static Tk_OptionSpec styleOptionSpecs[] = {
	{TK_OPTION_STRING_TABLE, "-orient", (char *) NULL, (char *) NULL,
		"horizontal", -1, Tk_Offset(Style, vertical),
		0, (ClientData) orientStringTable, 0},
	{TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
		(char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

struct Layout
{
	ElementLink *eLink;
	ElementLink *master;
	int useWidth;
	int useHeight;
	int x; /* left of ePad */
	int y; /* above ePad */
	int eWidth; /* ePad + iPad + useWidth + iPad + ePad */
	int eHeight; /* ePad + iPad + useHeight + iPad + ePad */
	int iWidth; /* iPad + useWidth + iPad */
	int iHeight; /* iPad + useHeight + iPad */
	int ePadX[2]; /* external horizontal padding */
	int ePadY[2]; /* external vertical padding */
	int iPadX[2]; /* internal horizontal padding */
	int iPadY[2]; /* internal vertical padding */
	int uPadX[2]; /* padding due to -union */
	int uPadY[2]; /* padding due to -union */
};

static void Style_DoExpandH(struct Layout *layout, int flags, int width)
{
	int extraWidth;
	int *ePadX, *iPadX;
	int eW, eE, iW, iE, eLeft, eRight, iLeft, iRight, eMax, iMax;

	if (!(flags & ELF_EXPAND_WE))
		return;

	extraWidth = width - layout->eWidth;
	if (extraWidth <= 0)
		return;

	/* External: can expand to left and right */
	eW = 0;
	eE = width;
	eLeft = layout->x;
	eRight = width - (layout->x + layout->eWidth);
	eMax = eLeft + eRight;

	/* Internal: can expand to max of ePadX[] or uPadX[] */
	iW = MAX(layout->ePadX[PAD_TOP_LEFT], layout->uPadX[PAD_TOP_LEFT]);
	iE = width - MAX(layout->ePadX[PAD_BOTTOM_RIGHT],
			 layout->uPadX[PAD_BOTTOM_RIGHT]);
	iLeft = layout->x + layout->ePadX[PAD_TOP_LEFT] - iW;
	iRight = iE - (layout->x + layout->eWidth - layout->ePadX[PAD_BOTTOM_RIGHT]);
	iMax = iLeft + iRight;

	ePadX = layout->ePadX;
	iPadX = layout->iPadX;

	/* Internal expansion */
	if (flags & ELF_iEXPAND_WE)
	{
		if ((flags & ELF_iEXPAND_WE) == ELF_iEXPAND_WE)
		{
			iPadX[PAD_TOP_LEFT] += MIN(iMax / 2, iLeft);
			layout->x = iW - ePadX[PAD_TOP_LEFT];
			layout->iWidth += iMax;
			layout->eWidth += iMax;
			iPadX[PAD_BOTTOM_RIGHT] = layout->iWidth - layout->eLink->neededWidth - iPadX[PAD_TOP_LEFT];
		}
		else if (flags & ELF_iEXPAND_W)
		{
			layout->x = iW - ePadX[PAD_TOP_LEFT];
			layout->iWidth += iMax;
			layout->eWidth += iMax;
			iPadX[PAD_TOP_LEFT] = layout->iWidth - layout->eLink->neededWidth - iPadX[PAD_BOTTOM_RIGHT];
		}
		else
		{
			layout->x = iW - ePadX[PAD_TOP_LEFT];
			layout->iWidth += iMax;
			layout->eWidth += iMax;
			iPadX[PAD_BOTTOM_RIGHT] = layout->iWidth - layout->eLink->neededWidth - iPadX[PAD_TOP_LEFT];
		}
		return;
	}

	/* External expansion */
	if (flags & ELF_eEXPAND_WE)
	{
		if ((flags & ELF_eEXPAND_WE) == ELF_eEXPAND_WE)
		{
			int amt = extraWidth / 2;

			layout->x = 0;
			layout->eWidth = width;
			if (ePadX[PAD_TOP_LEFT] + amt + layout->iWidth > iE)
				amt -= (ePadX[PAD_TOP_LEFT] + amt + layout->iWidth) - iE;
			ePadX[PAD_TOP_LEFT] += amt;
			ePadX[PAD_BOTTOM_RIGHT] += extraWidth - amt;
		}
		else if (flags & ELF_eEXPAND_W)
		{
			layout->x = 0;
			layout->eWidth = iE + ePadX[PAD_BOTTOM_RIGHT];
			ePadX[PAD_TOP_LEFT] = layout->eWidth - layout->iWidth - ePadX[PAD_BOTTOM_RIGHT];
		}
		else
		{
			layout->x = iW - ePadX[PAD_TOP_LEFT];
			layout->eWidth = width - layout->x;
			ePadX[PAD_BOTTOM_RIGHT] = layout->eWidth - layout->iWidth - ePadX[PAD_TOP_LEFT];
		}
	}
}

static void Style_DoExpandV(struct Layout *layout, int flags, int height)
{
	int extraHeight;
	int *ePadY, *iPadY;
	int eN, eS, iN, iS, eAbove, eBelow, iAbove, iBelow, eMax, iMax;

	if (!(flags & ELF_EXPAND_NS))
		return;

	extraHeight = height - layout->eHeight;
	if (extraHeight <= 0)
		return;

	ePadY = layout->ePadY;
	iPadY = layout->iPadY;

	/* External: can expand to top and bottom */
	eN = 0;
	eS = height;
	eAbove = layout->y;
	eBelow = height - (layout->y + layout->eHeight);
	eMax = eAbove + eBelow;

	/* Internal: can expand to max of ePadY[] or uPadY[] */
	iN = MAX(ePadY[PAD_TOP_LEFT], layout->uPadY[PAD_TOP_LEFT]);
	iS = height - MAX(ePadY[PAD_BOTTOM_RIGHT], layout->uPadY[PAD_BOTTOM_RIGHT]);
	iAbove = layout->y + ePadY[PAD_TOP_LEFT] - iN;
	iBelow = iS - (layout->y + layout->eHeight - ePadY[PAD_BOTTOM_RIGHT]);
	iMax = iAbove + iBelow;

	/* Internal expansion */
	if (flags & ELF_iEXPAND_NS)
	{
		if ((flags & ELF_iEXPAND_NS) == ELF_iEXPAND_NS)
		{
			iPadY[PAD_TOP_LEFT] += MIN(iMax / 2, iAbove);
			layout->y = iN - ePadY[PAD_TOP_LEFT];
			layout->iHeight += iMax;
			layout->eHeight += iMax;
			iPadY[PAD_BOTTOM_RIGHT] = layout->iHeight - layout->eLink->neededHeight - iPadY[PAD_TOP_LEFT];
		}
		else if (flags & ELF_iEXPAND_N)
		{
			layout->y = iN - ePadY[PAD_TOP_LEFT];
			layout->iHeight += iMax;
			layout->eHeight += iMax;
			iPadY[PAD_TOP_LEFT] = layout->iHeight - layout->eLink->neededHeight - iPadY[PAD_BOTTOM_RIGHT];
		}
		else
		{
			layout->y = iN - ePadY[PAD_TOP_LEFT];
			layout->iHeight += iMax;
			layout->eHeight += iMax;
			iPadY[PAD_BOTTOM_RIGHT] = layout->iHeight - layout->eLink->neededHeight - iPadY[PAD_TOP_LEFT];
		}
		return;
	}

	/* External expansion */
	if (flags & ELF_eEXPAND_NS)
	{
		if ((flags & ELF_eEXPAND_NS) == ELF_eEXPAND_NS)
		{
			int amt = extraHeight / 2;

			layout->y = 0;
			layout->eHeight = height;
			if (ePadY[PAD_TOP_LEFT] + amt + layout->iHeight > iS)
				amt -= (ePadY[PAD_TOP_LEFT] + amt + layout->iHeight) - iS;
			ePadY[PAD_TOP_LEFT] += amt;
			ePadY[PAD_BOTTOM_RIGHT] += extraHeight - amt;
		}
		else if (flags & ELF_eEXPAND_N)
		{
			layout->y = 0;
			layout->eHeight = iS + ePadY[PAD_BOTTOM_RIGHT];
			ePadY[PAD_TOP_LEFT] = layout->eHeight - layout->iHeight - ePadY[PAD_BOTTOM_RIGHT];
		}
		else
		{
			layout->y = iN - ePadY[PAD_TOP_LEFT];
			layout->eHeight = height - layout->y;
			ePadY[PAD_BOTTOM_RIGHT] = layout->eHeight - layout->iHeight - ePadY[PAD_TOP_LEFT];
		}
	}
}

static int Style_DoLayoutH(StyleDrawArgs *drawArgs, struct Layout layouts[])
{
	Style *style = (Style *) drawArgs->style;
	Style *masterStyle = style->master;
	ElementLink *eLinks1, *eLinks2, *eLink1, *eLink2;
	int x = 0;
	int w, e;
	int *ePadX, *iPadX, *uPadX, *ePadY, *iPadY, *uPadY;
	int numExpandWE = 0;
	int numSqueezeX = 0;
	int i, j, eLinkCount = 0;

	eLinks1 = masterStyle->elements;
	eLinks2 = style->elements;
	eLinkCount = style->numElements;

	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		/* Width before squeezing */
		layout->useWidth = eLink2->neededWidth;

		/* No -union padding yet */
		layout->uPadX[PAD_TOP_LEFT]     = 0;
		layout->uPadX[PAD_BOTTOM_RIGHT] = 0;
		layout->uPadY[PAD_TOP_LEFT]     = 0;
		layout->uPadY[PAD_BOTTOM_RIGHT] = 0;

		/* Count all non-union, non-detach squeezeable elements */
		if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;
		if (eLink1->flags & ELF_SQUEEZE_X)
			numSqueezeX++;
	}

	/* Calculate the padding around elements surrounded by -union elements */
	for (i = 0; i < eLinkCount; i++)
	{
		eLink1 = &eLinks1[i];

		if (eLink1->onion == NULL)
			continue;

		ePadX = eLink1->ePadX;
		ePadY = eLink1->ePadY;
		iPadX = eLink1->iPadX;
		iPadY = eLink1->iPadY;

		for (j = 0; j < eLink1->onionCount; j++)
		{
			struct Layout *layout = &layouts[eLink1->onion[j]];

			uPadX = layout->uPadX;
			uPadY = layout->uPadY;
			uPadX[PAD_TOP_LEFT] = MAX(uPadX[PAD_TOP_LEFT], iPadX[PAD_TOP_LEFT] + ePadX[PAD_TOP_LEFT]);
			uPadX[PAD_BOTTOM_RIGHT] = MAX(uPadX[PAD_BOTTOM_RIGHT], iPadX[PAD_BOTTOM_RIGHT] + ePadX[PAD_BOTTOM_RIGHT]);
			uPadY[PAD_TOP_LEFT] = MAX(uPadY[PAD_TOP_LEFT], iPadY[PAD_TOP_LEFT] + ePadY[PAD_TOP_LEFT]);
			uPadY[PAD_BOTTOM_RIGHT] = MAX(uPadY[PAD_BOTTOM_RIGHT], iPadY[PAD_BOTTOM_RIGHT] + ePadY[PAD_BOTTOM_RIGHT]);
		}
	}

	/* Left-to-right layout. Make the width of some elements less than they
	 * need */
	if (!masterStyle->vertical &&
		(drawArgs->width < style->neededWidth) &&
		(numSqueezeX > 0))
	{
		int extraWidth = (style->neededWidth - drawArgs->width) / numSqueezeX;
		/* Possible extra pixels */
		int fudge = (style->neededWidth - drawArgs->width) - extraWidth * numSqueezeX;
		int seen = 0;

		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			if (!(eLink1->flags & ELF_SQUEEZE_X))
				continue;

			/* Subtract extra pixels from right-most element */
			if (++seen == numSqueezeX)
				extraWidth += fudge;

			layout->useWidth -= extraWidth;
		}
	}

	/* Reduce the width of all non-union elements, except for the
	 * cases handled above. */
	if (drawArgs->width < style->neededWidth)
	{
		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			if (eLink1->onion != NULL)
				continue;

			ePadX = eLink1->ePadX;
			iPadX = eLink1->iPadX;
			uPadX = layout->uPadX;

			if ((eLink1->flags & ELF_SQUEEZE_X) &&
				((eLink1->flags & ELF_DETACH) ||
				masterStyle->vertical))
			{
				int width =
					MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]) +
					iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT] +
					MAX(ePadX[PAD_BOTTOM_RIGHT], uPadX[PAD_BOTTOM_RIGHT]);
				if (width > drawArgs->width)
					layout->useWidth -= (width - drawArgs->width);
			}
		}
	}

	/* Layout elements left-to-right */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		ePadX = eLink1->ePadX;
		iPadX = eLink1->iPadX;
		uPadX = layout->uPadX;

		if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		layout->eLink = eLink2;
		layout->master = eLink1;
		layout->x = MAX(x, abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT])));
		layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
		layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];

		for (j = 0; j < 2; j++) {
			layout->ePadX[j] = eLink1->ePadX[j];
			layout->ePadY[j] = eLink1->ePadY[j];
			layout->iPadX[j] = eLink1->iPadX[j];
			layout->iPadY[j] = eLink1->iPadY[j];
		}

		if (!masterStyle->vertical)
			x = layout->x + layout->eWidth;

		/* Count number that want to expand */
		if (eLink1->flags & ELF_EXPAND_WE)
			numExpandWE++;
	}

	/* Left-to-right layout. Expand some elements horizontally if we have
	 * more space available horizontally than is needed by the Style. */
	if (!masterStyle->vertical &&
		(drawArgs->width > style->neededWidth) &&
		(numExpandWE > 0))
	{
		int extraWidth = (drawArgs->width - style->neededWidth) / numExpandWE;
		/* Possible extra pixels */
		int fudge = (drawArgs->width - style->neededWidth) - extraWidth * numExpandWE;
		int eExtra, iExtra, seen = 0;

		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			if (!(eLink1->flags & ELF_EXPAND_WE))
				continue;

			/* Give extra pixels to right-most expander */
			if (++seen == numExpandWE)
				extraWidth += fudge;

			/* Shift following elements to the right */
			for (j = i + 1; j < eLinkCount; j++)
				if (layouts[j].eLink != NULL)
					layouts[j].x += extraWidth;

			ePadX = layout->ePadX;
			iPadX = layout->iPadX;

			/* External and internal expansion */
			if ((eLink1->flags & ELF_eEXPAND_WE) && (eLink1->flags & ELF_iEXPAND_WE))
			{
				eExtra = extraWidth / 2;
				iExtra = extraWidth - extraWidth / 2;
			}
			else
			{
				eExtra = extraWidth;
				iExtra = extraWidth;
			}

			/* External expansion */
			if (eLink1->flags & ELF_eEXPAND_WE)
			{
				if ((eLink1->flags & ELF_eEXPAND_WE) == ELF_eEXPAND_WE)
				{
					ePadX[PAD_TOP_LEFT] += eExtra / 2;
					ePadX[PAD_BOTTOM_RIGHT] += eExtra - eExtra / 2;
				}
				else if (eLink1->flags & ELF_eEXPAND_W)
					ePadX[PAD_TOP_LEFT] += eExtra;
				else
					ePadX[PAD_BOTTOM_RIGHT] += eExtra;
			}

			/* Internal expansion */
			if (eLink1->flags & ELF_iEXPAND_WE)
			{
				if ((eLink1->flags & ELF_iEXPAND_WE) == ELF_iEXPAND_WE)
				{
					iPadX[PAD_TOP_LEFT] += iExtra / 2;
					iPadX[PAD_BOTTOM_RIGHT] += iExtra - iExtra / 2;
				}
				else if (eLink1->flags & ELF_iEXPAND_W)
					iPadX[PAD_TOP_LEFT] += iExtra;
				else
					iPadX[PAD_BOTTOM_RIGHT] += iExtra;
				layout->iWidth += iExtra;
			}
			layout->eWidth += extraWidth;
		}
	}

	/* Top-to-bottom layout. Expand some elements horizontally */
	if (masterStyle->vertical && (numExpandWE > 0))
	{
		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			Style_DoExpandH(layout, eLink1->flags, drawArgs->width);
		}
	}

	/* Now handle column justification */
	if (drawArgs->width > style->neededWidth)
	{
		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			switch (drawArgs->justify)
			{
				case TK_JUSTIFY_LEFT:
					break;
				case TK_JUSTIFY_RIGHT:
					layout->x += drawArgs->width - style->neededWidth;
					break;
				case TK_JUSTIFY_CENTER:
					layout->x += (drawArgs->width - style->neededWidth) / 2;
					break;
			}
		}
	}

	/* Position and expand -detach elements */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (!(eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		ePadX = eLink1->ePadX;
		iPadX = eLink1->iPadX;
		uPadX = layout->uPadX;

		layout->eLink = eLink2;
		layout->master = eLink1;
		layout->x = abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]));
		layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
		layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];

		for (j = 0; j < 2; j++)
		{
			layout->ePadX[j] = eLink1->ePadX[j];
			layout->ePadY[j] = eLink1->ePadY[j];
			layout->iPadX[j] = eLink1->iPadX[j];
			layout->iPadY[j] = eLink1->iPadY[j];
		}

		Style_DoExpandH(layout, eLink1->flags, drawArgs->width);
	}

	/* Now calculate layout of -union elements. */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (eLink1->onion == NULL)
			continue;

		ePadX = eLink1->ePadX;
		iPadX = eLink1->iPadX;

		w = 10000, e = -10000;

		for (j = 0; j < eLink1->onionCount; j++)
		{
			struct Layout *layout2 = &layouts[eLink1->onion[j]];

			w = MIN(w, layout2->x + layout2->ePadX[PAD_TOP_LEFT]);
			e = MAX(e, layout2->x + layout2->ePadX[PAD_TOP_LEFT] + layout2->iWidth);
		}

		layout->eLink = eLink2;
		layout->master = eLink1;
		layout->x = w - iPadX[PAD_TOP_LEFT] - ePadX[PAD_TOP_LEFT];
		layout->iWidth = iPadX[PAD_TOP_LEFT] + (e - w) + iPadX[PAD_BOTTOM_RIGHT];
		layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];

		for (j = 0; j < 2; j++)
		{
			layout->ePadX[j] = eLink1->ePadX[j];
			layout->ePadY[j] = eLink1->ePadY[j];
			layout->iPadX[j] = eLink1->iPadX[j];
			layout->iPadY[j] = eLink1->iPadY[j];
		}
	}

	/* Expand -union elements if needed: horizontal */
	/* Expansion of "-union" elements is different than non-"-union" elements */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];
		int extraWidth;

		eLink1 = &eLinks1[i];

		if ((eLink1->onion == NULL) || !(eLink1->flags & ELF_EXPAND_WE))
			continue;

		if (drawArgs->width - layout->eWidth <= 0)
			continue;

		/* External and internal expansion: W */
		extraWidth = layout->x;
		if ((extraWidth > 0) && (eLink1->flags & ELF_EXPAND_W))
		{
			if ((eLink1->flags & ELF_EXPAND_W) == ELF_EXPAND_W)
			{
				int eExtra = extraWidth / 2;
				int iExtra = extraWidth - extraWidth / 2;

				/* External expansion */
				layout->ePadX[PAD_TOP_LEFT] += eExtra;
				layout->x = 0;
				layout->eWidth += extraWidth;

				/* Internal expansion */
				layout->iPadX[PAD_TOP_LEFT] += iExtra;
				layout->iWidth += iExtra;
			}

			/* External expansion only: W */
			else if (eLink1->flags & ELF_eEXPAND_W)
			{
				layout->ePadX[PAD_TOP_LEFT] += extraWidth;
				layout->x = 0;
				layout->eWidth += extraWidth;
			}

			/* Internal expansion only: W */
			else
			{
				layout->iPadX[PAD_TOP_LEFT] += extraWidth;
				layout->x = 0;
				layout->iWidth += extraWidth;
				layout->eWidth += extraWidth;
			}
		}

		/* External and internal expansion: E */
		extraWidth = drawArgs->width - (layout->x + layout->eWidth);
		if ((extraWidth > 0) && (eLink1->flags & ELF_EXPAND_E))
		{
			if ((eLink1->flags & ELF_EXPAND_E) == ELF_EXPAND_E)
			{
				int eExtra = extraWidth / 2;
				int iExtra = extraWidth - extraWidth / 2;

				/* External expansion */
				layout->ePadX[PAD_BOTTOM_RIGHT] += eExtra;
				layout->eWidth += extraWidth; /* all the space */

				/* Internal expansion */
				layout->iPadX[PAD_BOTTOM_RIGHT] += iExtra;
				layout->iWidth += iExtra;
			}

			/* External expansion only: E */
			else if (eLink1->flags & ELF_eEXPAND_E)
			{
				layout->ePadX[PAD_BOTTOM_RIGHT] += extraWidth;
				layout->eWidth += extraWidth;
			}

			/* Internal expansion only: E */
			else
			{
				layout->iPadX[PAD_BOTTOM_RIGHT] += extraWidth;
				layout->iWidth += extraWidth;
				layout->eWidth += extraWidth;
			}
		}
	}

	return eLinkCount;
}

static int Style_DoLayoutV(StyleDrawArgs *drawArgs, struct Layout layouts[])
{
	Style *style = (Style *) drawArgs->style;
	Style *masterStyle = style->master;
	ElementLink *eLinks1, *eLinks2, *eLink1, *eLink2;
	int y = 0;
	int n, s;
	int *ePadY, *iPadY, *uPadY;
	int numExpandNS = 0;
	int numSqueezeY = 0;
	int i, j, eLinkCount = 0;

	eLinks1 = masterStyle->elements;
	eLinks2 = style->elements;
	eLinkCount = style->numElements;

	for (i = 0; i < eLinkCount; i++)
	{
		eLink1 = &eLinks1[i];

		/* Count all non-union, non-detach squeezeable elements */
		if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;
		if (eLink1->flags & ELF_SQUEEZE_Y)
			numSqueezeY++;
	}

	/* Top-top-bottom layout. Make the height of some elements less than they
	 * need */
	if (masterStyle->vertical &&
		(drawArgs->height < style->neededHeight) &&
		(numSqueezeY > 0))
	{
		int extraHeight = (style->neededHeight - drawArgs->height) / numSqueezeY;
		/* Possible extra pixels */
		int fudge = (style->neededHeight - drawArgs->height) - extraHeight * numSqueezeY;
		int seen = 0;

		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			if (!(eLink1->flags & ELF_SQUEEZE_Y))
				continue;

			/* Subtract extra pixels from bottom-most element */
			if (++seen == numSqueezeY)
				extraHeight += fudge;

			layout->useHeight -= extraHeight;
		}
	}

	/* Reduce the height of all non-union elements, except for the
	 * cases handled above. */
	if (drawArgs->height < style->neededHeight)
	{
		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			if (eLink1->onion != NULL)
				continue;

			ePadY = eLink1->ePadY;
			iPadY = eLink1->iPadY;
			uPadY = layout->uPadY;

			if ((eLink1->flags & ELF_SQUEEZE_Y) &&
				((eLink1->flags & ELF_DETACH) ||
				!masterStyle->vertical))
			{
				int height =
					MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]) +
					iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT] +
					MAX(ePadY[PAD_BOTTOM_RIGHT], uPadY[PAD_BOTTOM_RIGHT]);
				if (height > drawArgs->height)
					layout->useHeight -= (height - drawArgs->height);
			}
		}
	}

	/* Layout elements top-to-bottom */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		ePadY = eLink1->ePadY;
		iPadY = eLink1->iPadY;
		uPadY = layout->uPadY;

		if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		layout->y = MAX(y, abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT])));
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

		if (masterStyle->vertical)
			y = layout->y + layout->eHeight;

		/* Count number that want to expand */
		if (eLink1->flags & ELF_EXPAND_NS)
			numExpandNS++;
	}

	/* Top-to-bottom layout. Expand some elements vertically if we have
	 * more space available vertically than is needed by the Style. */
	if (masterStyle->vertical &&
		(drawArgs->height > style->neededHeight) &&
		(numExpandNS > 0))
	{
		int extraHeight = (drawArgs->height - style->neededHeight) / numExpandNS;
		/* Possible extra pixels */
		int fudge = (drawArgs->height - style->neededHeight) - extraHeight * numExpandNS;
		int eExtra, iExtra, seen = 0;

		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			if (!(eLink1->flags & ELF_EXPAND_NS))
				continue;

			/* Give extra pixels to bottom-most expander */
			if (++seen == numExpandNS)
				extraHeight += fudge;

			/* Shift following elements down */
			for (j = i + 1; j < eLinkCount; j++)
				if (layouts[j].eLink != NULL)
					layouts[j].y += extraHeight;

			ePadY = layout->ePadY;
			iPadY = layout->iPadY;

			/* External and internal expansion */
			if ((eLink1->flags & ELF_eEXPAND_NS) && (eLink1->flags & ELF_iEXPAND_NS))
			{
				eExtra = extraHeight / 2;
				iExtra = extraHeight - extraHeight / 2;
			}
			else
			{
				eExtra = extraHeight;
				iExtra = extraHeight;
			}

			/* External expansion */
			if (eLink1->flags & ELF_eEXPAND_NS)
			{
				if ((eLink1->flags & ELF_eEXPAND_NS) == ELF_eEXPAND_NS)
				{
					ePadY[PAD_TOP_LEFT] += eExtra / 2;
					ePadY[PAD_BOTTOM_RIGHT] += eExtra - eExtra / 2;
				}
				else if (eLink1->flags & ELF_eEXPAND_N)
					ePadY[PAD_TOP_LEFT] += eExtra;
				else
					ePadY[PAD_BOTTOM_RIGHT] += eExtra;
			}

			/* Internal expansion */
			if (eLink1->flags & ELF_iEXPAND_NS)
			{
				if ((eLink1->flags & ELF_iEXPAND_NS) == ELF_iEXPAND_NS)
				{
					iPadY[PAD_TOP_LEFT] += iExtra / 2;
					iPadY[PAD_BOTTOM_RIGHT] += iExtra - iExtra / 2;
				}
				else if (eLink1->flags & ELF_iEXPAND_N)
					iPadY[PAD_TOP_LEFT] += iExtra;
				else
					iPadY[PAD_BOTTOM_RIGHT] += iExtra;
				layout->iHeight += iExtra;
			}
			layout->eHeight += extraHeight;
		}
	}

	/* Left-to-right layout. Expand some elements vertically */
	if (!masterStyle->vertical && (numExpandNS > 0))
	{
		for (i = 0; i < eLinkCount; i++)
		{
			struct Layout *layout = &layouts[i];

			eLink1 = &eLinks1[i];

			if ((eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
				continue;

			Style_DoExpandV(layout, eLinks1[i].flags, drawArgs->height);
		}
	}

	/* Position and expand -detach elements */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (!(eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		ePadY = eLink1->ePadY;
		iPadY = eLink1->iPadY;
		uPadY = layout->uPadY;

		layout->y = abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

		Style_DoExpandV(layout, eLink1->flags, drawArgs->height);
	}

	/* Now calculate layout of -union elements. */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (eLink1->onion == NULL)
			continue;

		ePadY = eLink1->ePadY;
		iPadY = eLink1->iPadY;

		n = 10000, s = -10000;

		for (j = 0; j < eLink1->onionCount; j++)
		{
			struct Layout *layout2 = &layouts[eLink1->onion[j]];

			n = MIN(n, layout2->y + layout2->ePadY[PAD_TOP_LEFT]);
			s = MAX(s, layout2->y + layout2->ePadY[PAD_TOP_LEFT] + layout2->iHeight);
		}

		layout->y = n - iPadY[PAD_TOP_LEFT] - ePadY[PAD_TOP_LEFT];
		layout->iHeight = iPadY[PAD_TOP_LEFT] + (s - n) + iPadY[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];
	}

	/* Expand -union elements if needed: vertical */
	for (i = 0; i < eLinkCount; i++)
	{
		struct Layout *layout = &layouts[i];
		int extraHeight;

		eLink1 = &eLinks1[i];

		if ((eLink1->onion == NULL) || !(eLink1->flags & ELF_EXPAND_NS))
			continue;

		if (drawArgs->height - layout->eHeight <= 0)
			continue;

		/* External and internal expansion: N */
		extraHeight = layout->y;
		if ((extraHeight > 0) && (eLink1->flags & ELF_EXPAND_N))
		{
			if ((eLink1->flags & ELF_EXPAND_N) == ELF_EXPAND_N)
			{
				int eExtra = extraHeight / 2;
				int iExtra = extraHeight - extraHeight / 2;

				/* External expansion */
				layout->ePadY[PAD_TOP_LEFT] += eExtra;
				layout->y = 0;
				layout->eHeight += extraHeight;

				/* Internal expansion */
				layout->iPadY[PAD_TOP_LEFT] += iExtra;
				layout->iHeight += iExtra;
			}

			/* External expansion only: N */
			else if (eLink1->flags & ELF_eEXPAND_N)
			{
				layout->ePadY[PAD_TOP_LEFT] += extraHeight;
				layout->y = 0;
				layout->eHeight += extraHeight;
			}

			/* Internal expansion only: N */
			else
			{
				layout->iPadY[PAD_TOP_LEFT] += extraHeight;
				layout->y = 0;
				layout->iHeight += extraHeight;
				layout->eHeight += extraHeight;
			}
		}

		/* External and internal expansion: S */
		extraHeight = drawArgs->height - (layout->y + layout->eHeight);
		if ((extraHeight > 0) && (eLink1->flags & ELF_EXPAND_S))
		{
			if ((eLink1->flags & ELF_EXPAND_S) == ELF_EXPAND_S)
			{
				int eExtra = extraHeight / 2;
				int iExtra = extraHeight - extraHeight / 2;

				/* External expansion */
				layout->ePadY[PAD_BOTTOM_RIGHT] += eExtra;
				layout->eHeight += extraHeight; /* all the space */

				/* Internal expansion */
				layout->iPadY[PAD_BOTTOM_RIGHT] += iExtra;
				layout->iHeight += iExtra;
			}

			/* External expansion only: S */
			else if (eLink1->flags & ELF_eEXPAND_S)
			{
				layout->ePadY[PAD_BOTTOM_RIGHT] += extraHeight;
				layout->eHeight += extraHeight;
			}

			/* Internal expansion only */
			else
			{
				layout->iPadY[PAD_BOTTOM_RIGHT] += extraHeight;
				layout->iHeight += extraHeight;
				layout->eHeight += extraHeight;
			}
		}
	}

	return eLinkCount;
}

/* Calculate the height and width of all the Elements */
static void Layout_Size(int vertical, int numLayouts, struct Layout layouts[], int *widthPtr, int *heightPtr)
{
	int i, W, N, E, S;
	int width = 0, height = 0;

	W = 10000, N = 10000, E = -10000, S = -10000;

	for (i = 0; i < numLayouts; i++)
	{
		struct Layout *layout = &layouts[i];
		int w, n, e, s;

		w = layout->x + layout->ePadX[PAD_TOP_LEFT] - MAX(layout->ePadX[PAD_TOP_LEFT], layout->uPadX[PAD_TOP_LEFT]);
		n = layout->y + layout->ePadY[PAD_TOP_LEFT] - MAX(layout->ePadY[PAD_TOP_LEFT], layout->uPadY[PAD_TOP_LEFT]);
		e = layout->x + layout->eWidth - layout->ePadX[PAD_BOTTOM_RIGHT] + MAX(layout->ePadX[PAD_BOTTOM_RIGHT], layout->uPadX[PAD_BOTTOM_RIGHT]);
		s = layout->y + layout->eHeight - layout->ePadY[PAD_BOTTOM_RIGHT] + MAX(layout->ePadY[PAD_BOTTOM_RIGHT], layout->uPadY[PAD_BOTTOM_RIGHT]);

		if (vertical)
		{
			N = MIN(N, n);
			S = MAX(S, s);
			width = MAX(width, e - w);
		}
		else
		{
			W = MIN(W, w);
			E = MAX(E, e);
			height = MAX(height, s - n);
		}
	}

	if (vertical)
		height = MAX(height, S - N);
	else
		width = MAX(width, E - W);

	(*widthPtr) = width;
	(*heightPtr) = height;
}

/* */
void Style_DoLayoutNeededV(StyleDrawArgs *drawArgs, struct Layout layouts[])
{
	Style *style = (Style *) drawArgs->style;
	Style *masterStyle = style->master;
	ElementLink *eLinks1, *eLinks2, *eLink1, *eLink2;
	int *ePadY, *iPadY, *uPadY;
	int i;
	int y = 0;

	eLinks1 = masterStyle->elements;
	eLinks2 = style->elements;

	/* Layout elements left-to-right, or top-to-bottom */
	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		ePadY = eLink1->ePadY;
		iPadY = eLink1->iPadY;
		uPadY = layout->uPadY;

		/* The size of a -union element is determined by the elements
		 * it surrounds */
		if (eLink1->onion != NULL)
			continue;

		/* -detached elements are positioned by themselves */
		if (eLink1->flags & ELF_DETACH)
			continue;

		layout->y = MAX(y, abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT])));
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

		if (masterStyle->vertical)
			y = layout->y + layout->eHeight;
	}

	/* -detached elements */
	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (!(eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		ePadY = eLink1->ePadY;
		iPadY = eLink1->iPadY;
		uPadY = layout->uPadY;

		layout->y = abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];
	}
}

/* Arrange all the Elements considering drawArgs.width but not drawArgs.height */
static void Style_DoLayout2(StyleDrawArgs *drawArgs, struct Layout layouts[])
{
	TreeCtrl *tree = drawArgs->tree;
	Style *style = (Style *) drawArgs->style;
	int state = drawArgs->state;
	int i;

	if (style->neededWidth == -1)
		panic("Style_DoLayout2: style.neededWidth == -1");
	if (style->minWidth > drawArgs->width)
		panic("Style_DoLayout2: style.minWidth %d > drawArgs.width %d",
			style->minWidth, drawArgs->width);

	Style_DoLayoutH(drawArgs, layouts);

	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];
		ElementLink *eLink = &style->elements[i];

		layout->useHeight = eLink->neededHeight;

		/* If a Text Element is given less width than it needs (due to
		 * -squeeze x layout), then it may wrap lines. This means
		 * the height can vary depending on the width. */
		if (eLink->elem->typePtr == &elemTypeText)
		{
			ElementArgs args;

			if (layout->iWidth == eLink->layoutWidth)
			{
				layout->useHeight = eLink->layoutHeight;
				continue;
			}

			if ((eLink->layoutWidth == -1) &&
				(layout->iWidth >= eLink->neededWidth))
				continue;

			args.tree = tree;
			args.state = state;
			args.elem = eLink->elem;
			args.layout.squeeze = layout->iWidth < eLink->neededWidth;
			args.layout.width = layout->iWidth;
			(*args.elem->typePtr->layoutProc)(&args);

			eLink->layoutWidth = layout->iWidth;
			eLink->layoutHeight = args.layout.height;
			layout->useHeight = eLink->layoutHeight;
		}
	}

	Style_DoLayoutNeededV(drawArgs, layouts);
}

/* Arrange all the Elements considering drawArgs.width and drawArgs.height */
static void Style_DoLayout(StyleDrawArgs *drawArgs, struct Layout layouts[], char *file, int line)
{
	TreeCtrl *tree = drawArgs->tree;
	Style *style = (Style *) drawArgs->style;
	int state = drawArgs->state;
	int i;

	if (style->neededWidth == -1)
		panic("Style_DoLayout(file %s line %d): style.neededWidth == -1",
			file, line);
	if (style->minWidth > drawArgs->width)
		panic("Style_DoLayout: style.minWidth %d > drawArgs.width %d",
			style->minWidth, drawArgs->width);

	Style_DoLayoutH(drawArgs, layouts);

	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];
		ElementLink *eLink = &style->elements[i];

		layout->useHeight = eLink->neededHeight;

		/* If a Text Element is given less width than it needs (due to
		 * -squeeze x layout), then it may wrap lines. This means
		 * the height can vary depending on the width. */
		if (eLink->elem->typePtr == &elemTypeText)
		{
			ElementArgs args;

			if (layout->iWidth == eLink->layoutWidth)
			{
				layout->useHeight = eLink->layoutHeight;
				continue;
			}

			if ((eLink->layoutWidth == -1) &&
				(layout->iWidth >= eLink->neededWidth))
				continue;

			args.tree = tree;
			args.state = state;
			args.elem = eLink->elem;
			args.layout.squeeze = layout->iWidth < eLink->neededWidth;
			args.layout.width = layout->iWidth;
			(*args.elem->typePtr->layoutProc)(&args);

			eLink->layoutWidth = layout->iWidth;
			eLink->layoutHeight = args.layout.height;
			layout->useHeight = eLink->layoutHeight;
		}
	}

	Style_DoLayoutV(drawArgs, layouts);
}

/* Arrange Elements to determine the needed height and width of the Style */
static void Style_NeededSize(TreeCtrl *tree, Style *style, int state, int *widthPtr, int *heightPtr, int squeeze)
{
	Style *masterStyle = style->master;
	ElementLink *eLinks1, *eLinks2, *eLink1, *eLink2;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
	int *ePadX, *iPadX, *uPadX, *ePadY, *iPadY, *uPadY;
	int i, j;
	int x = 0, y = 0;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	if (style->master != NULL)
	{
		eLinks1 = masterStyle->elements;
		eLinks2 = style->elements;
	}
	else
	{
		eLinks1 = masterStyle->elements;
		eLinks2 = masterStyle->elements;
	}

	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		/* No -union padding yet */
		layout->uPadX[PAD_TOP_LEFT]     = 0;
		layout->uPadX[PAD_BOTTOM_RIGHT] = 0;
		layout->uPadY[PAD_TOP_LEFT]     = 0;
		layout->uPadY[PAD_BOTTOM_RIGHT] = 0;
	}

	/* Figure out the padding around elements surrounded by -union elements */
	for (i = 0; i < style->numElements; i++)
	{
		eLink1 = &eLinks1[i];

		if (eLink1->onion == NULL)
			continue;

		ePadX = eLink1->ePadX;
		ePadY = eLink1->ePadY;
		iPadX = eLink1->iPadX;
		iPadY = eLink1->iPadY;

		for (j = 0; j < eLink1->onionCount; j++)
		{
			struct Layout *layout = &layouts[eLink1->onion[j]];

			uPadX = layout->uPadX;
			uPadY = layout->uPadY;
			uPadX[PAD_TOP_LEFT] = MAX(uPadX[PAD_TOP_LEFT], iPadX[PAD_TOP_LEFT] + ePadX[PAD_TOP_LEFT]);
			uPadX[PAD_BOTTOM_RIGHT] = MAX(uPadX[PAD_BOTTOM_RIGHT], iPadX[PAD_BOTTOM_RIGHT] + ePadX[PAD_BOTTOM_RIGHT]);
			uPadY[PAD_TOP_LEFT] = MAX(uPadY[PAD_TOP_LEFT], iPadY[PAD_TOP_LEFT] + ePadY[PAD_TOP_LEFT]);
			uPadY[PAD_BOTTOM_RIGHT] = MAX(uPadY[PAD_BOTTOM_RIGHT], iPadY[PAD_BOTTOM_RIGHT] + ePadY[PAD_BOTTOM_RIGHT]);
		}
	}

	/* Layout elements left-to-right, or top-to-bottom */
	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		ePadX = eLink1->ePadX;
		ePadY = eLink1->ePadY;
		iPadX = eLink1->iPadX;
		iPadY = eLink1->iPadY;
		uPadX = layout->uPadX;
		uPadY = layout->uPadY;

		/* The size of a -union element is determined by the elements
		 * it surrounds */
		if (eLink1->onion != NULL)
		{
			layout->x = layout->y = layout->eWidth = layout->eHeight = 0;
			layout->ePadX[PAD_TOP_LEFT]     = 0;
			layout->ePadX[PAD_BOTTOM_RIGHT] = 0;
			layout->ePadY[PAD_TOP_LEFT]     = 0;
			layout->ePadY[PAD_BOTTOM_RIGHT] = 0;
			layout->iPadX[PAD_TOP_LEFT]     = 0;
			layout->iPadX[PAD_BOTTOM_RIGHT] = 0;
			layout->iPadY[PAD_TOP_LEFT]     = 0;
			layout->iPadY[PAD_BOTTOM_RIGHT] = 0;
			continue;
		}

		if ((eLink2->neededWidth == -1) || (eLink2->neededHeight == -1))
		{
			ElementArgs args;

			args.tree = tree;
			args.state = state;
			args.elem = eLink2->elem;
			args.layout.squeeze = 0;
			args.layout.width = -1;
			(*args.elem->typePtr->layoutProc)(&args);
			eLink2->neededWidth = args.layout.width;
			eLink2->neededHeight = args.layout.height;

			eLink2->layoutWidth = -1;
			eLink2->layoutHeight = args.layout.height;
		}

		layout->useWidth = eLink2->neededWidth;
		layout->useHeight = eLink2->neededHeight;
		if (squeeze)
		{
			if (eLink1->flags & ELF_SQUEEZE_X)
				layout->useWidth = 0;
			if (eLink1->flags & ELF_SQUEEZE_Y)
				layout->useHeight = 0;
		}

		/* -detached elements are positioned by themselves */
		if (eLink1->flags & ELF_DETACH)
			continue;

		layout->eLink = eLink2;
		layout->x = MAX(x, abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT])));
		layout->y = MAX(y, abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT])));
		layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

		for (j = 0; j < 2; j++)
		{
			layout->ePadX[j] = eLink1->ePadX[j];
			layout->ePadY[j] = eLink1->ePadY[j];
			layout->iPadX[j] = eLink1->iPadX[j];
			layout->iPadY[j] = eLink1->iPadY[j];
		}

		if (masterStyle->vertical)
			y = layout->y + layout->eHeight;
		else
			x = layout->x + layout->eWidth;
	}

	/* -detached elements */
	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		eLink1 = &eLinks1[i];
		eLink2 = &eLinks2[i];

		if (!(eLink1->flags & ELF_DETACH) || (eLink1->onion != NULL))
			continue;

		ePadX = eLink1->ePadX;
		ePadY = eLink1->ePadY;
		iPadX = eLink1->iPadX;
		iPadY = eLink1->iPadY;
		uPadX = layout->uPadX;
		uPadY = layout->uPadY;

		layout->eLink = eLink2;
		layout->master = eLink1;
		layout->x = abs(ePadX[PAD_TOP_LEFT] - MAX(ePadX[PAD_TOP_LEFT], uPadX[PAD_TOP_LEFT]));
		layout->y = abs(ePadY[PAD_TOP_LEFT] - MAX(ePadY[PAD_TOP_LEFT], uPadY[PAD_TOP_LEFT]));
		layout->iWidth = iPadX[PAD_TOP_LEFT] + layout->useWidth + iPadX[PAD_BOTTOM_RIGHT];
		layout->iHeight = iPadY[PAD_TOP_LEFT] + layout->useHeight + iPadY[PAD_BOTTOM_RIGHT];
		layout->eWidth = ePadX[PAD_TOP_LEFT] + layout->iWidth + ePadX[PAD_BOTTOM_RIGHT];
		layout->eHeight = ePadY[PAD_TOP_LEFT] + layout->iHeight + ePadY[PAD_BOTTOM_RIGHT];

		for (j = 0; j < 2; j++)
		{
			layout->ePadX[j] = eLink1->ePadX[j];
			layout->ePadY[j] = eLink1->ePadY[j];
			layout->iPadX[j] = eLink1->iPadX[j];
			layout->iPadY[j] = eLink1->iPadY[j];
		}
	}

	Layout_Size(masterStyle->vertical, style->numElements, layouts,
		widthPtr, heightPtr);

	STATIC_FREE(layouts, struct Layout, style->numElements);
}

int TreeStyle_NeededWidth(TreeCtrl *tree, TreeStyle style, int state)
{
	int width, height;

	Style_NeededSize(tree, (Style *) style, state, &width, &height, FALSE);
	return width;
}

int TreeStyle_NeededHeight(TreeCtrl *tree, TreeStyle style, int state)
{
	int width, height;

	Style_NeededSize(tree, (Style *) style, state, &width, &height, FALSE);
	return height;
}

/* Calculate height of Style considering drawArgs.width */
int TreeStyle_UseHeight(StyleDrawArgs *drawArgs)
{
	TreeCtrl *tree = drawArgs->tree;
	Style *style = (Style *) drawArgs->style;
	int state = drawArgs->state;
	int layoutWidth = drawArgs->width;
	int height;

	if (style->neededWidth == -1)
	{
		Style_NeededSize(tree, style, state, &style->neededWidth,
			&style->neededHeight, FALSE);
		Style_NeededSize(tree, style, state, &style->minWidth,
			&style->minHeight, TRUE);

		style->layoutWidth = -1;
	}

	if ((layoutWidth == -1) ||
		(layoutWidth >= style->neededWidth) ||
		(style->neededWidth == style->minWidth))
	{
		height = style->neededHeight;
	}

	else if (layoutWidth <= style->minWidth)
	{
		height = style->minHeight;
	}

	else if (layoutWidth == style->layoutWidth)
	{
		height = style->layoutHeight;
	}

	else
	{
		struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
		int width;

		STATIC_ALLOC(layouts, struct Layout, style->numElements);

		Style_DoLayout2(drawArgs, layouts);

		Layout_Size(style->master->vertical, style->numElements, layouts,
			&width, &height);

		STATIC_FREE(layouts, struct Layout, style->numElements);

		style->layoutHeight = height;
	}

	return height;
}

void TreeStyle_Draw(StyleDrawArgs *drawArgs)
{
	Style *style = (Style *) drawArgs->style;
	TreeCtrl *tree = drawArgs->tree;
	ElementArgs args;
	int i;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
	int debugDraw = FALSE;

	if (drawArgs->width < style->minWidth)
		drawArgs->width = style->minWidth;
	if (drawArgs->height < style->minHeight)
		drawArgs->height = style->minHeight;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	Style_DoLayout(drawArgs, layouts, __FILE__, __LINE__);

	args.tree = tree;
	args.state = drawArgs->state;
	args.display.drawable = drawArgs->drawable;

	for (i = 0; i < style->numElements; i++)
	{
		struct Layout *layout = &layouts[i];

		if (debugDraw && layout->master->onion != NULL)
			continue;

		if ((layout->iWidth > 0) && (layout->iHeight > 0))
		{
			args.elem = layout->eLink->elem;
			args.display.x = drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT];
			args.display.y = drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT];
			args.display.width = layout->iWidth;
			args.display.height = layout->iHeight;
			args.display.pad[LEFT] = layout->iPadX[PAD_TOP_LEFT];
			args.display.pad[TOP] = layout->iPadY[PAD_TOP_LEFT];
			args.display.pad[RIGHT] = layout->iPadX[PAD_BOTTOM_RIGHT];
			args.display.pad[BOTTOM] = layout->iPadY[PAD_BOTTOM_RIGHT];
			if (debugDraw)
			{
				XColor *color[3];
				GC gc[3];

				if (layout->master->onion != NULL)
				{
					color[0] = Tk_GetColor(tree->interp, tree->tkwin, "blue2");
					gc[0] = Tk_GCForColor(color[0], Tk_WindowId(tree->tkwin));
					color[1] = Tk_GetColor(tree->interp, tree->tkwin, "blue3");
					gc[1] = Tk_GCForColor(color[1], Tk_WindowId(tree->tkwin));
				}
				else
				{
					color[0] = Tk_GetColor(tree->interp, tree->tkwin, "gray50");
					gc[0] = Tk_GCForColor(color[0], Tk_WindowId(tree->tkwin));
					color[1] = Tk_GetColor(tree->interp, tree->tkwin, "gray60");
					gc[1] = Tk_GCForColor(color[1], Tk_WindowId(tree->tkwin));
					color[2] = Tk_GetColor(tree->interp, tree->tkwin, "gray70");
					gc[2] = Tk_GCForColor(color[2], Tk_WindowId(args.tree->tkwin));
				}

				/* external */
				XFillRectangle(tree->display, args.display.drawable,
					gc[2],
					args.display.x - layout->ePadX[PAD_TOP_LEFT],
					args.display.y - layout->ePadY[PAD_TOP_LEFT],
					layout->eWidth, layout->eHeight);
				/* internal */
				XFillRectangle(tree->display, args.display.drawable,
					gc[1],
					args.display.x, args.display.y,
					args.display.width, args.display.height);
				/* needed */
				if (!layout->master->onion && !(layout->master->flags & ELF_DETACH))
				XFillRectangle(tree->display, args.display.drawable,
					gc[0],
					args.display.x + layout->iPadX[PAD_TOP_LEFT],
					args.display.y + layout->iPadY[PAD_TOP_LEFT],
					layout->eLink->neededWidth, layout->eLink->neededHeight);
			}
			else
				(*args.elem->typePtr->displayProc)(&args);
		}
	}

	if (debugDraw)
		for (i = 0; i < style->numElements; i++)
		{
			struct Layout *layout = &layouts[i];

			if (layout->master->onion == NULL)
				continue;
			if (layout->iWidth > 0 && layout->iHeight > 0)
			{
				args.elem = layout->eLink->elem;
				args.display.x = drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT];
				args.display.y = drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT];
				args.display.width = layout->iWidth;
				args.display.height = layout->iHeight;
				args.display.pad[LEFT] = layout->iPadX[PAD_TOP_LEFT];
				args.display.pad[TOP] = layout->iPadY[PAD_TOP_LEFT];
				args.display.pad[RIGHT] = layout->iPadX[PAD_BOTTOM_RIGHT];
				args.display.pad[BOTTOM] = layout->iPadY[PAD_BOTTOM_RIGHT];
				{
					XColor *color[3];
					GC gc[3];

					color[0] = Tk_GetColor(tree->interp, tree->tkwin, "blue2");
					gc[0] = Tk_GCForColor(color[0], Tk_WindowId(tree->tkwin));
					color[1] = Tk_GetColor(tree->interp, tree->tkwin, "blue3");
					gc[1] = Tk_GCForColor(color[1], Tk_WindowId(tree->tkwin));

					/* external */
					XDrawRectangle(tree->display, args.display.drawable,
						gc[0],
						args.display.x - layout->ePadX[PAD_TOP_LEFT],
						args.display.y - layout->ePadY[PAD_TOP_LEFT],
						layout->eWidth - 1, layout->eHeight - 1);
					/* internal */
					XDrawRectangle(tree->display, args.display.drawable,
						gc[1],
						args.display.x, args.display.y,
						args.display.width - 1, args.display.height - 1);
				}
			}
		}

	STATIC_FREE(layouts, struct Layout, style->numElements);
}

static void Element_FreeResources(TreeCtrl *tree, Element *elem)
{
	ElementArgs args;
	Tcl_HashEntry *hPtr;

	if (elem->master == NULL)
	{
		hPtr = Tcl_FindHashEntry(&tree->elementHash, elem->name);
		Tcl_DeleteHashEntry(hPtr);
	}
	args.tree = tree;
	args.elem = elem;
	(*elem->typePtr->deleteProc)(&args);
	Tk_FreeConfigOptions((char *) elem,
		elem->typePtr->optionTable,
		tree->tkwin);
	WFREE(elem, Element);
}

static ElementLink *ElementLink_Init(ElementLink *eLink, Element *elem)
{
	memset(eLink, '\0', sizeof(ElementLink));
	eLink->elem = elem;
	return eLink;
}

static void ElementLink_FreeResources(TreeCtrl *tree, ElementLink *eLink)
{
	if (eLink->elem->master != NULL)
		Element_FreeResources(tree, eLink->elem);
	if (eLink->onion != NULL)
		wipefree((char *) eLink->onion, sizeof(int) * eLink->onionCount);
}

void TreeStyle_FreeResources(TreeCtrl *tree, TreeStyle style_)
{
	Style *style = (Style *) style_;
	int i;
	Tcl_HashEntry *hPtr;

	if (style->master == NULL)
	{
		hPtr = Tcl_FindHashEntry(&tree->styleHash, style->name);
		Tcl_DeleteHashEntry(hPtr);
	}
	if (style->numElements > 0)
	{
		for (i = 0; i < style->numElements; i++)
			ElementLink_FreeResources(tree, &style->elements[i]);
		wipefree((char *) style->elements, sizeof(ElementLink) *
			style->numElements);
	}
	WFREE(style, Style);
}

static ElementLink *Style_FindElem(TreeCtrl *tree, Style *style, Element *master, int *index)
{
	int i;

	for (i = 0; i < style->numElements; i++)
	{
		if (style->elements[i].elem->name == master->name)
		{
			if (index != NULL) (*index) = i;
			return &style->elements[i];
		}
	}

	return NULL;
}

int TreeStyle_FindElement(TreeCtrl *tree, TreeStyle style_, TreeElement elem_, int *index)
{
	if (Style_FindElem(tree, (Style *) style_, (Element *) elem_, index) == NULL)
	{
		FormatResult(tree->interp, "style %s does not use element %s",
			((Style *) style_)->name, ((Element *) elem_)->name);
		return TCL_ERROR;
	}
	return TCL_OK;
}

/* Create an instance Element if it doesn't exist in this Style */
static ElementLink *Style_CreateElem(TreeCtrl *tree, Style *style, Element *masterElem, int *isNew)
{
	ElementLink *eLink = NULL;
	Element *elem;
	ElementArgs args;
	int i;

	if (masterElem->master != NULL)
		panic("Style_CreateElem called with instance Element");

	if (isNew != NULL) (*isNew) = FALSE;

	for (i = 0; i < style->numElements; i++)
	{
		eLink = &style->elements[i];

		if (eLink->elem == masterElem)
		{
			/* Master Styles use master Elements only */
			if (style->master == NULL)
				return eLink;

			/* Allocate instance Element here */
			break;
		}

		/* Instance Style already has instance Element */
		if (eLink->elem->name == masterElem->name)
			return eLink;
	}

	/* Error: Element isn't in the master Style */
	if (i == style->numElements)
		return NULL;

	elem = (Element *) ckalloc(masterElem->typePtr->size);
	memset(elem, '\0', masterElem->typePtr->size);
	elem->typePtr = masterElem->typePtr;
	elem->name = masterElem->name;
	elem->master = masterElem;
	args.tree = tree;
	args.elem = elem;

	/* FIXME: free memory if these calls could actually fail */
	if ((*elem->typePtr->createProc)(&args) != TCL_OK)
		return NULL;

	if (Tk_InitOptions(tree->interp, (char *) elem,
		elem->typePtr->optionTable, tree->tkwin) != TCL_OK)
		return NULL;

	args.config.objc = 0;
	args.config.flagSelf = 0;
	if ((*elem->typePtr->configProc)(&args) != TCL_OK)
		return NULL;

	args.change.flagSelf = args.config.flagSelf;
	args.change.flagTree = 0;
	args.change.flagMaster = 0;
	(*elem->typePtr->changeProc)(&args);

	eLink->elem = elem;
	if (isNew != NULL) (*isNew) = TRUE;
	return eLink;
}

TreeStyle TreeStyle_NewInstance(TreeCtrl *tree, TreeStyle style_)
{
	Style *style = (Style *) style_;
	Style *copy;
	ElementLink *eLink;
	int i;

	copy = (Style *) ckalloc(sizeof(Style));
	memset(copy, '\0', sizeof(Style));
	copy->name = style->name;
	copy->neededWidth = -1;
	copy->neededHeight = -1;
	copy->master = style;
	copy->numElements = style->numElements;
	if (style->numElements > 0)
	{
		copy->elements = (ElementLink *) ckalloc(sizeof(ElementLink) * style->numElements);
		memset(copy->elements, '\0', sizeof(ElementLink) * style->numElements);
		for (i = 0; i < style->numElements; i++)
		{
			eLink = &copy->elements[i];

			/* The only fields needed by an instance */
			eLink->elem = style->elements[i].elem;
			eLink->neededWidth = -1;
			eLink->neededHeight = -1;
		}
	}

	return (TreeStyle) copy;
}

static int Element_FromObj(TreeCtrl *tree, Tcl_Obj *obj, Element **elemPtr)
{
	char *name;
	Tcl_HashEntry *hPtr;

	name = Tcl_GetString(obj);
	hPtr = Tcl_FindHashEntry(&tree->elementHash, name);
	if (hPtr == NULL)
	{
		Tcl_AppendResult(tree->interp, "element \"", name, "\" doesn't exist",
			NULL);
		return TCL_ERROR;
	}
	(*elemPtr) = (Element *) Tcl_GetHashValue(hPtr);
	return TCL_OK;
}

int TreeElement_FromObj(TreeCtrl *tree, Tcl_Obj *obj, TreeElement *elemPtr)
{
	return Element_FromObj(tree, obj, (Element **) elemPtr);
}

int TreeElement_IsType(TreeCtrl *tree, TreeElement elem_, CONST char *type)
{
	return strcmp(((Element *) elem_)->typePtr->name, type) == 0;
}

int TreeStyle_FromObj(TreeCtrl *tree, Tcl_Obj *obj, TreeStyle *stylePtr)
{
	char *name;
	Tcl_HashEntry *hPtr;

	name = Tcl_GetString(obj);
	hPtr = Tcl_FindHashEntry(&tree->styleHash, name);
	if (hPtr == NULL)
	{
		Tcl_AppendResult(tree->interp, "style \"", name, "\" doesn't exist",
			NULL);
		return TCL_ERROR;
	}
	(*stylePtr) = (TreeStyle) Tcl_GetHashValue(hPtr);
	return TCL_OK;
}

static Tcl_Obj *Element_ToObj(Element *elem)
{
	return Tcl_NewStringObj(elem->name, -1);
}

Tcl_Obj *TreeStyle_ToObj(TreeStyle style_)
{
	Style *style = (Style *) style_;

	return Tcl_NewStringObj(style->name, -1);
}

static void Style_Changed(TreeCtrl *tree, Style *masterStyle)
{
	TreeItem item;
	TreeItemColumn column;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	int columnIndex, layout;
	int updateDInfo = FALSE;

	hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	while (hPtr != NULL)
	{
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		column = TreeItem_GetFirstColumn(tree, item);
		columnIndex = 0;
		layout = FALSE;
		while (column != NULL)
		{
			Style *style = (Style *) TreeItemColumn_GetStyle(tree, column);
			if ((style != NULL) && (style->master == masterStyle))
			{
				style->neededWidth = style->neededHeight = -1;
				Tree_InvalidateColumnWidth(tree, columnIndex);
				TreeItemColumn_InvalidateSize(tree, column);
				layout = TRUE;
			}
			columnIndex++;
			column = TreeItemColumn_GetNext(tree, column);
		}
		if (layout)
		{
			TreeItem_InvalidateHeight(tree, item);
			Tree_InvalidateItemDInfo(tree, item, NULL);
			updateDInfo = TRUE;
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
	if (updateDInfo)
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
}

static void Style_ChangeElementsAux(TreeCtrl *tree, Style *style, int count, Element **elemList, int *map)
{
	ElementLink *eLink, *eLinks = NULL;
	int i, staticKeep[STATIC_SIZE], *keep = staticKeep;

	STATIC_ALLOC(keep, int, style->numElements);

	if (count > 0)
		eLinks = (ElementLink *) ckalloc(sizeof(ElementLink) * count);

	/* Assume we are discarding all the old ElementLinks */
	for (i = 0; i < style->numElements; i++)
		keep[i] = 0;

	for (i = 0; i < count; i++)
	{
		if (map[i] != -1)
		{
			eLinks[i] = style->elements[map[i]];
			keep[map[i]] = 1;
		}
		else
		{
			eLink = ElementLink_Init(&eLinks[i], elemList[i]);
			eLink->neededWidth = eLink->neededHeight = -1;
		}
	}

	if (style->numElements > 0)
	{
		/* Free unused ElementLinks */
		for (i = 0; i < style->numElements; i++)
		{
			if (!keep[i])
				ElementLink_FreeResources(tree, &style->elements[i]);
		}
		wipefree((char *) style->elements, sizeof(ElementLink) *
			style->numElements);
	}

	STATIC_FREE(keep, int, style->numElements);

	style->elements = eLinks;
	style->numElements = count;
}

static void Style_ChangeElements(TreeCtrl *tree, Style *masterStyle, int count, Element **elemList, int *map)
{
	TreeItem item;
	TreeItemColumn column;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	int columnIndex, layout;
	int updateDInfo = FALSE;
	int i, j, k;

	/* Update -union lists */
	for (i = 0; i < masterStyle->numElements; i++)
	{
		ElementLink *eLink = &masterStyle->elements[i];
		int staticKeep[STATIC_SIZE], *keep = staticKeep;
		int onionCnt = 0, *onion = NULL;

		if (eLink->onion == NULL)
			continue;

		STATIC_ALLOC(keep, int, eLink->onionCount);

		/* Check every Element in this -union */
		for (j = 0; j < eLink->onionCount; j++)
		{
			ElementLink *eLink2 = &masterStyle->elements[eLink->onion[j]];

			/* Check the new list of Elements */
			keep[j] = -1;
			for (k = 0; k < count; k++)
			{
				/* This new Element is in the -union */
				if (elemList[k] == eLink2->elem)
				{
					keep[j] = k;
					onionCnt++;
					break;
				}
			}
		}

		if (onionCnt > 0)
		{
			if (onionCnt != eLink->onionCount)
				onion = (int *) ckalloc(sizeof(int) * onionCnt);
			else
				onion = eLink->onion;
			k = 0;
			for (j = 0; j < eLink->onionCount; j++)
			{
				if (keep[j] != -1)
					onion[k++] = keep[j];
			}
		}

		STATIC_FREE(keep, int, eLink->onionCount);

		if (onionCnt != eLink->onionCount)
		{
			wipefree((char *) eLink->onion, sizeof(int) * eLink->onionCount);
			eLink->onion = onion;
			eLink->onionCount = onionCnt;
		}
	}

	Style_ChangeElementsAux(tree, masterStyle, count, elemList, map);

	hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	while (hPtr != NULL)
	{
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		column = TreeItem_GetFirstColumn(tree, item);
		columnIndex = 0;
		layout = FALSE;
		while (column != NULL)
		{
			Style *style = (Style *) TreeItemColumn_GetStyle(tree, column);
			if ((style != NULL) && (style->master == masterStyle))
			{
				Style_ChangeElementsAux(tree, style, count, elemList, map);
				style->neededWidth = style->neededHeight = -1;
				Tree_InvalidateColumnWidth(tree, columnIndex);
				TreeItemColumn_InvalidateSize(tree, column);
				layout = TRUE;
			}
			columnIndex++;
			column = TreeItemColumn_GetNext(tree, column);
		}
		if (layout)
		{
			TreeItem_InvalidateHeight(tree, item);
			Tree_InvalidateItemDInfo(tree, item, NULL);
			updateDInfo = TRUE;
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
	if (updateDInfo)
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
}

static void Style_ElemChanged(TreeCtrl *tree, Style *masterStyle,
	Element *masterElem, int flagM, int flagT, int csM)
{
	TreeItem item;
	TreeItemColumn column;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	ElementLink *eLink;
	int i, columnIndex;
	ElementArgs args;
	int eMask, iMask;
	int updateDInfo = FALSE;

	args.tree = tree;
	args.change.flagTree = flagT;
	args.change.flagMaster = flagM;
	args.change.flagSelf = 0;

	hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	while (hPtr != NULL)
	{
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		column = TreeItem_GetFirstColumn(tree, item);
		columnIndex = 0;
		iMask = 0;
		while (column != NULL)
		{
			Style *style = (Style *) TreeItemColumn_GetStyle(tree, column);
			if ((style != NULL) && (style->master == masterStyle))
			{
				for (i = 0; i < style->numElements; i++)
				{
					eLink = &style->elements[i];
					if (eLink->elem == masterElem)
					{
						if (csM & CS_LAYOUT)
							eLink->neededWidth = eLink->neededHeight = -1;
						break;
					}
					/* Instance element */
					if (eLink->elem->master == masterElem)
					{
						args.elem = eLink->elem;
						eMask = (*masterElem->typePtr->changeProc)(&args);
						if (eMask & CS_LAYOUT)
							eLink->neededWidth = eLink->neededHeight = -1;
						iMask |= eMask;
						break;
					}
				}
				iMask |= csM;
				if (iMask & CS_LAYOUT)
				{
					style->neededWidth = style->neededHeight = -1;
					Tree_InvalidateColumnWidth(tree, columnIndex);
					TreeItemColumn_InvalidateSize(tree, column);
				}
			}
			columnIndex++;
			column = TreeItemColumn_GetNext(tree, column);
		}
		if (iMask & CS_LAYOUT)
		{
			TreeItem_InvalidateHeight(tree, item);
			updateDInfo = TRUE;
		}
		if (iMask & CS_DISPLAY)
			Tree_InvalidateItemDInfo(tree, item, NULL);
		hPtr = Tcl_NextHashEntry(&search);
	}
	if (updateDInfo)
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);
}

TreeStyle TreeStyle_GetMaster(TreeCtrl *tree, TreeStyle style_)
{
	return (TreeStyle) ((Style *) style_)->master;
}

static Tcl_Obj *confTextObj = NULL;

Tcl_Obj *TreeStyle_GetText(TreeCtrl *tree, TreeStyle style_)
{
	Style *style = (Style *) style_;
	ElementLink *eLink;
	int i;

	if (confTextObj == NULL)
	{
		confTextObj = Tcl_NewStringObj("-text", -1);
		Tcl_IncrRefCount(confTextObj);
	}

	for (i = 0; i < style->numElements; i++)
	{
		eLink = &style->elements[i];
		if (eLink->elem->typePtr == &elemTypeText)
		{
			Tcl_Obj *resultObjPtr;
			resultObjPtr = Tk_GetOptionValue(tree->interp,
				(char *) eLink->elem, eLink->elem->typePtr->optionTable,
				confTextObj, tree->tkwin);
			return resultObjPtr;
		}
	}

	return NULL;
}

void TreeStyle_SetText(TreeCtrl *tree, TreeStyle style_, Tcl_Obj *textObj)
{
	Style *style = (Style *) style_;
	Style *masterStyle = style->master;
	ElementLink *eLink;
	int i;

	if (masterStyle == NULL)
		panic("TreeStyle_SetText called with master Style");

	if (confTextObj == NULL)
	{
		confTextObj = Tcl_NewStringObj("-text", -1);
		Tcl_IncrRefCount(confTextObj);
	}

	for (i = 0; i < style->numElements; i++)
	{
		eLink = &masterStyle->elements[i];
		if (eLink->elem->typePtr == &elemTypeText)
		{
			Tcl_Obj *objv[2];
			ElementArgs args;

			eLink = Style_CreateElem(tree, style, eLink->elem, NULL);
			objv[0] = confTextObj;
			objv[1] = textObj;
			args.tree = tree;
			args.elem = eLink->elem;
			args.config.objc = 2;
			args.config.objv = objv;
			args.config.flagSelf = 0;
			(void) (*eLink->elem->typePtr->configProc)(&args);

			args.change.flagSelf = args.config.flagSelf;
			args.change.flagTree = 0;
			args.change.flagMaster = 0;
			(*eLink->elem->typePtr->changeProc)(&args);

			eLink->neededWidth = eLink->neededHeight = -1;
			style->neededWidth = style->neededHeight = -1;
			return;
		}
	}
}

static void Style_Deleted(TreeCtrl *tree, Style *masterStyle)
{
	TreeItem item;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	TreeItemColumn column;
	int columnIndex;

	if (masterStyle->master != NULL)
		panic("Style_Deleted called with instance Style");

	hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	while (hPtr != NULL)
	{
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		column = TreeItem_GetFirstColumn(tree, item);
		columnIndex = 0;
		while (column != NULL)
		{
			Style *style = (Style *) TreeItemColumn_GetStyle(tree, column);
			if ((style != NULL) && (style->master == masterStyle))
			{
				Tree_InvalidateColumnWidth(tree, columnIndex);
				TreeItemColumn_ForgetStyle(tree, column);
				TreeItem_InvalidateHeight(tree, item);
				Tree_FreeItemDInfo(tree, item, NULL);
			}
			columnIndex++;
			column = TreeItemColumn_GetNext(tree, column);
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
}

static void Element_Changed(TreeCtrl *tree, Element *masterElem, int flagM, int flagT, int csM)
{
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	Style *masterStyle;
	ElementLink *eLink;
	int i;

	hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
	while (hPtr != NULL)
	{
		masterStyle = (Style *) Tcl_GetHashValue(hPtr);
		for (i = 0; i < masterStyle->numElements; i++)
		{
			eLink = &masterStyle->elements[i];
			if (eLink->elem == masterElem)
			{
				Style_ElemChanged(tree, masterStyle, masterElem, flagM, flagT, csM);
				break;
			}
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
}

static void Element_Deleted(TreeCtrl *tree, Element *masterElem)
{
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	Style *masterStyle;
	ElementLink *eLink;
	int i, j;

	hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
	while (hPtr != NULL)
	{
		masterStyle = (Style *) Tcl_GetHashValue(hPtr);
		for (i = 0; i < masterStyle->numElements; i++)
		{
			eLink = &masterStyle->elements[i];
			if (eLink->elem == masterElem)
			{
				Element *staticElemList[STATIC_SIZE], **elemList = staticElemList;
				int staticElemMap[STATIC_SIZE], *elemMap = staticElemMap;

				STATIC_ALLOC(elemList, Element *, masterStyle->numElements);
				STATIC_ALLOC(elemMap, int, masterStyle->numElements);

				for (j = 0; j < masterStyle->numElements; j++)
				{
					if (j == i)
						continue;
					elemList[(j < i) ? j : (j - 1)] =
						masterStyle->elements[j].elem;
					elemMap[(j < i) ? j : (j - 1)] = j;
				}
				Style_ChangeElements(tree, masterStyle,
					masterStyle->numElements - 1, elemList, elemMap);
				STATIC_FREE(elemList, Element *, masterStyle->numElements + 1);
				STATIC_FREE(elemMap, int, masterStyle->numElements + 1);
				break;
			}
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
}

void TreeStyle_TreeChanged(TreeCtrl *tree, int flagT)
{
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	Element *masterElem;
	ElementArgs args;
	int eMask;

	if (flagT == 0)
		return;

	args.tree = tree;
	args.change.flagTree = flagT;
	args.change.flagMaster = 0;
	args.change.flagSelf = 0;

	hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
	while (hPtr != NULL)
	{
		masterElem = (Element *) Tcl_GetHashValue(hPtr);
		args.elem = masterElem;
		eMask = (*masterElem->typePtr->changeProc)(&args);
		Element_Changed(tree, masterElem, 0, flagT, eMask);
		hPtr = Tcl_NextHashEntry(&search);
	}
}

int TreeStyle_ElementCget(TreeCtrl *tree, TreeStyle style_, Tcl_Obj *elemObj, Tcl_Obj *obj)
{
	Style *style = (Style *) style_;
	Tcl_Obj *resultObjPtr = NULL;
	Element *elem;
	ElementLink *eLink;

	if (Element_FromObj(tree, elemObj, &elem) != TCL_OK)
		return TCL_ERROR;

	eLink = Style_FindElem(tree, style, elem, NULL);
	if ((eLink != NULL) && (eLink->elem == elem) && (style->master != NULL))
		eLink = NULL;
	if (eLink == NULL)
	{
		FormatResult(tree->interp, "style %s does not use element %s",
			style->name, elem->name);
		return TCL_ERROR;
	}

	resultObjPtr = Tk_GetOptionValue(tree->interp, (char *) eLink->elem,
		eLink->elem->typePtr->optionTable, obj, tree->tkwin);
	if (resultObjPtr == NULL)
		return TCL_ERROR;
	Tcl_SetObjResult(tree->interp, resultObjPtr);
	return TCL_OK;
}

int TreeStyle_ElementConfigure(TreeCtrl *tree, TreeStyle style_, Tcl_Obj *elemObj, int objc, Tcl_Obj **objv, int *eMask)
{
	Style *style = (Style *) style_;
	Element *elem;
	ElementLink *eLink;
	ElementArgs args;

	(*eMask) = 0;

	if (Element_FromObj(tree, elemObj, &elem) != TCL_OK)
		return TCL_ERROR;

	if (objc <= 1)
	{
		Tcl_Obj *resultObjPtr;

		eLink = Style_FindElem(tree, style, elem, NULL);
		if ((eLink != NULL) && (eLink->elem == elem) && (style->master != NULL))
			eLink = NULL;
		if (eLink == NULL)
		{
			FormatResult(tree->interp, "style %s does not use element %s",
				style->name, elem->name);
			return TCL_ERROR;
		}

		resultObjPtr = Tk_GetOptionInfo(tree->interp, (char *) eLink->elem,
			eLink->elem->typePtr->optionTable,
			(objc == 0) ? (Tcl_Obj *) NULL : objv[0],
			tree->tkwin);
		if (resultObjPtr == NULL)
			return TCL_ERROR;
		Tcl_SetObjResult(tree->interp, resultObjPtr);
	}
	else
	{
		int isNew;

		eLink = Style_CreateElem(tree, style, elem, &isNew);
		if (eLink == NULL)
		{
			FormatResult(tree->interp, "style %s does not use element %s",
				style->name, elem->name);
			return TCL_ERROR;
		}

		/* Do this before configProc(). If eLink was just allocated and an
		 * error occurs in configProc() it won't be done */
		(*eMask) = 0;
		if (isNew)
		{
			eLink->neededWidth = eLink->neededHeight = -1;
			style->neededWidth = style->neededHeight = -1;
			(*eMask) = CS_DISPLAY | CS_LAYOUT;
		}

		args.tree = tree;
		args.elem = eLink->elem;
		args.config.objc = objc;
		args.config.objv = objv;
		args.config.flagSelf = 0;
		if ((*args.elem->typePtr->configProc)(&args) != TCL_OK)
			return TCL_ERROR;

		args.change.flagSelf = args.config.flagSelf;
		args.change.flagTree = 0;
		args.change.flagMaster = 0;
		(*eMask) |= (*elem->typePtr->changeProc)(&args);

		if (!isNew && ((*eMask) & CS_LAYOUT))
		{
			eLink->neededWidth = eLink->neededHeight = -1;
			style->neededWidth = style->neededHeight = -1;
		}
	}
	return TCL_OK;
}

int TreeStyle_ElementActual(TreeCtrl *tree, TreeStyle style_, int state, Tcl_Obj *elemObj, Tcl_Obj *obj)
{
	Style *style = (Style *) style_;
	Element *masterElem;
	ElementLink *eLink;
	ElementArgs args;

	if (Element_FromObj(tree, elemObj, &masterElem) != TCL_OK)
		return TCL_ERROR;

	eLink = Style_FindElem(tree, style, masterElem, NULL);
	if (eLink == NULL)
	{
		FormatResult(tree->interp, "style %s does not use element %s",
			style->name, masterElem->name);
		return TCL_ERROR;
	}
\
	args.tree = tree;
	args.elem = eLink->elem;
	args.state = state;
	args.actual.obj = obj;
	return (*masterElem->typePtr->actualProc)(&args);
}

static Element *Element_CreateAndConfig(TreeCtrl *tree, ElementType *type, char *name, int objc, Tcl_Obj *CONST objv[])
{
	Element *elem;
	ElementArgs args;

	elem = (Element *) ckalloc(type->size);
	memset(elem, '\0', type->size);
	elem->name = Tk_GetUid(name);
	elem->typePtr = type;
	args.tree = tree;
	args.elem = elem;
	if ((*type->createProc)(&args) != TCL_OK)
		return NULL;

	if (Tk_InitOptions(tree->interp, (char *) elem,
		type->optionTable, tree->tkwin) != TCL_OK)
	{
		WFREE(elem, Element);
		return NULL;
	}
	args.config.objc = objc;
	args.config.objv = objv;
	args.config.flagSelf = 0;
	if ((*type->configProc)(&args) != TCL_OK)
	{
		(*type->deleteProc)(&args);
		Tk_FreeConfigOptions((char *) elem,
			elem->typePtr->optionTable,
			tree->tkwin);
		WFREE(elem, Element);
		return NULL;
	}

	args.change.flagSelf = args.config.flagSelf;
	args.change.flagTree = 0;
	args.change.flagMaster = 0;
	(*type->changeProc)(&args);

	return elem;
}

int TreeElementCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	static CONST char *commandNames[] = { "cget", "configure", "create", "delete",
		"names", "type", (char *) NULL };
	enum { COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_CREATE, COMMAND_DELETE,
		COMMAND_NAMES, COMMAND_TYPE };
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
		case COMMAND_CGET:
		{
			Tcl_Obj *resultObjPtr = NULL;
			Element *elem;

			if (objc != 5)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name option");
				return TCL_ERROR;
			}
			if (Element_FromObj(tree, objv[3], &elem) != TCL_OK)
				return TCL_ERROR;
			resultObjPtr = Tk_GetOptionValue(interp, (char *) elem,
				elem->typePtr->optionTable, objv[4], tree->tkwin);
			if (resultObjPtr == NULL)
				return TCL_ERROR;
			Tcl_SetObjResult(interp, resultObjPtr);
			break;
		}

		case COMMAND_CONFIGURE:
		{
			Tcl_Obj *resultObjPtr = NULL;
			Element *elem;
			int eMask;

			if (objc < 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name ?option? ?value option value...?");
				return TCL_ERROR;
			}
			if (Element_FromObj(tree, objv[3], &elem) != TCL_OK)
				return TCL_ERROR;
			if (objc <= 5)
			{
				resultObjPtr = Tk_GetOptionInfo(interp, (char *) elem,
					elem->typePtr->optionTable,
					(objc == 4) ? (Tcl_Obj *) NULL : objv[4],
					tree->tkwin);
				if (resultObjPtr == NULL)
					return TCL_ERROR;
				Tcl_SetObjResult(interp, resultObjPtr);
			}
			else
			{
				ElementArgs args;

				args.tree = tree;
				args.elem = elem;
				args.config.objc = objc - 4;
				args.config.objv = objv + 4;
				args.config.flagSelf = 0;
				if ((*elem->typePtr->configProc)(&args) != TCL_OK)
					return TCL_ERROR;

				args.change.flagSelf = args.config.flagSelf;
				args.change.flagTree = 0;
				args.change.flagMaster = 0;
				eMask = (*elem->typePtr->changeProc)(&args);

				Element_Changed(tree, elem, args.change.flagSelf, 0, eMask);
			}
			break;
		}

		case COMMAND_CREATE:
		{
			char *name, *typeStr;
			int length;
			int isNew;
			Element *elem;
			ElementType *typePtr, *matchPtr = NULL;
			Tcl_HashEntry *hPtr;

			if (objc < 5)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name type ?option value...?");
				return TCL_ERROR;
			}
			name = Tcl_GetStringFromObj(objv[3], &length);
			if (!length)
				return TCL_ERROR;
			hPtr = Tcl_FindHashEntry(&tree->elementHash, name);
			if (hPtr != NULL)
			{
				FormatResult(interp, "element \"%s\" already exists", name);
				return TCL_ERROR;
			}
			typeStr = Tcl_GetStringFromObj(objv[4], &length);
			if (!length)
			{
				FormatResult(interp,
					"invalid element type \"\"");
				return TCL_ERROR;
			}
			for (typePtr = elementTypeList;
				typePtr != NULL;
				typePtr = typePtr->next)
			{
				if ((typeStr[0] == typePtr->name[0]) &&
					!strncmp(typeStr, typePtr->name, length))
				{
					if (matchPtr != NULL)
					{
						FormatResult(interp,
							"ambiguous element type \"%s\"",
							typeStr);
						return TCL_ERROR;
					}
					matchPtr = typePtr;
				}
			}
			if (matchPtr == NULL)
			{
				FormatResult(interp, "unknown element type \"%s\"", typeStr);
				return TCL_ERROR;
			}
			typePtr = matchPtr;
			elem = Element_CreateAndConfig(tree, typePtr, name, objc - 5, objv + 5);
			if (elem == NULL)
				return TCL_ERROR;
			hPtr = Tcl_CreateHashEntry(&tree->elementHash, name, &isNew);
			Tcl_SetHashValue(hPtr, elem);
			Tcl_SetObjResult(interp, Element_ToObj(elem));
			break;
		}

		case COMMAND_DELETE:
		{
			Element *elem;
			int i;

			if (objc < 3)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "?name ...?");
				return TCL_ERROR;
			}
			for (i = 3; i < objc; i++)
			{
				if (Element_FromObj(tree, objv[i], &elem) != TCL_OK)
					return TCL_ERROR;
				Element_Deleted(tree, elem);
				Element_FreeResources(tree, elem);
			}
			break;
		}

		case COMMAND_NAMES:
		{
			Tcl_Obj *listObj;
			Tcl_HashSearch search;
			Tcl_HashEntry *hPtr;
			Element *elem;

			listObj = Tcl_NewListObj(0, NULL);
			hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
			while (hPtr != NULL)
			{
				elem = (Element *) Tcl_GetHashValue(hPtr);
				Tcl_ListObjAppendElement(interp, listObj, Element_ToObj(elem));
				hPtr = Tcl_NextHashEntry(&search);
			}
			Tcl_SetObjResult(interp, listObj);
			break;
		}

		case COMMAND_TYPE:
		{
			Element *elem;

			if (objc != 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name");
				return TCL_ERROR;
			}
			if (Element_FromObj(tree, objv[3], &elem) != TCL_OK)
				return TCL_ERROR;
			Tcl_SetResult(interp, elem->typePtr->name, TCL_STATIC); /* Tk_Uid */
			break;
		}
	}
	return TCL_OK;
}

static Style *Style_CreateAndConfig(TreeCtrl *tree, char *name, int objc, Tcl_Obj *CONST objv[])
{
	Style *style;

	style = (Style *) ckalloc(sizeof(Style));
	memset(style, '\0', sizeof(Style));
	style->optionTable = Tk_CreateOptionTable(tree->interp, styleOptionSpecs); 
	style->name = Tk_GetUid(name);

	if (Tk_InitOptions(tree->interp, (char *) style,
		style->optionTable, tree->tkwin) != TCL_OK)
	{
		WFREE(style, Style);
		return NULL;
	}

	if (Tk_SetOptions(tree->interp, (char *) style,
		style->optionTable, objc, objv, tree->tkwin,
		NULL, NULL) != TCL_OK)
	{
		Tk_FreeConfigOptions((char *) style, style->optionTable, tree->tkwin);
		WFREE(style, Style);
		return NULL;
	}

	return style;
}

void TreeStyle_ListElements(TreeCtrl *tree, TreeStyle style_)
{
	Style *style = (Style *) style_;
	Tcl_Obj *listObj;
	ElementLink *eLink;
	int i;

	if (style->numElements <= 0)
		return;

	listObj = Tcl_NewListObj(0, NULL);
	for (i = 0; i < style->numElements; i++)
	{
		eLink = &style->elements[i];
		if ((style->master != NULL) && (eLink->elem->master == NULL))
			continue;
		Tcl_ListObjAppendElement(tree->interp, listObj,
			Element_ToObj(eLink->elem));
	}
	Tcl_SetObjResult(tree->interp, listObj);
}

static int StyleLayoutCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	Style *style;
	Element *elem;
	ElementLink *eLink;
	int i, index;
	static CONST char *optionNames[] = { "-padx", "-pady", "-ipadx",
		"-ipady", "-expand", "-union", "-detach", "-iexpand",
		"-squeeze", (char *) NULL };
	enum { OPTION_PADX, OPTION_PADY, OPTION_iPADX, OPTION_iPADY,
		OPTION_EXPAND,
		OPTION_UNION, OPTION_DETACH, OPTION_iEXPAND, OPTION_SQUEEZE };

	if (objc < 5)
	{
		Tcl_WrongNumArgs(interp, 3, objv, "name element ?option? ?value? ?option value ...?");
		return TCL_ERROR;
	}

	if (TreeStyle_FromObj(tree, objv[3], (TreeStyle *) &style) != TCL_OK)
		return TCL_ERROR;

	if (Element_FromObj(tree, objv[4], &elem) != TCL_OK)
		return TCL_ERROR;

	eLink = Style_FindElem(tree, style, elem, NULL);
	if (eLink == NULL)
	{
		FormatResult(interp, "style %s does not use element %s",
			style->name, elem->name);
		return TCL_ERROR;
	}

	/* T style layout S E */
	if (objc == 5)
	{
		char flags[4];
		int n;
		Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
		Tcl_Obj *unionObj = Tcl_NewListObj(0, NULL);

		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-padx", -1));
		Tcl_ListObjAppendElement(interp, listObj, TreeCtrl_NewPadAmountObj(eLink->ePadX));
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-pady", -1));
		Tcl_ListObjAppendElement(interp, listObj, TreeCtrl_NewPadAmountObj(eLink->ePadY));
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-ipadx", -1));
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewIntObj(eLink->iPadX[PAD_TOP_LEFT]));
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-ipady", -1));
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewIntObj(eLink->iPadY[PAD_TOP_LEFT]));

		n = 0;
		if (eLink->flags & ELF_eEXPAND_W) flags[n++] = 'w';
		if (eLink->flags & ELF_eEXPAND_N) flags[n++] = 'n';
		if (eLink->flags & ELF_eEXPAND_E) flags[n++] = 'e';
		if (eLink->flags & ELF_eEXPAND_S) flags[n++] = 's';
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-expand", -1));
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(flags, n));

		n = 0;
		if (eLink->flags & ELF_iEXPAND_W) flags[n++] = 'w';
		if (eLink->flags & ELF_iEXPAND_N) flags[n++] = 'n';
		if (eLink->flags & ELF_iEXPAND_E) flags[n++] = 'e';
		if (eLink->flags & ELF_iEXPAND_S) flags[n++] = 's';
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-iexpand", -1));
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(flags, n));

		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-detach", -1));
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj((eLink->flags & ELF_DETACH) ? "yes" : "no", -1));

		n = 0;
		if (eLink->flags & ELF_SQUEEZE_X) flags[n++] = 'x';
		if (eLink->flags & ELF_SQUEEZE_Y) flags[n++] = 'y';
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-squeeze", -1));
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(flags, n));

		for (i = 0; i < eLink->onionCount; i++)
			Tcl_ListObjAppendElement(interp, unionObj,
				Element_ToObj(style->elements[eLink->onion[i]].elem));
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-union", -1));
		Tcl_ListObjAppendElement(interp, listObj, unionObj);

		Tcl_SetObjResult(interp, listObj);
		return TCL_OK;
	}

	/* T style layout S E option */
	if (objc == 6)
	{
		Tcl_Obj *objPtr = NULL;

		if (Tcl_GetIndexFromObj(interp, objv[5], optionNames, "option",
			0, &index) != TCL_OK)
			return TCL_ERROR;
		switch (index)
		{
			case OPTION_PADX:
			{
				 objPtr = TreeCtrl_NewPadAmountObj(eLink->ePadX);
				 break;
			}
			case OPTION_PADY:
			{
				 objPtr = TreeCtrl_NewPadAmountObj(eLink->ePadY);
				 break;
			}
			case OPTION_iPADX:
			{
				 objPtr = TreeCtrl_NewPadAmountObj(eLink->iPadX);
				 break;
			}
			case OPTION_iPADY:
			{
				 objPtr = TreeCtrl_NewPadAmountObj(eLink->iPadY);
				 break;
			}
			case OPTION_DETACH:
			{
				objPtr = Tcl_NewBooleanObj(eLink->flags & ELF_DETACH);
				break;
			}
			case OPTION_EXPAND:
			{
				char flags[4];
				int n = 0;

				if (eLink->flags & ELF_eEXPAND_W) flags[n++] = 'w';
				if (eLink->flags & ELF_eEXPAND_N) flags[n++] = 'n';
				if (eLink->flags & ELF_eEXPAND_E) flags[n++] = 'e';
				if (eLink->flags & ELF_eEXPAND_S) flags[n++] = 's';
				if (n)
					objPtr = Tcl_NewStringObj(flags, n);
				break;
			}
			case OPTION_iEXPAND:
			{
				char flags[4];
				int n = 0;

				if (eLink->flags & ELF_iEXPAND_W) flags[n++] = 'w';
				if (eLink->flags & ELF_iEXPAND_N) flags[n++] = 'n';
				if (eLink->flags & ELF_iEXPAND_E) flags[n++] = 'e';
				if (eLink->flags & ELF_iEXPAND_S) flags[n++] = 's';
				if (n)
					objPtr = Tcl_NewStringObj(flags, n);
				break;
			}
			case OPTION_SQUEEZE:
			{
				char flags[2];
				int n = 0;

				if (eLink->flags & ELF_SQUEEZE_X) flags[n++] = 'x';
				if (eLink->flags & ELF_SQUEEZE_Y) flags[n++] = 'y';
				if (n)
					objPtr = Tcl_NewStringObj(flags, n);
				break;
			}
			case OPTION_UNION:
			{
				int i;

				if (eLink->onionCount == 0)
					break;
				objPtr = Tcl_NewListObj(0, NULL);
				for (i = 0; i < eLink->onionCount; i++)
					Tcl_ListObjAppendElement(interp, objPtr,
						Element_ToObj(style->elements[eLink->onion[i]].elem));
				break;
			}
		}
		if (objPtr != NULL)
			Tcl_SetObjResult(interp, objPtr);
		return TCL_OK;
	}

	for (i = 5; i < objc; i += 2)
	{
		if (i + 2 > objc)
		{
			FormatResult(interp, "value for \"%s\" missing",
				Tcl_GetString(objv[i]));
			return TCL_ERROR;
		}
		if (Tcl_GetIndexFromObj(interp, objv[i], optionNames, "option",
			0, &index) != TCL_OK)
		{
			return TCL_ERROR;
		}
		switch (index)
		{
			case OPTION_PADX:
			{
				if (TreeCtrl_GetPadAmountFromObj(interp,
					tree->tkwin, objv[i + 1],
					&eLink->ePadX[PAD_TOP_LEFT],
					&eLink->ePadX[PAD_BOTTOM_RIGHT]) != TCL_OK)
					return TCL_ERROR;
				break;
			}
			case OPTION_PADY:
			{
				if (TreeCtrl_GetPadAmountFromObj(interp,
					tree->tkwin, objv[i + 1],
					&eLink->ePadY[PAD_TOP_LEFT],
					&eLink->ePadY[PAD_BOTTOM_RIGHT]) != TCL_OK)
					return TCL_ERROR;
				break;
			}
			case OPTION_iPADX:
			{
				if (TreeCtrl_GetPadAmountFromObj(interp,
					tree->tkwin, objv[i + 1],
					&eLink->iPadX[PAD_TOP_LEFT],
					&eLink->iPadX[PAD_BOTTOM_RIGHT]) != TCL_OK)
					return TCL_ERROR;
				break;
			}
			case OPTION_iPADY:
			{
				if (TreeCtrl_GetPadAmountFromObj(interp,
					tree->tkwin, objv[i + 1],
					&eLink->iPadY[PAD_TOP_LEFT],
					&eLink->iPadY[PAD_BOTTOM_RIGHT]) != TCL_OK)
					return TCL_ERROR;
				break;
			}
			case OPTION_DETACH:
			{
				int detach;
				if (Tcl_GetBooleanFromObj(interp, objv[i + 1], &detach) != TCL_OK)
					return TCL_ERROR;
				if (detach)
					eLink->flags |= ELF_DETACH;
				else
					eLink->flags &= ~ELF_DETACH;
				break;
			}
			case OPTION_EXPAND:
			{
				char *expand;
				int len, k;
				expand = Tcl_GetStringFromObj(objv[i + 1], &len);
				eLink->flags &= ~ELF_eEXPAND;
				for (k = 0; k < len; k++)
				{
					switch (expand[k])
					{
						case 'w': case 'W': eLink->flags |= ELF_eEXPAND_W; break;
						case 'n': case 'N': eLink->flags |= ELF_eEXPAND_N; break;
						case 'e': case 'E': eLink->flags |= ELF_eEXPAND_E; break;
						case 's': case 'S': eLink->flags |= ELF_eEXPAND_S; break;
						default:
						{
							Tcl_ResetResult(tree->interp);
							Tcl_AppendResult(tree->interp, "bad expand value \"",
								expand, "\": must be a string ",
								"containing zero or more of n, e, s, and w",
								(char *) NULL);
							return TCL_ERROR;
						}
					}
				}
				break;
			}
			case OPTION_iEXPAND:
			{
				char *expand;
				int len, k;
				expand = Tcl_GetStringFromObj(objv[i + 1], &len);
				eLink->flags &= ~ELF_iEXPAND;
				for (k = 0; k < len; k++)
				{
					switch (expand[k])
					{
						case 'w': case 'W': eLink->flags |= ELF_iEXPAND_W; break;
						case 'n': case 'N': eLink->flags |= ELF_iEXPAND_N; break;
						case 'e': case 'E': eLink->flags |= ELF_iEXPAND_E; break;
						case 's': case 'S': eLink->flags |= ELF_iEXPAND_S; break;
						default:
						{
							Tcl_ResetResult(tree->interp);
							Tcl_AppendResult(tree->interp, "bad iexpand value \"",
								expand, "\": must be a string ",
								"containing zero or more of n, e, s, and w",
								(char *) NULL);
							return TCL_ERROR;
						}
					}
				}
				break;
			}
			case OPTION_SQUEEZE:
			{
				char *string;
				int len, k;
				string = Tcl_GetStringFromObj(objv[i + 1], &len);
				eLink->flags &= ~(ELF_SQUEEZE_X | ELF_SQUEEZE_Y);
				for (k = 0; k < len; k++)
				{
					switch (string[k])
					{
						case 'x': case 'X': eLink->flags |= ELF_SQUEEZE_X; break;
						case 'y': case 'Y': eLink->flags |= ELF_SQUEEZE_Y; break;
						default:
						{
							Tcl_ResetResult(tree->interp);
							Tcl_AppendResult(tree->interp, "bad squeeze value \"",
								string, "\": must be a string ",
								"containing zero or more of x and y",
								(char *) NULL);
							return TCL_ERROR;
						}
					}
				}
				break;
			}
			case OPTION_UNION:
			{
				int objc1;
				Tcl_Obj **objv1;
				int j, k, n, *onion, count = 0;

				if (Tcl_ListObjGetElements(interp, objv[i + 1],
					&objc1, &objv1) != TCL_OK)
					return TCL_ERROR;
				if (objc1 == 0)
				{
					if (eLink->onion != NULL)
					{
						wipefree((char *) eLink->onion,
							sizeof(int) * eLink->onionCount);
						eLink->onionCount = 0;
						eLink->onion = NULL;
					}
					break;
				}
				onion = (int *) ckalloc(sizeof(int) * objc1);
				for (j = 0; j < objc1; j++)
				{
					Element *elem2;
					ElementLink *eLink2;

					if (Element_FromObj(tree, objv1[j], &elem2) != TCL_OK)
					{
						ckfree((char *) onion);
						return TCL_ERROR;
					}

					eLink2 = Style_FindElem(tree, style, elem2, &n);
					if (eLink2 == NULL)
					{
						ckfree((char *) onion);
						FormatResult(interp,
							"style %s does not use element %s",
							style->name, elem2->name);
						return TCL_ERROR;
					}
					if (eLink == eLink2)
					{
						ckfree((char *) onion);
						FormatResult(interp,
							"element %s can't form union with itself",
							elem2->name);
						return TCL_ERROR;
					}
					/* Silently ignore duplicates */
					for (k = 0; k < count; k++)
					{
						if (onion[k] == n)
							break;
					}
					if (k < count)
						continue;
					onion[count++] = n;
				}
				if (eLink->onion != NULL)
					wipefree((char *) eLink->onion,
						sizeof(int) * eLink->onionCount);
				if (count == objc1)
					eLink->onion = onion;
				else
				{
					eLink->onion = (int *) ckalloc(sizeof(int) * count);
					for (k = 0; k < count; k++)
						eLink->onion[k] = onion[k];
					ckfree((char *) onion);
				}
				eLink->onionCount = count;
				break;
			}
		}
	}
	Style_Changed(tree, style);
	return TCL_OK;
}

int TreeStyleCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	static CONST char *commandNames[] = { "cget", "configure", "create", "delete",
		"elements", "layout", "names", (char *) NULL };
	enum { COMMAND_CGET, COMMAND_CONFIGURE, COMMAND_CREATE, COMMAND_DELETE,
		COMMAND_ELEMENTS, COMMAND_LAYOUT, COMMAND_NAMES };
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
		case COMMAND_CGET:
		{
			Tcl_Obj *resultObjPtr;
			Style *style;

			if (objc != 5)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name option");
				return TCL_ERROR;
			}
			if (TreeStyle_FromObj(tree, objv[3], (TreeStyle *) &style) != TCL_OK)
				return TCL_ERROR;
			resultObjPtr = Tk_GetOptionValue(interp, (char *) style,
				style->optionTable, objv[4], tree->tkwin);
			if (resultObjPtr == NULL)
				return TCL_ERROR;
			Tcl_SetObjResult(interp, resultObjPtr);
			break;
		}

		case COMMAND_CONFIGURE:
		{
			Tcl_Obj *resultObjPtr = NULL;
			Style *style;

			if (objc < 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name ?option? ?value option value...?");
				return TCL_ERROR;
			}
			if (TreeStyle_FromObj(tree, objv[3], (TreeStyle *) &style) != TCL_OK)
				return TCL_ERROR;
			if (objc <= 5)
			{
				resultObjPtr = Tk_GetOptionInfo(interp, (char *) style,
					style->optionTable,
					(objc == 4) ? (Tcl_Obj *) NULL : objv[4],
					tree->tkwin);
				if (resultObjPtr == NULL)
					return TCL_ERROR;
				Tcl_SetObjResult(interp, resultObjPtr);
			}
			else
			{
				if (Tk_SetOptions(tree->interp, (char *) style,
					style->optionTable, objc - 4, objv + 4, tree->tkwin,
					NULL, NULL) != TCL_OK)
					return TCL_ERROR;
				Style_Changed(tree, style);
			}
			break;
		}

		case COMMAND_CREATE:
		{
			char *name;
			int len;
			Tcl_HashEntry *hPtr;
			int isNew;
			Style *style;

			if (objc < 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name ?option value...?");
				return TCL_ERROR;
			}
			name = Tcl_GetStringFromObj(objv[3], &len);
			if (!len)
			{
				FormatResult(interp, "invalid style name \"\"");
				return TCL_ERROR;
			}
			hPtr = Tcl_FindHashEntry(&tree->styleHash, name);
			if (hPtr != NULL)
			{
				FormatResult(interp, "style \"%s\" already exists", name);
				return TCL_ERROR;
			}
			style = Style_CreateAndConfig(tree, name, objc - 4, objv + 4);
			if (style == NULL)
				return TCL_ERROR;
			hPtr = Tcl_CreateHashEntry(&tree->styleHash, name, &isNew);
			Tcl_SetHashValue(hPtr, style);
			Tcl_SetObjResult(interp, TreeStyle_ToObj((TreeStyle) style));
			break;
		}

		case COMMAND_DELETE:
		{
			Style *style;
			int i;

			if (objc < 3)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "?name ...?");
				return TCL_ERROR;
			}
			for (i = 3; i < objc; i++)
			{
				if (TreeStyle_FromObj(tree, objv[i], (TreeStyle *) &style) != TCL_OK)
					return TCL_ERROR;
				Style_Deleted(tree, style);
				TreeStyle_FreeResources(tree, (TreeStyle) style);
			}
			break;
		}

		/* T style elements S ?{E ...}? */
		case COMMAND_ELEMENTS:
		{
			Style *style;
			Element *elem, **elemList = NULL;
			int i, j, count = 0;
			int staticMap[STATIC_SIZE], *map = staticMap;
			int listObjc;
			Tcl_Obj **listObjv;

			if (objc < 4 || objc > 5)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "name ?element element...?");
				return TCL_ERROR;
			}
			if (TreeStyle_FromObj(tree, objv[3], (TreeStyle *) &style) != TCL_OK)
				return TCL_ERROR;
			if (objc == 5)
			{
				if (Tcl_ListObjGetElements(interp, objv[4], &listObjc, &listObjv) != TCL_OK)
					return TCL_ERROR;
				if (listObjc > 0)
					elemList = (Element **) ckalloc(sizeof(Element *) * listObjc);
				for (i = 0; i < listObjc; i++)
				{
					if (Element_FromObj(tree, listObjv[i], &elem) != TCL_OK)
					{
						ckfree((char *) elemList);
						return TCL_ERROR;
					}

					/* Ignore duplicate elements */
					for (j = 0; j < count; j++)
					{
						if (elemList[j] == elem)
							break;
					}
					if (j < count)
						continue;

					elemList[count++] = elem;
				}

				STATIC_ALLOC(map, int, count);

				for (i = 0; i < count; i++)
					map[i] = -1;

				/* Reassigning Elements to a Style */
				if (style->numElements > 0)
				{
					/* Check each Element */
					for (i = 0; i < count; i++)
					{
						/* See if this Element is already used by the Style */
						for (j = 0; j < style->numElements; j++)
						{
							if (elemList[i] == style->elements[j].elem)
							{
								/* Preserve it */
								map[i] = j;
								break;
							}
						}
					}
				}
				Style_ChangeElements(tree, style, count, elemList, map);
				if (elemList != NULL)
					ckfree((char *) elemList);
				STATIC_FREE(map, int, count);
				break;
			}
			TreeStyle_ListElements(tree, (TreeStyle) style);
			break;
		}

		/* T style layout S E ?option? ?value? ?option value ...? */
		case COMMAND_LAYOUT:
		{
			return StyleLayoutCmd(clientData, interp, objc, objv);
		}

		case COMMAND_NAMES:
		{
			Tcl_Obj *listObj;
			Tcl_HashSearch search;
			Tcl_HashEntry *hPtr;
			TreeStyle style;

			listObj = Tcl_NewListObj(0, NULL);
			hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
			while (hPtr != NULL)
			{
				style = (TreeStyle) Tcl_GetHashValue(hPtr);
				Tcl_ListObjAppendElement(interp, listObj,
					TreeStyle_ToObj(style));
				hPtr = Tcl_NextHashEntry(&search);
			}
			Tcl_SetObjResult(interp, listObj);
			break;
		}
	}
	return TCL_OK;
}

char *TreeStyle_Identify(StyleDrawArgs *drawArgs, int x, int y)
{
	TreeCtrl *tree = drawArgs->tree;
	Style *style = (Style *) drawArgs->style;
	int state = drawArgs->state;
	ElementLink *eLink = NULL;
	int i;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;

	if (style->neededWidth == -1)
	{
		Style_NeededSize(tree, style, state, &style->neededWidth,
			&style->neededHeight, FALSE);
		Style_NeededSize(tree, style, state, &style->minWidth,
			&style->minHeight, TRUE);
	}
	if (drawArgs->width < style->minWidth)
		drawArgs->width = style->minWidth;
	if (drawArgs->height < style->minHeight)
		drawArgs->height = style->minHeight;

	x -= drawArgs->x;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	Style_DoLayout(drawArgs, layouts, __FILE__, __LINE__);

	for (i = style->numElements - 1; i >= 0; i--)
	{
		struct Layout *layout = &layouts[i];
		eLink = layout->eLink;
		if ((x >= layout->x + layout->ePadX[PAD_TOP_LEFT]) && (x < layout->x + layout->ePadX[PAD_TOP_LEFT] + layout->iWidth) &&
			(y >= layout->y + layout->ePadY[PAD_TOP_LEFT]) && (y < layout->y + layout->ePadY[PAD_TOP_LEFT] + layout->iHeight))
		{
			goto done;
		}
	}
	eLink = NULL;
done:
	STATIC_FREE(layouts, struct Layout, style->numElements);
	if (eLink != NULL)
		return (char *) eLink->elem->name;
	return NULL;
}

void TreeStyle_Identify2(StyleDrawArgs *drawArgs,
	int x1, int y1, int x2, int y2, Tcl_Obj *listObj)
{
	TreeCtrl *tree = drawArgs->tree;
	Style *style = (Style *) drawArgs->style;
	int state = drawArgs->state;
	ElementLink *eLink;
	int i;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;

	if (style->neededWidth == -1)
	{
		Style_NeededSize(tree, style, state, &style->neededWidth,
			&style->neededHeight, FALSE);
		Style_NeededSize(tree, style, state, &style->minWidth,
			&style->minHeight, TRUE);
	}
	if (drawArgs->width < style->minWidth)
		drawArgs->width = style->minWidth;
	if (drawArgs->height < style->minHeight)
		drawArgs->height = style->minHeight;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	Style_DoLayout(drawArgs, layouts, __FILE__, __LINE__);

	for (i = style->numElements - 1; i >= 0; i--)
	{
		struct Layout *layout = &layouts[i];
		eLink = layout->eLink;
		if ((drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT] < x2) &&
			(drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT] + layout->iWidth > x1) &&
			(drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT] < y2) &&
			(drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT] + layout->iHeight > y1))
		{
			Tcl_ListObjAppendElement(drawArgs->tree->interp, listObj,
				Tcl_NewStringObj(eLink->elem->name, -1));
		}
	}

	STATIC_FREE(layouts, struct Layout, style->numElements);
}

int TreeStyle_Remap(TreeCtrl *tree, TreeStyle styleFrom_, TreeStyle styleTo_, int objc, Tcl_Obj *CONST objv[])
{
	Style *styleFrom = (Style *) styleFrom_;
	Style *styleTo = (Style *) styleTo_;
	int i, indexFrom, indexTo;
	int staticMap[STATIC_SIZE], *map = staticMap;
	ElementLink *eLink;
	Element *elemFrom, *elemTo;
	Element *staticElemMap[STATIC_SIZE], **elemMap = staticElemMap;
	int styleFromNumElements = styleFrom->numElements;
	int result = TCL_OK;

	/* Must be instance */
	if ((styleFrom == NULL) || (styleFrom->master == NULL))
		return TCL_ERROR;

	/* Must be master */
	if ((styleTo == NULL) || (styleTo->master != NULL))
		return TCL_ERROR;

	/* Nothing to do */
	if (styleFrom->master == styleTo)
		return TCL_OK;

	if (objc & 1)
		return TCL_ERROR;

	STATIC_ALLOC(map, int, styleFrom->numElements);
	STATIC_ALLOC(elemMap, Element *, styleFrom->numElements);

	for (i = 0; i < styleFrom->numElements; i++)
		map[i] = -1;

	for (i = 0; i < objc; i += 2)
	{
		/* Get the old-style element */
		if (Element_FromObj(tree, objv[i], &elemFrom) != TCL_OK)
		{
			result = TCL_ERROR;
			goto done;
		}

		/* Verify the old style uses the element */
		eLink = Style_FindElem(tree, styleFrom->master, elemFrom, &indexFrom);
		if (eLink == NULL)
		{
			FormatResult(tree->interp, "style %s does not use element %s",
				styleFrom->name, elemFrom->name);
			result = TCL_ERROR;
			goto done;
		}

		/* Get the new-style element */
		if (Element_FromObj(tree, objv[i + 1], &elemTo) != TCL_OK)
		{
			result = TCL_ERROR;
			goto done;
		}

		/* Verify the new style uses the element */
		eLink = Style_FindElem(tree, styleTo, elemTo, &indexTo);
		if (eLink == NULL)
		{
			FormatResult(tree->interp, "style %s does not use element %s",
				styleTo->name, elemTo->name);
			result = TCL_ERROR;
			goto done;
		}

		/* Must be the same type */
		if (elemFrom->typePtr != elemTo->typePtr)
		{
			FormatResult(tree->interp, "can't map element type %s to %s",
				elemFrom->typePtr->name, elemTo->typePtr->name);
			result = TCL_ERROR;
			goto done;
		}

		/* See if the instance style has any info for this element */
		eLink = &styleFrom->elements[indexFrom];
		if (eLink->elem->master != NULL)
		{
			map[indexFrom] = indexTo;
			elemMap[indexFrom] = eLink->elem;
		}
	}

	for (i = 0; i < styleFrom->numElements; i++)
	{
		eLink = &styleFrom->elements[i];
		indexTo = map[i];

		/* Free info for any Elements not being remapped */
		if ((indexTo == -1) && (eLink->elem->master != NULL))
		{
			elemFrom = eLink->elem->master;
			Element_FreeResources(tree, eLink->elem);
			eLink->elem = elemFrom;
		}

		/* Remap this Element */
		if (indexTo != -1)
		{
			elemMap[i]->master = styleTo->elements[indexTo].elem;
			elemMap[i]->name = styleTo->elements[indexTo].elem->name;
		}
	}

	if (styleFrom->numElements != styleTo->numElements)
	{
		if (styleFrom->numElements > 0)
			wipefree((char *) styleFrom->elements, sizeof(ElementLink) *
			styleFrom->numElements);
		styleFrom->elements = (ElementLink *) ckalloc(sizeof(ElementLink) *
			styleTo->numElements);
		memset(styleFrom->elements, '\0', sizeof(ElementLink) * styleTo->numElements);
	}
	for (i = 0; i < styleTo->numElements; i++)
	{
		styleFrom->elements[i].elem = styleTo->elements[i].elem;
		styleFrom->elements[i].neededWidth = -1;
		styleFrom->elements[i].neededHeight = -1;
	}
	for (i = 0; i < styleFrom->numElements; i++)
	{
		indexTo = map[i];
		if (indexTo != -1)
			styleFrom->elements[indexTo].elem = elemMap[i];
	}
	styleFrom->name = styleTo->name;
	styleFrom->master = styleTo;
	styleFrom->neededWidth = styleFrom->neededHeight = -1;
	styleFrom->numElements = styleTo->numElements;

done:
	STATIC_FREE(map, int, styleFromNumElements);
	STATIC_FREE(elemMap, Element *, styleFromNumElements);
	return result;
}

int TreeStyle_GetSortData(TreeCtrl *tree, TreeStyle style_, int elemIndex, int type, long *lv, double *dv, char **sv)
{
	Style *style = (Style *) style_;
	ElementLink *eLink = style->elements;
	int i;

	if (elemIndex == -1)
	{
		for (i = 0; i < style->numElements; i++)
		{
			if (eLink->elem->typePtr == &elemTypeText)
				return Element_GetSortData(tree, eLink->elem, type, lv, dv, sv);
			eLink++;
		}
	}
	else
	{
		if ((elemIndex < 0) || (elemIndex >= style->numElements))
			panic("bad elemIndex %d to TreeStyle_GetSortData", elemIndex);
		eLink = &style->elements[elemIndex];
		if (eLink->elem->typePtr == &elemTypeText)
			return Element_GetSortData(tree, eLink->elem, type, lv, dv, sv);
	}

	FormatResult(tree->interp, "can't find text element in style %s",
		style->name);
	return TCL_ERROR;
}

int TreeStyle_ValidateElements(StyleDrawArgs *drawArgs, int objc, Tcl_Obj *CONST objv[])
{
	Style *style = (Style *) drawArgs->style;
	Style *master = style->master ? style->master : style;
	Element *elem;
	ElementLink *eLink;
	int i;

	for (i = 0; i < objc; i++)
	{
		if (Element_FromObj(drawArgs->tree, objv[i], &elem) != TCL_OK)
			return TCL_ERROR;

		eLink = Style_FindElem(drawArgs->tree, master, elem, NULL);
		if (eLink == NULL)
		{
			FormatResult(drawArgs->tree->interp,
				"style %s does not use element %s",
				style->name, elem->name);
			return TCL_ERROR;
		}
	}
	return TCL_OK;
}

int TreeStyle_GetElemRects(StyleDrawArgs *drawArgs, int objc,
	Tcl_Obj *CONST objv[], XRectangle rects[])
{
	Style *style = (Style *) drawArgs->style;
	Style *master = style->master ? style->master : style;
	int i, j, count = 0;
	struct Layout staticLayouts[STATIC_SIZE], *layouts = staticLayouts;
	Element *staticElems[STATIC_SIZE], **elems = staticElems;
	ElementLink *eLink;

	STATIC_ALLOC(elems, Element *, objc);

	for (j = 0; j < objc; j++)
	{
		if (Element_FromObj(drawArgs->tree, objv[j], &elems[j]) != TCL_OK)
		{
			count = -1;
			goto done;
		}

		eLink = Style_FindElem(drawArgs->tree, master, elems[j], NULL);
		if (eLink == NULL)
		{
			FormatResult(drawArgs->tree->interp,
				"style %s does not use element %s",
				style->name, elems[j]->name);
			count = -1;
			goto done;
		}
	}

	if (drawArgs->width < style->minWidth)
		drawArgs->width = style->minWidth;
	if (drawArgs->height < style->minHeight)
		drawArgs->height = style->minHeight;

	STATIC_ALLOC(layouts, struct Layout, style->numElements);

	Style_DoLayout(drawArgs, layouts, __FILE__, __LINE__);

	for (i = style->numElements - 1; i >= 0; i--)
	{
		struct Layout *layout = &layouts[i];
		if (objc > 0)
		{
			for (j = 0; j < objc; j++)
				if (elems[j] == layout->eLink->elem ||
					elems[j] == layout->master->elem)
					break;
			if (j == objc)
				continue;
		}
		rects[count].x = drawArgs->x + layout->x + layout->ePadX[PAD_TOP_LEFT];
		rects[count].y = drawArgs->y + layout->y + layout->ePadY[PAD_TOP_LEFT];
		rects[count].width = layout->iWidth;
		rects[count].height = layout->iHeight;
		count++;
	}

	STATIC_FREE(layouts, struct Layout, style->numElements);

done:
	STATIC_FREE(elems, Element *, objc);
	return count;
}

int TreeStyle_ChangeState(TreeCtrl *tree, TreeStyle style_, int state1, int state2)
{
	Style *style = (Style *) style_;
	ElementLink *eLink;
	ElementArgs args;
	int i, eMask, mask = 0;

	args.tree = tree;
	args.states.state1 = state1;
	args.states.state2 = state2;

	for (i = 0; i < style->numElements; i++)
	{
		eLink = &style->elements[i];
		args.elem = eLink->elem;
		eMask = (*eLink->elem->typePtr->stateProc)(&args);
		if (eMask)
		{
			if (eMask & CS_LAYOUT)
				eLink->neededWidth = eLink->neededHeight = -1;
			mask |= eMask;
		}
	}

	if (mask & CS_LAYOUT)
		style->neededWidth = style->neededHeight = -1;

	return mask;
}

void TreeStyle_UndefineState(TreeCtrl *tree, int state)
{
	TreeItem item;
	TreeItemColumn column;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	ElementLink *eLink;
	int i, columnIndex;
	ElementArgs args;
	int eMask, cMask, iMask;
	int updateDInfo = FALSE;

	args.tree = tree;
	args.state = state;

	hPtr = Tcl_FirstHashEntry(&tree->itemHash, &search);
	while (hPtr != NULL)
	{
		item = (TreeItem) Tcl_GetHashValue(hPtr);
		column = TreeItem_GetFirstColumn(tree, item);
		columnIndex = 0;
		iMask = 0;
		args.states.state1 = TreeItem_GetState(tree, item);
		args.states.state2 = args.states.state1 & ~state;
		while (column != NULL)
		{
			Style *style = (Style *) TreeItemColumn_GetStyle(tree, column);
			if (style != NULL)
			{
				cMask = 0;
				for (i = 0; i < style->numElements; i++)
				{
					eLink = &style->elements[i];
					args.elem = eLink->elem;
					eMask = (*args.elem->typePtr->stateProc)(&args);
					if (eMask & CS_LAYOUT)
						eLink->neededWidth = eLink->neededHeight = -1;
					cMask |= eMask;
					/* Instance element */
					if (eLink->elem->master != NULL)
						(*args.elem->typePtr->undefProc)(&args);
				}
				if (cMask & CS_LAYOUT)
				{
					style->neededWidth = style->neededHeight = -1;
					Tree_InvalidateColumnWidth(tree, columnIndex);
					TreeItemColumn_InvalidateSize(tree, column);
				}
				iMask |= cMask;
			}
			columnIndex++;
			column = TreeItemColumn_GetNext(tree, column);
		}
		if (iMask & CS_LAYOUT)
		{
			TreeItem_InvalidateHeight(tree, item);
			updateDInfo = TRUE;
		}
		if (iMask & CS_DISPLAY)
			Tree_InvalidateItemDInfo(tree, item, NULL);
		TreeItem_Undefine(tree, item, state);
		hPtr = Tcl_NextHashEntry(&search);
	}
	if (updateDInfo)
		Tree_DInfoChanged(tree, DINFO_REDO_RANGES);

	hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
	while (hPtr != NULL)
	{
		args.elem = (Element *) Tcl_GetHashValue(hPtr);
		(*args.elem->typePtr->undefProc)(&args);
		hPtr = Tcl_NextHashEntry(&search);
	}
}

int TreeStyle_NumElements(TreeCtrl *tree, TreeStyle style_)
{
	return ((Style *) style_)->numElements;
}

int TreeStyle_Init(Tcl_Interp *interp)
{
	ElementType *typePtr;

	elementTypeList = &elemTypeBitmap;
	elemTypeBitmap.next = &elemTypeBorder;
	elemTypeBorder.next = &elemTypeImage;
	elemTypeImage.next = &elemTypeRect;
	elemTypeRect.next = &elemTypeText;
	elemTypeText.next = NULL;

	for (typePtr = elementTypeList;
		typePtr != NULL;
		typePtr = typePtr->next)
	{
		typePtr->optionTable = Tk_CreateOptionTable(interp,
			typePtr->optionSpecs);
	}

	return TCL_OK;
}

void TreeStyle_Free(TreeCtrl *tree)
{
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	Element *elem;
	Style *style;

	while (1)
	{
		hPtr = Tcl_FirstHashEntry(&tree->styleHash, &search);
		if (hPtr == NULL)
			break;
		style = (Style *) Tcl_GetHashValue(hPtr);
		TreeStyle_FreeResources(tree, (TreeStyle) style);
	}

	while (1)
	{
		hPtr = Tcl_FirstHashEntry(&tree->elementHash, &search);
		if (hPtr == NULL)
			break;
		elem = (Element *) Tcl_GetHashValue(hPtr);
		Element_FreeResources(tree, elem);
	}

	Tcl_DeleteHashTable(&tree->elementHash);
	Tcl_DeleteHashTable(&tree->styleHash);
}


