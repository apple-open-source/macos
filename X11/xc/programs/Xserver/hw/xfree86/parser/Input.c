/* $XFree86: xc/programs/Xserver/hw/xfree86/parser/Input.c,v 1.15 2004/02/13 23:58:49 dawes Exp $ */
/* 
 * 
 * Copyright (c) 1997  Metro Link Incorporated
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of the Metro Link shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Metro Link.
 * 
 */
/*
 * Copyright (c) 1997-2003 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/* View/edit this file with tab stops set to 4 */

#include "xf86Parser.h"
#include "xf86tokens.h"
#include "Configint.h"

extern LexRec val;

static
xf86ConfigSymTabRec InputTab[] =
{
	{ENDSECTION, "endsection"},
	{IDENTIFIER, "identifier"},
	{OPTION, "option"},
	{DRIVER, "driver"},
	{-1, ""},
};

#define CLEANUP xf86freeInputList

XF86ConfInputPtr
xf86parseInputSection (void)
{
	int has_ident = FALSE;
	int token;
	parsePrologue (XF86ConfInputPtr, XF86ConfInputRec)

	while ((token = xf86getToken (InputTab)) != ENDSECTION)
	{
		switch (token)
		{
		case COMMENT:
			ptr->inp_comment = xf86addComment(ptr->inp_comment, val.str);
			break;
		case IDENTIFIER:
			if (xf86getSubToken (&(ptr->inp_comment)) != STRING)
				Error (QUOTE_MSG, "Identifier");
			if (has_ident == TRUE)
				Error (MULTIPLE_MSG, "Identifier");
			ptr->inp_identifier = val.str;
			has_ident = TRUE;
			break;
		case DRIVER:
			if (xf86getSubToken (&(ptr->inp_comment)) != STRING)
				Error (QUOTE_MSG, "Driver");
			ptr->inp_driver = val.str;
			break;
		case OPTION:
			ptr->inp_option_lst = xf86parseOption(ptr->inp_option_lst);
			break;
		case EOF_TOKEN:
			Error (UNEXPECTED_EOF_MSG, NULL);
			break;
		default:
			Error (INVALID_KEYWORD_MSG, xf86tokenString ());
			break;
		}
	}

	if (!has_ident)
		Error (NO_IDENT_MSG, NULL);

#ifdef DEBUG
	printf ("InputDevice section parsed\n");
#endif

	return ptr;
}

#undef CLEANUP

void
xf86printInputSection (FILE * cf, XF86ConfInputPtr ptr)
{
	while (ptr)
	{
		fprintf (cf, "Section \"InputDevice\"\n");
		if (ptr->inp_comment)
			fprintf (cf, "%s", ptr->inp_comment);
		if (ptr->inp_identifier)
			fprintf (cf, "\tIdentifier  \"%s\"\n", ptr->inp_identifier);
		if (ptr->inp_driver)
			fprintf (cf, "\tDriver      \"%s\"\n", ptr->inp_driver);
		xf86printOptionList(cf, ptr->inp_option_lst, 1);
		fprintf (cf, "EndSection\n\n");
		ptr = ptr->list.next;
	}
}

void
xf86freeInputList (XF86ConfInputPtr ptr)
{
	XF86ConfInputPtr prev;

	while (ptr)
	{
		TestFree (ptr->inp_identifier);
		TestFree (ptr->inp_driver);
		TestFree (ptr->inp_comment);
		xf86optionListFree (ptr->inp_option_lst);

		prev = ptr;
		ptr = ptr->list.next;
		xf86conffree (prev);
	}
}

int
xf86validateInput (XF86ConfigPtr p)
{
	XF86ConfInputPtr input = p->conf_input_lst;

#if 0 /* Enable this later */
	if (!input) {
		xf86validationError ("At least one InputDevice section is required.");
		return (FALSE);
	}
#endif

	while (input) {
		if (!input->inp_driver) {
			xf86validationError (UNDEFINED_INPUTDRIVER_MSG, input->inp_identifier);
			return (FALSE);
		}
		input = input->list.next;
	}
	return (TRUE);
}

XF86ConfInputPtr
xf86findInput (const char *ident, XF86ConfInputPtr p)
{
	while (p)
	{
		if (xf86nameCompare (ident, p->inp_identifier) == 0)
			return (p);

		p = p->list.next;
	}
	return (NULL);
}

XF86ConfInputPtr
xf86findInputByDriver (const char *driver, XF86ConfInputPtr p)
{
	while (p)
	{
		if (xf86nameCompare (driver, p->inp_driver) == 0)
			return (p);

		p = p->list.next;
	}
	return (NULL);
}

