/*
 * $XConsortium: bbox.c,v 2.35 91/07/10 19:34:59 converse Exp $
 *
 *
 *			COPYRIGHT 1987, 1989
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
/* $XFree86: xc/programs/xmh/bbox.c,v 1.2 2001/10/28 03:34:38 tsi Exp $ */

/* bbox.c -- management of buttons and buttonboxes. 
 *
 * This module implements a simple interface to buttonboxes, allowing a client
 * to create new buttonboxes and manage their contents. 
 */

#include "xmh.h"
#include "bboxint.h"

static XtTranslations	RadioButtonTranslations = NULL;


void BBoxInit(void)
{
    RadioButtonTranslations =
	XtParseTranslationTable("<Btn1Down>,<Btn1Up>:set()\n");
}


/*
 * Create a new button box.  The widget for it will be a child of the given
 * scrn's widget, and it will be added to the scrn's pane. 
 */

ButtonBox BBoxCreate(
    Scrn	scrn,
    char	*name)	/* name of the buttonbox widgets */
{
    Cardinal	n;
    ButtonBox	buttonbox = XtNew(ButtonBoxRec);
    Arg		args[5];

    n = 0;
    XtSetArg(args[n], XtNallowVert, True);	n++;
    XtSetArg(args[n], XtNskipAdjust, True);	n++;
    
    buttonbox->outer =
	XtCreateManagedWidget(name, viewportWidgetClass, scrn->widget,
			      args, n);
    buttonbox->inner =
	XtCreateManagedWidget(name, boxWidgetClass, buttonbox->outer,
			      args, (Cardinal) 0);
    buttonbox->numbuttons = 0;
    buttonbox->button = (Button *) NULL;
    buttonbox->scrn = scrn;
    return buttonbox;
}


ButtonBox RadioBBoxCreate(
    Scrn	scrn,
    char	*name)	/* name of the buttonbox widgets */
{
    return BBoxCreate(scrn, name);
}


/* Create a new button, and add it to a buttonbox. */

static void bboxAddButton(
    ButtonBox	buttonbox,
    char	*name,
    WidgetClass	kind,
    Boolean	enabled,
    Boolean	radio)
{
    Button	button;
    Cardinal	i;
    Widget	radio_group;
    Arg		args[5];

    buttonbox->numbuttons++;
    buttonbox->button = (Button *) 
	XtRealloc((char *) buttonbox->button,
		  (unsigned) buttonbox->numbuttons * sizeof(Button));
    button = buttonbox->button[buttonbox->numbuttons - 1] = XtNew(ButtonRec);
    button->buttonbox = buttonbox;
    button->name = XtNewString(name);
    button->menu = (Widget) NULL;

    i = 0;
    if (!enabled) {
	XtSetArg(args[i], XtNsensitive, False);		i++;
    }

    if (radio && kind == toggleWidgetClass) {
	if (buttonbox->numbuttons > 1)
	    radio_group = (button == buttonbox->button[0]) 
		? (buttonbox->button[1]->widget)
		: (buttonbox->button[0]->widget);
	else radio_group = NULL;
	XtSetArg(args[i], XtNradioGroup, radio_group);		i++;
	XtSetArg(args[i], XtNradioData, button->name);		i++;
    }

    /* Prevent the folder buttons from picking up labels from resources */

    if (buttonbox == buttonbox->scrn->folderbuttons) {
	XtSetArg(args[i], XtNlabel, button->name);	i++;
    }

    button->widget =
	XtCreateManagedWidget(name, kind, buttonbox->inner, args, i);

    if (radio)
	XtOverrideTranslations(button->widget, RadioButtonTranslations);
}


void BBoxAddButton(
    ButtonBox	buttonbox,
    char	*name,
    WidgetClass	kind,
    Boolean	enabled)
{
    bboxAddButton(buttonbox, name, kind, enabled, False);
}    


void RadioBBoxAddButton(
    ButtonBox	buttonbox,
    char	*name,
    Boolean	enabled)
{
    bboxAddButton(buttonbox, name, toggleWidgetClass, enabled, True);
}


/* Set the current button in a radio buttonbox. */

void RadioBBoxSet(
    Button button)
{
    XawToggleSetCurrent(button->widget, button->name);
}


/* Get the name of the current button in a radio buttonbox. */

char *RadioBBoxGetCurrent(
    ButtonBox buttonbox)
{
    return ((char *) XawToggleGetCurrent(buttonbox->button[0]->widget));
}


/* Remove the given button from its buttonbox, and free all resources
 * used in association with the button.  If the button was the current
 * button in a radio buttonbox, the current button becomes the first 
 * button in the box.
 */

void BBoxDeleteButton(
    Button	button)
{
    ButtonBox	buttonbox;
    int		i, found;
    
    if (button == NULL) return;
    buttonbox = button->buttonbox;
    found = False;

    for (i=0 ; i<buttonbox->numbuttons; i++) {
	if (found)
	    buttonbox->button[i-1] = buttonbox->button[i];
	else if (buttonbox->button[i] == button) {
	    found = True;
 
	    /* Free the resources used by the given button. */

	    if (button->menu != NULL && button->menu != NoMenuForButton)
		XtDestroyWidget(button->menu);
	    XtDestroyWidget(button->widget);
	    XtFree(button->name);
	    XtFree((char *) button);
	} 
    }
    if (found)
	buttonbox->numbuttons--;
}


void RadioBBoxDeleteButton(
    Button	button)
{
    ButtonBox	buttonbox;
    Boolean	reradio = False;
    char *	current;

    if (button == NULL) return;
    buttonbox = button->buttonbox;
    current = RadioBBoxGetCurrent(buttonbox);
    if (current) reradio = ! strcmp(current, button->name);
    BBoxDeleteButton(button);

    if (reradio && BBoxNumButtons(buttonbox))
	RadioBBoxSet(buttonbox->button[0]);
}


/* Enable or disable the given button widget. */

static void SendEnableMsg(
    Widget	widget,
    int		value)	/* TRUE for enable, FALSE for disable. */
{
    static Arg arglist[] = {{XtNsensitive, (XtArgVal)False}};
    arglist[0].value = (XtArgVal) value;
    XtSetValues(widget, arglist, XtNumber(arglist));
}


/* Enable the given button (if it's not already). */

void BBoxEnable(
    Button button)
{
    SendEnableMsg(button->widget, True);
}


/* Disable the given button (if it's not already). */

void BBoxDisable(
    Button button)
{
    SendEnableMsg(button->widget, False);
}


/* Given a buttonbox and a button name, find the button in the box with that
   name. */

Button BBoxFindButtonNamed(
    ButtonBox buttonbox,
    char *name)
{
    register int i;
    for (i=0 ; i<buttonbox->numbuttons; i++)
	if (strcmp(name, buttonbox->button[i]->name) == 0)
	    return buttonbox->button[i];
    return (Button) NULL;
}


/* Given a buttonbox and a widget, find the button which is that widget. */

Button BBoxFindButton(
    ButtonBox	buttonbox,
    Widget	w)
{
    register int i;
    for (i=0; i < buttonbox->numbuttons; i++)
	if (buttonbox->button[i]->widget == w)
	    return buttonbox->button[i];
    return (Button) NULL;
}


/* Return the nth button in the given buttonbox. */

Button BBoxButtonNumber(
    ButtonBox buttonbox,
    int n)
{
    return buttonbox->button[n];
}


/* Return how many buttons are in a buttonbox. */

int BBoxNumButtons(
    ButtonBox buttonbox)
{
    return buttonbox->numbuttons;
}


/* Given a button, return its name. */

char *BBoxNameOfButton(
    Button button)
{
    return button->name;
}


/* Given a button, return its menu. */

Widget	BBoxMenuOfButton(
    Button button)
{
    return button->menu;
}


/* Set maximum size for a bbox so that it cannot be resized any taller 
 * than the space needed to stack all the buttons on top of each other.
 * Allow the user to set the minimum size.
 */

void BBoxLockSize(
    ButtonBox buttonbox)
{
    Dimension	maxheight;
    Arg		args[1];

    if (buttonbox == NULL) return;
    maxheight = (Dimension) GetHeight(buttonbox->inner);
    XtSetArg(args[0], XtNmax, maxheight);	/* for Paned widget */
    XtSetValues(buttonbox->outer, args, (Cardinal) 1);
}


Boolean BBoxIsGrandparent(
    ButtonBox	buttonbox,
    Widget	widget)
{
    return (XtParent(XtParent(widget)) == buttonbox->inner);
}


void BBoxMailFlag(
    ButtonBox	buttonbox,
    char*	name,
    int		up)
{
    Arg		args[1];
    Pixel	flag;
    Button	button = BBoxFindButtonNamed(buttonbox, name);

    if (button) {
	/* avoid unnecessary exposures */
	XtSetArg(args[0], XtNleftBitmap, &flag);
	XtGetValues(button->widget, args, (Cardinal)1);
	if (up && flag != app_resources.flag_up) {
	    XtSetArg(args[0], XtNleftBitmap, app_resources.flag_up);
	    XtSetValues(button->widget, args, (Cardinal)1);
	}
	else if (!up && flag != app_resources.flag_down) {
	    XtSetArg(args[0], XtNleftBitmap, app_resources.flag_down);
	    XtSetValues(button->widget, args, (Cardinal)1);
	}
    }
}
