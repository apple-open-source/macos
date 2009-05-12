/*
 * Public header file.
 * Copyright (c) 1995-1998 Markku Rossi.
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

#ifndef AFM_H
#define AFM_H

#ifndef ___P
#if PROTOTYPES
#define ___P(protos) protos
#else /* no PROTOTYPES */
#define ___P(protos) ()
#endif /* no PROTOTYPES */
#endif

/**********************************************************************
 *
 * 	AFM file entities.
 *
 **********************************************************************/

/* AFM types. */

#define AFM_TYPE_STRING		1
#define AFM_TYPE_NAME		2
#define AFM_TYPE_NUMBER		3
#define AFM_TYPE_INTEGER	4
#define AFM_TYPE_ARRAY		5
#define AFM_TYPE_BOOLEAN	6

typedef char *AFMString;
typedef char *AFMName;
typedef double AFMNumber;
typedef long AFMInteger;

typedef enum
{
  AFMFalse = 0,
  AFMTrue = 1
} AFMBoolean;

typedef struct afm_array_st *AFMArray;

struct afm_node_st
{
  int type;

  union
    {
      AFMString string;
      AFMName name;
      AFMNumber number;
      AFMInteger integer;
      AFMArray array;
      AFMBoolean boolean;
    } u;
};

typedef struct afm_node_st AFMNode;

struct afm_array_st
{
  AFMNumber num_items;
  AFMNode *items;
};


/* AFM file data structures. */

/*
 * Sorry!  I know that those mixed case variable names are ugly,
 * but this is the way how they are written in Adobe's document
 * so I think that this is the best way for documentary reasons.
 */

/* Global Font Information*/
struct global_font_information_st
{
  AFMString FontName;
  AFMString FullName;
  AFMString FamilyName;
  AFMString Weight;
  AFMNumber FontBBox_llx;
  AFMNumber FontBBox_lly;
  AFMNumber FontBBox_urx;
  AFMNumber FontBBox_ury;
  AFMString Version;
  AFMString Notice;
  AFMString EncodingScheme;
  AFMInteger MappingScheme;
  AFMInteger EscChar;
  AFMString CharacterSet;
  AFMInteger Characters;
  AFMBoolean IsBaseFont;
  AFMNumber VVector_0;
  AFMNumber VVector_1;
  AFMBoolean IsFixedV;
  AFMNumber CapHeight;
  AFMNumber XHeight;
  AFMNumber Ascender;
  AFMNumber Descender;
  AFMArray BlendAxisTypes;
  AFMArray BlendDesignPositions;
  AFMArray BlendDesignMap;
  AFMArray WeightVector;
};

typedef struct global_font_information_st AFMGlobalFontInformation;


/* Writing Direction Metrics. */
struct writing_direction_metrics_st
{
  AFMBoolean is_valid;
  AFMNumber UnderlinePosition;
  AFMNumber UnderlineThickness;
  AFMNumber ItalicAngle;
  AFMNumber CharWidth_x;
  AFMNumber CharWidth_y;
  AFMBoolean IsFixedPitch;
};

typedef struct writing_direction_metrics_st AFMWritingDirectionMetrics;


/* Multiple Master Axis Information. */
struct multiple_master_axis_info_st
{
  AFMString AxisType;
  AFMString AxisLabel;
};

typedef struct multiple_master_axis_info_st AFMMultipleMasterAxisInformation;


/* Individual Character Metrics. */

struct ligature_st
{
  AFMName successor;
  AFMName ligature;
};

typedef struct ligature_st AFMLigature;

/* Single individual character. */
struct individual_character_metrics_st
{
  AFMInteger character_code;	/* default charcode (-1 if not encoded) */
  AFMNumber w0x;		/* character width x in writing direction 0 */
  AFMNumber w0y;		/* character width y in writing direction 0 */
  AFMNumber w1x;		/* character width x in writing direction 1 */
  AFMNumber w1y;		/* character width y in writing direction 1 */
  AFMName name;			/* character name */
  AFMNumber vv_x;		/* local VVector x */
  AFMNumber vv_y;		/* local VVector y */

  /* character bounding box. */
  AFMNumber llx;
  AFMNumber lly;
  AFMNumber urx;
  AFMNumber ury;

  AFMNumber num_ligatures;
  AFMLigature *ligatures;
};

typedef struct individual_character_metrics_st AFMIndividualCharacterMetrics;


/* Kerning Data. */

/* Track Kerning Data. */
struct track_kern_st
{
  AFMInteger degree;
  AFMNumber min_ptsize;
  AFMNumber min_kern;
  AFMNumber max_ptsize;
  AFMNumber max_kern;
};

typedef struct track_kern_st AFMTrackKern;


/* Pair-Wise Kerning. */
struct pair_wise_kerning_st
{
  AFMName name1;
  AFMName name2;
  AFMNumber kx;
  AFMNumber ky;
};

typedef struct pair_wise_kerning_st AFMPairWiseKerning;


/* Composite fonts. */

/* Single composite component. */
struct composite_component_st
{
  AFMName name;
  AFMNumber deltax;
  AFMNumber deltay;
};

typedef struct composite_component_st AFMCompositeComponent;

struct composite_st
{
  AFMName name;
  AFMInteger num_components;
  AFMCompositeComponent *components;
};

typedef struct composite_st AFMComposite;


/**********************************************************************
 *
 * 	Library API.
 *
 **********************************************************************/

/* Constants. */

#define UNITS_PER_POINT	1000

/* Successful operation. */
#define AFM_SUCCESS	0

/*
 * AFM information levels.  The AFM libarary returns always Global
 * Font information, Writing Direction Metrics and Individual
 * Character Metrics.  Other fields can be retrieved by defining some
 * of the following flags to afm_open_{font, file}() functions.
 */
#define AFM_I_MINIMUM		0x00
#define AFM_I_COMPOSITES	0x01
#define AFM_I_KERN_PAIRS	0x02
#define AFM_I_TRACK_KERNS	0x04
#define AFM_I_ALL		0xffffffff

/*
 * Flags for the encoding functions.
 */
#define AFM_ENCODE_ACCEPT_COMPOSITES	0x01

typedef unsigned int AFMError;

typedef struct afm_handle_st *AFMHandle;

/* Supported encoding types. */
typedef enum
{
  AFM_ENCODING_DEFAULT,		/* Font's default encoding. */
  AFM_ENCODING_ISO_8859_1,	/* ISO-8859-1 */
  AFM_ENCODING_ISO_8859_2,	/* ISO-8859-2 */
  AFM_ENCODING_ISO_8859_3,	/* ISO-8859-3 */
  AFM_ENCODING_ISO_8859_4,	/* ISO-8859-4 */
  AFM_ENCODING_ISO_8859_5,	/* ISO-8859-5 */
  AFM_ENCODING_ISO_8859_7,	/* ISO-8859-7 */
  AFM_ENCODING_ISO_8859_9,	/* ISO-8859-9 */
  AFM_ENCODING_ISO_8859_10,	/* ISO-8859-10 */
  AFM_ENCODING_IBMPC,		/* IBM PC */
  AFM_ENCODING_ASCII,		/* 7 bit ASCII */
  AFM_ENCODING_MAC,		/* Mac */
  AFM_ENCODING_VMS,		/* VMS multinational */
  AFM_ENCODING_HP8,		/* HP Roman-8 */
  AFM_ENCODING_KOI8		/* Adobe Standard Cyrillic Font KOI8 */
} AFMEncoding;

/* Special encoding types for individual characters. */
#define AFM_ENC_NONE		((void *) 0)
#define AFM_ENC_NON_EXISTENT 	((void *) 1)


/* AFM information for a single PostScript font. */

struct afm_font_st
{
  /* AFM Library's private data. */
  struct afm_font_private_data_st *private;

  AFMNumber version;		/* AFM format specification version number. */
  unsigned int info_level;

  /*
   * Current font encoding.  Following values are valid:
   *
   *   AFM_ENC_NONE		character is not encoded
   *   AFM_ENC_NON_EXISTENT	character is encoded, but font does not
   *				have the specified character
   *   <pointer to character's metrics>
   *				character is encoded and it exists in font
   */
  AFMIndividualCharacterMetrics *encoding[256];

  AFMGlobalFontInformation global_info;
  AFMWritingDirectionMetrics writing_direction_metrics[2];

  AFMInteger num_character_metrics;
  AFMIndividualCharacterMetrics *character_metrics;

  AFMInteger num_composites;
  AFMComposite *composites;

  AFMInteger num_kern_pairs;
  AFMPairWiseKerning *kern_pairs;

  AFMInteger num_track_kerns;
  AFMTrackKern *track_kerns;
};

typedef struct afm_font_st *AFMFont;



/*
 * Prototypes for public functions.
 */

/*
 * Format error <error> to human readable format to buffer <buf>.
 * Buffer must be long enough for error messages (256 bytes is good).
 */
void afm_error_to_string ___P ((AFMError error, char *buf));

/*
 * Open AFM library.  <path> specifies the search path for the AFM
 * files.  A handle to the library is returned in <handle_return>.
 */
AFMError afm_create ___P ((const char *path, unsigned int verbose_level,
			   AFMHandle *handle_return));

/*
 * Close AFM library handle <handle>.
 */
AFMError afm_destroy ___P ((AFMHandle handle));

/*
 * Set AFM library's verbose level to <level>.  Value 0 means "no output".
 */
AFMError afm_set_verbose ___P ((AFMHandle handle, unsigned int level));

/*
 * Return a prefix to the font <fontname>'s data.  Various font
 * resource file names can be constructed from the returned prefix:
 *   AFM	<prefix>.afm
 *   PFA	<prefix>.pfa
 *
 * Returned prefix belongs to AFM library, user should not modify it.
 */
AFMError afm_font_prefix ___P ((AFMHandle handle, const char *fontname,
				const char **prefix_return));

/*
 * Open font <name> and return font handle in <font_return>.
 */
AFMError afm_open_font ___P ((AFMHandle handle, unsigned int info_level,
			      const char *name, AFMFont *font_return));

/*
 * Open AFM file <filename> and return font handle in <font_return>.
 */
AFMError afm_open_file ___P ((AFMHandle handle, unsigned int info_level,
			      const char *filename, AFMFont *font_return));

/*
 * Open built-in default font (Courier).
 */
AFMError afm_open_default_font ___P ((AFMHandle handle, AFMFont *font_return));

/*
 * Close font <font>.
 */
AFMError afm_close_font ___P ((AFMFont font));

/*
 * Dump font information to file <fp>.
 */
void afm_font_dump ___P ((FILE *fp, AFMFont font));

/*
 * Return the width of the string <string, stringlen>.
 */
AFMError afm_font_stringwidth ___P ((AFMFont font, AFMNumber ptsize,
				     char *string, unsigned int stringlen,
				     AFMNumber *w0x_return,
				     AFMNumber *w0y_return));

/*
 * Return the width of the character <ch>.
 */
AFMError afm_font_charwidth ___P ((AFMFont font, AFMNumber ptsize,
				   char ch, AFMNumber *w0x_return,
				   AFMNumber *w0y_return));

/*
 * Encode character code <code> to print as character <name>.
 * If <name> is NULL, encoding is removed.  <flags> can contain
 * any combination of the AFM_ENCODE_* values.
 */
AFMError afm_font_encode ___P ((AFMFont font, unsigned char code, char *name,
				unsigned int flags));

/*
 * Apply encoding <enc> to font <font>.  <flags> can contain any
 * combination of the AFM_ENCODE_* values.
 */
AFMError afm_font_encoding ___P ((AFMFont font, AFMEncoding enc,
				  unsigned int flags));

#endif /* not AFM_H */
