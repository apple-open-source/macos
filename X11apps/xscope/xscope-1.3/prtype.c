/* ************************************************** *
 *						      *
 *  Type Printing for X11 protocol		      *
 *						      *
 *	James Peterson, 1988			      *
 * Copyright (C) 1988 MCC
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of MCC not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  MCC makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * MCC DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL MCC BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *						      *
 * Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * ************************************************** */

#include "scope.h"
#include "x11.h"

/*
  For each of the types we need a way to print that type.
  Types are of varieties:

  (1) BUILTIN -- we have a separate routine to interpret and print
  each built-in type.
  (2) ENUMERATED, SET -- we have one routine which prints, given the
  data and the list of values.
  (3) RECORDS -- a separate routine for each to print each field.

*/

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */


/* print representation of a character for debugging */
static char *
printrep (unsigned short  c)
{
  static char pr[8];

  if (c < 32)
    {
      /* control characters */
      pr[0] = '^';
      pr[1] = c + 64;
      pr[2] = '\0';
    }
  else if (c < 127)
      {
	/* printing characters */
	pr[0] = (char) c;
	pr[1] = '\0';
      }
    else if (c == 127)
	return("<del>");
      else if (c <= 0377)
	  {
	    /* upper 128 codes from 128 to 255;  print as \ooo - octal  */
	    pr[0] = '\\';
	    pr[3] = '0' + (c & 7);
	    c = c >> 3;
	    pr[2] = '0' + (c & 7);
	    c = c >> 3;
	    pr[1] = '0' + (c & 3);
	    pr[4] = '\0';
	  }
	else
	  {
	    /* very large number -- print as 0xffff - 4 digit hex */
	    sprintf(pr, "0x%04x", c);
	  }
  return(pr);
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/*
  we use indentation for two purposes:

   (1) To show substructure of records inside records ...
   (2) To separate the bytes from the client (on the left) from
       those from the server (on the right).

   Each indention level is one tab (8 spaces).
*/

#define MaxIndent 10
char Leader[MaxIndent + 1];
static short    CurrentLevel = 0;

void
SetIndentLevel (
    short   which)
{
  short   i;

  if (which > MaxIndent)
    which = MaxIndent;
  if (which < 0)
    which = 0;
  if (which == CurrentLevel)
    return;

  /* set the indent level to <which> */
  /* -> set the Print Leader to <which> tabs */
  for (i = 0; i < which; i++)
    Leader[i] = '\t';
  Leader[which] = '\0';
  CurrentLevel = which;
}

static void
ModifyIndentLevel (
     short   amount)
{
  SetIndentLevel(CurrentLevel + amount);
}

static short
SizeofLeader (void)
{
  return (CurrentLevel * 8);
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* if we want verbose enough output, we will dump the buffer in hex */

void
DumpItem (
    const char   *name,
    FD      fd,
    const unsigned char *buf,
    long    n)
{
  if (n == 0)
    return;

  fprintf(stdout, "%s%20s (fd %d): ", Leader, name, fd);

  DumpHexBuffer(buf, n);
  fprintf(stdout, "\n");
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

int
PrintINT8(const unsigned char *buf)
{
  /* print a INT8 -- 8-bit signed integer */
  short   n = IByte (buf);
  if (n > 127)
    n = 256 - n;
  fprintf(stdout, "%d", n);
  return 1;
}

int
PrintINT16(const unsigned char *buf)
{
  /* print a INT16 -- 16-bit signed integer */
  long    n = IShort (buf);
  if (n > 32767)
    n = 65536 - n;
  fprintf(stdout, "%ld", n);
  return 2;
}

int
PrintINT32(const unsigned char *buf)
{
  /* print a INT32 -- 32-bit signed integer */
  long    n = ILong (buf);
  fprintf(stdout, "%ld", n);
  return 4;
}

/* ************************************************************ */

int
PrintCARD8(const unsigned char *buf)
{
  /* print a CARD8 -- 8-bit unsigned integer */
  unsigned short   n = IByte (buf);
  fprintf(stdout, "%02x", (unsigned)(n & 0xff));
  return 1;
}

int
PrintCARD16(const unsigned char *buf)
{
  /* print a CARD16 -- 16-bit unsigned integer */
  unsigned long   n = IShort (buf);
  fprintf(stdout, "%04x", (unsigned)(n & 0xffff));
  return 1;
}

int
PrintCARD32(const unsigned char *buf)
{
  /* print a CARD32 -- 32-bit unsigned integer */
  unsigned long   n = ILong (buf);
  fprintf(stdout, "%08lx", n);
  return(4);
}

/* ************************************************************ */

int
PrintBYTE(const unsigned char *buf)
{
  /* print a BYTE -- 8-bit value */
  short   n = IByte (buf);
  fprintf(stdout, "%02x", n);
  return(1);
}


int
PrintCHAR8(const unsigned char *buf)
{
  /* print a CHAR8 -- 8-bit character */
  unsigned short   n = IByte (buf);
  fprintf(stdout, "%s", printrep(n));
  return(1);
}


int
PrintSTRING16(const unsigned char *buf)
{
  /* print a CHAR2B -- 16-bit character which is never byte-swapped */
  unsigned short   n = IChar2B (buf);
  fprintf(stdout, "%s", printrep(n));
  return 2 + n;
}

int
PrintSTR(const unsigned char *buf)
{
  /* STR have the length (1 byte) then a string of CHAR8 */
  short   n;
  short   i;

  n = IByte(buf++);
  for (i = 0; i < n; i++)
    fprintf(stdout, "%s", printrep(buf[i]));
  return(n+1);
}

/* ************************************************************ */

int
PrintWINDOW(const unsigned char *buf)
{
  /* print a WINDOW -- CARD32  plus 0 = None */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "None");
  else
    fprintf(stdout, "WIN %08lx", n);
  return(4);
}

int
PrintWINDOWD(const unsigned char *buf)
{
  /* print a WINDOWD -- CARD32  plus 0 = PointerWindow, 1 = InputFocus */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "PointerWindow");
  else if (n == 1)
      fprintf(stdout, "InputFocus");
    else
      (void)PrintWINDOW(buf);
  return 4;
}

int
PrintWINDOWNR(const unsigned char *buf)
{
  /* print a WINDOWNR -- CARD32  plus 0 = None, 1 = PointerRoot */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "None");
  else if (n == 1)
      fprintf(stdout, "PointerRoot");
    else
      (void)PrintWINDOW(buf);
  return 4;
}


int
PrintPIXMAP(const unsigned char *buf)
{
  /* print a PIXMAP -- CARD32  plus 0 = None */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "None");
  else
    fprintf(stdout, "PXM %08lx", n);
  return 4;
}

int
PrintPIXMAPNPR(const unsigned char *buf)
{
  /* print a PIXMAPNPR -- CARD32  plus 0 = None, 1 = ParentRelative */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "None");
  else if (n == 1)
      fprintf(stdout, "ParentRelative");
    else
      PrintPIXMAP(buf);
  return 4;
}

int
PrintPIXMAPC(const unsigned char *buf)
{
  /* print a PIXMAPC -- CARD32  plus 0 = CopyFromParent */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "CopyFromParent");
  else
    PrintPIXMAP(buf);
  return 4;
}


int
PrintCURSOR(const unsigned char *buf)
{
  /* print a CURSOR -- CARD32  plus 0 = None */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "None");
  else
    fprintf(stdout, "CUR %08lx", n);
  return 4;
}


int
PrintFONT(const unsigned char *buf)
{
  /* print a FONT -- CARD32  plus 0 = None */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "None");
  else
    fprintf(stdout, "FNT %08lx", n);
  return 4;
}


int
PrintGCONTEXT(const unsigned char *buf)
{
  /* print a GCONTEXT -- CARD32 */
  long    n = ILong (buf);
  fprintf(stdout, "GXC %08lx", n);
  return 4;
}


int
PrintCOLORMAP(const unsigned char *buf)
{
  /* print a COLORMAP -- CARD32 plus 0 = None */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "None");
  else
    fprintf(stdout, "CMP %08lx", n);
  return(4);
}

int
PrintCOLORMAPC(const unsigned char *buf)
{
  /* print a COLORMAPC -- CARD32 plus 0 = CopyFromParent */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "CopyFromParent");
  else
    (void)PrintCOLORMAP(buf);
  return 4;
}


int
PrintDRAWABLE(const unsigned char *buf)
{
  /* print a DRAWABLE -- CARD32 */
  long    n = ILong (buf);
  fprintf(stdout, "DWB %08lx", n);
  return 4;
}

int
PrintFONTABLE(const unsigned char *buf)
{
  /* print a FONTABLE -- CARD32 */
  long    n = ILong (buf);
  fprintf(stdout, "FTB %08lx", n);
  return 4;
}

/* ************************************************************ */

#define NumberofAtoms 68

static const char   * const AtomTable[NumberofAtoms + 1] =
{
  "NONE", "PRIMARY", "SECONDARY", "ARC", "ATOM", "BITMAP", "CARDINAL",
  "COLORMAP", "CURSOR", "CUT_BUFFER0", "CUT_BUFFER1", "CUT_BUFFER2",
  "CUT_BUFFER3", "CUT_BUFFER4", "CUT_BUFFER5", "CUT_BUFFER6",
  "CUT_BUFFER7", "DRAWABLE", "FONT", "INTEGER", "PIXMAP", "POINT",
  "RECTANGLE", "RESOURCE_MANAGER", "RGB_COLOR_MAP", "RGB_BEST_MAP",
  "RGB_BLUE_MAP", "RGB_DEFAULT_MAP", "RGB_GRAY_MAP", "RGB_GREEN_MAP",
  "RGB_RED_MAP", "STRING", "VISUALID", "WINDOW", "WM_COMMAND",
  "WM_HINTS", "WM_CLIENT_MACHINE", "WM_ICON_NAME", "WM_ICON_SIZE",
  "WM_NAME", "WM_NORMAL_HINTS", "WM_SIZE_HINTS", "WM_ZOOM_HINTS",
  "MIN_SPACE", "NORM_SPACE", "MAX_SPACE", "END_SPACE", "SUPERSCRIPT_X",
  "SUPERSCRIPT_Y", "SUBSCRIPT_X", "SUBSCRIPT_Y", "UNDERLINE_POSITION",
  "UNDERLINE_THICKNESS", "STRIKEOUT_ASCENT", "STRIKEOUT_DESCENT",
  "ITALIC_ANGLE", "X_HEIGHT", "QUAD_WIDTH", "WEIGHT", "POINT_SIZE",
  "RESOLUTION", "COPYRIGHT", "NOTICE", "FONT_NAME", "FAMILY_NAME",
  "FULL_NAME", "CAP_HEIGHT", "WM_CLASS", "WM_TRANSIENT_FOR"
  };

/* for atoms, we print the built-in atoms.  We could expand to printing
   the user defined ones, too. */

int
PrintATOM(const unsigned char *buf)
{
  /* print a ATOM -- CARD32 plus 0 = None */
  long    n = ILong (buf);
  if (0 <= n && n <= NumberofAtoms)
    fprintf(stdout, "<%s>", AtomTable[n]);
  else
    fprintf(stdout, "ATM %08lx", n);
  return(4);
}

int
PrintATOMT(const unsigned char *buf)
{
  /* print a ATOMT -- CARD32 plus 0 = AnyPropertyType */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "AnyPropertyType");
  else
    (void)PrintATOM(buf);
  return 4;
}


int
PrintVISUALID(const unsigned char *buf)
{
  /* print a VISUALID -- CARD32 plus 0 = None */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "None");
  else
    fprintf(stdout, "VIS %08lx", n);
  return 4;
}

int
PrintVISUALIDC(const unsigned char *buf)
{
  /* print a VISUALIDC -- CARD32 plus 0 = CopyFromParent */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "CopyFromParent");
  else
    PrintVISUALID(buf);
  return 4;
}


int
PrintTIMESTAMP(const unsigned char *buf)
{
  /* print a TIMESTAMP -- CARD32 plus 0 as the current time */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "CurrentTime");
  else
    fprintf(stdout, "TIM %08lx", n);
  return 4;
}


int
PrintRESOURCEID(const unsigned char *buf)
{
  /* print a RESOURCEID -- CARD32 plus 0 = AllTemporary */
  long    n = ILong (buf);
  if (n == 0)
    fprintf(stdout, "AllTemporary");
  else
    fprintf(stdout, "RID %08lx", n);
  return 4;
}


int
PrintKEYSYM(const unsigned char *buf)
{
  /* print a KEYSYM -- CARD32 */
  long    n = ILong (buf);
  fprintf(stdout, "KYS %08lx", n);
  return(4);
}

int
PrintKEYCODE(const unsigned char *buf)
{
  /* print a KEYCODE -- CARD8 */
  unsigned short n = IByte (buf);
  fprintf(stdout, "%d (%s)", n, printrep(n));
  return(1);
}

int
PrintKEYCODEA(const unsigned char *buf)
{
  /* print a KEYCODEA -- CARD8 plus 0 = AnyKey */
  long    n = IByte (buf);
  if (n == 0)
    fprintf(stdout, "AnyKey");
  else
    (void)PrintKEYCODE(buf);
  return 1;
}


int
PrintBUTTON(const unsigned char *buf)
{
  /* print a BUTTON -- CARD8 */
  unsigned short n = IByte (buf);
  fprintf(stdout, "%d (%s)", n, printrep(n));
  return 1;
}

int
PrintBUTTONA(const unsigned char *buf)
{
  /* print a BUTTONA -- CARD8 plus 0 = AnyButton */
  long    n = IByte (buf);
  if (n == 0)
    fprintf(stdout, "AnyButton");
  else
    PrintBUTTON(buf);
  return 1;
}


/* this is an interesting cheat -- we call DecodeEvent to print an event */
/* should work, but its never been tried */
int
PrintEVENTFORM(const unsigned char *buf)
{
  /* print an EVENT_FORM -- event format */
  DecodeEvent(-1, buf, (long)-1);
  return 32;
}

/* ************************************************************ */

int
PrintENUMERATED(
     const unsigned char *buf,
     short   length,
     struct ValueListEntry  *ValueList)
{
  long   n;
  struct ValueListEntry  *p;

  if (length == 1)
    n = IByte(buf);
  else if (length == 2)
      n = IShort(buf);
    else
      n = ILong(buf);

  p = ValueList;
  while (p != NULL && p->Value != n)
    p = p->Next;

  if (p != NULL)
    fprintf(stdout, "%s", p->Name);
  else
    fprintf(stdout, "**INVALID** (%ld)", n);

  return length;
}

/* ************************************************************ */

int
PrintSET(
     const unsigned char *buf,
     short   length,
     struct ValueListEntry  *ValueList)
{
  unsigned long   n;
  struct ValueListEntry  *p;
  Boolean MatchesAll = false;
  Boolean FoundOne = false;

  if (length == 1)
    n = IByte(buf);
  else if (length == 2)
      n = IShort(buf);
    else
      n = ILong(buf);

  if (n != 0)
    {
      /* first check if the value matches ALL of the bits. */
      MatchesAll = true;
      for (p = ValueList; MatchesAll && (p != NULL); p = p->Next)
	{
	  if ((p->Value & n) == 0)
	      MatchesAll = false;
	}

      if (!MatchesAll)
	/* if it matches some, but not all, print only those it matches */
	for (p = ValueList; p != NULL; p = p->Next)
	  {
	    if ((p->Value & n) != 0)
	      {
		if (FoundOne)
		  fprintf(stdout, " | ");
		fprintf(stdout, "%s", p->Name);
		FoundOne = true;
	      }
	  }
    }

  if (MatchesAll)
    fprintf(stdout, "<ALL>");
  else if (!FoundOne)
    fprintf(stdout, "0");

  return length;
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void
PrintField (
    const unsigned char *buf,
    short   start,
    short   length,
    short   FieldType,
    const char *name)
{
  if (Verbose == 0)
    return;
  
  if (length == 0)
    return;

  fprintf(stdout, "%s%20s: ", Leader, name);

  if (debuglevel & 8)
    DumpHexBuffer(&(buf[start]), (long)length);

  switch (TD[FieldType].Type)
    {
	  case BUILTIN:
		 (*TD[FieldType].PrintProc)(&buf[start]);
		 break;

	  case ENUMERATED:
		 PrintENUMERATED(&buf[start], length, TD[FieldType].ValueList);
		 break;

	  case SET:
		 PrintSET(&buf[start], length, TD[FieldType].ValueList);
		 break;

	  case RECORD:
		 ModifyIndentLevel(1);
		 fprintf(stdout, "\n");
		 if (Verbose < 3)
		   return;
		 (*TD[FieldType].PrintProc)(&buf[start]);
		 ModifyIndentLevel(-1);
		 break;
    }
  fprintf(stdout, "\n");
  fflush(stdout);
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* print a list of things.  The things are of type <ListType>.
   They start at <buf>.  There are <number> things in the list */

long
PrintList (
    const unsigned char *buf,
    long   number,
    short   ListType,
    const char *name)
{
  long    n;
  long    i;
  long    sum;

  if (number == 0)
    return(0);

  fprintf(stdout, "%s%20s: (%ld)\n", Leader, name, number);
  if (Verbose < 2)
    return(0);

  ModifyIndentLevel(1);
  sum = 0;
  for (i = 0; i < number; i++)
    {
      switch (TD[ListType].Type)
	{
		    case BUILTIN:
			    n = (*TD[ListType].PrintProc)(buf);
			    break;
		    case RECORD:
			    n = (*TD[ListType].PrintProc)(buf);
			    break;
		    default:
			    fprintf(stdout, "**INVALID**");
			    n = 0;
			    break;
	}
      buf = buf + n;
      sum = sum + n;
      fprintf(stdout, "%s---\n", Leader);
    }

  ModifyIndentLevel(-1);
  return(sum);
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* print a list of STRs.  Similar to PrintList
   They start at <buf>.  There are <number> things in the list */

long
PrintListSTR (
    const unsigned char *buf,
    long   number,
    const char *name)
{
  long    n;
  long    i;
  long    sum;

  if (number == 0)
    return(0);

  fprintf(stdout, "%s%20s: (%ld)\n", Leader, name, number);
  if (Verbose < 2)
    return(0);

  ModifyIndentLevel(1);
  sum = 0;
  for (i = 0; i < number; i++)
    {
      fprintf(stdout, "%s", Leader);
      n = PrintSTR(buf);
      buf = buf + n;
      sum = sum + n;
      fprintf(stdout, "\n");
    }

  ModifyIndentLevel(-1);
  return(sum);
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */


int
PrintBytes(
    const unsigned char *buf,
    long   number,
    const char *name)
{
  /* print a list of BYTE -- 8-bit character */
  long   i;
  short   column;

  if (number == 0)
    return(0);

  fprintf(stdout, "%s%20s: ", Leader, name);
  column = SizeofLeader() + 25;
  for (i = 0; i < number; i++)
    {
      if (column > 80)
	{
	  if (Verbose < 2)
	    break;
	  fprintf(stdout, "\n%s%20s: ", Leader, "");
	  column = SizeofLeader() + 25;
	}
      fprintf(stdout, "%02x ",((unsigned int) buf[i]));
      column += 3;
    }
  fprintf(stdout, "\n");

  return(number);
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */


/* print a String of CHAR8 -- 8-bit characters */

int
PrintString8(
    const unsigned char *buf,
    int   number,
    const char *name)
{
  short   i;

  if (number == 0)
    return(0);

  fprintf(stdout, "%s%20s: \"", Leader, name);
  for (i = 0; i < number; i++)
    fprintf(stdout, "%s", printrep(buf[i]));
  fprintf(stdout, "\"\n");

  return(number);
}


/* print a String of CHAR16 -- 16-bit characters */

int
PrintString16(
    const unsigned char *buf,
    int   number,
    const char *name)
{
  long   i;
  unsigned short   c;

  if (number == 0)
    return(0);

  fprintf(stdout, "%s%20s: \"", Leader, name);
  for (i = 0; i < number; i += 2)
    {
      c = IChar2B(&buf[i]);
      fprintf(stdout, "%s", printrep(c));
    }
  fprintf(stdout, "\"\n");

  return(number);
}

void
PrintTString8(
    const unsigned char *buf,
    long   number,
    const char *name)
{
  long   i;
  int	off;

  if (number == 0)
    return;

  off = 0;
  if (TranslateText)
    off = 0x20;
  fprintf(stdout, "%s%20s: \"", Leader, name);
  for (i = 0; i < number; i++)
    fprintf(stdout, "%s", printrep(buf[i] + off));
  fprintf(stdout, "\"\n");
}


/* print a String of CHAR2B -- 16-bit characters */
void
PrintTString16(
    const unsigned char *buf,
    long   number,
    const char *name)
{
  long   i;
  unsigned short   c;
  int	off;

  if (number == 0)
      return;

  off = 0;
  if (TranslateText)
    off = 0x20;
  fprintf(stdout, "%s%20s: \"", Leader, name);
  for (i = 0; i < number; i += 2)
    {
      c = IChar2B(&buf[i]);
      fprintf(stdout, "%s", printrep(c + off));
    }
  fprintf(stdout, "\"\n");
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/*
  A Value List is two things:

  (1) A controlling bitmask.  For each one bit in the control,
  a value is in the list.
  (2) A list of values.
*/

void
PrintValues(    
    const unsigned char   *control,
    int     clength,
    int     ctype,
    const unsigned char   *values,
    const char            *name)
{
  long    cmask;
  struct ValueListEntry  *p;

  /* first get the control mask */
  if (clength == 1)
    cmask = IByte(control);
  else if (clength == 2)
      cmask = IShort(control);
    else
      cmask = ILong(control);

  /* now if it is zero, ignore and return */
  if (cmask == 0)
    return;

  /* there are bits in the controlling bitmask, figure out which */
  /* the ctype is a set type, so this code is similar to PrintSET */
  fprintf(stdout, "%s%20s:\n", Leader, name);
  ModifyIndentLevel(1);
  for (p = TD[ctype].ValueList; p != NULL; p = p->Next)
    {
      if ((p->Value & cmask) != 0)
	{
	  short m;
	  if (littleEndian)
	    m=0;
	  else
	    m = 4 - p->Length;
	  PrintField(values, m, p->Length, p->Type, p->Name);
	  values += 4;
	}
    }
  ModifyIndentLevel(-1);
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* PolyText8 and PolyText16 take lists of characters with possible
   font changes in them. */

void
PrintTextList8(
    const unsigned char *buf,
    int     length,
    const char *name)
{
  short   n;

  fprintf(stdout, "%s%20s:\n", Leader, name);
  while (length > 1)
    {
      n = IByte(&buf[0]);
      if (n != 255)
	{
	  PrintField(buf, 1, 1, INT8, "delta");
	  PrintTString8(&buf[2], (long)n, "text item 8 string");
	  buf += n + 2;
	  length -= n + 2;
	}
      else
	{
	  PrintField(buf, 1, 4, FONT, "font-shift-id");
	  buf += 4;
	  length -= 4;
	}
    }
}

void
PrintTextList16(
    const unsigned char *buf,
    int     length,
    const char *name)
{
  short   n;

  fprintf(stdout, "%s%20s:\n", Leader, name);
  while (length > 1)
    {
      n = IByte(&buf[0]);
      if (n != 255)
	{
	  PrintField(buf, 1, 1, INT8, "delta");
	  PrintTString16(&buf[2], (long)n, "text item 16 string");
	  buf += n + 2;
	  length -= n + 2;
	}
      else
	{
	  PrintField(buf, 1, 4, FONT, "font-shift-id");
	  buf += 4;
	  length -= 4;
	}
    }
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

#define MAXline 78

void
DumpHexBuffer(
    const unsigned char *buf,
    long    n)
{
  long   i;
  short   column;
  char    h[6] /* one hex or octal character */ ;

  column = 27 + SizeofLeader();
  for (i = 0; i < n; i++)
    {
      /* get the hex representations */
      sprintf(h, "%02x",(0xff & buf[i]));

      /* check if these characters will fit on this line */
      if ((column + strlen(h) + 1) > MAXline)
	{
	  /* line will be too long -- print it */
	  fprintf(stdout, "\n");
	  column = 0;
	}
      fprintf(stdout, "%s ", h);
      column += 3;
    }
}

void
PrintValueRec (
    unsigned long   key,
    unsigned long   cmask,
    short   ctype)
{
    unsigned char   *values;
    struct ValueListEntry  *p;
    ValuePtr	    value;

    value = GetValueRec (key);
    if (!value)
	return;
    values = (unsigned char *) value->values;
    
    /* now if it is zero, ignore and return */
    if (cmask == 0)
	return;

    /* there are bits in the controlling bitmask, figure out which */
    /* the ctype is a set type, so this code is similar to PrintSET */
    ModifyIndentLevel(1);
    for (p = TD[ctype].ValueList; p != NULL; p = p->Next)
    {
	if ((p->Value & cmask) != 0)
	{
	    short m;
	    if (littleEndian)
		m=0;
	    else
		m = 4 - p->Length;
	    PrintField(values, m, p->Length, p->Type, p->Name);
	}
	values += 4;
    }
    ModifyIndentLevel(-1);
}
