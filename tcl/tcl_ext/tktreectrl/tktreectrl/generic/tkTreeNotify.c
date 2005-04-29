#include "tkTreeCtrl.h"

/* tkMacOSXPort.h should include this, I think, like the other platform
 * tk*Port.h files. Needed for TclFormatInt() */
#if defined(MAC_OSX_TK)
#include "tclInt.h"
#endif

static int EVENT_EXPAND,
	DETAIL_EXPAND_BEFORE,
	DETAIL_EXPAND_AFTER;
static int EVENT_COLLAPSE,
	DETAIL_COLLAPSE_BEFORE,
	DETAIL_COLLAPSE_AFTER;
static int EVENT_SELECTION;
static int EVENT_ACTIVEITEM;
static int EVENT_SCROLL,
	DETAIL_SCROLL_X,
	DETAIL_SCROLL_Y;

/*
 * %-substitution for any event
 */
static void Percents_Any(QE_ExpandArgs *args)
{
	struct {
		TreeCtrl *tree;
	} *data = args->clientData;

	switch (args->which)
	{
		case 'd': /* detail */
			QE_ExpandDetail(args->bindingTable, args->event, args->detail,
				args->result);
			break;

		case 'e': /* event */
			QE_ExpandEvent(args->bindingTable, args->event, args->result);
			break;

		case 'W': /* object */
			QE_ExpandString((char *) args->object, args->result);
			break;

		case 'T': /* tree */
			QE_ExpandString(Tk_PathName(data->tree->tkwin), args->result);
			break;

		default:
			QE_ExpandUnknown(args->which, args->result);
			break;
	}
}

static void Percents_Expand(QE_ExpandArgs *args)
{
	struct {
		TreeCtrl *tree;
		int id;
	} *data = args->clientData;

	switch (args->which)
	{
		case 'I':
			QE_ExpandNumber(data->id, args->result);
			break;

		default:
			Percents_Any(args);
			break;
	}
}

static void Percents_Selection(QE_ExpandArgs *args)
{
	struct {
		TreeCtrl *tree;
		int *select;
		int *deselect;
		int count;
	} *data = args->clientData;
	int *items = NULL;
	int i = 0;
	char string[TCL_INTEGER_SPACE];

	switch (args->which)
	{
		case 'c':
			QE_ExpandNumber(data->count, args->result);
			break;
		case 'D':
		case 'S':
			items = (args->which == 'D') ? data->deselect : data->select;
			if (items == NULL)
			{
				Tcl_DStringAppend(args->result, "{}", 2);
				break;
			}
			Tcl_DStringStartSublist(args->result);
			while (items[i] != -1)
			{
				TclFormatInt(string, items[i]);
				Tcl_DStringAppendElement(args->result, string);
				i++;
			}
			Tcl_DStringEndSublist(args->result);
			break;

		default:
			Percents_Any(args);
			break;
	}
}

static void Percents_ActiveItem(QE_ExpandArgs *args)
{
	struct {
		TreeCtrl *tree;
		int prev;
		int current;
	} *data = args->clientData;

	switch (args->which)
	{
		case 'c':
			QE_ExpandNumber(data->current, args->result);
			break;

		case 'p':
			QE_ExpandNumber(data->prev, args->result);
			break;

		default:
			Percents_Any(args);
			break;
	}
}

static void Percents_Scroll(QE_ExpandArgs *args)
{
	struct {
		TreeCtrl *tree;
		double lower;
		double upper;
	} *data = args->clientData;

	switch (args->which)
	{
		case 'l':
			QE_ExpandDouble(data->lower, args->result);
			break;

		case 'u':
			QE_ExpandDouble(data->upper, args->result);
			break;

		default:
			Percents_Any(args);
			break;
	}
}

int TreeNotifyCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	Tcl_Obj *CONST objv[])
{
	TreeCtrl *tree = (TreeCtrl *) clientData;
	static CONST char *commandName[] = { "bind", "configure", "detailnames",
		"eventnames", "generate", "install", "linkage", "uninstall",
		(char *) NULL };
	enum {
		COMMAND_BIND, COMMAND_CONFIGURE, COMMAND_DETAILNAMES,
		COMMAND_EVENTNAMES, COMMAND_GENERATE, COMMAND_INSTALL,
		COMMAND_LINKAGE, COMMAND_UNINSTALL
	};
	int index;

	if (objc < 3)
	{
		Tcl_WrongNumArgs(interp, 2, objv, "command ?arg arg...?");
		return TCL_ERROR;
	}

	if (Tcl_GetIndexFromObj(interp, objv[2], commandName, "command", 0,
		&index) != TCL_OK)
	{
		return TCL_ERROR;
	}

	switch (index)
	{
		case COMMAND_BIND:
		{
			return QE_BindCmd(tree->bindingTable, 2, objc, objv);
		}

		case COMMAND_CONFIGURE:
		{
			return QE_ConfigureCmd(tree->bindingTable, 2, objc, objv);
		}

		/* T notify detailnames $eventName */
		case COMMAND_DETAILNAMES:
		{
			char *eventName;

			if (objc != 4)
			{
				Tcl_WrongNumArgs(interp, 3, objv, "eventName");
				return TCL_ERROR;
			}
			eventName = Tcl_GetString(objv[3]);
			return QE_GetDetailNames(tree->bindingTable, eventName);
		}

		/* T notify eventnames */
		case COMMAND_EVENTNAMES:
		{
			if (objc != 3)
			{
				Tcl_WrongNumArgs(interp, 3, objv, (char *) NULL);
				return TCL_ERROR;
			}
			return QE_GetEventNames(tree->bindingTable);
		}

		case COMMAND_GENERATE:
		{
			return QE_GenerateCmd(tree->bindingTable, 2, objc, objv);
		}

		case COMMAND_INSTALL:
		{
			return QE_InstallCmd(tree->bindingTable, 2, objc, objv);
		}

		case COMMAND_LINKAGE:
		{
			return QE_LinkageCmd(tree->bindingTable, 2, objc, objv);
		}

		case COMMAND_UNINSTALL:
		{
			return QE_UninstallCmd(tree->bindingTable, 2, objc, objv);
		}
	}

	return TCL_OK;
}

void TreeNotify_OpenClose(TreeCtrl *tree, TreeItem item, int state, int before)
{
	QE_Event event;
	struct {
		TreeCtrl *tree; /* Must be first*/
		int id;
	} data;

	data.tree = tree;
	data.id = TreeItem_GetID(tree, item);

	if (state & STATE_OPEN)
	{
		event.type = EVENT_EXPAND;
		event.detail = before ? DETAIL_EXPAND_BEFORE : DETAIL_EXPAND_AFTER;
	}
	else
	{
		event.type = EVENT_COLLAPSE;
		event.detail = before ? DETAIL_COLLAPSE_BEFORE : DETAIL_COLLAPSE_AFTER;
	}
	event.clientData = (ClientData) &data;
	(void) QE_BindEvent(tree->bindingTable, &event);
}

void TreeNotify_Selection(TreeCtrl *tree, TreeItem select[], TreeItem deselect[])
{
	QE_Event event;
	struct {
		TreeCtrl *tree; /* Must be first*/
		int *select;
		int *deselect;
		int count;
	} data;
	int staticS[20], staticD[20];
	int i;

	data.tree = tree;

	if (select == NULL)
		data.select = NULL;
	else
	{
		for (i = 0; select[i] != NULL; i++)
			/* nothing */;
		if (i < sizeof(staticS) / sizeof(staticS[0]))
			data.select = staticS;
		else
			data.select = (int *) ckalloc(sizeof(int) * (i + 1));
		for (i = 0; select[i] != NULL; i++)
			data.select[i] = TreeItem_GetID(tree, select[i]);
		data.select[i] = -1;
	}

	if (deselect == NULL)
		data.deselect = NULL;
	else
	{
		for (i = 0; deselect[i] != NULL; i++)
			/* nothing */;
		if (i < sizeof(staticD) / sizeof(staticD[0]))
			data.deselect = staticD;
		else
			data.deselect = (int *) ckalloc(sizeof(int) * (i + 1));
		for (i = 0; deselect[i] != NULL; i++)
			data.deselect[i] = TreeItem_GetID(tree, deselect[i]);
		data.deselect[i] = -1;
	}

	data.count = tree->selectCount;

	event.type = EVENT_SELECTION;
	event.detail = 0;
	event.clientData = (ClientData) &data;

	(void) QE_BindEvent(tree->bindingTable, &event);

	if ((select != NULL) && (data.select != staticS))
		ckfree((char *) data.select);
	if ((deselect != NULL) && (data.deselect != staticD))
		ckfree((char *) data.deselect);
}

void TreeNotify_ActiveItem(TreeCtrl *tree, TreeItem itemPrev, TreeItem itemCur)
{
	QE_Event event;
	struct {
		TreeCtrl *tree; /* Must be first*/
		int prev;
		int current;
	} data;

	data.tree = tree;
	data.prev = TreeItem_GetID(tree, itemPrev);
	data.current = TreeItem_GetID(tree, itemCur);

	event.type = EVENT_ACTIVEITEM;
	event.detail = 0;
	event.clientData = (ClientData) &data;

	(void) QE_BindEvent(tree->bindingTable, &event);
}

void TreeNotify_Scroll(TreeCtrl *tree, double fractions[2], int vertical)
{
	QE_Event event;
	struct {
		TreeCtrl *tree; /* Must be first*/
		double lower;
		double upper;
	} data;

	data.tree = tree;
	data.lower = fractions[0];
	data.upper = fractions[1];

	event.type = EVENT_SCROLL;
	event.detail = vertical ? DETAIL_SCROLL_Y : DETAIL_SCROLL_X;
	event.clientData = (ClientData) &data;

	(void) QE_BindEvent(tree->bindingTable, &event);
}

int TreeNotify_Init(TreeCtrl *tree)
{
	tree->bindingTable = QE_CreateBindingTable(tree->interp);

	EVENT_EXPAND = QE_InstallEvent(tree->bindingTable, "Expand", Percents_Expand);
	DETAIL_EXPAND_BEFORE = QE_InstallDetail(tree->bindingTable, "before", EVENT_EXPAND, NULL);
	DETAIL_EXPAND_AFTER = QE_InstallDetail(tree->bindingTable, "after", EVENT_EXPAND, NULL);

	EVENT_COLLAPSE = QE_InstallEvent(tree->bindingTable, "Collapse", Percents_Expand);
	DETAIL_COLLAPSE_BEFORE = QE_InstallDetail(tree->bindingTable, "before", EVENT_COLLAPSE, NULL);
	DETAIL_COLLAPSE_AFTER = QE_InstallDetail(tree->bindingTable, "after", EVENT_COLLAPSE, NULL);

	EVENT_SELECTION = QE_InstallEvent(tree->bindingTable, "Selection", Percents_Selection);

	EVENT_ACTIVEITEM = QE_InstallEvent(tree->bindingTable, "ActiveItem", Percents_ActiveItem);

	EVENT_SCROLL = QE_InstallEvent(tree->bindingTable, "Scroll", Percents_Scroll);
	DETAIL_SCROLL_X = QE_InstallDetail(tree->bindingTable, "x", EVENT_SCROLL, NULL);
	DETAIL_SCROLL_Y = QE_InstallDetail(tree->bindingTable, "y", EVENT_SCROLL, NULL);

	return TCL_OK;
}

