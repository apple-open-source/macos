/* $XFree86: xc/programs/Xserver/hw/xfree86/parser/Pointer.c,v 1.14 2004/02/13 23:58:50 dawes Exp $ */
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

static xf86ConfigSymTabRec PointerTab[] =
{
	{PROTOCOL, "protocol"},
	{EMULATE3, "emulate3buttons"},
	{EM3TIMEOUT, "emulate3timeout"},
	{ENDSUBSECTION, "endsubsection"},
	{ENDSECTION, "endsection"},
	{PDEVICE, "device"},
	{PDEVICE, "port"},
	{BAUDRATE, "baudrate"},
	{SAMPLERATE, "samplerate"},
	{CLEARDTR, "cleardtr"},
	{CLEARRTS, "clearrts"},
	{CHORDMIDDLE, "chordmiddle"},
	{PRESOLUTION, "resolution"},
	{DEVICE_NAME, "devicename"},
	{ALWAYSCORE, "alwayscore"},
	{PBUTTONS, "buttons"},
	{ZAXISMAPPING, "zaxismapping"},
	{-1, ""},
};

static xf86ConfigSymTabRec ZMapTab[] =
{
	{XAXIS, "x"},
	{YAXIS, "y"},
	{-1, ""},
};

#define CLEANUP xf86freeInputList

XF86ConfInputPtr
xf86parsePointerSection (void)
{
	char *s, *s1, *s2;
	int l;
	int token;
	parsePrologue (XF86ConfInputPtr, XF86ConfInputRec)

	while ((token = xf86getToken (PointerTab)) != ENDSECTION)
	{
		switch (token)
		{
		case COMMENT:
			ptr->inp_comment = xf86addComment(ptr->inp_comment, val.str);
			break;
		case PROTOCOL:
			if (xf86getSubToken (&(ptr->inp_comment)) != STRING)
				Error (QUOTE_MSG, "Protocol");
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("Protocol"),
												val.str);
			break;
		case PDEVICE:
			if (xf86getSubToken (&(ptr->inp_comment)) != STRING)
				Error (QUOTE_MSG, "Device");
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("Device"),
												val.str);
			break;
		case EMULATE3:
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("Emulate3Buttons"),
												NULL);
			break;
		case EM3TIMEOUT:
			if (xf86getSubToken (&(ptr->inp_comment)) != NUMBER || val.num < 0)
				Error (POSITIVE_INT_MSG, "Emulate3Timeout");
			s = xf86uLongToString(val.num);
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("Emulate3Timeout"),
												s);
			break;
		case CHORDMIDDLE:
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("ChordMiddle"),
												NULL);
			break;
		case PBUTTONS:
			if (xf86getSubToken (&(ptr->inp_comment)) != NUMBER || val.num < 0)
				Error (POSITIVE_INT_MSG, "Buttons");
			s = xf86uLongToString(val.num);
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("Buttons"), s);
			break;
		case BAUDRATE:
			if (xf86getSubToken (&(ptr->inp_comment)) != NUMBER || val.num < 0)
				Error (POSITIVE_INT_MSG, "BaudRate");
			s = xf86uLongToString(val.num);
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("BaudRate"), s);
			break;
		case SAMPLERATE:
			if (xf86getSubToken (&(ptr->inp_comment)) != NUMBER || val.num < 0)
				Error (POSITIVE_INT_MSG, "SampleRate");
			s = xf86uLongToString(val.num);
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("SampleRate"), s);
			break;
		case PRESOLUTION:
			if (xf86getSubToken (&(ptr->inp_comment)) != NUMBER || val.num < 0)
				Error (POSITIVE_INT_MSG, "Resolution");
			s = xf86uLongToString(val.num);
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("Resolution"), s);
			break;
		case CLEARDTR:
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("ClearDTR"), NULL);
			break;
		case CLEARRTS:
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("ClearRTS"), NULL);
			break;
		case ZAXISMAPPING:
			switch (xf86getToken(ZMapTab)) {
			case NUMBER:
				if (val.num < 0)
					Error (ZAXISMAPPING_MSG, NULL);
				s1 = xf86uLongToString(val.num);
				if (xf86getSubToken (&(ptr->inp_comment)) != NUMBER || val.num < 0)
					Error (ZAXISMAPPING_MSG, NULL);
				s2 = xf86uLongToString(val.num);
				l = strlen(s1) + 1 + strlen(s2) + 1;
				s = xf86confmalloc(l);
				sprintf(s, "%s %s", s1, s2);
				xf86conffree(s1);
				xf86conffree(s2);
				break;
			case XAXIS:
				s = xf86configStrdup("x");
				break;
			case YAXIS:
				s = xf86configStrdup("y");
				break;
			default:
				Error (ZAXISMAPPING_MSG, NULL);
				break;
			}
			ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
												xf86configStrdup("ZAxisMapping"),
												s);
			break;
		case ALWAYSCORE:
			break;
		case EOF_TOKEN:
			Error (UNEXPECTED_EOF_MSG, NULL);
			break;
		default:
			Error (INVALID_KEYWORD_MSG, xf86tokenString ());
			break;
		}
	}

	ptr->inp_identifier = xf86configStrdup(CONF_IMPLICIT_POINTER);
	ptr->inp_driver = xf86configStrdup("mouse");
	ptr->inp_option_lst = xf86addNewOption(ptr->inp_option_lst,
										xf86configStrdup("CorePointer"), NULL);

#ifdef DEBUG
	printf ("Pointer section parsed\n");
#endif

	return ptr;
}

#undef CLEANUP

