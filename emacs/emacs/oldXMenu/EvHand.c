#include "copyright.h"

/* $Header: /cvs/Darwin/src/live/emacs/emacs/oldXMenu/EvHand.c,v 1.1.1.3 2001/10/31 17:59:54 jevans Exp $ */
/* Copyright    Massachusetts Institute of Technology    1985	*/

/*
 * XMenu:	MIT Project Athena, X Window system menu package
 *
 * 	XMenuEventHandler - Set the XMenu asynchronous event handler.
 *
 *	Author:		Tony Della Fera, DEC
 *			December 19, 1985
 *
 */

#include "XMenuInt.h"

XMenuEventHandler(handler)
    int (*handler)();
{
    /*
     * Set the global event handler variable.
     */
    _XMEventHandler = handler;
}

