/* $XFree86: xc/programs/Xserver/hw/xfree86/parser/xf86Optrec.h,v 1.12 2004/02/13 23:58:50 dawes Exp $ */
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
 * Copyright (c) 1997-2001 by The XFree86 Project, Inc.
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


/* 
 * This file contains the Option Record that is passed between the Parser,
 * and Module setup procs.
 */
#ifndef _xf86Optrec_h_
#define _xf86Optrec_h_
#include <stdio.h>

/* 
 * all records that need to be linked lists should contain a GenericList as
 * their first field.
 */
typedef struct generic_list_rec
{
	void *next;
}
GenericListRec, *GenericListPtr, *glp;

/*
 * All options are stored using this data type.
 */
typedef struct
{
	GenericListRec list;
	char *opt_name;
	char *opt_val;
	int opt_used;
	char *opt_comment;
}
XF86OptionRec, *XF86OptionPtr;


XF86OptionPtr xf86addNewOption(XF86OptionPtr head, char *name, char *val);
XF86OptionPtr xf86optionListDup(XF86OptionPtr opt);
void xf86optionListFree(XF86OptionPtr opt);
char *xf86optionName(XF86OptionPtr opt);
char *xf86optionValue(XF86OptionPtr opt);
XF86OptionPtr xf86newOption(char *name, char *value);
XF86OptionPtr xf86nextOption(XF86OptionPtr list);
XF86OptionPtr xf86findOption(XF86OptionPtr list, const char *name);
char *xf86findOptionValue(XF86OptionPtr list, const char *name);
int xf86findOptionBoolean (XF86OptionPtr, const char *, int);
XF86OptionPtr xf86optionListCreate(const char **options, int count, int used);
XF86OptionPtr xf86optionListMerge(XF86OptionPtr head, XF86OptionPtr tail);
char *xf86configStrdup (const char *s);
int xf86nameCompare (const char *s1, const char *s2);
char *xf86uLongToString(unsigned long i);
void xf86debugListOptions(XF86OptionPtr);
XF86OptionPtr xf86parseOption(XF86OptionPtr head);
void xf86printOptionList(FILE *fp, XF86OptionPtr list, int tabs);


#endif /* _xf86Optrec_h_ */
