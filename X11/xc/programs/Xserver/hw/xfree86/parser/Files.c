/* $XFree86: xc/programs/Xserver/hw/xfree86/parser/Files.c,v 1.17 2004/02/13 23:58:49 dawes Exp $ */
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

#include "X11/Xos.h"
#include "xf86Parser.h"
#include "xf86tokens.h"
#include "Configint.h"

extern LexRec val;

static xf86ConfigSymTabRec FilesTab[] =
{
	{ENDSECTION, "endsection"},
	{FONTPATH, "fontpath"},
	{RGBPATH, "rgbpath"},
	{MODULEPATH, "modulepath"},
	{INPUTDEVICES, "inputdevices"},
	{LOGFILEPATH, "logfile"},
	{-1, ""},
};

static char *
prependRoot (char *pathname)
{
#ifndef __EMX__
	return pathname;
#else
	/* XXXX caveat: multiple path components in line */
	return (char *) __XOS2RedirRoot (pathname);
#endif
}

#define CLEANUP xf86freeFiles

XF86ConfFilesPtr
xf86parseFilesSection (void)
{
	int i, j;
	int k, l;
	char *str;
	int token;
	parsePrologue (XF86ConfFilesPtr, XF86ConfFilesRec)

	while ((token = xf86getToken (FilesTab)) != ENDSECTION)
	{
		switch (token)
		{
		case COMMENT:
			ptr->file_comment = xf86addComment(ptr->file_comment, val.str);
			break;
		case FONTPATH:
			if (xf86getSubToken (&(ptr->file_comment)) != STRING)
				Error (QUOTE_MSG, "FontPath");
			j = FALSE;
			str = prependRoot (val.str);
			if (ptr->file_fontpath == NULL)
			{
				ptr->file_fontpath = xf86confmalloc (1);
				ptr->file_fontpath[0] = '\0';
				i = strlen (str) + 1;
			}
			else
			{
				i = strlen (ptr->file_fontpath) + strlen (str) + 1;
				if (ptr->file_fontpath[strlen (ptr->file_fontpath) - 1] != ',')
				{
					i++;
					j = TRUE;
				}
			}
			ptr->file_fontpath =
				xf86confrealloc (ptr->file_fontpath, i);
			if (j)
				strcat (ptr->file_fontpath, ",");

			strcat (ptr->file_fontpath, str);
			xf86conffree (val.str);
			break;
		case RGBPATH:
			if (xf86getSubToken (&(ptr->file_comment)) != STRING)
				Error (QUOTE_MSG, "RGBPath");
			ptr->file_rgbpath = val.str;
			break;
		case MODULEPATH:
			if (xf86getSubToken (&(ptr->file_comment)) != STRING)
				Error (QUOTE_MSG, "ModulePath");
			l = FALSE;
			str = prependRoot (val.str);
			if (ptr->file_modulepath == NULL)
			{
				ptr->file_modulepath = xf86confmalloc (1);
				ptr->file_modulepath[0] = '\0';
				k = strlen (str) + 1;
			}
			else
			{
				k = strlen (ptr->file_modulepath) + strlen (str) + 1;
				if (ptr->file_modulepath[strlen (ptr->file_modulepath) - 1] != ',')
				{
					k++;
					l = TRUE;
				}
			}
			ptr->file_modulepath = xf86confrealloc (ptr->file_modulepath, k);
			if (l)
				strcat (ptr->file_modulepath, ",");

			strcat (ptr->file_modulepath, str);
			xf86conffree (val.str);
			break;
		case INPUTDEVICES:
			if (xf86getSubToken (&(ptr->file_comment)) != STRING)
				Error (QUOTE_MSG, "InputDevices");
			l = FALSE;
			str = prependRoot (val.str);
			if (ptr->file_inputdevs == NULL)
			{
				ptr->file_inputdevs = xf86confmalloc (1);
				ptr->file_inputdevs[0] = '\0';
				k = strlen (str) + 1;
			}
			else
			{
				k = strlen (ptr->file_inputdevs) + strlen (str) + 1;
				if (ptr->file_inputdevs[strlen (ptr->file_inputdevs) - 1] != ',')
				{
					k++;
					l = TRUE;
				}
			}
			ptr->file_inputdevs = xf86confrealloc (ptr->file_inputdevs, k);
			if (l)
				strcat (ptr->file_inputdevs, ",");

			strcat (ptr->file_inputdevs, str);
			xf86conffree (val.str);
			break;
		case LOGFILEPATH:
			if (xf86getSubToken (&(ptr->file_comment)) != STRING)
				Error (QUOTE_MSG, "LogFile");
			ptr->file_logfile = val.str;
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
	printf ("File section parsed\n");
#endif

	return ptr;
}

#undef CLEANUP

void
xf86printFileSection (FILE * cf, XF86ConfFilesPtr ptr)
{
	char *p, *s;

	if (ptr == NULL)
		return;

	if (ptr->file_comment)
		fprintf (cf, "%s", ptr->file_comment);
	if (ptr->file_logfile)
		fprintf (cf, "\tLogFile      \"%s\"\n", ptr->file_logfile);
	if (ptr->file_rgbpath)
		fprintf (cf, "\tRgbPath      \"%s\"\n", ptr->file_rgbpath);
	if (ptr->file_modulepath)
	{
		s = ptr->file_modulepath;
		p = index (s, ',');
		while (p)
		{
			*p = '\000';
			fprintf (cf, "\tModulePath   \"%s\"\n", s);
			*p = ',';
			s = p;
			s++;
			p = index (s, ',');
		}
		fprintf (cf, "\tModulePath   \"%s\"\n", s);
	}
	if (ptr->file_inputdevs)
	{
		s = ptr->file_inputdevs;
		p = index (s, ',');
		while (p)
		{
			*p = '\000';
			fprintf (cf, "\tInputDevices   \"%s\"\n", s);
			*p = ',';
			s = p;
			s++;
			p = index (s, ',');
		}
		fprintf (cf, "\tInputdevs   \"%s\"\n", s);
	}
	if (ptr->file_fontpath)
	{
		s = ptr->file_fontpath;
		p = index (s, ',');
		while (p)
		{
			*p = '\000';
			fprintf (cf, "\tFontPath     \"%s\"\n", s);
			*p = ',';
			s = p;
			s++;
			p = index (s, ',');
		}
		fprintf (cf, "\tFontPath     \"%s\"\n", s);
	}
}

void
xf86freeFiles (XF86ConfFilesPtr p)
{
	if (p == NULL)
		return;

	TestFree (p->file_logfile);
	TestFree (p->file_rgbpath);
	TestFree (p->file_modulepath);
	TestFree (p->file_inputdevs);
	TestFree (p->file_fontpath);
	TestFree (p->file_comment);

	xf86conffree (p);
}
