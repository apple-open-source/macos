/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1990, NeXT, Inc.
 */

#include "NXCType.h"
#define FIXSIGNEDCHAR(i) if ((i & 0xFFFFFF80) == 0xFFFFFF80) i &= 0x000000FF

unsigned char *
NXToAscii(c)
	unsigned int c;
{
	static unsigned char woops[] = " ";

	FIXSIGNEDCHAR(c);
	if (c < 128) {		/* handle stuff already ASCII */
		woops[0] = c;
		return (unsigned char *) woops;
	}
	switch (c) {
		case 128:	/* Figspace */
			return (unsigned char *) " ";
		case 129:	/* Agrave */
		case 130:	/* Aacute */
		case 131:	/* Acircumflex */
		case 132:	/* Atilde */
		case 133:	/* Adieresis */
		case 134:	/* Aring */
			return (unsigned char *) "A";
		case 135:	/* Ccedilla */
			return (unsigned char *) "C";
		case 136:	/* Egrave */
		case 137:	/* Eacute */
		case 138:	/* Ecircumflex */
		case 139:	/* Edieresis */
			return (unsigned char *) "E";
		case 140:	/* Igrave */
		case 141:	/* Iacute */
		case 142:	/* Icircumflex */
		case 143:	/* Idieresis */
			return (unsigned char *) "I";
		case 145:	/* Ntilde */
			return (unsigned char *) "N";
		case 146:	/* Ograve */
		case 147:	/* Oacute */
		case 148:	/* Ocircumflex */
		case 149:	/* Otilde */
		case 150:	/* Odieresis */
		case 233:	/* Oslash */
			return (unsigned char *) "O";
		case 151:	/* Ugrave */
		case 152:	/* Uacute */
		case 153:	/* Ucircumflex */
		case 154:	/* Udieresis */
			return (unsigned char *) "U";
		case 155:	/* Yacute */
			return (unsigned char *) "Y";
		case 230:	/* eth */
		case 144:	/* Eth */
			return (unsigned char *) "TH";
		case 156:	/* Thorn */
		case 252:	/* thorn */
			return (unsigned char *) "th";

		case 174:	/* fi */
			return (unsigned char *) "fi";
		case 175:	/* fl */
			return (unsigned char *) "fl";

		case 213:	/* agrave */
		case 214:	/* aacute */
		case 215:	/* acircumflex */
		case 216:	/* atilde */
		case 217:	/* adieresis */
		case 218:	/* aring */
			return (unsigned char *) "a";
		case 219:	/* ccedilla */
			return (unsigned char *) "c";
		case 220:	/* egrave */
		case 221:	/* eacute */
		case 222:	/* ecircumflex */
		case 223:	/* edieresis */
			return (unsigned char *) "e";
		case 225:	/* AE */
			return (unsigned char *) "AE";
		case 224:	/* igrave */
		case 226:	/* iacute */
		case 228:	/* icircumflex */
		case 229:	/* idieresis */
			return (unsigned char *) "i";
		case 231:	/* ntilde */
			return (unsigned char *) "n";
		case 232:	/* Lslash */
			return (unsigned char *) "L";
		case 234:	/* OE */
			return (unsigned char *) "OE";
		case 236:	/* ograve */
		case 237:	/* oacute */
		case 238:	/* ocircumflex */
		case 239:	/* otilde */
		case 240:	/* odieresis */
		case 249:	/* oslash */
			return (unsigned char *) "o";
		case 241:	/* ae */
			return (unsigned char *) "ae";
		case 242:	/* ugrave */
		case 243:	/* uacute */
		case 244:	/* ucircumflex */
		case 246:	/* udieresis */
			return (unsigned char *) "u";
		case 245:	/* dotlessi */
			return (unsigned char *) "i";
		case 247:	/* yacute */
		case 253:	/* ydieresis */
			return (unsigned char *) "y";
		case 248:	/* lslash */
			return (unsigned char *) "l";
		case 250:	/* oe */
			return (unsigned char *) "oe";
		case 251:	/* germandbls */
			return (unsigned char *) "ss";
	        /*
		 * non-letter cases:
		 */
		case 158:	/* multiply */		
			return (unsigned char *) "x";
		case 159:	/* divide */		
			return (unsigned char *) "/";

		case 161:	/* exclamdown */	
			return (unsigned char *) "!";
		case 169:	/* quotesingle */	
			return (unsigned char *) "'";

		case 170:	/* quotedblleft */
		case 186:	/* quotedblright */
		case 185:	/* quotedblbase */
			return (unsigned char *) "\"";
		case 171:	/* guillemotleft */
			return (unsigned char *) "<<";
		case 187:	/* guillemotright */
			return (unsigned char *) ">>";
		case 184:	/* quotesinglbase */
			return (unsigned char *) "'";
		case 172:	/* guilsinglleft */
			return (unsigned char *) "<";
		case 173:	/* guilsinglright */
			return (unsigned char *) ">";
		case 180:	/* periodcentered */
			return (unsigned char *) ".";
		case 181:	/* brokenbar */
			return (unsigned char *) "|";
		case 183:	/* bullet */
			return (unsigned char *) "*";
		case 188:	/* ellipsis */
			return (unsigned char *) "...";
		case 191:	/* questiondown */
			return (unsigned char *) "?";
		case 192:	/* onesuperior */
			return (unsigned char *) "1";
		case 201:	/* twosuperior */
			return (unsigned char *) "2";
		case 204:	/* threesuperior */
			return (unsigned char *) "3";
		case 208:	/* emdash */
			return (unsigned char *) "--";
		case 209:	/* plusminus */
			return (unsigned char *) "+-";
		case 210:	/* onequarter */
			return (unsigned char *) "1/4";
		case 211:	/* onehalf */
			return (unsigned char *) "1/2";
		case 212:	/* threequarters */
			return (unsigned char *) "3/4";
		case 227:	/* ordfeminine */
			return (unsigned char *) "a";
		case 235:	/* ordmasculine */
			return (unsigned char *) "o";
		case 157:	/* mu */
			return (unsigned char *) "u";
		case 160:	/* copyright */
			return (unsigned char *) "(C)";
		case 163:	/* sterling */
			return (unsigned char *) "L";
		case 164:	/* fraction */
			return (unsigned char *) "/";
		case 165:	/* yen */
			return (unsigned char *) "Y";
		case 166:	/* florin */
			return (unsigned char *) "f";
		case 176:	/* registered */
			return (unsigned char *) "(R)";
		case 190:	/* logicalnot */
		case 177:	/* endash */
			return (unsigned char *) "-";
		case 178:	/* dagger */
			return (unsigned char *) "+";
		case 179:	/* daggerdbl */
			return (unsigned char *) "++";
		case 189:	/* perthousand */
			return (unsigned char *) "0/00";
		case 193:	/* grave */
			return (unsigned char *) "`";
		case 194:	/* acute */
			return (unsigned char *) "'";
		case 195:	/* circumflex */
			return (unsigned char *) "^";
		case 196:	/* tilde */
			return (unsigned char *) "~";

		/*
		 * Default & fallback cases:
		 */
		case 162:	/* cent */
		case 167:	/* section */
		case 168:	/* currency */
		case 182:	/* paragraph */
		case 197:	/* macron */
		case 198:	/* breve */
		case 199:	/* dotaccent */
		case 200:	/* dieresis */
		case 202:	/* ring */
		case 203:	/* cedilla */
		case 205:	/* hungarumlaut */
		case 206:	/* ogonek */
		case 207:	/* caron */
		default:
			return (unsigned char *) "_";
	}
}
