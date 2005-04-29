/* $XFree86: xc/programs/Xserver/hw/xfree86/parser/Vendor.c,v 1.18 2004/02/13 23:58:50 dawes Exp $ */
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

static xf86ConfigSymTabRec VendorSubTab[] =
{
	{ENDSUBSECTION, "endsubsection"},
	{IDENTIFIER, "identifier"},
	{OPTION, "option"},
	{-1, ""},
};

#define CLEANUP xf86freeVendorSubList

XF86ConfVendSubPtr
xf86parseVendorSubSection (void)
{
	int has_ident = FALSE;
	int token;
	parsePrologue (XF86ConfVendSubPtr, XF86ConfVendSubRec)

	while ((token = xf86getToken (VendorSubTab)) != ENDSUBSECTION)
	{
		switch (token)
		{
		case COMMENT:
			ptr->vs_comment = xf86addComment(ptr->vs_comment, val.str);
			break;
		case IDENTIFIER:
			if (xf86getSubToken (&(ptr->vs_comment)))
				Error (QUOTE_MSG, "Identifier");
			if (has_ident == TRUE)
				Error (MULTIPLE_MSG, "Identifier");
			ptr->vs_identifier = val.str;
			has_ident = TRUE;
			break;
		case OPTION:
			ptr->vs_option_lst = xf86parseOption(ptr->vs_option_lst);
			break;

		case EOF_TOKEN:
			Error (UNEXPECTED_EOF_MSG, NULL);
			break;
		default:
			Error (INVALID_KEYWORD_MSG, xf86tokenString ());
			break;
		}
	}

#ifdef DEBUG
	printf ("Vendor subsection parsed\n");
#endif

	return ptr;
}

#undef CLEANUP

static xf86ConfigSymTabRec VendorTab[] =
{
	{ENDSECTION, "endsection"},
	{IDENTIFIER, "identifier"},
	{OPTION, "option"},
	{SUBSECTION, "subsection"},
	{-1, ""},
};

#define CLEANUP xf86freeVendorList

XF86ConfVendorPtr
xf86parseVendorSection (void)
{
	int has_ident = FALSE;
	int token;
	parsePrologue (XF86ConfVendorPtr, XF86ConfVendorRec)

	while ((token = xf86getToken (VendorTab)) != ENDSECTION)
	{
		switch (token)
		{
		case COMMENT:
			ptr->vnd_comment = xf86addComment(ptr->vnd_comment, val.str);
			break;
		case IDENTIFIER:
			if (xf86getSubToken (&(ptr->vnd_comment)) != STRING)
				Error (QUOTE_MSG, "Identifier");
			if (has_ident == TRUE)
				Error (MULTIPLE_MSG, "Identifier");
			ptr->vnd_identifier = val.str;
			has_ident = TRUE;
			break;
		case OPTION:
			ptr->vnd_option_lst = xf86parseOption(ptr->vnd_option_lst);
			break;
		case SUBSECTION:
			if (xf86getSubToken (&(ptr->vnd_comment)) != STRING)
				Error (QUOTE_MSG, "SubSection");
			{
				HANDLE_LIST (vnd_sub_lst, xf86parseVendorSubSection,
							XF86ConfVendSubPtr);
			}
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
	printf ("Vendor section parsed\n");
#endif

	return ptr;
}

#undef CLEANUP

void
xf86printVendorSection (FILE * cf, XF86ConfVendorPtr ptr)
{
	XF86ConfVendSubPtr pptr;

	while (ptr)
	{
		fprintf (cf, "Section \"Vendor\"\n");
		if (ptr->vnd_comment)
			fprintf (cf, "%s", ptr->vnd_comment);
		if (ptr->vnd_identifier)
			fprintf (cf, "\tIdentifier     \"%s\"\n", ptr->vnd_identifier);

		xf86printOptionList(cf, ptr->vnd_option_lst, 1);
		for (pptr = ptr->vnd_sub_lst; pptr; pptr = pptr->list.next)
		{
			fprintf (cf, "\tSubSection \"Vendor\"\n");
			if (pptr->vs_comment)
				fprintf (cf, "%s", pptr->vs_comment);
			if (pptr->vs_identifier)
				fprintf (cf, "\t\tIdentifier \"%s\"\n", pptr->vs_identifier);
			xf86printOptionList(cf, pptr->vs_option_lst, 2);
			fprintf (cf, "\tEndSubSection\n");
		}
		fprintf (cf, "EndSection\n\n");
		ptr = ptr->list.next;
	}
}

void
xf86freeVendorList (XF86ConfVendorPtr p)
{
	if (p == NULL)
		return;
	xf86freeVendorSubList (p->vnd_sub_lst);
	TestFree (p->vnd_identifier);
	TestFree (p->vnd_comment);
	xf86optionListFree (p->vnd_option_lst);
	xf86conffree (p);
}

void
xf86freeVendorSubList (XF86ConfVendSubPtr ptr)
{
	XF86ConfVendSubPtr prev;

	while (ptr)
	{
		TestFree (ptr->vs_identifier);
		TestFree (ptr->vs_name);
		TestFree (ptr->vs_comment);
		xf86optionListFree (ptr->vs_option_lst);
		prev = ptr;
		ptr = ptr->list.next;
		xf86conffree (prev);
	}
}

XF86ConfVendorPtr
xf86findVendor (const char *name, XF86ConfVendorPtr list)
{
	while (list)
	{
		if (xf86nameCompare (list->vnd_identifier, name) == 0)
			return (list);
		list = list->list.next;
	}
	return (NULL);
}

