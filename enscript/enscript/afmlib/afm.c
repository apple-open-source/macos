/*
 * AFM library public interface.
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

#include "afmint.h"
#include "afm.h"

/*
 * Static variables
 */

static char *default_path = "/usr/local/lib/ps:/usr/lib/ps";

static char *error_names[] =
{
  "AFM Success",
  "AFM Error",
  "out of memory",
  "illegal argument",
  "unknown font",
  "syntax error",
  "unsupported format",
  "file IO failed",
  "file is not an AFM file",
};

/*
 * Prototypes for static functions.
 */

static void read_font_map ___P ((AFMHandle handle, char *name));
static void apply_encoding ___P ((AFMFont font, AFMEncodingTable *enc,
				  unsigned int flags));


/*
 * Global functions.
 */

void
afm_error_to_string (AFMError error, char *buf)
{
  char *syserr;
  int code, syserrno;

  code = error & 0xffff;
  syserrno = (error >> 16) & 0xffff;

  if (syserrno)
    syserr = strerror (syserrno);
  else
    syserr = NULL;

  if (code >= NUM_ERRORS)
    {
      sprintf (buf, "afm_error_to_string(): illegal error code: %d\n",
	       error);
      return;
    }

  if (code == 0)
    sprintf (buf, "AFM Success");
  else if (code == 1)
    sprintf (buf, "%s%s%s", "AFM Error",
	     syserr ? ":" : "",
	     syserr ? syserr : "");
  else
    sprintf (buf, "AFM Error: %s%s%s", error_names[code],
	     syserr ? ": " : "",
	     syserr ? syserr : "");
}


AFMError
afm_create (const char *path, unsigned int verbose_level,
	    AFMHandle *handle_return)
{
  AFMHandle handle;
  AFMError error = AFM_SUCCESS;
  const char *cp, *cp2;
  int len;
  char buf[512];
  struct stat stat_st;

  /* Init handle. */

  handle = (AFMHandle) calloc (1, sizeof (*handle));
  if (handle == NULL)
    {
      error = AFM_ERROR_MEMORY;
      goto error_out;
    }

  handle->font_map = strhash_init ();
  if (handle->font_map == NULL)
    {
      error = AFM_ERROR_MEMORY;
      goto error_out;
    }

  handle->verbose = verbose_level;

  /* Traverse path. */

  if (path == NULL)
    path = default_path;

  afm_message (handle, 1, "AFM: scanning path...\n");
  for (cp = path; cp; cp = strchr (cp, PATH_SEPARATOR))
    {
      if (cp != path)
	cp++;

      cp2 = strchr (cp, PATH_SEPARATOR);
      if (cp2)
	len = cp2 - cp;
      else
	len = strlen (cp);

      memcpy (buf, cp, len);
      buf[len] = '\0';
      if (len > 0 && buf[len - 1] == '/')
	buf[len - 1] = '\0';

      strcat (buf, "/font.map");

      if (stat (buf, &stat_st) == 0)
	read_font_map (handle, buf);
    }

  *handle_return = handle;

  return AFM_SUCCESS;


  /* Error handling. */

 error_out:

  (void) afm_destroy (handle);

  return error;
}


AFMError
afm_destroy (AFMHandle handle)
{
  char *key;
  int keylen;
  char *cp;

  if (handle == NULL)
    return AFM_ERROR_ARGUMENT;

  /* Free filenames. */
  while (strhash_get_first (handle->font_map, &key, &keylen, (void *) &cp))
    free (cp);

  strhash_free (handle->font_map);
  free (handle);

  return AFM_SUCCESS;
}


AFMError
afm_set_verbose (AFMHandle handle, unsigned int level)
{
  if (handle == NULL)
    return AFM_ERROR_ARGUMENT;

  handle->verbose = level;

  return AFM_SUCCESS;
}


AFMError
afm_font_prefix (AFMHandle handle, const char *fontname,
		 const char **prefix_return)
{
  char *filename;

  if (handle == NULL || fontname == NULL || prefix_return == NULL)
    return AFM_ERROR_ARGUMENT;

  /* Lookup font. */
  if (!strhash_get (handle->font_map, fontname, strlen (fontname),
		    (void *) &filename))
    return AFM_ERROR_UNKNOWN_FONT;

  *prefix_return = filename;

  return AFM_SUCCESS;
}


AFMError
afm_open_font (AFMHandle handle, unsigned int info_level,
	       const char *fontname, AFMFont *font_return)
{
  char *filename;
  char fname[512];

  if (handle == NULL || fontname == NULL)
    return AFM_ERROR_ARGUMENT;

  /* Lookup font. */
  if (!strhash_get (handle->font_map, fontname, strlen (fontname),
		    (void *) &filename))
    return AFM_ERROR_UNKNOWN_FONT;

  /* Append suffix to the filename. */
  sprintf (fname, "%s.afm", filename);

  return afm_open_file (handle, info_level, fname, font_return);
}


AFMError
afm_open_file (AFMHandle handle, unsigned int info_level,
	       const char *filename, AFMFont *font_return)
{
  AFMFont font;
  AFMError error = AFM_SUCCESS;

  if (handle == NULL || filename == NULL)
    return AFM_ERROR_ARGUMENT;

  font = (AFMFont) calloc (1, sizeof (*font));
  if (font == NULL)
    return AFM_ERROR_MEMORY;

  font->private
    = (struct afm_font_private_data_st *) calloc (1, sizeof (*font->private));
  if (font->private == NULL)
    {
      error = AFM_ERROR_MEMORY;
      goto error_out;
    }
  font->private->fontnames = strhash_init ();
  if (font->private->fontnames == NULL)
    {
      error = AFM_ERROR_MEMORY;
      goto error_out;
    }

  font->private->compositenames = strhash_init ();
  if (font->private->compositenames == NULL)
    {
      error = AFM_ERROR_MEMORY;
      goto error_out;
    }

  font->info_level = info_level;

  /* Parse file. */
  if (setjmp (handle->jmpbuf))
    {
      /* Error during parse. */
      error = handle->parse_error;
      goto error_out;
    }
  else
    {
      afm_parse_file (handle, filename, font);
      /* Parse successful. */
    }

  *font_return = font;
  return AFM_SUCCESS;


  /* Error handling. */

 error_out:

  (void) afm_close_font (font);

  return error;
}


#define FREE(ptr) if (ptr) free (ptr)
AFMError
afm_close_font (AFMFont font)
{
  int i;

  if (font == NULL)
    return AFM_ERROR_ARGUMENT;

  /* Global info. */
  FREE (font->global_info.FontName);
  FREE (font->global_info.FullName);
  FREE (font->global_info.FamilyName);
  FREE (font->global_info.Weight);
  FREE (font->global_info.Version);
  FREE (font->global_info.Notice);
  FREE (font->global_info.EncodingScheme);
  FREE (font->global_info.CharacterSet);

  /* Character metrics. */
  for (i = 0; i < font->num_character_metrics; i++)
    FREE (font->character_metrics[i].name);
  FREE (font->character_metrics);

  /* Composites. */
  for (i = 0; i < font->num_composites; i++)
    FREE (font->composites[i].name);
  FREE (font->composites);

  /* Kern pairs. */
  for (i = 0; i < font->num_kern_pairs; i++)
    {
      FREE (font->kern_pairs[i].name1);
      FREE (font->kern_pairs[i].name2);
    }
  FREE (font->kern_pairs);

  /* Track kern. */
  FREE (font->track_kerns);

  /* Private data. */
  strhash_free (font->private->fontnames);
  strhash_free (font->private->compositenames);

  free (font);

  return AFM_SUCCESS;
}


#define STR(str) (str ? str : "")
#define BOOL(val) (val ? "true" : "false")
void
afm_font_dump (FILE *fp, AFMFont font)
{
  int i;

  fprintf (fp, "AFM Format Specification version: %g\n", font->version);
  fprintf (fp, "Global Font Information\n");
  fprintf (fp, "  FontName:\t%s\n", 	STR (font->global_info.FontName));
  fprintf (fp, "  FullName:\t%s\n", 	STR (font->global_info.FullName));
  fprintf (fp, "  FamilyName:\t%s\n", 	STR (font->global_info.FamilyName));
  fprintf (fp, "  Weight:\t%s\n", 	STR (font->global_info.Weight));
  fprintf (fp, "  FontBBox:\t%g %g %g %g\n",
	   font->global_info.FontBBox_llx, font->global_info.FontBBox_lly,
	   font->global_info.FontBBox_urx, font->global_info.FontBBox_ury);
  fprintf (fp, "  Version:\t%s\n", 	STR (font->global_info.Version));
  fprintf (fp, "  Notice:\t%s\n", 	STR (font->global_info.Notice));
  fprintf (fp, "  EncodingScheme:\t%s\n",
	   STR (font->global_info.EncodingScheme));
  fprintf (fp, "  MappingScheme:\t%ld\n", font->global_info.MappingScheme);
  fprintf (fp, "  EscChar:\t%ld\n", font->global_info.EscChar);
  fprintf (fp, "  CharacterSet:\t%s\n", STR (font->global_info.CharacterSet));
  fprintf (fp, "  Characters:\t%ld\n", 	font->global_info.Characters);
  fprintf (fp, "  IsBaseFont:\t%s\n", 	BOOL(font->global_info.IsBaseFont));
  fprintf (fp, "  VVector:\t%g %g\n",
	   font->global_info.VVector_0,	font->global_info.VVector_1);
  fprintf (fp, "  IsFixedV:\t%s\n", 	BOOL(font->global_info.IsFixedV));
  fprintf (fp, "  CapHeight:\t%g\n", 	font->global_info.CapHeight);
  fprintf (fp, "  XHeight:\t%g\n", 	font->global_info.XHeight);
  fprintf (fp, "  Ascender:\t%g\n", 	font->global_info.Ascender);
  fprintf (fp, "  Descender:\t%g\n", 	font->global_info.Descender);

  for (i = 0; i < 2; i++)
    if (font->writing_direction_metrics[i].is_valid)
      {
	fprintf (fp, "Writing Direction %d\n", i);
	fprintf (fp, "  UnderlinePosition: %g\n",
		 font->writing_direction_metrics[i].UnderlinePosition);
	fprintf (fp, "  UnderlineThickness: %g\n",
		 font->writing_direction_metrics[i].UnderlineThickness);
	fprintf (fp, "  ItalicAngle: %g\n",
		 font->writing_direction_metrics[i].ItalicAngle);
	fprintf (fp, "  CharWidth: %g %g\n",
		 font->writing_direction_metrics[i].CharWidth_x,
		 font->writing_direction_metrics[i].CharWidth_y);
	fprintf (fp, "  IsFixedPitch: %s\n",
		 BOOL (font->writing_direction_metrics[i].IsFixedPitch));
      }

  /* Individual Character Metrics. */
  fprintf (fp, "Individual Character Metrics %ld\n",
	   font->num_character_metrics);
  for (i = 0; i < font->num_character_metrics; i++)
    {
      AFMIndividualCharacterMetrics *cm;
      cm = &font->character_metrics[i];

      fprintf (fp, "  C %ld ; N %s ; B %g %g %g %g\n",
	       cm->character_code, STR (cm->name),
	       cm->llx, cm->lly, cm->urx, cm->ury);
      fprintf (fp,
	       "    W0X %g ; W0Y %g ; W1X %g ; W1Y %g ; VV %g %g\n",
	       cm->w0x, cm->w0y, cm->w1x, cm->w1y, cm->vv_x, cm->vv_y);
    }

  /* Composite Character Data. */
  fprintf (fp, "Composite Character Data %ld\n", font->num_composites);
  for (i = 0; i < font->num_composites; i++)
    {
      AFMComposite *cm;
      int j;

      cm = &font->composites[i];

      fprintf (fp, "  CC %s %ld", cm->name, cm->num_components);
      for (j = 0; j < cm->num_components; j++)
	fprintf (fp, " ; PCC %s %g %g",
		 cm->components[j].name,
		 cm->components[j].deltax,
		 cm->components[j].deltay);
      fprintf (fp, "\n");
    }

  /* Kern pairs. */
  fprintf (fp, "Pair-Wise Kerning %ld\n", font->num_kern_pairs);
  for (i = 0; i < font->num_kern_pairs; i++)
    {
      AFMPairWiseKerning *kp;
      kp = &font->kern_pairs[i];

      fprintf (fp, "  KP %s %s %g %g\n", STR (kp->name1), STR (kp->name2),
	       kp->kx, kp->ky);
    }

  fprintf (fp, "Track Kerning %ld\n", font->num_track_kerns);
  for (i = 0; i < font->num_track_kerns; i++)
    {
      AFMTrackKern *tk;
      tk = &font->track_kerns[i];

      fprintf (fp, "  TrackKern %ld %g %g %g %g\n", tk->degree,
	       tk->min_ptsize, tk->min_kern,
	       tk->max_ptsize, tk->max_kern);
    }
}


AFMError
afm_font_stringwidth (AFMFont font, AFMNumber ptsize, char *string,
		      unsigned int stringlen, AFMNumber *w0x_return,
		      AFMNumber *w0y_return)
{
  unsigned int i;
  AFMNumber x = 0.0;
  AFMNumber y = 0.0;
  AFMIndividualCharacterMetrics *cm;

  if (!font || !string || !font->writing_direction_metrics[0].is_valid)
    return AFM_ERROR_ARGUMENT;

  /* Check shortcut. */
  if (font->writing_direction_metrics[0].IsFixedPitch)
    {
      /* This is the easy case. */
      x = stringlen * font->writing_direction_metrics[0].CharWidth_x;
      y = stringlen * font->writing_direction_metrics[0].CharWidth_y;
    }
  else
    {
      /* Count character by character. */
      for (i = 0; i < stringlen; i++)
	{
	  cm = font->encoding[(unsigned char) string[i]];
	  if (cm == AFM_ENC_NONE || cm == AFM_ENC_NON_EXISTENT)
	    {
	      /* Use the undef font. */
	      x += font->private->undef->w0x;
	      y += font->private->undef->w0y;
	    }
	  else
	    {
	      /* Font found and valid, take values. */
	      x += cm->w0x;
	      y += cm->w0y;
	    }
	}
    }

  *w0x_return = x / UNITS_PER_POINT * ptsize;
  *w0y_return = y / UNITS_PER_POINT * ptsize;

  return AFM_SUCCESS;
}


AFMError
afm_font_charwidth (AFMFont font, AFMNumber ptsize, char ch,
		    AFMNumber *w0x_return, AFMNumber *w0y_return)
{
  AFMNumber x = 0.0;
  AFMNumber y = 0.0;
  AFMIndividualCharacterMetrics *cm;

  if (!font || !font->writing_direction_metrics[0].is_valid)
    return AFM_ERROR_ARGUMENT;

  /* Check shortcut. */
  if (font->writing_direction_metrics[0].IsFixedPitch)
    {
      x = font->writing_direction_metrics[0].CharWidth_x;
      y = font->writing_direction_metrics[0].CharWidth_y;
    }
  else
    {
      cm = font->encoding[(unsigned char) ch];
      if (cm == AFM_ENC_NONE || cm == AFM_ENC_NON_EXISTENT)
	{
	  /* Use the undef font. */
	  x = font->private->undef->w0x;
	  y = font->private->undef->w0y;
	}
      else
	{
	  /* Font found and valid, take values. */
	  x = cm->w0x;
	  y = cm->w0y;
	}
    }

  *w0x_return = x / UNITS_PER_POINT * ptsize;
  *w0y_return = y / UNITS_PER_POINT * ptsize;

  return AFM_SUCCESS;
}


AFMError
afm_font_encode (AFMFont font, unsigned char code, char *name,
		 unsigned int flags)
{
  AFMIndividualCharacterMetrics *cm;
  AFMComposite *comp;

  if (font == NULL)
    return AFM_ERROR_ARGUMENT;

  if (name)
    {
      /* Get font. */
      if (!strhash_get (font->private->fontnames, name, strlen (name),
			(void *) &cm))
	{
	  /* Check composite characters. */
	  if ((flags & AFM_ENCODE_ACCEPT_COMPOSITES) == 0
	      || strhash_get (font->private->compositenames, name,
			      strlen (name), (void *) &comp) == 0)
	    cm = AFM_ENC_NON_EXISTENT;
	  else
	    {
	      /*
	       * Ok, composite character found, now find the character
	       * specified by the first composite component.
	       */
	      if (!strhash_get (font->private->fontnames,
				comp->components[0].name,
				strlen (comp->components[0].name),
				(void *) &cm))
		cm = AFM_ENC_NON_EXISTENT;
	    }
	}
    }
  else
    cm = AFM_ENC_NONE;

  font->encoding[(unsigned int) code] = cm;

  return AFM_SUCCESS;
}


AFMError
afm_font_encoding (AFMFont font, AFMEncoding enc, unsigned int flags)
{
  int i;
  AFMIndividualCharacterMetrics *cm;

  if (font == NULL)
    return AFM_ERROR_ARGUMENT;

  switch (enc)
    {
    case AFM_ENCODING_DEFAULT:
      /* Clear encoding. */
      for (i = 0; i < 256; i++)
	font->encoding[i] = AFM_ENC_NONE;

      /* Apply font's default encoding. */
      for (i = 0; i < font->num_character_metrics; i++)
	{
	  cm = &font->character_metrics[i];
	  font->encoding[cm->character_code] = cm;
	}
      break;

    case AFM_ENCODING_ISO_8859_1:
      apply_encoding (font, afm_88591_encoding, flags);
      break;

    case AFM_ENCODING_ISO_8859_2:
      apply_encoding (font, afm_88592_encoding, flags);
      break;

    case AFM_ENCODING_ISO_8859_3:
      apply_encoding (font, afm_88593_encoding, flags);
      break;

    case AFM_ENCODING_ISO_8859_4:
      apply_encoding (font, afm_88594_encoding, flags);
      break;

    case AFM_ENCODING_ISO_8859_5:
      apply_encoding (font, afm_88595_encoding, flags);
      break;

    case AFM_ENCODING_ISO_8859_7:
      apply_encoding (font, afm_88597_encoding, flags);
      break;

    case AFM_ENCODING_ISO_8859_9:
      apply_encoding (font, afm_88599_encoding, flags);
      break;

    case AFM_ENCODING_ISO_8859_10:
      apply_encoding (font, afm_885910_encoding, flags);
      break;

    case AFM_ENCODING_IBMPC:
      apply_encoding (font, afm_ibmpc_encoding, flags);
      break;

    case AFM_ENCODING_ASCII:
      /*
       * First apply one encoding (all have equal first 128 characters),
       * then zap last 128 chars.
       */
      apply_encoding (font, afm_88591_encoding, flags);
      for (i = 128; i < 256; i++)
	font->encoding[i] = AFM_ENC_NONE;
      break;

    case AFM_ENCODING_MAC:
      apply_encoding (font, afm_mac_encoding, flags);
      break;

    case AFM_ENCODING_VMS:
      apply_encoding (font, afm_vms_encoding, flags);
      break;

    case AFM_ENCODING_HP8:
      apply_encoding (font, afm_hp8_encoding, flags);
      break;

    case AFM_ENCODING_KOI8:
      apply_encoding (font, afm_koi8_encoding, flags);
      break;
    }

  return AFM_SUCCESS;
}


/*
 * Internal help functions.
 */


void
afm_message (AFMHandle handle, unsigned int level, char *message)
{
  if (handle->verbose < level)
    return;

  fprintf (stderr, "%s", message);
}


void
afm_error (AFMHandle handle, char *message)
{
  fprintf (stderr, "AFM Error: %s\n", message);
}


/*
 * Static functions.
 */

static void
read_font_map (AFMHandle handle, char *name)
{
  FILE *fp;
  char buf[512];
  char fullname[512];
  unsigned int dirlen;
  char *cp, *cp2;
  char msg[256];

  sprintf (msg, "AFM: reading font map \"%s\"\n", name);
  afm_message (handle, 1, msg);

  fp = fopen (name, "r");
  if (fp == NULL)
    {
      sprintf (msg, "AFM: couldn't open font map \"%s\": %s\n", name,
	       strerror (errno));
      afm_message (handle, 1, msg);
      return;
    }

  /* Get directory */
  cp = strrchr (name, '/');
  if (cp)
    {
      dirlen = cp - name + 1;
      memcpy (fullname, name, dirlen);
    }
  else
    {
      dirlen = 2;
      memcpy (fullname, "./", dirlen);
    }

  while (fgets (buf, sizeof (buf), fp))
    {
      char font[256];
      char file[256];

      if (sscanf (buf, "%s %s", font, file) != 2)
	{
	  sprintf (msg, "malformed line in font map \"%s\":\n%s",
		   name, buf);
	  afm_error (handle, msg);
	  continue;
	}

      /* Do we already have this font? */
      if (strhash_get (handle->font_map, font, strlen (font), (void *) &cp))
	continue;

      /* Append file name. */
      strcpy (fullname + dirlen, file);
      cp = (char *) malloc (strlen (fullname) + 1);
      if (cp == NULL)
	{
	  afm_error (handle, "couldn't add font: out of memory");
	  goto out;
	}
      strcpy (cp, fullname);

      sprintf (msg, "AFM: font mapping: %s -> %s\n", font, cp);
      afm_message (handle, 2, msg);
      (void) strhash_put (handle->font_map, font, strlen (font), cp,
			  (void *) &cp2);
    }

 out:
  fclose (fp);
}


static void
apply_encoding (AFMFont font, AFMEncodingTable *enc, unsigned int flags)
{
  int i;
  AFMIndividualCharacterMetrics *cm;
  AFMComposite *comp;

  for (i = 0; enc[i].code >= 0; i++)
    {
      if (enc[i].character == AFM_ENC_NONE)
	font->encoding[enc[i].code] = AFM_ENC_NONE;
      else if (enc[i].character == AFM_ENC_NON_EXISTENT)
	font->encoding[enc[i].code] = AFM_ENC_NON_EXISTENT;
      else
	{
	  if (strhash_get (font->private->fontnames, enc[i].character,
			   strlen (enc[i].character), (void *) &cm))
	    font->encoding[enc[i].code] = cm;
	  else
	    {
	      /* Check composite characters. */
	      if ((flags & AFM_ENCODE_ACCEPT_COMPOSITES) == 0
		  || strhash_get (font->private->compositenames,
				  enc[i].character, strlen (enc[i].character),
				  (void *) &comp) == 0)
		font->encoding[enc[i].code] = AFM_ENC_NON_EXISTENT;
	      else
		{
		  /* Composite character found. */
		  if (strhash_get (font->private->fontnames,
				   comp->components[0].name,
				   strlen (comp->components[0].name),
				   (void *) &cm))
		    font->encoding[enc[i].code] = cm;
		  else
		    font->encoding[enc[i].code] = AFM_ENC_NON_EXISTENT;
		}
	    }
	}
    }
}
