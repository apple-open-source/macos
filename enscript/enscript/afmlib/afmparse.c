/*
 * Parse AFM files.
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

#include "afmint.h"
#include "afm.h"

/*
 * Definitions.
 */

#define ISSPACE(ch) \
  ((ch) == ' ' || (ch) == '\n' || (ch) == '\r' || (ch) == '\t' || (ch) == ';')

#define GET_VALUE(typenum) get_type (handle, ctx, (typenum), &node)

struct parse_ctx_st
{
  FILE *fp;
  char token[1024];		/* maximum line length is 255, this should be
				   enought */
  unsigned int tokenlen;	/* length of the token */
};

typedef struct parse_ctx_st ParseCtx;

/*
 * Static variables.
 */

/*
 * The AFM keys.  This array must be kept sorted because keys are
 * searched by using binary search.
 */
static struct keyname_st
{
  char *name;
  AFMKey key;
} keynames[] =
{
  {"Ascender", 			kAscender},
  {"Axes", 			kAxes},
  {"AxisLabel", 		kAxisLabel},
  {"AxisType", 			kAxisType},
  {"B",				kB},
  {"BlendAxisTypes", 		kBlendAxisTypes},
  {"BlendDesignMap", 		kBlendDesignMap},
  {"BlendDesignPositions", 	kBlendDesignPositions},
  {"C",				kC},
  {"CC", 			kCC},
  {"CH",			kCH},
  {"CapHeight", 		kCapHeight},
  {"CharWidth", 		kCharWidth},
  {"CharacterSet", 		kCharacterSet},
  {"Characters", 		kCharacters},
  {"Comment", 			kComment},
  {"Descendents", 		kDescendents},
  {"Descender", 		kDescender},
  {"EncodingScheme", 		kEncodingScheme},
  {"EndAxis", 			kEndAxis},
  {"EndCharMetrics", 		kEndCharMetrics},
  {"EndCompFontMetrics", 	kEndCompFontMetrics},
  {"EndComposites", 		kEndComposites},
  {"EndDescendent", 		kEndDescendent},
  {"EndDirection", 		kEndDirection},
  {"EndFontMetrics", 		kEndFontMetrics},
  {"EndKernData", 		kEndKernData},
  {"EndKernPairs",		kEndKernPairs},
  {"EndMaster", 		kEndMaster},
  {"EndMasterFontMetrics", 	kEndMasterFontMetrics},
  {"EndTrackKern", 		kEndTrackKern},
  {"EscChar", 			kEscChar},
  {"FamilyName", 		kFamilyName},
  {"FontBBox", 			kFontBBox},
  {"FontName", 			kFontName},
  {"FullName", 			kFullName},
  {"IsBaseFont", 		kIsBaseFont},
  {"IsFixedPitch", 		kIsFixedPitch},
  {"IsFixedV", 			kIsFixedV},
  {"ItalicAngle", 		kItalicAngle},
  {"KP", 			kKP},
  {"KPH", 			kKPH},
  {"KPX", 			kKPX},
  {"KPY", 			kKPY},
  {"L",				kL},
  {"MappingScheme", 		kMappingScheme},
  {"Masters", 			kMasters},
  {"MetricsSets", 		kMetricsSets},
  {"N",				kN},
  {"Notice", 			kNotice},
  {"PCC", 			kPCC},
  {"StartAxis", 		kStartAxis},
  {"StartCharMetrics", 		kStartCharMetrics},
  {"StartCompFontMetrics", 	kStartCompFontMetrics},
  {"StartComposites", 		kStartComposites},
  {"StartDescendent", 		kStartDescendent},
  {"StartDirection", 		kStartDirection},
  {"StartFontMetrics", 		kStartFontMetrics},
  {"StartKernData", 		kStartKernData},
  {"StartKernPairs",		kStartKernPairs},
  {"StartMaster", 		kStartMaster},
  {"StartMasterFontMetrics", 	kStartMasterFontMetrics},
  {"StartTrackKern", 		kStartTrackKern},
  {"TrackKern", 		kTrackKern},
  {"UnderlinePosition", 	kUnderlinePosition},
  {"UnderlineThickness", 	kUnderlineThickness},
  {"VV",			kVV},
  {"VVector", 			kVVector},
  {"Version", 			kVersion},
  {"W",				kW},
  {"W0",			kW0},
  {"W0X",			kW0X},
  {"W0Y",			kW0Y},
  {"W1",			kW1},
  {"W1X",			kW1X},
  {"W1Y",			kW1Y},
  {"WX",			kWX},
  {"WY",			kWY},
  {"Weight", 			kWeight},
  {"WeightVector", 		kWeightVector},
  {"XHeight", 			kXHeight},

  {NULL, 0},
};

#define NUM_KEYS (sizeof (keynames) / sizeof (struct keyname_st) - 1)

/*
 * Prototypes for static functions.
 */

/* Throw parse error <error>.  Never returns. */
static void parse_error ___P ((AFMHandle handle, AFMError error));

static int get_token ___P ((AFMHandle handle, ParseCtx *ctx));
static int get_line_token ___P ((AFMHandle handle, ParseCtx *ctx));
static void get_key ___P ((AFMHandle handle, ParseCtx *ctx,
			   AFMKey *key_return));
static void get_type ___P ((AFMHandle handle, ParseCtx *ctx, int type,
			    AFMNode *type_return));
static void read_character_metrics ___P ((AFMHandle handle, ParseCtx *ctx,
					  AFMFont font));
static void read_kern_pairs ___P ((AFMHandle handle, ParseCtx *ctx,
				   AFMFont font));
static void read_track_kerns ___P ((AFMHandle handle, ParseCtx *ctx,
				    AFMFont font));
static void read_composites ___P ((AFMHandle handle, ParseCtx *ctx,
				   AFMFont font));

/*
 * Global functions.
 */

void
afm_parse_file (AFMHandle handle, const char *filename, AFMFont font)
{
  AFMKey key;
  AFMNode node;
  ParseCtx context;
  ParseCtx *ctx = &context;
  int wd = 0;			/* Writing direction. */
  int done = 0;

  ctx->fp = fopen (filename, "r");
  if (ctx->fp == NULL)
    parse_error (handle, SYSERROR (AFM_ERROR_FILE_IO));

  /* Check that file is really an AFM file. */

  get_key (handle, ctx, &key);
  if (key != kStartFontMetrics)
    parse_error (handle, AFM_ERROR_NOT_AFM_FILE);
  GET_VALUE (AFM_TYPE_NUMBER);
  font->version = node.u.number;

  /* Parse it. */
  while (!done)
    {
      get_key (handle, ctx, &key);
      switch (key)
	{
	case kComment:
	  (void) get_line_token (handle, ctx);
	  continue;
	  break;

	  /* File structure. */

	case kStartFontMetrics:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->version = node.u.number;
	  break;

	case kEndFontMetrics:
	  done = 1;
	  break;

	case kStartCompFontMetrics:
	case kEndCompFontMetrics:
	case kStartMasterFontMetrics:
	case kEndMasterFontMetrics:
	  parse_error (handle, AFM_ERROR_UNSUPPORTED_FORMAT);
	  break;

	  /* Global font information. */
	case kFontName:
	  GET_VALUE (AFM_TYPE_STRING);
	  font->global_info.FontName = node.u.string;
	  break;

	case kFullName:
	  GET_VALUE (AFM_TYPE_STRING);
	  font->global_info.FullName = node.u.string;
	  break;

	case kFamilyName:
	  GET_VALUE (AFM_TYPE_STRING);
	  font->global_info.FamilyName = node.u.string;
	  break;

	case kWeight:
	  GET_VALUE (AFM_TYPE_STRING);
	  font->global_info.Weight = node.u.string;
	  break;

	case kFontBBox:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->global_info.FontBBox_llx = node.u.number;
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->global_info.FontBBox_lly = node.u.number;
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->global_info.FontBBox_urx = node.u.number;
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->global_info.FontBBox_ury = node.u.number;
	  break;

	case kVersion:
	  GET_VALUE (AFM_TYPE_STRING);
	  font->global_info.Version = node.u.string;
	  break;

	case kNotice:
	  GET_VALUE (AFM_TYPE_STRING);
	  font->global_info.Notice = node.u.string;
	  break;

	case kEncodingScheme:
	  GET_VALUE (AFM_TYPE_STRING);
	  font->global_info.EncodingScheme = node.u.string;
	  break;

	case kMappingScheme:
	  GET_VALUE (AFM_TYPE_INTEGER);
	  font->global_info.MappingScheme = node.u.integer;
	  break;

	case kEscChar:
	  GET_VALUE (AFM_TYPE_INTEGER);
	  font->global_info.EscChar = node.u.integer;
	  break;

	case kCharacterSet:
	  GET_VALUE (AFM_TYPE_STRING);
	  font->global_info.CharacterSet = node.u.string;
	  break;

	case kCharacters:
	  GET_VALUE (AFM_TYPE_INTEGER);
	  font->global_info.Characters = node.u.integer;
	  break;

	case kIsBaseFont:
	  GET_VALUE (AFM_TYPE_BOOLEAN);
	  font->global_info.IsBaseFont = node.u.boolean;
	  break;

	case kVVector:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->global_info.VVector_0 = node.u.number;
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->global_info.VVector_1 = node.u.number;
	  break;

	case kIsFixedV:
	  GET_VALUE (AFM_TYPE_BOOLEAN);
	  font->global_info.IsFixedV = node.u.boolean;
	  break;

	case kCapHeight:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->global_info.CapHeight = node.u.number;
	  break;

	case kXHeight:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->global_info.XHeight = node.u.number;
	  break;

	case kAscender:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->global_info.Ascender = node.u.number;
	  break;

	case kDescender:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->global_info.Descender = node.u.number;
	  break;

	  /* Writing directions. */
	case kStartDirection:
	  GET_VALUE (AFM_TYPE_INTEGER);
	  wd = node.u.integer;
	  font->writing_direction_metrics[wd].is_valid = AFMTrue;
	  break;

	case kUnderlinePosition:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->writing_direction_metrics[wd].UnderlinePosition
	    = node.u.number;
	  break;

	case kUnderlineThickness:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->writing_direction_metrics[wd].UnderlineThickness
	    = node.u.number;
	  break;

	case kItalicAngle:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->writing_direction_metrics[wd].ItalicAngle = node.u.number;
	  break;

	case kCharWidth:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->writing_direction_metrics[wd].CharWidth_x = node.u.number;
	  GET_VALUE (AFM_TYPE_NUMBER);
	  font->writing_direction_metrics[wd].CharWidth_y = node.u.number;
	  break;

	case kIsFixedPitch:
	  GET_VALUE (AFM_TYPE_BOOLEAN);
	  font->writing_direction_metrics[wd].IsFixedPitch = node.u.boolean;
	  break;

	case kEndDirection:
	  break;

	  /* Individual Character Metrics. */
	case kStartCharMetrics:
	  GET_VALUE (AFM_TYPE_INTEGER);
	  font->num_character_metrics = node.u.integer;
	  font->character_metrics
	    = ((AFMIndividualCharacterMetrics *)
	       calloc (font->num_character_metrics + 1,
		       sizeof (AFMIndividualCharacterMetrics)));
	  if (font->character_metrics == NULL)
	    parse_error (handle, AFM_ERROR_MEMORY);

	  read_character_metrics (handle, ctx, font);
	  break;

	  /* Kerning Data. */
	case kStartKernData:
	  break;

	case kStartKernPairs:
	  if (font->info_level & AFM_I_KERN_PAIRS)
	    {
	      GET_VALUE (AFM_TYPE_INTEGER);
	      font->num_kern_pairs = node.u.integer;
	      font->kern_pairs =
		(AFMPairWiseKerning *) calloc (font->num_kern_pairs + 1,
					       sizeof (AFMPairWiseKerning));
	      if (font->kern_pairs == NULL)
		parse_error (handle, AFM_ERROR_MEMORY);

	      read_kern_pairs (handle, ctx, font);
	    }
	  else
	    {
	      do
		{
		  (void) get_line_token (handle, ctx);
		  get_key (handle, ctx, &key);
		}
	      while (key != kEndKernPairs);
	    }
	  break;

	case kStartTrackKern:
	  if (font->info_level & AFM_I_TRACK_KERNS)
	    {
	      GET_VALUE (AFM_TYPE_INTEGER);
	      font->num_track_kerns = node.u.integer;
	      font->track_kerns
		= (AFMTrackKern *) calloc (font->num_track_kerns + 1,
					   sizeof (AFMTrackKern));
	      if (font->track_kerns == NULL)
		parse_error (handle, AFM_ERROR_MEMORY);

	      read_track_kerns (handle, ctx, font);
	    }
	  else
	    {
	      do
		{
		  (void) get_line_token (handle, ctx);
		  get_key (handle, ctx, &key);
		}
	      while (key != kEndTrackKern);
	    }
	  break;

	case kEndKernData:
	  break;

	  /* Composite Character Data. */
	case kStartComposites:
	  if (font->info_level & AFM_I_COMPOSITES)
	    {
	      GET_VALUE (AFM_TYPE_INTEGER);
	      font->num_composites = node.u.integer;
	      font->composites
		= (AFMComposite *) calloc (font->num_composites + 1,
					   sizeof (AFMComposite));
	      if (font->composites == NULL)
		parse_error (handle, AFM_ERROR_MEMORY);

	      read_composites (handle, ctx, font);
	    }
	  else
	    {
	      do
		{
		  (void) get_line_token (handle, ctx);
		  get_key (handle, ctx, &key);
		}
	      while (key != kEndComposites);
	    }
	  break;

	default:
	  /* Ignore. */
	  break;
	}
    }
  fclose (ctx->fp);

  /* Check post conditions. */

  if (!font->writing_direction_metrics[0].is_valid
      && !font->writing_direction_metrics[1].is_valid)
    /* No direction specified, 0 implied. */
    font->writing_direction_metrics[0].is_valid = AFMTrue;

  /* Undef character. */
  if (!strhash_get (font->private->fontnames, "space", 5,
		    (void *) font->private->undef))
    {
      /* Character "space" is not defined.  Select the first one. */
      assert (font->num_character_metrics > 0);
      font->private->undef = &font->character_metrics[0];
    }

  /* Fixed pitch. */
  if (font->writing_direction_metrics[0].is_valid
      && font->writing_direction_metrics[0].IsFixedPitch)
    {
      /* Take one, it doesn't matter which one. */
      font->writing_direction_metrics[0].CharWidth_x
	= font->character_metrics[0].w0x;
      font->writing_direction_metrics[0].CharWidth_y
	= font->character_metrics[0].w0y;
    }
  if (font->writing_direction_metrics[1].is_valid
      && font->writing_direction_metrics[1].IsFixedPitch)
    {
      font->writing_direction_metrics[1].CharWidth_x
	= font->character_metrics[1].w1x;
      font->writing_direction_metrics[1].CharWidth_y
	= font->character_metrics[1].w1y;
    }
}


/*
 * Static functions.
 */

static void
parse_error (AFMHandle handle, AFMError error)
{
  handle->parse_error = error;
  longjmp (handle->jmpbuf, 1);

  /* If this is reached, then all is broken. */
  fprintf (stderr, "AFM: fatal internal longjmp() error.\n");
  abort ();
}


static int
get_token (AFMHandle handle, ParseCtx *ctx)
{
  int ch;
  int i;

  /* Skip the leading whitespace. */
  while ((ch = getc (ctx->fp)) != EOF)
    if (!ISSPACE (ch))
      break;

  if (ch == EOF)
    return 0;

  ungetc (ch, ctx->fp);

  /* Get name. */
  for (i = 0, ch = getc (ctx->fp);
       i < sizeof (ctx->token) && ch != EOF && !ISSPACE (ch);
       i++, ch = getc (ctx->fp))
    ctx->token[i] = ch;

  if (i >= sizeof (ctx->token))
    /* Line is too long, this is against AFM specification. */
    parse_error (handle, AFM_ERROR_SYNTAX);

  ctx->token[i] = '\0';
  ctx->tokenlen = i;

  return 1;
}


static int
get_line_token (AFMHandle handle, ParseCtx *ctx)
{
  int i, ch;

  /* Skip the leading whitespace. */
  while ((ch = getc (ctx->fp)) != EOF)
    if (!ISSPACE (ch))
      break;

  if (ch == EOF)
    return 0;

  ungetc (ch, ctx->fp);

  /* Read to the end of the line. */
  for (i = 0, ch = getc (ctx->fp);
       i < sizeof (ctx->token) && ch != EOF && ch != '\n';
       i++, ch = getc (ctx->fp))
    ctx->token[i] = ch;

  if (i >= sizeof (ctx->token))
    parse_error (handle, AFM_ERROR_SYNTAX);

  /* Skip all trailing whitespace. */
  for (i--; i >= 0 && ISSPACE (ctx->token[i]); i--)
    ;
  i++;

  ctx->token[i] = '\0';
  ctx->tokenlen = i;

  return 1;
}


static int
match_key (char *key)
{
  int lower = 0;
  int upper = NUM_KEYS;
  int midpoint, cmpvalue;
  AFMBoolean found = AFMFalse;

  while ((upper >= lower) && !found)
    {
      midpoint = (lower + upper) / 2;
      if (keynames[midpoint].name == NULL)
	break;

      cmpvalue = strcmp (key, keynames[midpoint].name);
      if (cmpvalue == 0)
	found = AFMTrue;
      else if (cmpvalue < 0)
	upper = midpoint - 1;
      else
	lower = midpoint + 1;
    }

  if (found)
    return keynames[midpoint].key;

  return -1;
}


static void
get_key (AFMHandle handle, ParseCtx *ctx, AFMKey *key_return)
{
  int key;
  char msg[256];

  while (1)
    {
      if (!get_token (handle, ctx))
	/* Unexpected EOF. */
	parse_error (handle, AFM_ERROR_SYNTAX);

      key = match_key (ctx->token);
      if (key >= 0)
	{
	  *key_return = key;
	  return;
	}

      /* No match found.  According to standard, we must skip this key. */
      sprintf (msg, "skipping key \"%s\"", ctx->token);
      afm_error (handle, msg);
      get_line_token (handle, ctx);
    }

  /* NOTREACHED */
}


/* Reader for AFM types. */
static void
get_type (AFMHandle handle, ParseCtx *ctx, int type, AFMNode *type_return)
{
  char buf[256];

  switch (type)
    {
    case AFM_TYPE_STRING:
      if (!get_line_token (handle, ctx))
	parse_error (handle, AFM_ERROR_SYNTAX);

      type_return->u.string = (AFMString) calloc (1, ctx->tokenlen + 1);
      if (type_return->u.string == NULL)
	parse_error (handle, AFM_ERROR_MEMORY);

      memcpy (type_return->u.string, ctx->token, ctx->tokenlen);
      break;

    case AFM_TYPE_NAME:
      if (!get_token (handle, ctx))
	parse_error (handle, AFM_ERROR_SYNTAX);

      type_return->u.name = (AFMName) calloc (1, ctx->tokenlen + 1);
      if (type_return->u.string == NULL)
	parse_error (handle, AFM_ERROR_MEMORY);

      memcpy (type_return->u.name, ctx->token, ctx->tokenlen);
      break;

    case AFM_TYPE_NUMBER:
      if (!get_token (handle, ctx))
	parse_error (handle, AFM_ERROR_SYNTAX);

      memcpy (buf, ctx->token, ctx->tokenlen);
      buf[ctx->tokenlen] = '\0';
      type_return->u.number = atof (buf);
      break;

    case AFM_TYPE_INTEGER:
      if (!get_token (handle, ctx))
	parse_error (handle, AFM_ERROR_SYNTAX);

      memcpy (buf, ctx->token, ctx->tokenlen);
      buf[ctx->tokenlen] = '\0';
      type_return->u.integer = atoi (buf);
      break;

    case AFM_TYPE_ARRAY:
      fprintf (stderr, "Array types not implemented yet.\n");
      abort ();
      break;

    case AFM_TYPE_BOOLEAN:
      if (!get_token (handle, ctx))
	parse_error (handle, AFM_ERROR_SYNTAX);

      memcpy (buf, ctx->token, ctx->tokenlen);
      buf[ctx->tokenlen] = '\0';

      if (strcmp (buf, "true") == 0)
	type_return->u.boolean = AFMTrue;
      else if (strcmp (buf, "false") == 0)
	type_return->u.boolean = AFMFalse;
      else
	parse_error (handle, AFM_ERROR_SYNTAX);
      break;

    default:
      fprintf (stderr, "get_type(): illegal type %d\n", type_return->type);
      abort ();
      break;
    }
}


static void
read_character_metrics (AFMHandle handle, ParseCtx *ctx, AFMFont font)
{
  int i = 0;
  AFMNode node;
  AFMIndividualCharacterMetrics *cm = NULL;
  AFMKey key;
  int done = 0;
  int first = 1;

  while (!done)
    {
      get_key (handle, ctx, &key);
      switch (key)
	{
	case kC:
	  if (first)
	    first = 0;
	  else
	    i++;
	  if (i >= font->num_character_metrics)
	    parse_error (handle, AFM_ERROR_SYNTAX);

	  cm = &font->character_metrics[i];
	  GET_VALUE (AFM_TYPE_INTEGER);
	  cm->character_code = node.u.integer;
	  if (cm->character_code >= 0 && cm->character_code <= 255)
	    font->encoding[cm->character_code] = cm;
	  break;

	case kCH:
	  printf ("* CH\n");
	  break;

	case kWX:
	case kW0X:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->w0x = node.u.number;
	  cm->w0y = 0.0;
	  break;

	case kW1X:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->w1x = node.u.number;
	  cm->w1y = 0.0;
	  break;

	case kWY:
	case kW0Y:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->w0y = node.u.number;
	  cm->w0x = 0.0;
	  break;

	case kW1Y:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->w1y = node.u.number;
	  cm->w1x = 0.0;
	  break;

	case kW:
	case kW0:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->w0x = node.u.number;
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->w0y = node.u.number;
	  break;

	case kW1:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->w1x = node.u.number;
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->w1y = node.u.number;
	  break;

	case kVV:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->vv_x = node.u.number;
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->vv_y = node.u.number;
	  break;

	case kN:
	  GET_VALUE (AFM_TYPE_NAME);
	  cm->name = node.u.name;
	  if (!strhash_put (font->private->fontnames, cm->name,
			    strlen (cm->name), cm, NULL))
	    parse_error (handle, AFM_ERROR_MEMORY);
	  break;

	case kB:
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->llx = node.u.number;
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->lly = node.u.number;
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->urx = node.u.number;
	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->ury = node.u.number;
	  break;

	case kL:
	  /* XXX Skip ligatures. */
	  get_line_token (handle, ctx);
	  break;

	case kEndCharMetrics:
	  if (i != font->num_character_metrics - 1)
	    {
	      /*
	       * My opinion is that this is a syntax error; the
	       * creator of this AFM file should have been smart
	       * enought to count these character metrics.  Well,
	       * maybe that is too much asked...
	       */
	      font->num_character_metrics = i + 1;
	    }

	  done = 1;
	  break;

	default:
	  parse_error (handle, AFM_ERROR_SYNTAX);
	  break;
	}
    }
}


static void
read_kern_pairs (AFMHandle handle, ParseCtx *ctx, AFMFont font)
{
  int i;
  AFMNode node;
  AFMPairWiseKerning *kp;
  AFMKey key;

  for (i = 0; i < font->num_kern_pairs; i++)
    {
      kp = &font->kern_pairs[i];
      get_key (handle, ctx, &key);

      switch (key)
	{
	case kKP:
	case kKPX:
	case kKPY:
	  GET_VALUE (AFM_TYPE_NAME);
	  kp->name1 = node.u.name;

	  GET_VALUE (AFM_TYPE_NAME);
	  kp->name2 = node.u.name;

	  GET_VALUE (AFM_TYPE_NUMBER);

	  switch (key)
	    {
	    case kKP:
	      kp->kx = node.u.number;
	      GET_VALUE (AFM_TYPE_NUMBER);
	      kp->ky = node.u.number;
	      break;

	    case kKPX:
	      kp->kx = node.u.number;
	      kp->ky = 0.0;
	      break;

	    case kKPY:
	      kp->ky = node.u.number;
	      kp->kx = 0.0;
	      break;

	    default:
	      fprintf (stderr, "AFM: fatal corruption\n");
	      abort ();
	      break;
	    }
	  break;

	case kKPH:
	  /* XXX ignore. */
	  break;

	default:
	  parse_error (handle, AFM_ERROR_SYNTAX);
	  break;
	}
    }

  /* Get end token. */
  get_key (handle, ctx, &key);
  if (key != kEndKernPairs)
    parse_error (handle, AFM_ERROR_SYNTAX);
}


static void
read_track_kerns (AFMHandle handle, ParseCtx *ctx, AFMFont font)
{
  int i;
  AFMNode node;
  AFMTrackKern *tk;
  AFMKey key;

  for (i = 0; i < font->num_kern_pairs; i++)
    {
      tk = &font->track_kerns[i];
      get_key (handle, ctx, &key);

      /* TrackKern degree min-ptsize min-kern max-ptrsize max-kern */

      if (key != kTrackKern)
	parse_error (handle, AFM_ERROR_SYNTAX);

      GET_VALUE (AFM_TYPE_INTEGER);
      tk->degree = node.u.integer;

      GET_VALUE (AFM_TYPE_NUMBER);
      tk->min_ptsize = node.u.number;

      GET_VALUE (AFM_TYPE_NUMBER);
      tk->min_kern = node.u.number;

      GET_VALUE (AFM_TYPE_NUMBER);
      tk->max_ptsize = node.u.number;

      GET_VALUE (AFM_TYPE_NUMBER);
      tk->max_kern = node.u.number;
    }

  /* Get end token. */
  get_key (handle, ctx, &key);
  if (key != kEndTrackKern)
    parse_error (handle, AFM_ERROR_SYNTAX);
}


static void
read_composites (AFMHandle handle, ParseCtx *ctx, AFMFont font)
{
  int i, j;
  AFMNode node;
  AFMComposite *cm;
  AFMKey key;

  for (i = 0; i < font->num_composites; i++)
    {
      cm = &font->composites[i];
      get_key (handle, ctx, &key);

      if (key != kCC)
	parse_error (handle, AFM_ERROR_SYNTAX);

      GET_VALUE (AFM_TYPE_NAME);
      cm->name = node.u.name;

      /* Create name -> AFMComposite mapping. */
      if (!strhash_put (font->private->compositenames, cm->name,
			strlen (cm->name), cm, NULL))
	parse_error (handle, AFM_ERROR_MEMORY);

      GET_VALUE (AFM_TYPE_INTEGER);
      cm->num_components = node.u.integer;
      cm->components
	= (AFMCompositeComponent *) calloc (cm->num_components + 1,
					    sizeof (AFMCompositeComponent));

      /* Read composite components. */
      for (j = 0; j < cm->num_components; j++)
	{
	  /* Read "PCC". */
	  get_key (handle, ctx, &key);
	  if (key != kPCC)
	    parse_error (handle, AFM_ERROR_SYNTAX);

	  /* Read values. */

	  GET_VALUE (AFM_TYPE_NAME);
	  cm->components[j].name = node.u.name;

	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->components[j].deltax = node.u.number;

	  GET_VALUE (AFM_TYPE_NUMBER);
	  cm->components[j].deltay = node.u.number;
	}
    }

  /* Get end token. */
  get_key (handle, ctx, &key);
  if (key != kEndComposites)
    parse_error (handle, AFM_ERROR_SYNTAX);
}
