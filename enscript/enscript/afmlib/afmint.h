/*
 * Internal header for the AFM library.
 * Copyright (c) 1995-1999 Markku Rossi.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef AFMINT_H
#define AFMINT_H

/*
 * Config stuffs.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifndef ___P
#if PROTOTYPES
#define ___P(protos) protos
#else /* no PROTOTYPES */
#define ___P(protos) ()
#endif /* no PROTOTYPES */
#endif

#if STDC_HEADERS

#include <stdlib.h>
#include <string.h>

#else /* no STDC_HEADERS */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#ifndef HAVE_STRCHR
#define strchr index
#define strrchr rindex
#endif
char *strchr ();
char *strrchr ();

#ifndef HAVE_MEMCPY
#define memcpy(d, s, n) bcopy((s), (d), (n))
#endif

#ifndef HAVE_STRERROR
extern char *strerror ___P ((int));
#endif

#endif /* no STDC_HEADERS */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <setjmp.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "afm.h"
#include "strhash.h"


/*
 * Types and definitions.
 */

/* Error codes. */
#define AFM_ERROR			1
#define AFM_ERROR_MEMORY		2
#define AFM_ERROR_ARGUMENT		3
#define AFM_ERROR_UNKNOWN_FONT		4
#define AFM_ERROR_SYNTAX		5
#define AFM_ERROR_UNSUPPORTED_FORMAT	6
#define AFM_ERROR_FILE_IO		7
#define AFM_ERROR_NOT_AFM_FILE		8
#define NUM_ERRORS			9

/* Pack error and global errno. */
#define SYSERROR(code) (errno << 16 | (code))


/* Keys. */

typedef enum
{
  kComment,

  /* File structure. */
  kStartFontMetrics,
  kEndFontMetrics,
  kStartCompFontMetrics,
  kEndCompFontMetrics,
  kStartDescendent,
  kEndDescendent,
  kStartMasterFontMetrics,
  kEndMasterFontMetrics,

  /* Control information. */
  kMetricsSets,
  kDescendents,
  kMasters,
  kAxes,

  /* Global font information. */
  kFontName,
  kFullName,
  kFamilyName,
  kWeight,
  kFontBBox,
  kVersion,
  kNotice,
  kEncodingScheme,
  kMappingScheme,
  kEscChar,
  kCharacterSet,
  kCharacters,
  kIsBaseFont,
  kVVector,
  kIsFixedV,
  kCapHeight,
  kXHeight,
  kAscender,
  kDescender,
  kWeightVector,
  kBlendDesignPositions,
  kBlendDesignMap,
  kBlendAxisTypes,

  /* Writing direction information. */
  kStartDirection,
  kEndDirection,
  kUnderlinePosition,
  kUnderlineThickness,
  kItalicAngle,
  kCharWidth,
  kIsFixedPitch,

  /* Individual character metrics. */
  kStartCharMetrics,
  kEndCharMetrics,
  kC,
  kCH,
  kWX,
  kW0X,
  kW1X,
  kWY,
  kW0Y,
  kW1Y,
  kW,
  kW0,
  kW1,
  kVV,
  kN,
  kB,
  kL,

  /* Kerning data. */
  kStartKernData,
  kEndKernData,
  kStartTrackKern,
  kEndTrackKern,
  kTrackKern,
  kStartKernPairs,
  kEndKernPairs,
  kKP,
  kKPH,
  kKPX,
  kKPY,

  /* Composite character data. */
  kStartComposites,
  kEndComposites,
  kCC,
  kPCC,

  /* Axis information. */
  kStartAxis,
  kEndAxis,
  kAxisType,
  kAxisLabel,

  /* Master Design Information */
  kStartMaster,
  kEndMaster

} AFMKey;


struct afm_handle_st
{
  unsigned int verbose;		/* verbose level */
  StringHashPtr font_map;	/* fontname -> AFM filename mapping */

  /* Parse support. */
  jmp_buf jmpbuf;
  AFMError parse_error;		/* Error that caused longjmp(). */
};


/* Store library's private font data to this structure. */
struct afm_font_private_data_st
{
  /* Character that is used for undefined codes (' '). */
  AFMIndividualCharacterMetrics *undef;

  StringHashPtr fontnames;	/* fontname -> character info mapping */
  StringHashPtr compositenames;	/* composite -> AFMComposite mapping */
};


/*
 * Encoding tables.
 */

struct encoding_table_st
{
  int code;
  char *character;
};

typedef struct encoding_table_st AFMEncodingTable;

extern AFMEncodingTable afm_88591_encoding[];
extern AFMEncodingTable afm_88592_encoding[];
extern AFMEncodingTable afm_88593_encoding[];
extern AFMEncodingTable afm_88594_encoding[];
extern AFMEncodingTable afm_88595_encoding[];
extern AFMEncodingTable afm_88597_encoding[];
extern AFMEncodingTable afm_88599_encoding[];
extern AFMEncodingTable afm_885910_encoding[];
extern AFMEncodingTable afm_ibmpc_encoding[];
extern AFMEncodingTable afm_mac_encoding[];
extern AFMEncodingTable afm_vms_encoding[];
extern AFMEncodingTable afm_hp8_encoding[];
extern AFMEncodingTable afm_koi8_encoding[];


/*
 * Global help functions.
 */

/* Print message if <level> is larger than library's verbose level. */
void afm_message ___P ((AFMHandle handle, unsigned int level, char *message));

/* Print error message to stderr. */
void afm_error ___P ((AFMHandle handle, char *message));


/*
 * AFM file parsing
 */

/* Parse AFM file <filename> and fill up font <font>. */
void afm_parse_file ___P ((AFMHandle handle, const char *filename,
			   AFMFont font));

#endif /* not AFMINT_H */
