/*
 * dpsabbrev.c -- Implementation of Client Library abbrev mode
 *
 * (c) Copyright 1993-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */
/* $XFree86: xc/lib/dps/dpsabbrev.c,v 1.3 2001/04/01 14:00:03 tsi Exp $ */

#include <string.h>
#include "publictypes.h"
#include "DPS/dpsclient.h"
#include "dpsprivate.h"

static DPSAbbrevRec abbrev[] = {
    {1, "add", "!+"},
    {10, "ashow", "!a"}, 
    {12, "awidthshow", "!A"}, 
    {13, "begin", "!B"}, 
    {14, "bind", "!b"}, 
    {20, "clip", "!CL"}, 
    {22, "closepath", "!c"}, 
    {23, "concat", "!CC"}, 
    {25, "copy", "!CP"},
    {39, "currentmatrix", "!CM"}, 
    {40, "currentpoint", "!Cp"}, 
    {43, "curveto", "!C"}, 
    {51, "def", "!D"},
    {53, "dict", "!d"},
    {62, "exch", "!E"}, 
    {63, "exec", "!e"}, 
    {66, "fill", "!F"}, 
    {67, "findfont", "!f"}, 
    {77, "grestore", "!G"}, 
    {78, "gsave", "!g"}, 
    {85, "ifelse", "!I"}, 
    {88, "index", "!i"}, 
    {98, "length", "!LE"}, 
    {99, "lineto", "!L"},
    {100, "load", "!l"},
    {103, "makefont", "!MF"}, 
    {104, "matrix", "!m"}, 
    {107, "moveto", "!M"}, 
    {111, "newpath", "!N"}, 
    {113, "null", "!n"},
    {117, "pop", "!p"},
    {122, "rcurveto", "!RC"}, 
    {127, "rectclip", "!R"}, 
    {128, "rectfill", "!RF"}, 
    {129, "rectstroke", "!RS"}, 
    {131, "repeat", "!RP"}, 
    {132, "restore", "!Rs"}, 
    {133, "rlineto", "!r"}, 
    {134, "rmoveto", "!RM"}, 
    {135, "roll", "!RL"}, 
    {136, "rotate", "!RO"}, 
    {137, "round", "!RN"}, 
    {138, "save", "!SV"}, 
    {139, "scale", "!SC"}, 
    {140, "scalefont", "!SF"}, 
    {142, "selectfont", "!s"}, 
    {147, "setcmykcolor", "!Sc"}, 
    {148, "setdash", "!SD"}, 
    {149, "setfont", "!Sf"}, 
    {150, "setgray", "!Sg"}, 
    {152, "sethsbcolor", "!Sh"}, 
    {153, "setlinecap", "!SL"}, 
    {154, "setlinejoin", "!SJ"}, 
    {155, "setlinewidth", "!SW"}, 
    {156, "setmatrix", "!SM"}, 
    {157, "setrgbcolor", "!Sr"}, 
    {158, "setshared", "!SS"}, 
    {160, "show", "!S"}, 
    {161, "showpage", "!SP"}, 
    {167, "stroke", "!ST"}, 
    {170, "systemdict", "!Sd"}, 
    {173, "translate", "!T"}, 
    {182, "userdict", "!u"}, 
    {186, "where", "!w"}, 
    {187, "widthshow", "!W"}, 
    {194, "xshow", "!X"}, 
    {195, "xyshow", "!x"}, 
    {196, "yshow", "!Y"}, 
};

static short abbrevPtr[] = {
    -1,	/* 0 */
    0,	/* 1 */
    -1,	/* 2 */
    -1,	/* 3 */
    -1,	/* 4 */
    -1,	/* 5 */
    -1,	/* 6 */
    -1,	/* 7 */
    -1,	/* 8 */
    -1,	/* 9 */
    1,	/* 10 */
    -1,	/* 11 */
    2,	/* 12 */
    3,	/* 13 */
    4,	/* 14 */
    -1,	/* 15 */
    -1,	/* 16 */
    -1,	/* 17 */
    -1,	/* 18 */
    -1,	/* 19 */
    5,	/* 20 */
    -1,	/* 21 */
    6,	/* 22 */
    7,	/* 23 */
    -1,	/* 24 */
    8,	/* 25 */
    -1,	/* 26 */
    -1,	/* 27 */
    -1,	/* 28 */
    -1,	/* 29 */
    -1,	/* 30 */
    -1,	/* 31 */
    -1,	/* 32 */
    -1,	/* 33 */
    -1,	/* 34 */
    -1,	/* 35 */
    -1,	/* 36 */
    -1,	/* 37 */
    -1,	/* 38 */
    9,	/* 39 */
    10,	/* 40 */
    -1,	/* 41 */
    -1,	/* 42 */
    11,	/* 43 */
    -1,	/* 44 */
    -1,	/* 45 */
    -1,	/* 46 */
    -1,	/* 47 */
    -1,	/* 48 */
    -1,	/* 49 */
    -1,	/* 50 */
    12,	/* 51 */
    -1,	/* 52 */
    13,	/* 53 */
    -1,	/* 54 */
    -1,	/* 55 */
    -1,	/* 56 */
    -1,	/* 57 */
    -1,	/* 58 */
    -1,	/* 59 */
    -1,	/* 60 */
    -1,	/* 61 */
    14,	/* 62 */
    15,	/* 63 */
    -1,	/* 64 */
    -1,	/* 65 */
    16,	/* 66 */
    17,	/* 67 */
    -1,	/* 68 */
    -1,	/* 69 */
    -1,	/* 70 */
    -1,	/* 71 */
    -1,	/* 72 */
    -1,	/* 73 */
    -1,	/* 74 */
    -1,	/* 75 */
    -1,	/* 76 */
    18,	/* 77 */
    19,	/* 78 */
    -1,	/* 79 */
    -1,	/* 80 */
    -1,	/* 81 */
    -1,	/* 82 */
    -1,	/* 83 */
    -1,	/* 84 */
    20,	/* 85 */
    -1,	/* 86 */
    -1,	/* 87 */
    21,	/* 88 */
    -1,	/* 89 */
    -1,	/* 90 */
    -1,	/* 91 */
    -1,	/* 92 */
    -1,	/* 93 */
    -1,	/* 94 */
    -1,	/* 95 */
    -1,	/* 96 */
    -1,	/* 97 */
    22,	/* 98 */
    23,	/* 99 */
    24,	/* 100 */
    -1,	/* 101 */
    -1,	/* 102 */
    25,	/* 103 */
    26,	/* 104 */
    -1,	/* 105 */
    -1,	/* 106 */
    27,	/* 107 */
    -1,	/* 108 */
    -1,	/* 109 */
    -1,	/* 110 */
    28,	/* 111 */
    -1,	/* 112 */
    29,	/* 113 */
    -1,	/* 114 */
    -1,	/* 115 */
    -1,	/* 116 */
    30,	/* 117 */
    -1,	/* 118 */
    -1,	/* 119 */
    -1,	/* 120 */
    -1,	/* 121 */
    31,	/* 122 */
    -1,	/* 123 */
    -1,	/* 124 */
    -1,	/* 125 */
    -1,	/* 126 */
    32,	/* 127 */
    33,	/* 128 */
    34,	/* 129 */
    -1,	/* 130 */
    35,	/* 131 */
    36,	/* 132 */
    37,	/* 133 */
    38,	/* 134 */
    39,	/* 135 */
    40,	/* 136 */
    41,	/* 137 */
    42,	/* 138 */
    43,	/* 139 */
    44,	/* 140 */
    -1,	/* 141 */
    45,	/* 142 */
    -1,	/* 143 */
    -1,	/* 144 */
    -1,	/* 145 */
    -1,	/* 146 */
    46,	/* 147 */
    47,	/* 148 */
    48,	/* 149 */
    49,	/* 150 */
    -1,	/* 151 */
    50,	/* 152 */
    51,	/* 153 */
    52,	/* 154 */
    53,	/* 155 */
    54,	/* 156 */
    55,	/* 157 */
    56,	/* 158 */
    -1,	/* 159 */
    57,	/* 160 */
    58,	/* 161 */
    -1,	/* 162 */
    -1,	/* 163 */
    -1,	/* 164 */
    -1,	/* 165 */
    -1,	/* 166 */
    59,	/* 167 */
    -1,	/* 168 */
    -1,	/* 169 */
    60,	/* 170 */
    -1,	/* 171 */
    -1,	/* 172 */
    61,	/* 173 */
    -1,	/* 174 */
    -1,	/* 175 */
    -1,	/* 176 */
    -1,	/* 177 */
    -1,	/* 178 */
    -1,	/* 179 */
    -1,	/* 180 */
    -1,	/* 181 */
    62,	/* 182 */
    -1,	/* 183 */
    -1,	/* 184 */
    -1,	/* 185 */
    63,	/* 186 */
    64,	/* 187 */
    -1,	/* 188 */
    -1,	/* 189 */
    -1,	/* 190 */
    -1,	/* 191 */
    -1,	/* 192 */
    -1,	/* 193 */
    65,	/* 194 */
    66,	/* 195 */
    67,	/* 196 */
    -1,	/* 197 */
    -1,	/* 198 */
    -1,	/* 199 */
    -1,	/* 200 */
    -1,	/* 201 */
    -1,	/* 202 */
    -1,	/* 203 */
    -1,	/* 204 */
    -1,	/* 205 */
    -1,	/* 206 */
    -1,	/* 207 */
    -1,	/* 208 */
    -1,	/* 209 */
    -1,	/* 210 */
    -1,	/* 211 */
};

void DPSFetchAbbrevList(DPSAbbrevRec **list, int *count)
{
    *list = abbrev;
    *count = sizeof(abbrev) / sizeof(abbrev[0]);
}

char *DPSGetSysnameAbbrev(int n)
{
    if ((unsigned) n > sizeof(abbrevPtr) / sizeof(abbrevPtr[0])) return NULL;
    if (abbrevPtr[n] == -1) return NULL;
    return abbrev[abbrevPtr[n]].abbrev;
}

char *DPSGetOperatorAbbrev(char *op)
{
    int min, max, n;
    int res;

    min = 0;
    max = sizeof(abbrev) / sizeof(abbrev[0]) - 1;

    while (min <= max) {
	n = (max + min) / 2;
	res = strcmp(op, abbrev[n].operatorName);
	if (res == 0) return abbrev[n].abbrev;
	if (res < 0) max = n - 1;
	if (res > 0) min = n + 1;
    }
    return NULL;
}
