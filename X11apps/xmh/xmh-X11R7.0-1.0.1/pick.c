/*
 * $XConsortium: pick.c,v 2.45 91/07/17 21:26:54 converse Exp $
 *
 *
 *			  COPYRIGHT 1987
 *		   DIGITAL EQUIPMENT CORPORATION
 *		       MAYNARD, MASSACHUSETTS
 *			ALL RIGHTS RESERVED.
 *
 * THE INFORMATION IN THIS SOFTWARE IS SUBJECT TO CHANGE WITHOUT NOTICE AND
 * SHOULD NOT BE CONSTRUED AS A COMMITMENT BY DIGITAL EQUIPMENT CORPORATION.
 * DIGITAL MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THIS SOFTWARE FOR
 * ANY PURPOSE.  IT IS SUPPLIED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 *
 * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT
 * RIGHTS, APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN
 * ADDITION TO THAT SET FORTH ABOVE.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Digital Equipment Corporation not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 */
/* $XFree86: xc/programs/xmh/pick.c,v 1.2 2001/08/01 00:45:06 tsi Exp $ */

/* pick.c -- handle a pick subwidget. */

#include "xmh.h"

#define WTlabel		labelWidgetClass
#define WTbutton	commandWidgetClass
#define WTtextentry	asciiTextWidgetClass

#define	RTfrom		0
#define	RTto		1
#define	RTcc		2
#define RTdate		3
#define	RTsubject	4
#define	RTsearch	5
#define	RTother		6
#define	RTignore	7

#define FIRSTROWTYPE		RTfrom
#define LASTUSEFULROWTYPE	RTother
#define NUMROWTYPE		(RTignore+1)

static int stdwidth = -1;	/* Width to make text fields, and other
				   things that want to be the same width as
				   text fields. */

static char *TypeName[NUMROWTYPE];
static short true_data = 1;	/* radio data */
static short false_data = 0;	/* radio data */

typedef struct {
   WidgetClass	type;		/* Encode what type of Widget this is. */
   Widget 	widget;		/* The widget id itself. */
   struct _RowListRec *row;	/* Which row this widget is in. */
} FormEntryRec, *FormEntry;

typedef struct _RowListRec {
   short	type;		/* Encode what type of list this is. */
   Widget	widget;		/* Widget containing this row */
   short	numwidgets;	/* How many widgets in this list. */
   FormEntry 	*wlist;		/* List of widgets. */
   struct _GroupRec *group;	/* Which group this is in. */
} RowListRec, *RowList;

typedef struct _GroupRec {
   short	 numrows;	/* How many rows of widget. */
   Widget	widget;		/* Widget containing this group */
   RowList	*rlist;		/* List of widget rows. */
   struct _FormBoxRec *form;	/* Which form this is in. */
} GroupRec, *Group;

typedef struct _FormBoxRec {
   Widget outer;	/* Outer widget (contains scrollbars if any) */
   Widget inner;	/* Inner widget (contains master form) */
   short numgroups;	/* How many groups of form entries we have. */
   Group *glist;	/* List of form groups. */
   struct _PickRec *pick; /* Which pick this is in. */
} FormBoxRec, *FormBox;

typedef struct _PickRec {
   Scrn scrn;			/* Scrn containing this pick. */
   Widget label;		/* Widget with label for this pick. */
   Toc toc;			/* Toc for folder being scanned. */
   FormBox general;		/* Form for general info about this pick. */
   FormBox details;		/* Form for details about this pick. */
} PickRec;


static FormEntry CreateWidget(RowList, WidgetClass, ArgList, Cardinal);
static void DeleteWidget(FormEntry);
static void AddDetailGroup(FormBox);

void InitPick(void)
{
    TypeName[RTfrom]	= "From:";
    TypeName[RTto]	= "To:";
    TypeName[RTcc]	= "Cc:";
    TypeName[RTdate]	= "Date:";
    TypeName[RTsubject] = "Subject:";
    TypeName[RTsearch]	= "Search:";
    TypeName[RTother]	= NULL;

    /* Translations which will prevent the Search and Replace functionality
     * of the Text widget in text fields of pick and popups.  The toc's
     * Search and Replace functionality is removed in the app defaults file.
     */
    NoTextSearchAndReplace = XtParseTranslationTable
	("Ctrl<Key>R: no-op(RingBell)\n\
          Ctrl<Key>S: no-op(RingBell)\n");
}


static void PrepareToUpdate(FormBox form)
{
    XawFormDoLayout(form->inner, FALSE);
}

static void ExecuteUpdate(FormBox form)
{
    XawFormDoLayout(form->inner, TRUE);
    XtManageChild(form->inner);
    XtManageChild(form->outer);
}

static void AddLabel(RowList row, char *text, int usestd)
{
    static Arg arglist[] = {
	{XtNlabel, (XtArgVal)NULL},
	{XtNborderWidth, (XtArgVal) 0},
	{XtNjustify, (XtArgVal) XtJustifyRight},
	{XtNwidth, (XtArgVal) NULL}
    };

    arglist[0].value = (XtArgVal) text;
    arglist[XtNumber(arglist) - 1].value = (XtArgVal) stdwidth;
    (void) CreateWidget(row, WTlabel, arglist,
			usestd ? XtNumber(arglist) : XtNumber(arglist) - 1);
}


static void AddButton(RowList row, char *text, void (*func)(XMH_CB_ARGS))
{
    FormEntry entry;
    static Arg args[] = {
	{XtNlabel, (XtArgVal)NULL},
    };

    args[0].value = (XtArgVal)text;
    entry = CreateWidget( row, WTbutton, args, XtNumber(args) );
    XtAddCallback( entry->widget, XtNcallback, func, (XtPointer)entry );
}


static void AddToggle(
    RowList	row,
    char	*text,
    int		initial_state,
    Widget	radio_group,
    XtPointer	radio_data)
{
    FormEntry	entry;
    Arg		args[4];
    int		n = 0;
    XtTranslations	translations;

    XtSetArg(args[n], XtNlabel, text);			n++;
    XtSetArg(args[n], XtNstate, initial_state);		n++;
    XtSetArg(args[n], XtNradioGroup, radio_group);	n++;
    XtSetArg(args[n], XtNradioData, radio_data);	n++;
    entry = CreateWidget(row, toggleWidgetClass, args, (Cardinal)n);
    translations = XtParseTranslationTable("<Btn1Down>,<Btn1Up>:set()\n");
    XtOverrideTranslations(entry->widget, translations);
}


static void AddTextEntry(RowList row, char *str)
{
    FormEntry	entry;
    static Arg arglist[] = {
	{XtNstring, (XtArgVal) NULL},
	{XtNwidth, (XtArgVal) NULL},
	{XtNlength, (XtArgVal) 300},
	{XtNresize, (XtArgVal) XawtextResizeBoth},
	{XtNeditType, (XtArgVal)XawtextEdit},
    };
    arglist[0].value = (XtArgVal) str;
    arglist[1].value = (XtArgVal) stdwidth;
    entry = CreateWidget( row, WTtextentry, arglist, XtNumber(arglist) );
    XtOverrideTranslations(entry->widget, NoTextSearchAndReplace);
}


static void ChangeTextEntry(FormEntry entry, char *str)
{
    Arg arglist[1];
    char *ptr;

    XtSetArg(arglist[0], XtNstring, &ptr);
    XtGetValues(entry->widget, arglist, (Cardinal) 1);
    if (strcmp(str, ptr) == 0) 
        return;

    XtSetArg(arglist[0], XtNstring, str);
    XtSetValues(entry->widget, arglist, (Cardinal) 1);
}

/* ARGSUSED */
static void ExecRowOr(
    Widget w,			/* unused */
    XtPointer closure,		/* FormEntry */
    XtPointer call_data)	/* unused */
{
    FormEntry entry = (FormEntry)closure;
    RowList row = entry->row;
    FormBox form = row->group->form;
    PrepareToUpdate(form);
    DeleteWidget(entry);
    AddLabel(row, "or", FALSE);
    AddTextEntry(row, "");
    AddButton(row, "Or", ExecRowOr);
    ExecuteUpdate(form);
}
    

/* ARGSUSED */
static void ExecGroupOr(
    Widget w,			/* unused */
    XtPointer closure,		/* FormEntry */
    XtPointer call_data)	/* unused */
{
    FormBox form = ((FormEntry)closure)->row->group->form;
/* %%%    XUnmapWindow(theDisplay, XtWindow(form->inner)); */
    PrepareToUpdate(form);
    AddDetailGroup(form);
    ExecuteUpdate(form);
/* %%%    XtMapWidget(form->inner); */
}

static char **argv;
static int argvsize;


static void AppendArgv(char *ptr)
{
    argvsize++;
    argv = ResizeArgv(argv, argvsize);
    argv[argvsize - 1] = XtNewString(ptr);
}

static void EraseLast(void)
{
    argvsize--;
    XtFree((char *) argv[argvsize]);
    argv[argvsize] = 0;
}



static Boolean ParseRow(RowList row)
{
    int		result = FALSE;
    int		i;
    FormEntry	entry;
    char	str[1000];
    Arg	  	args[4];
    char	*ptr;
    char	*other;

    if (row->type > LASTUSEFULROWTYPE)
	return FALSE;

    for (i = 3; i < row->numwidgets; i += 2) {
	entry = row->wlist[i];
	XtSetArg(args[0], XtNstring, &ptr);
	XtGetValues(entry->widget, args, (Cardinal) 1);
	if (ptr && *ptr) {
	    if (!result) {
		result = TRUE;
		if (! (*((short *)
			 (XawToggleGetCurrent(row->wlist[0]->widget)))))
		    AppendArgv("-not");
		AppendArgv("-lbrace");
	    }
	    switch (row->type) {
		case RTfrom: 
		    AppendArgv("-from");
		    break;
		case RTto: 
		    AppendArgv("-to");
		    break;
		case RTcc: 
		    AppendArgv("-cc");
		    break;
		case RTdate: 
		    AppendArgv("-date");
		    break;
		case RTsubject: 
		    AppendArgv("-subject");
		    break;
		case RTsearch: 
		    AppendArgv("-search");
		    break;
		case RTother: 
		    XtSetArg(args[0], XtNstring, &other);
		    XtGetValues(row->wlist[2]->widget, args, (Cardinal) 1);
		    (void) sprintf(str, "--%s", other);
		    AppendArgv(str);
		    break;
	    }
	    AppendArgv(ptr);
	    AppendArgv("-or");
	}
    }
    if (result) {
	EraseLast();
	AppendArgv("-rbrace");
	AppendArgv("-and");
    }
    return result;
}
	    

static Boolean ParseGroup(Group group)
{
    int found = FALSE;
    int i;
    for (i=0 ; i<group->numrows ; i++)
	found |= ParseRow(group->rlist[i]);
    if (found) {
	EraseLast();
	AppendArgv("-rbrace");
	AppendArgv("-or");
	AppendArgv("-lbrace");
    }
    return found;
}

/* ARGSUSED */
static void ExecOK(
    Widget w,			/* unused */
    XtPointer closure,		/* FormEntry */
    XtPointer call_data)	/* unused */
{
    Pick pick = ((FormEntry)closure)->row->group->form->pick;
    Toc toc = pick->toc;
    FormBox details = pick->details;
    FormBox general = pick->general;
    Group group = general->glist[0];
    RowList row0 = group->rlist[0];
    RowList row1 = group->rlist[1];
    RowList row2 = group->rlist[2];
    char *fromseq;
    char *toseq;
    char *datefield;
    char *fromdate;
    char *todate;
    short removeoldmsgs =
	*((short*)XawToggleGetCurrent(row2->wlist[1]->widget));
    int i, found;
    char *folderpath;
    int cmd_status;
    Arg args[5];

    XtSetArg(args[0], XtNstring, &toseq);
    XtGetValues(row0->wlist[1]->widget, args, (Cardinal) 1);
    if (strcmp(toseq, "all") == 0) {
	PopupError(pick->scrn->parent,
		   "Can't create a sequence called \"all\".");
	return;
    }
    XtSetArg(args[0], XtNstring, &fromseq);
    XtGetValues(row0->wlist[3]->widget, args, (Cardinal) 1);
    if (TocGetSeqNamed(toc, fromseq) == NULL) {
	char str[200];
	(void) sprintf(str, "Sequence \"%s\" doesn't exist!", fromseq);
	PopupError(pick->scrn->parent, str);
	return;
    }

    argv = MakeArgv(1);
    argvsize = 0;
    AppendArgv("pick");
    AppendArgv(folderpath = TocMakeFolderName(toc));
    XtFree(folderpath);
    AppendArgv(fromseq);
    AppendArgv("-sequence");
    AppendArgv(toseq);
    if (removeoldmsgs)
	AppendArgv("-zero");
    else
	AppendArgv("-nozero");
    XtSetArg(args[0], XtNstring, &datefield);
    XtGetValues(row1->wlist[5]->widget, args, (Cardinal) 1);
    if (*datefield) {
	AppendArgv("-datefield");
	AppendArgv(datefield);
    }
    XtSetArg(args[0], XtNstring, &fromdate);
    XtGetValues(row1->wlist[1]->widget, args, (Cardinal) 1);
    if (*fromdate) {
	AppendArgv("-after");
	AppendArgv(fromdate);
	AppendArgv("-and");
    }
    XtSetArg(args[0], XtNstring, &todate);
    XtGetValues(row1->wlist[3]->widget, args, (Cardinal) 1);
    if (*todate) {
	AppendArgv("-before");
	AppendArgv(todate);
	AppendArgv("-and");
    }
    found = FALSE;
    AppendArgv("-lbrace");
    AppendArgv("-lbrace");
    for (i=0 ; i<details->numgroups ; i++)
	found |= ParseGroup(details->glist[i]);
    EraseLast();
    EraseLast();
    if (found) AppendArgv("-rbrace");
    else if (*fromdate || *todate) EraseLast();
    if (app_resources.debug) {
	for (i=0 ; i<argvsize ; i++)
	    (void) fprintf(stderr, "%s ", argv[i]);
	(void) fprintf(stderr, "\n");
    }
    if (app_resources.block_events_on_busy) ShowBusyCursor();

    cmd_status = DoCommand(argv, (char*)NULL, (char*)NULL);
    TocReloadSeqLists(toc);
    TocChangeViewedSeq(toc, TocGetSeqNamed(toc, toseq));

    if (app_resources.block_events_on_busy) UnshowBusyCursor();
    if (cmd_status == 0 /*succeeded*/) DestroyScrn(pick->scrn);
    for (i=0 ; i<argvsize ; i++) XtFree((char *) argv[i]);
    XtFree((char *) argv);
}


/* ARGSUSED */
static void ExecCancel(
    Widget w,			/* unused */
    XtPointer closure,		/* FormEntry */
    XtPointer call_data)	/* unused */
{
    Pick pick = ((FormEntry)closure)->row->group->form->pick;
    Scrn scrn = pick->scrn;
    (void) DestroyScrn(scrn);
}


static FormEntry CreateWidget(
    RowList row,
    WidgetClass class,
    ArgList args,
    Cardinal num_args)
{
    static Arg arglist[] = {
	{XtNfromHoriz, (XtArgVal)NULL},
	{XtNresizable, (XtArgVal)TRUE},
	{XtNtop, (XtArgVal) XtChainTop},
	{XtNleft, (XtArgVal) XtChainLeft},
	{XtNbottom, (XtArgVal) XtChainTop},
	{XtNright, (XtArgVal) XtChainLeft},
    };
    ArgList merged_args;
    FormEntry entry;

    row->numwidgets++;
    row->wlist = (FormEntry *)
	XtRealloc((char *) row->wlist,
		  (unsigned) row->numwidgets * sizeof(FormEntry));
    entry = XtNew(FormEntryRec);
    entry->row = row;
    entry->type = class;
    row->wlist[row->numwidgets - 1] = entry;
    if (row->numwidgets > 1)
	arglist[0].value = (XtArgVal) row->wlist[row->numwidgets - 2]->widget;
    else
	arglist[0].value = (XtArgVal) NULL;

    merged_args = XtMergeArgLists(args, num_args, arglist, XtNumber(arglist) );

    entry->widget = XtCreateManagedWidget( (String) NULL, class, row->widget,
					   merged_args,
					   num_args + XtNumber(arglist) );
			
    XtFree((char *) merged_args);
    return entry;
}
    

static void
DeleteWidget(FormEntry entry)
{
    RowList row = entry->row;
    int i;
    XtDestroyWidget(entry->widget);
    for (i = 0; i < row->numwidgets; i++)
	if (row->wlist[i] == entry)
	    break;
    row->numwidgets--;
    for (; i < row->numwidgets; i++)
	row->wlist[i] = row->wlist[i + 1];
}


/* Figure out how wide text fields and labels should be so that they'll all
   line up correctly, and be big enough to hold everything, but not too big. */

static void FindStdWidth(void)
{
    stdwidth = 100;		/* %%% HACK! */
}


static RowList AddRow(Group group, int type)
{
    static Arg arglist[] = {
	{XtNborderWidth, (XtArgVal) 0},
	{XtNfromVert, (XtArgVal) NULL},
	{XtNresizable, (XtArgVal) TRUE},
	{XtNtop, (XtArgVal) XtChainTop},
	{XtNleft, (XtArgVal) XtChainLeft},
	{XtNbottom, (XtArgVal) XtChainTop},
	{XtNright, (XtArgVal) XtChainLeft}
    };
    RowList row;
    group->numrows++;
    group->rlist = (RowList *)
	XtRealloc((char *) group->rlist,
		  (unsigned) group->numrows * sizeof(RowList));
    group->rlist[group->numrows - 1] = row = XtNew(RowListRec);
    row->type = type;
    row->numwidgets = 0;
    row->wlist = (FormEntry *) NULL;
    row->group = group;
    if (group->numrows > 1)
	arglist[1].value = (XtArgVal)group->rlist[group->numrows - 2]->widget;
    else
	arglist[1].value = (XtArgVal) NULL;
    row->widget = XtCreateWidget("rowform", formWidgetClass, group->widget,
				  arglist, XtNumber(arglist) );
    if (type == RTignore) return row;
    AddToggle(row, "Pick", TRUE, (Widget)NULL, (XtPointer)(&true_data));
    AddToggle(row, "Skip", FALSE, row->wlist[row->numwidgets - 1]->widget,
	      (XtPointer)(&false_data));
    if (TypeName[type])
	AddLabel(row, TypeName[type], TRUE);
    else
	AddTextEntry(row, "");
    AddTextEntry(row, "");
    AddButton(row, "Or", ExecRowOr);
    XtManageChild(row->widget);
    return row;
}


static Group AddGroup(FormBox form)
{
    static Arg arglist[] = {
	{XtNborderWidth, (XtArgVal) 0},
	{XtNfromVert, (XtArgVal) NULL},
	{XtNresizable, (XtArgVal) TRUE},
	{XtNtop, (XtArgVal) XtChainTop},
	{XtNleft, (XtArgVal) XtChainLeft},
	{XtNbottom, (XtArgVal) XtChainTop},
	{XtNright, (XtArgVal) XtChainLeft}
    };
    Group group;
    form->numgroups++;
    form->glist = (Group *)
	XtRealloc((char *) form->glist,
		  (unsigned) form->numgroups * sizeof(Group));
    form->glist[form->numgroups - 1] = group =
	(Group) XtMalloc((Cardinal) sizeof(GroupRec));
    group->numrows = 0;
    group->form = form;
    group->rlist = (RowList *) NULL;
    if (form->numgroups > 1)
	arglist[1].value = (XtArgVal)form->glist[form->numgroups - 2]->widget;
    else
	arglist[1].value = (XtArgVal)NULL;
    group->widget = XtCreateWidget("groupform", formWidgetClass, form->inner,
				    arglist, XtNumber(arglist) );
    return group;
}



static void
AddDetailGroup(FormBox form)
{
    Group group;
    RowList row;
    int     type;

    if (form->numgroups > 0) {
	group = form->glist[form->numgroups - 1];
	row = group->rlist[group->numrows - 1];
	DeleteWidget(row->wlist[0]);
	AddLabel(row, "- or -", FALSE);
    }
    group = AddGroup(form);
    for (type = FIRSTROWTYPE; type <= LASTUSEFULROWTYPE; type++)
	(void) AddRow(group, type);
    row =  AddRow(group, RTignore);
    AddButton(row, "- Or -", ExecGroupOr);
    XtManageChild(row->widget);
    if (XtIsRealized(XtParent(group->widget)))
	XtRealizeWidget(group->widget); 
    XtManageChild(group->widget);
}


static void AddGeneralGroup(FormBox form)
{
    Group group;
    RowList row;
    Widget widgetList[4];
    group = AddGroup(form);
    row =  AddRow(group, RTignore);
    widgetList[0] = row->widget;
    AddLabel(row, "Creating sequence:", FALSE);
    AddTextEntry(row, "");
    AddLabel(row, "with msgs from sequence:", FALSE);
    AddTextEntry(row, "");
    row =  AddRow(group, RTignore);
    widgetList[1] = row->widget;
    AddLabel(row, "Date range:", FALSE);
    AddTextEntry(row, "");
    AddLabel(row, " - ", FALSE);
    AddTextEntry(row, "");
    AddLabel(row, "Date field:", FALSE);
    AddTextEntry(row, "");
    row =  AddRow(group, RTignore);
    widgetList[2] = row->widget;
    AddLabel(row, "Clear old entries from sequence?", FALSE);
    AddToggle(row, "Yes", TRUE, (Widget) NULL, (XtPointer)(&true_data));
    AddToggle(row, "No", FALSE, row->wlist[row->numwidgets - 1]->widget,
	      (XtPointer)(&false_data));
    row =  AddRow(group, RTignore);
    widgetList[3] = row->widget;
    AddButton(row, "OK", ExecOK);
    AddButton(row, "Cancel", ExecCancel);
    XtManageChildren(widgetList, XtNumber(widgetList));
    XtManageChild(group->widget);
}


static void InitGeneral(Pick pick, char *fromseq, char *toseq)
{
    RowList row;
    row = pick->general->glist[0]->rlist[0];
    ChangeTextEntry(row->wlist[1], toseq);
    ChangeTextEntry(row->wlist[3], fromseq);
}


static void CleanForm(FormBox form)
{
    int i, j, k;
    Group group;
    RowList row;
    FormEntry entry;
    for (i=0 ; i<form->numgroups ; i++) {
	group = form->glist[i];
	for (j=0 ; j<group->numrows ; j++) {
	    row = group->rlist[j];
	    for (k=0 ; k<row->numwidgets ; k++) {
		entry = row->wlist[k];
		if (entry->type == WTtextentry)
		    ChangeTextEntry(entry, "");
	    }
	}
    }
}


static FormBox MakeAForm(Pick pick)
{
    static Arg arglist1[] = {
	{XtNallowHoriz, (XtArgVal)TRUE},
	{XtNallowVert, (XtArgVal)TRUE},
/* %%%	{XtNallowResize, (XtArgVal)TRUE}, */
	{XtNmin, (XtArgVal)50},
	{XtNmax, (XtArgVal)1500}
    };
    static Arg arglist2[] = {
	{XtNborderWidth, (XtArgVal) 0}
    };
    FormBox result;
    result = (FormBox) XtMalloc((Cardinal) sizeof(FormBoxRec));
    result->outer = XtCreateWidget( "pick", viewportWidgetClass,
				    pick->scrn->widget,
				    arglist1, XtNumber(arglist1) );
    result->inner = XtCreateWidget( "form", formWidgetClass, result->outer,
				    arglist2, XtNumber(arglist2) );
    result->pick = pick;
    result->numgroups = 0;
    result->glist = (Group *) NULL;
    return result;
}


void AddPick(Scrn scrn, Toc toc, char *fromseq, char *toseq)
{
    Pick pick;
    FormBox general, details;
    char str[100];
#ifdef notdef
    int height;
#endif /* notdef */

    if (scrn->pick) {
	pick = scrn->pick;
	CleanForm(pick->details);
	CleanForm(pick->general);
    } else {
	pick = scrn->pick = (Pick) XtMalloc((Cardinal) sizeof(PickRec));
	pick->scrn = scrn;
	pick->label = CreateTitleBar(scrn, "pickTitlebar");
	pick->details = details = MakeAForm(pick);
	pick->general = general = MakeAForm(pick);
	FindStdWidth();
	XawPanedSetRefigureMode(scrn->widget, False);
	PrepareToUpdate(details);
	AddDetailGroup(details);
	ExecuteUpdate(details);
	PrepareToUpdate(general);
	AddGeneralGroup(general);
	ExecuteUpdate(general);
#ifdef notdef
	height = general->inner->core.height;
	if (general->inner->core.width > scrn->widget->core.width)
	    height += XtScrollMgrGetThickness(general->outer);
	XawPanedSetMinMax(scrn->widget, general->outer, height, height);
	XawPanedSetMinMax(scrn->widget, general->outer, 10, 10000);
#endif /* notdef */
	XawPanedSetRefigureMode(scrn->widget, True);
    }
    pick->toc = toc;
    InitGeneral(pick, fromseq, toseq);
    (void) sprintf(str, "Pick: %s", TocName(toc));
    ChangeLabel(pick->label, str);
    StoreWindowName(scrn, str);
}
