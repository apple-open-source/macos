/*
 * Help utilities.
 * Copyright (c) 1995-1999 Markku Rossi.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
 * This file is part of GNU enscript.
 *
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

#include "gsint.h"

/*
 * Types and definitions.
 */

#define CFG_FATAL(body)						\
  do {								\
    fprintf (stderr, "%s:%s:%d: ", program, fname, line);	\
    fprintf body;						\
    fprintf (stderr, "\n");					\
    fflush (stderr);						\
    exit (1);							\
  } while (0)


/*
 * Static variables.
 */

/*
 * 7bit ASCII fi(nland), se (sweden) scand encodings (additions to 7bit ASCII
 * enc).
 */
static struct
{
  int code;
  char *name;
} enc_7bit_ascii_fise[] =
{
  {'{',		"adieresis"},
  {'|',		"odieresis"},
  {'}',		"aring"},
  {'[',		"Adieresis"},
  {'\\',	"Odieresis"},
  {']',		"Aring"},
  {0, NULL},
};

/*
 * 7bit ASCII dk (denmark), no(rway) scand encodings (additions to 7bit ASCII
 * enc).
 */
static struct
{
  int code;
  char *name;
} enc_7bit_ascii_dkno[] =
{
  {'{',		"ae"},
  {'|',		"oslash"},
  {'}',		"aring"},
  {'[',		"AE"},
  {'\\',	"Oslash"},
  {']',		"Aring"},
  {0, NULL},
};


/*
 * Global functions.
 */

#define GET_TOKEN(from) (strtok ((from), " \t\n"))
#define GET_LINE_TOKEN(from) (strtok ((from), "\n"))

#define CHECK_TOKEN() 							\
  if (token2 == NULL) 							\
    CFG_FATAL ((stderr, _("missing argument: %s"), token));

int
read_config (char *path, char *file)
{
  FILE *fp;
  Buffer fname;
  char buf[4096];
  char *token, *token2;
  int line = 0;

  buffer_init (&fname);
  buffer_append (&fname, path);
  buffer_append (&fname, "/");
  buffer_append (&fname, file);

  fp = fopen (buffer_ptr (&fname), "r");

  buffer_uninit (&fname);

  if (fp == NULL)
    return 0;

  while (fgets (buf, sizeof (buf), fp))
    {
      line++;

      if (buf[0] == '#')
	continue;

      token = GET_TOKEN (buf);
      if (token == NULL)
	/* Empty line. */
	continue;

      if (MATCH (token, "AcceptCompositeCharacters:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  accept_composites = atoi (token2);
	}
      else if (MATCH (token, "AFMPath:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (afm_path);
	  afm_path = xstrdup (token2);
	}
      else if (MATCH (token, "AppendCtrlD:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  append_ctrl_D = atoi (token2);
	}
      else if (MATCH (token, "Clean7Bit:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  clean_7bit = atoi (token2);
	}
      else if (MATCH (token, "DefaultEncoding:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (encoding_name);
	  encoding_name = xstrdup (token2);
	}
      else if (MATCH (token, "DefaultFancyHeader:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (fancy_header_default);
	  fancy_header_default = xstrdup (token2);
	}
      else if (MATCH (token, "DefaultMedia:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (media_name);
	  media_name = xstrdup (token2);
	}
      else if (MATCH (token, "DefaultOutputMethod:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  if (MATCH (token2, "printer"))
	    output_file = OUTPUT_FILE_NONE;
	  else if (MATCH (token2, "stdout"))
	    output_file = OUTPUT_FILE_STDOUT;
	  else
	    CFG_FATAL ((stderr, _("illegal value \"%s\" for option %s"),
			token2, token));
	}
      else if (MATCH (token, "DownloadFont:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  strhash_put (download_fonts, token2, strlen (token2) + 1, NULL,
		       NULL);
	}
      else if (MATCH (token, "EscapeChar:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  escape_char = atoi (token2);
	  if (escape_char < 0 || escape_char > 255)
	    CFG_FATAL ((stderr, _("invalid value \"%s\" for option %s"),
			token2, token));
	}
      else if (MATCH (token, "FormFeedType:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  if (MATCH (token2, "column"))
	    formfeed_type = FORMFEED_COLUMN;
	  else if (MATCH (token2, "page"))
	    formfeed_type = FORMFEED_PAGE;
	  else
	    CFG_FATAL ((stderr, _("illegal value \"%s\" for option %s"),
			token2, token));
	}
      else if (MATCH (token, "GeneratePageSize:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  generate_PageSize = atoi (token2);
	}
      else if (MATCH (token, "HighlightBarGray:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  highlight_bar_gray = atof (token2);
	}
      else if (MATCH (token, "HighlightBars:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  highlight_bars = atoi (token2);
	}
      else if (MATCH (token, "LibraryPath:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (libpath);
	  libpath = xstrdup (token2);
	}
      else if (MATCH (token, "MarkWrappedLines:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (mark_wrapped_lines_style_name);
	  mark_wrapped_lines_style_name = xstrdup (token2);
	}
      else if (MATCH (token, "Media:"))
	{
	  char *name;
	  int w, h, llx, lly, urx, ury;

	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  name = token2;

	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  w = atoi (token2);

	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  h = atoi (token2);

	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  llx = atoi (token2);

	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  lly = atoi (token2);

	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  urx = atoi (token2);

	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  ury = atoi (token2);

	  add_media (name, w, h, llx, lly, urx, ury);
	}
      else if (MATCH (token, "NoJobHeaderSwitch:"))
	{
	  token2 = GET_LINE_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (no_job_header_switch);
	  no_job_header_switch = xstrdup (token2);
	}
      else if (MATCH (token, "NonPrintableFormat:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (npf_name);
	  npf_name = xstrdup (token2);
	}
      else if (MATCH (token, "OutputFirstLine:"))
	{
	  token2 = GET_LINE_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (output_first_line);
	  output_first_line = xstrdup (token2);
	}
      else if (MATCH (token, "PageLabelFormat:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (page_label_format);
	  page_label_format = xstrdup (token2);
	}
      else if (MATCH (token, "PagePrefeed:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  page_prefeed = atoi (token2);
	}
      else if (MATCH (token, "PostScriptLevel:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  pslevel = atoi (token2);
	}
      else if (MATCH (token, "Printer:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (printer);
	  printer = xstrdup (token2);
	}
      else if (MATCH (token, "QueueParam:"))
	{
	  token2 = GET_LINE_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (queue_param);
	  queue_param = xstrdup (token2);
	}
      else if (MATCH (token, "SetPageDevice:"))
	{
	  token2 = GET_LINE_TOKEN (NULL);
	  CHECK_TOKEN ();
	  parse_key_value_pair (pagedevice, token2);
	}
      else if (MATCH (token, "Spooler:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (spooler_command);
	  spooler_command = xstrdup (token2);
	}
      else if (MATCH (token, "StatesBinary:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (states_binary);
	  states_binary = xstrdup (token2);
	}
      else if (MATCH (token, "StatesColor:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  states_color = atoi (token2);
	}
      else if (MATCH (token, "StatesConfigFile:"))
	{
	  token2 = GET_LINE_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (states_config_file);
	  states_config_file = xstrdup (token2);
	}
      else if (MATCH (token, "StatesHighlightStyle:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (states_highlight_style);
	  states_highlight_style = xstrdup (token2);
	}
      else if (MATCH (token, "StatesPath:"))
	{
	  token2 = GET_LINE_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (states_path);
	  states_path = xstrdup (token2);
	}
      else if (MATCH (token, "StatusDict:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  parse_key_value_pair (statusdict, token2);
	}
      else if (MATCH (token, "TOCFormat:"))
	{
	  token2 = GET_LINE_TOKEN (NULL);
	  CHECK_TOKEN ();
	  toc_fmt_string = xstrdup (token2);
	}
      else if (MATCH (token, "Underlay:"))
	{
	  token2 = GET_LINE_TOKEN (NULL);
	  CHECK_TOKEN ();
	  underlay = xmalloc (strlen (token2) + 1);
	  strcpy (underlay, token2);
	}
      else if (MATCH (token, "UnderlayAngle:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  ul_angle = atof (token2);
	  ul_angle_p = 1;
	}
      else if (MATCH (token, "UnderlayFont:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  if (!parse_font_spec (token2, &ul_font, &ul_ptsize, NULL))
	    CFG_FATAL ((stderr, _("malformed font spec: %s"), token2));
	}
      else if (MATCH (token, "UnderlayGray:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  ul_gray = atof (token2);
	}
      else if (MATCH (token, "UnderlayPosition:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (ul_position);
	  ul_position = xstrdup (token2);
	  ul_position_p = 1;
	}
      else if (MATCH (token, "UnderlayStyle:"))
	{
	  token2 = GET_TOKEN (NULL);
	  CHECK_TOKEN ();
	  xfree (ul_style_str);
	  ul_style_str = xstrdup (token2);
	}
      else
	CFG_FATAL ((stderr, _("illegal option: %s"), token));
    }
  return 1;
}


void
add_media (char *name, int w, int h, int llx, int lly, int urx, int ury)
{
  MediaEntry *entry;

  MESSAGE (2,
	   (stderr,
	    "add_media: name=%s, w=%d, h=%d, llx=%d, lly=%d, urx=%d, ury=%d\n",
	    name, w, h, llx, lly, urx, ury));

  entry = xcalloc (1, sizeof (*entry));
  entry->name = xmalloc (strlen (name) + 1);

  strcpy (entry->name, name);
  entry->w = w;
  entry->h = h;
  entry->llx = llx;
  entry->lly = lly;
  entry->urx = urx;
  entry->ury = ury;

  entry->next = media_names;
  media_names = entry;
}


void
do_list_missing_characters (int *array)
{
  int i;
  int count = 0;

  for (i = 0; i < 256; i++)
    if (array[i])
      {
	fprintf (stderr, "%3d ", i);
	count++;
	if (count % 15 == 0)
	  fprintf (stderr, "\n");
      }

  if (count % 15 != 0)
    fprintf (stderr, "\n");
}


int
file_existsp (char *name, char *suffix)
{
  FileLookupCtx ctx;
  int result;

  ctx.name = name;
  ctx.suffix =  suffix ? suffix : "";
  ctx.fullname = buffer_alloc ();

  result = pathwalk (libpath, file_lookup, &ctx);

  buffer_free (ctx.fullname);

  return result;
}


int
paste_file (char *name, char *suffix)
{
  char buf[512];
  char resources[512];
  FILE *fp;
  FileLookupCtx ctx;
  int pending_comment = 0;
  int line = 0;

  ctx.name = name;
  ctx.suffix = suffix ? suffix : "";
  ctx.fullname = buffer_alloc ();

  if (!pathwalk (libpath, file_lookup, &ctx))
    {
      buffer_free (ctx.fullname);
      return 0;
    }
  fp = fopen (buffer_ptr (ctx.fullname), "r");
  if (fp == NULL)
    {
      buffer_free (ctx.fullname);
      return 0;
    }

  /* Find the end of the header. */
#define HDR_TAG "% -- code follows this line --"
  while ((fgets (buf, sizeof (buf), fp)))
    {
      line++;
      if (strncmp (buf, HDR_TAG, strlen (HDR_TAG)) == 0)
	break;
    }

  /* Dump rest of file. */
  while ((fgets (buf, sizeof (buf), fp)))
    {
      line++;

      /*
       * Document needed resources?
       */
#define RESOURCE_DSC 	"%%DocumentNeededResources:"
#define CONT_DSC 	"%%+"
      if (strncmp (buf, RESOURCE_DSC, strlen (RESOURCE_DSC)) == 0)
	{
	  char *cp, *cp2;

	  strcpy (resources, buf + strlen (RESOURCE_DSC));
	  pending_comment = 1;

	parse_resources:
	  /* Register needed resources. */
	  cp = GET_TOKEN (resources);
	  if (cp == NULL)
	    /* Get the next line. */
	    continue;

	  if (MATCH (cp, "font"))
	    {
	      for (cp = GET_TOKEN (NULL); cp; cp = GET_TOKEN (NULL))
		/* Is this font already known? */
		if (!strhash_get (res_fonts, cp, strlen (cp) + 1,
				  (void **) &cp2))
		  {
		    /* Not it is not,  we must include this resource. */
		    fprintf (ofp, "%%%%IncludeResource: font %s\n", cp);

		    /*
		     * And register that this resource is needed in
		     * this document.
		     */
		    strhash_put (res_fonts, cp, strlen (cp) + 1, NULL, NULL);
		  }

	      /* Do not pass this DSC row to the output. */
	      continue;
	    }
	  else
	    /* Unknown resource, ignore. */
	    continue;
	}
      else if (pending_comment
	       && strncmp (buf, CONT_DSC, strlen (CONT_DSC)) == 0)
	{
	  strcpy (resources, buf + strlen (CONT_DSC));
	  goto parse_resources;
	}
      else
	pending_comment = 0;

      /*
       * `%Format' directive?
       */
#define DIRECTIVE_FORMAT "%Format:"
      if (strncmp (buf, DIRECTIVE_FORMAT, strlen (DIRECTIVE_FORMAT)) == 0)
	{
	  int i, j;
	  char name[256];
	  char *cp, *cp2;
	  errno = 0;

	  /* Skip the leading whitespace. */
	  for (i = strlen (DIRECTIVE_FORMAT); buf[i] && isspace (buf[i]); i++)
	    ;
	  if (!buf[i])
	    FATAL ((stderr, _("%s:%d: %%Format: no name"),
		    buffer_ptr (ctx.fullname), line));

	  /* Copy name. */
	  for (j = 0;
	       j < sizeof (name) - 1 && buf[i] && !isspace (buf[i]);
	       i++)
	    name[j++] = buf[i];
	  name[j] = '\0';

	  if (j >= sizeof (name) - 1)
	    FATAL ((stderr, _("%s:%d: %%Format: too long name, maxlen=%d"),
		    buffer_ptr (ctx.fullname), line, sizeof (name) - 1));

	  /* Find the start of the format string. */
	  for (; buf[i] && isspace (buf[i]); i++)
	    ;

	  /* Find the end. */
	  j = strlen (buf);
	  for (j--; isspace (buf[j]) && j > i; j--)
	    ;
	  j++;

	  MESSAGE (2, (stderr, "%%Format: %s %.*s\n", name, j - i, buf + i));

	  cp = xmalloc (j - i + 1);
	  memcpy (cp, buf + i, j - i);
	  cp[j - i] = '\0';

	  strhash_put (user_strings, name, strlen (name) + 1, cp,
		       (void **) &cp2);
	  if (cp2)
	    FATAL ((stderr,
		    _("%s:%d: %%Format: name \"%s\" is already defined"),
		    buffer_ptr (ctx.fullname), line, name));

	  /* All done with the `%Format' directive. */
	  continue;
	}

      /*
       * `%HeaderHeight' directive?
       */
#define DIRECTIVE_HEADERHEIGHT "%HeaderHeight:"
      if (strncmp (buf, DIRECTIVE_HEADERHEIGHT,
		   strlen (DIRECTIVE_HEADERHEIGHT)) == 0)
	  {
	    int i;

	    /* Find the start of the pts argument. */
	    for (i = strlen (DIRECTIVE_HEADERHEIGHT);
		 buf[i] && !isspace (buf[i]); i++)
	      ;
	    if (!buf[i])
	      FATAL ((stderr, _("%s:%d: %%HeaderHeight: no argument"),
		      buffer_ptr (ctx.fullname), line));

	    d_header_h = atoi (buf + i);
	    MESSAGE (2, (stderr, "%%HeaderHeight: %d\n", d_header_h));
	    continue;
	  }

      /*
       * `%FooterHeight' directive?
       */
#define DIRECTIVE_FOOTERHEIGHT "%FooterHeight:"
      if (strncmp (buf, DIRECTIVE_FOOTERHEIGHT,
		   strlen (DIRECTIVE_FOOTERHEIGHT)) == 0)
	{
	  int i;

	  /* Find the start of the pts argument. */
	  for (i = strlen (DIRECTIVE_FOOTERHEIGHT);
	       buf[i] && !isspace (buf[i]); i++)
	    ;
	  if (!buf[i])
	    FATAL ((stderr, _("%s:%d: %%FooterHeight: no argument"),
		    buffer_ptr (ctx.fullname), line));

	  d_footer_h = atoi (buf + i);
	  MESSAGE (2, (stderr, "%%FooterHeight: %d\n", d_footer_h));
	  continue;
	}

      /* Nothing special, just copy it to the output. */
      fputs (buf, ofp);
    }

  fclose (fp);
  buffer_free (ctx.fullname);

  return 1;
}


int
parse_font_spec (char *spec_a, char **name_return, FontPoint *size_return,
		 InputEncoding *encoding_return)
{
  int i, j;
  char *cp, *cp2;
  char *spec;
  char *encp;

  spec = xstrdup (spec_a);

  /* Check for the `namesize:encoding' format. */
  encp = strrchr (spec, ':');
  if (encp)
    {
      *encp = '\0';
      encp++;
    }

  /* The `name@ptsize' format? */
  cp = strchr (spec, '@');
  if (cp)
    {
      i = cp - spec;
      if (cp[1] == '\0')
	{
	  /* No ptsize after '@'. */
	  xfree (spec);
	  return 0;
	}
      cp++;
    }
  else
    {
      /* The old `nameptsize' format. */
      i = strlen (spec) - 1;
      if (i <= 0 || !ISNUMBERDIGIT (spec[i]))
	{
	  xfree (spec);
	  return 0;
	}

      for (i--; i >= 0 && ISNUMBERDIGIT (spec[i]); i--)
	;
      if (i < 0)
	{
	  xfree (spec);
	  return 0;
	}
      if (spec[i] == '/')
	{
	  /* We accept one slash for the `pt/pt' format. */
	  for (i--; i >= 0 && ISNUMBERDIGIT (spec[i]); i--)
	    ;
	  if (i < 0)
	    {
	      xfree (spec);
	      return 0;
	    }
	}
      i++;

      /* Now, <i> points to the end of the name.  Let's set the <cp>
         to the beginning of the point size and share a little code
         with the other format. */
      cp = spec + i;
    }

  /* Check the font point size. */
  cp2 = strchr (cp, '/');
  if (cp2)
    {
      *cp2++ = '\0';
      size_return->w = atof (cp);
      size_return->h = atof (cp2);
    }
  else
    size_return->w = size_return->h = atof (cp);

  /* Extract the font name. */
  *name_return = (char *) xcalloc (1, i + 1);
  strncpy (*name_return, spec, i);

  /* Check the input encoding. */
  if (encp)
    {
      int found = 0;

      if (encoding_return == NULL)
	{
	  /* We don't allow it here. */
	  xfree (spec);
	  return 0;
	}

      for (i = 0; !found && encodings[i].names[0]; i++)
	for (j = 0; j < 3; j++)
	  if (encodings[i].names[j] != NULL && MATCH (encodings[i].names[j],
						      encp))
	    {
	      /* Found a match. */
	      *encoding_return = encodings[i].encoding;
	      encp = encodings[i].names[0];
	      found = 1;
	      break;
	    }

      if (!found)
	{
	  xfree (spec);
	  return 0;
	}
    }
  else
    {
      /* The spec didn't contain the encoding part.  Use our global default. */
      encp = encoding_name;
      if (encoding_return)
	*encoding_return = encoding;
    }
  xfree (spec);

  MESSAGE (2, (stderr,
	       "parse_font_spec(): name=%.*s, size=%g/%g, encoding=%s\n", i,
	       *name_return, size_return->w, size_return->h,
	       encp));

  if (size_return->w < 0.0 && size_return->h < 0.0)
    MESSAGE (0, (stderr, _("%s: warning: font size is negative\n"), program));
  else if (size_return->w < 0.0)
    MESSAGE (0, (stderr, _("%s: warning: font width is negative\n"), program));
  else if (size_return->h < 0.0)
    MESSAGE (0, (stderr, _("%s: warning: font height is negative\n"),
		 program));

  return 1;
}


void
read_font_info (void)
{
  CachedFontInfo *font_info;
  AFMFont font;
  int font_info_cached = 1;
  int font_cached = 1;
  int i;
  unsigned int enc_flags = 0;
  char buf[256];
  Buffer fkey;

  MESSAGE (2, (stderr, _("reading AFM info for font \"%s\"\n"), Fname));

  if (accept_composites)
    enc_flags = AFM_ENCODE_ACCEPT_COMPOSITES;

  /* Open font */

  buffer_init (&fkey);

  buffer_append (&fkey, Fname);
  sprintf (buf, "@%f:%d", Fpt.w, encoding);
  buffer_append (&fkey, buf);

  if (!strhash_get (afm_info_cache, buffer_ptr (&fkey),
		    strlen (buffer_ptr (&fkey)), (void **) &font_info))
    {
      AFMError error;

      /* Couldn't find it from our cache, open open AFM file. */
      if (!strhash_get (afm_cache, Fname, strlen (Fname), (void **) &font))
	{
	  /* AFM file was not cached, open it from disk. */
	  error = afm_open_font (afm, AFM_I_COMPOSITES, Fname, &font);
	  if (error != AFM_SUCCESS)
	    {
#define COUR "Courier"
	      /*
	       * Do not report failures for "Courier*" fonts because
	       * AFM library's default font will fix them.
	       */
	      if (strncmp (Fname, COUR, strlen (COUR)) != 0)
		MESSAGE (0,
			 (stderr,
			  _("couldn't open AFM file for font \"%s\", using default\n"),
			  Fname));
	      error = afm_open_default_font (afm, &font);
	      if (error != AFM_SUCCESS)
		{
		  afm_error_to_string (error, buf);
		  FATAL ((stderr,
			  _("couldn't open AFM file for the default font: %s"),
			  buf));
		}
	    }

	  /* Apply encoding. */
	  switch (encoding)
	    {
	    case ENC_ISO_8859_1:
	      (void) afm_font_encoding (font, AFM_ENCODING_ISO_8859_1,
					enc_flags);
	      break;

	    case ENC_ISO_8859_2:
	      (void) afm_font_encoding (font, AFM_ENCODING_ISO_8859_2,
					enc_flags);
	      break;

	    case ENC_ISO_8859_3:
	      (void) afm_font_encoding (font, AFM_ENCODING_ISO_8859_3,
					enc_flags);
	      break;

	    case ENC_ISO_8859_4:
	      (void) afm_font_encoding (font, AFM_ENCODING_ISO_8859_4,
					enc_flags);
	      break;

	    case ENC_ISO_8859_5:
	      (void) afm_font_encoding (font, AFM_ENCODING_ISO_8859_5,
					enc_flags);
	      break;

	    case ENC_ISO_8859_7:
	      (void) afm_font_encoding (font, AFM_ENCODING_ISO_8859_7,
					enc_flags);
	      break;

	    case ENC_ISO_8859_9:
	      (void) afm_font_encoding (font, AFM_ENCODING_ISO_8859_9,
					enc_flags);
	      break;

	    case ENC_ISO_8859_10:
	      (void) afm_font_encoding (font, AFM_ENCODING_ISO_8859_10,
					enc_flags);
	      break;

	    case ENC_ASCII:
	      (void) afm_font_encoding (font, AFM_ENCODING_ASCII, enc_flags);
	      break;

	    case ENC_ASCII_FISE:
	      /* First apply standard 7bit ASCII encoding. */
	      (void) afm_font_encoding (font, AFM_ENCODING_ASCII, enc_flags);

	      /* Then add those scand characters. */
	      for (i = 0; enc_7bit_ascii_fise[i].name; i++)
		(void) afm_font_encode (font, enc_7bit_ascii_fise[i].code,
					enc_7bit_ascii_fise[i].name,
					enc_flags);
	      break;

	    case ENC_ASCII_DKNO:
	      /* First apply standard 7bit ASCII encoding. */
	      (void) afm_font_encoding (font, AFM_ENCODING_ASCII, enc_flags);

	      /* Then add those scand characters. */
	      for (i = 0; enc_7bit_ascii_dkno[i].name; i++)
		(void) afm_font_encode (font, enc_7bit_ascii_dkno[i].code,
					enc_7bit_ascii_dkno[i].name,
					enc_flags);
	      break;

	    case ENC_IBMPC:
	      (void) afm_font_encoding (font, AFM_ENCODING_IBMPC, enc_flags);
	      break;

	    case ENC_MAC:
	      (void) afm_font_encoding (font, AFM_ENCODING_MAC, enc_flags);
	      break;

	    case ENC_VMS:
	      (void) afm_font_encoding (font, AFM_ENCODING_VMS, enc_flags);
	      break;

	    case ENC_HP8:
	      (void) afm_font_encoding (font, AFM_ENCODING_HP8, enc_flags);
	      break;

	    case ENC_KOI8:
	      (void) afm_font_encoding (font, AFM_ENCODING_KOI8, enc_flags);
	      break;

	    case ENC_PS:
	      /* Let's use font's default encoding -- nothing here. */
	      break;
	    }

	  /* Put it to the AFM cache. */
	  if (!strhash_put (afm_cache, Fname, strlen (Fname), font, NULL))
	    font_cached = 0;
	}

      font_info = (CachedFontInfo *) xcalloc (1, sizeof (*font_info));
      /* Read character widths and types. */
      for (i = 0; i < 256; i++)
	{
	  AFMNumber w0x, w0y;

	  (void) afm_font_charwidth (font, Fpt.w, i, &w0x, &w0y);
	  font_info->font_widths[i] = w0x;

	  if (font->encoding[i] == AFM_ENC_NONE)
	    font_info->font_ctype[i] = ' ';
	  else if (font->encoding[i] == AFM_ENC_NON_EXISTENT)
	    font_info->font_ctype[i] = '.';
	  else
	    font_info->font_ctype[i] = '*';
	}

      font_info->font_is_fixed
	= font->writing_direction_metrics[0].IsFixedPitch;
      font_info->font_bbox_lly = font->global_info.FontBBox_lly;

      if (!font_cached)
	(void) afm_close_font (font);

      /* Store font information to the AFM information cache. */
      if (!strhash_put (afm_info_cache, buffer_ptr (&fkey),
			strlen (buffer_ptr (&fkey)), font_info, NULL))
	font_info_cached = 0;
    }

  /* Select character widths and types. */
  memcpy (font_widths, font_info->font_widths, 256 * sizeof (double));
  memcpy (font_ctype, font_info->font_ctype, 256);

  font_is_fixed = font_info->font_is_fixed;
  font_bbox_lly = font_info->font_bbox_lly;

  if (!font_info_cached)
    xfree (font_info);

  buffer_uninit (&fkey);
}


void
download_font (char *name)
{
  AFMError error;
  const char *prefix;
  struct stat stat_st;
  Buffer fname;
  unsigned char buf[4096];
  FILE *fp;
  int i;
  char *cp;

  /* Get font prefix. */
  error = afm_font_prefix (afm, name, &prefix);
  if (error != AFM_SUCCESS)
    /* Font is unknown, nothing to download. */
    return;

  /* Check if we have a font description file. */

  buffer_init (&fname);

  /* .pfa */
  buffer_append (&fname, prefix);
  buffer_append (&fname, ".pfa");
  if (stat (buffer_ptr (&fname), &stat_st) != 0)
    {
      /* .pfb */
      buffer_clear (&fname);
      buffer_append (&fname, prefix);
      buffer_append (&fname, ".pfb");
      if (stat (buffer_ptr (&fname), &stat_st) != 0)
	{
	  /* Couldn't find font description file, nothing to download. */
	  buffer_uninit (&fname);
	  return;
	}
    }

  /* Ok, fine.  Font was found. */

  MESSAGE (1, (stderr, _("downloading font \"%s\"\n"), name));
  fp = fopen (buffer_ptr (&fname), "rb");
  if (fp == NULL)
    {
      MESSAGE (0, (stderr,
		   _("couldn't open font description file \"%s\": %s\n"),
		   buffer_ptr (&fname), strerror (errno)));
      buffer_uninit (&fname);
      return;
    }
  buffer_uninit (&fname);

  /* Dump file. */
  fprintf (ofp, "%%%%BeginResource: font %s\n", name);

  /* Check file type. */
  i = fgetc (fp);
  if (i == EOF)
    {
      /* Not much to do here. */
      ;
    }
  else if (i == 128)
    {
      int done = 0;
      unsigned int chunk;
      unsigned int to_read;
      int last_was_cr;
      int j;

      /* IBM PC Format */

      ungetc (i, fp);

      while (!done)
	{
	  /* Read 6-byte long header. */
	  i = fread (buf, 1, 6, fp);
	  if (i != 6)
	    break;

	  chunk = buf[2] | (buf[3] << 8) | (buf[4] << 16) | (buf[5] << 24);

	  /* Check chunk type. */
	  switch (buf[1])
	    {
	    case 1:		/* ASCII */
	      last_was_cr = 0;
	      while (chunk > 0)
		{
		  to_read = sizeof (buf) < chunk ? sizeof (buf) : chunk;
		  i = fread (buf, 1, to_read, fp);
		  if (i == 0)
		    {
		      done = 1;
		      break;
		    }

		  /* Check and fix Mac-newlines. */
		  for (j = 0; j < i; j++)
		    {
		      if (j == 0 && last_was_cr && buf[0] != '\n')
			{
			  fputc ('\n', ofp);
			  fputc (buf[0], ofp);
			}
		      else if (buf[j] == '\r' && j + 1 < i
			       && buf[j + 1] != '\n')
			{
			  fputc ('\n', ofp);
			}
		      else if (buf[j] != '\r')
			fputc (buf[j], ofp);
		    }

		  chunk -= i;
		  last_was_cr = (buf[i - 1] == '\r');
		}
	      break;

	    case 2:		/* binary data */
	      while (chunk > 0)
		{
		  to_read = sizeof (buf) < chunk ? sizeof (buf) : chunk;
		  i = fread (buf, 1, to_read, fp);
		  if (i == 0)
		    {
		      done = 1;
		      break;
		    }

		  for (j = 0; j < i; j++)
		    {
		      fprintf (ofp, "%02X", buf[j]);
		      if ((j + 1) % 32 == 0)
			fprintf (ofp, "\n");
		    }
		  chunk -= i;
		}
	      break;

	    case 3:		/* EOF */
	      done = 1;
	      break;
	    }

	  /* Force a linebreak after each chunk. */
	  fprintf (ofp, "\n");
	}
    }
  else
    {
      /* Plain ASCII. */
      ungetc (i, fp);
      while ((i = fread (buf, 1, sizeof (buf), fp)) != 0)
	fwrite (buf, 1, i, ofp);
    }

  fprintf (ofp, "%%%%EndResource\n");

  /* Remove font from needed resources. */
  (void) strhash_delete (res_fonts, name, strlen (name) + 1, (void **) &cp);

  fclose (fp);
}


char *
escape_string (char *string)
{
  int i, j;
  int len;
  char *cp;

  /* Count the length of the result string. */
  for (len = 0, i = 0; string[i]; i++)
    switch (string[i])
      {
      case '(':
      case ')':
      case '\\':
	len += 2;
	break;

      default:
	len++;
      }

  /* Create result. */
  cp = xmalloc (len + 1);
  if (cp == NULL)
      return NULL;
  for (i = 0, j = 0; string[i]; i++)
    switch (string[i])
      {
      case '(':
      case ')':
      case '\\':
	cp[j++] = '\\';
	/* FALLTHROUGH */

      default:
	cp[j++] = string[i];
	break;
      }
  cp[j++] = '\0';

  return cp;
}



/*
 * Help macros for the format_user_string() function.
 */

#define NEED_NBYTES(n) 				\
  do {						\
    if (rbufpos + (n) >= rbuflen)		\
      {						\
        rbuflen += (n) + 1024;			\
        rbuf = xrealloc (rbuf, rbuflen);	\
      }						\
  } while (0)

#define APPEND_CH(ch)				\
  do {						\
    int a;					\
    NEED_NBYTES (width);			\
    if (width && justification < 0)		\
      rbuf[rbufpos++] = (ch);			\
    for (a = 0; a < width - 1; a++)		\
      rbuf[rbufpos++] = ' ';			\
    if (!width || justification > 0)		\
      rbuf[rbufpos++] = (ch);			\
  } while (0)

#define APPEND_STR(str)				\
  do {						\
    int len = strlen ((str));			\
    int nspace;					\
						\
    if (len > width)				\
      nspace = 0;				\
    else					\
      nspace = width - len;			\
						\
    NEED_NBYTES (nspace + len);			\
    if (width && justification > 0)		\
      for (; nspace; nspace--)			\
	rbuf[rbufpos++] = ' ';			\
						\
    memcpy (rbuf + rbufpos, str, len);		\
    rbufpos += len;				\
						\
    if (width && justification < 0)		\
      for (; nspace; nspace--)			\
	rbuf[rbufpos++] = ' ';			\
  } while (0)

char *
format_user_string (char *context_name, char *str)
{
  char *cp;
  char *rbuf = NULL;
  int rbuflen = 0;
  int rbufpos = 0;
  int i = 0;
  int j;
  char buf[512];
  char buf2[512];
  int width = 0;
  int justification = 1;

  /* Format string. */
  for (i = 0; str[i] != '\0'; i++)
    {
      int type;

      type = str[i];

      if (type == '%' || type == '$')
	{
	  i++;
	  width = 0;
	  justification = 1;

	  /* Get optional width and justification. */
	  if (str[i] == '-')
	    {
	      i++;
	      justification = -1;
	    }
	  while (isdigit (str[i]))
	    width = width * 10 + str[i++] - '0';

	  /* Handle escapes. */
	  if (type == '%')
	    {
	      /* General state related %-escapes. */
	      switch (str[i])
		{
		case '%':	/* `%%' character `%' */
		  APPEND_CH ('%');
		  break;

		case 'c':	/* `%c' trailing component of pwd. */
		  getcwd (buf, sizeof (buf));
		  cp = strrchr (buf, '/');
		  if (cp)
		    cp++;
		  else
		    cp = buf;
		  APPEND_STR (cp);
		  break;

		case 'C':	/* `%C' runtime in `hh:mm:ss' format */
		  sprintf (buf, "%02d:%02d:%02d", run_tm.tm_hour,
			   run_tm.tm_min, run_tm.tm_sec);
		  APPEND_STR (buf);
		  break;

		case 'd':	/* `%d' current working directory */
		  getcwd (buf, sizeof (buf));
		  APPEND_STR (buf);
		  break;

		case 'D':
		  if (str[i + 1] == '{')
		    {
		      /* `%D{}' format run date with strftime() */
		      for (j = 0, i += 2;
			   j < sizeof (buf2) && str[i] && str[i] != '}';
			   i++, j++)
			buf2[j] = str[i];
		      if (str[i] != '}')
			FATAL ((stderr,
				_("%s: too long format for %%D{} escape"),
				context_name));

		      buf2[j] = '\0';
		      strftime (buf, sizeof (buf), buf2, &run_tm);
		    }
		  else
		    {
		      /* `%D' run date in `yy-mm-dd' format */
		      sprintf (buf, "%02d-%02d-%02d", run_tm.tm_year % 100,
			       run_tm.tm_mon + 1, run_tm.tm_mday);
		    }
		  APPEND_STR (buf);
		  break;

		case 'E':	/* `%E' run date in `yy/mm/dd' format */
		  sprintf (buf, "%02d/%02d/%02d", run_tm.tm_year % 100,
			   run_tm.tm_mon + 1, run_tm.tm_mday);
		  APPEND_STR (buf);
		  break;

		case 'F':	/* `%F' run date in `dd.mm.yyyy' format */
		  sprintf (buf, "%d.%d.%d",
			   run_tm.tm_mday,
			   run_tm.tm_mon + 1,
			   run_tm.tm_year + 1900);
		  APPEND_STR (buf);
		  break;

		case 'H':	/* `%H' document title */
		  APPEND_STR (title);
		  break;

		case 'm':	/* `%m' the hostname up to the first `.' */
		  (void) gethostname (buf, sizeof (buf));
		  cp = strchr (buf, '.');
		  if (cp)
		    *cp = '\0';
		  APPEND_STR (buf);
		  break;

		case 'M':	/* `%M' the full hostname */
		  (void) gethostname (buf, sizeof (buf));
		  APPEND_STR (buf);
		  break;

		case 'n':	/* `%n' username */
		  APPEND_STR (passwd->pw_name);
		  break;

		case 'N':	/* `%N' pw_gecos up to the first `,' char */
		  strcpy (buf, passwd->pw_gecos);
		  cp = strchr (buf, ',');
		  if (cp)
		    *cp = '\0';
		  APPEND_STR (buf);
		  break;

		case 't':	/* `%t' runtime in 12-hour am/pm format */
		  sprintf (buf, "%d:%d%s",
			   run_tm.tm_hour > 12
			   ? run_tm.tm_hour - 12 : run_tm.tm_hour,
			   run_tm.tm_min,
			   run_tm.tm_hour > 12 ? "pm" : "am");
		  APPEND_STR (buf);
		  break;

		case 'T':	/* `%T' runtime in 24-hour format */
		  sprintf (buf, "%d:%d", run_tm.tm_hour, run_tm.tm_min);
		  APPEND_STR (buf);
		  break;

		case '*':	/* `%*' runtime in 24-hour format with secs */
		  sprintf (buf, "%d:%d:%d", run_tm.tm_hour, run_tm.tm_min,
			   run_tm.tm_sec);
		  APPEND_STR (buf);
		  break;

		case 'W':	/* `%W' run date in `mm/dd/yy' format */
		  sprintf (buf, "%02d/%02d/%02d", run_tm.tm_mon + 1,
			   run_tm.tm_mday, run_tm.tm_year % 100);
		  APPEND_STR (buf);
		  break;

		default:
		  FATAL ((stderr, _("%s: unknown `%%' escape `%c' (%d)"),
			  context_name, str[i], str[i]));
		  break;
		}
	    }
	  else
	    {
	      /* Input file related $-escapes. */
	      switch (str[i])
		{
		case '$':	/* `$$' character `$' */
		  APPEND_CH ('$');
		  break;

		case '%':	/* `$%' current page number */
		  if (slicing)
		    sprintf (buf, "%d%c", current_pagenum, slice - 1 + 'A');
		  else
		    sprintf (buf, "%d", current_pagenum);
		  APPEND_STR (buf);
		  break;

		case '=':	/* `$=' number of pages in this file */
		  APPEND_CH ('\001');
		  break;

		case 'p':	/* `$p' number of pages processed so far */
		  sprintf (buf, "%d", total_pages);
		  APPEND_STR (buf);
		  break;

		case '(':	/* $(ENVVAR)  */
		  for (j = 0, i++;
		       str[i] && str[i] != ')' && j < sizeof (buf) - 1;
		       i++)
		    buf[j++] = str[i];

		  if (str[i] == '\0')
		    FATAL ((stderr, _("%s: no closing ')' for $() escape"),
			    context_name));
		  if (str[i] != ')')
		    FATAL ((stderr, _("%s: too long variable name for $() escape"),
			    context_name));

		  buf[j] = '\0';

		  cp = getenv (buf);
		  if (cp == NULL)
		    cp = "";
		  APPEND_STR (cp);
		  break;

		case 'C':	/* `$C' modtime in `hh:mm:ss' format */
		  sprintf (buf, "%02d:%02d:%02d", mod_tm.tm_hour,
			   mod_tm.tm_min, mod_tm.tm_sec);
		  APPEND_STR (buf);
		  break;

		case 'D':
		  if (str[i + 1] == '{')
		    {
		      /* `$D{}' format modification date with strftime() */
		      for (j = 0, i += 2;
			   j < sizeof (buf2) && str[i] && str[i] != '}';
			   i++, j++)
			buf2[j] = str[i];
		      if (str[i] != '}')
			FATAL ((stderr,
				_("%s: too long format for $D{} escape"),
				context_name));

		      buf2[j] = '\0';
		      strftime (buf, sizeof (buf), buf2, &mod_tm);
		    }
		  else
		    {
		      /* `$D' mod date in `yy-mm-dd' format */
		      sprintf (buf, "%02d-%02d-%02d", mod_tm.tm_year % 100,
			       mod_tm.tm_mon + 1, mod_tm.tm_mday);
		    }
		  APPEND_STR (buf);
		  break;

		case 'E':	/* `$E' mod date in `yy/mm/dd' format */
		  sprintf (buf, "%02d/%02d/%02d", mod_tm.tm_year % 100,
			   mod_tm.tm_mon + 1, mod_tm.tm_mday);
		  APPEND_STR (buf);
		  break;

		case 'F':	/* `$F' run date in `dd.mm.yyyy' format */
		  sprintf (buf, "%d.%d.%d",
			   mod_tm.tm_mday,
			   mod_tm.tm_mon + 1,
			   mod_tm.tm_year + 1900);
		  APPEND_STR (buf);
		  break;

		case 't':	/* `$t' runtime in 12-hour am/pm format */
		  sprintf (buf, "%d:%d%s",
			   mod_tm.tm_hour > 12
			   ? mod_tm.tm_hour - 12 : mod_tm.tm_hour,
			   mod_tm.tm_min,
			   mod_tm.tm_hour > 12 ? "pm" : "am");
		  APPEND_STR (buf);
		  break;

		case 'T':	/* `$T' runtime in 24-hour format */
		  sprintf (buf, "%d:%d", mod_tm.tm_hour, mod_tm.tm_min);
		  APPEND_STR (buf);
		  break;

		case '*':	/* `$*' runtime in 24-hour format with secs */
		  sprintf (buf, "%d:%d:%d", mod_tm.tm_hour, mod_tm.tm_min,
			   mod_tm.tm_sec);
		  APPEND_STR (buf);
		  break;

		case 'v':	/* `$v': input file number */
		  sprintf (buf, "%d", input_filenum);
		  APPEND_STR (buf);
		  break;

		case 'V':	/* `$V': input file number in --toc format */
		  if (toc)
		    {
		      sprintf (buf, "%d-", input_filenum);
		      APPEND_STR (buf);
		    }
		  break;

		case 'W':	/* `$W' run date in `mm/dd/yy' format */
		  sprintf (buf, "%02d/%02d/%02d", mod_tm.tm_mon + 1,
			   mod_tm.tm_mday, mod_tm.tm_year % 100);
		  APPEND_STR (buf);
		  break;

		case 'N':	/* `$N' the full name of the printed file */
		  APPEND_STR (fname);
		  break;

		case 'n':	/* `$n' input file name without directory */
		  cp = strrchr (fname, '/');
		  if (cp)
		    cp++;
		  else
		    cp = fname;
		  APPEND_STR (cp);
		  break;

		case 'L':	/* `$L' number of lines in this file. */
		  /* This is valid only for TOC-strings. */
		  sprintf (buf, "%d", current_file_linenum - 1);
		  APPEND_STR (buf);
		  break;

		default:
		  FATAL ((stderr, _("%s: unknown `$' escape `%c' (%d)"),
			  context_name, str[i], str[i]));
		  break;
		}
	    }
	  /* Reset width so the else-arm goes ok at the next round. */
	  width = 0;
	  justification = 1;
	}
      else
	APPEND_CH (str[i]);
    }
  APPEND_CH ('\0');

  /* Escape PS specials. */
  cp = escape_string (rbuf);
  xfree (rbuf);

  return cp;
}


void
parse_key_value_pair (StringHashPtr set, char *kv)
{
  char *cp;
  Buffer key;

  cp = strchr (kv, ':');
  if (cp == NULL)
    {
      if (strhash_delete (set, kv, strlen (kv) + 1, (void **) &cp))
	xfree (cp);
    }
  else
    {
      buffer_init (&key);
      buffer_append_len (&key, kv, cp - kv);

      strhash_put (set, buffer_ptr (&key), strlen (buffer_ptr (&key)) + 1,
		   xstrdup (cp + 1), (void **) &cp);
      if (cp)
	xfree (cp);

      buffer_uninit (&key);
    }
}


int
count_key_value_set (StringHashPtr set)
{
  int i = 0, got, j;
  char *cp;
  void *value;

  for (got = strhash_get_first (set, &cp, &j, &value); got;
       got = strhash_get_next (set, &cp, &j, &value))
    i++;

  return i;
}


int
pathwalk (char *path, PathWalkProc proc, void *context)
{
  char buf[512];
  char *cp;
  char *cp2;
  int len, i;

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

      i = (*proc) (buf, context);
      if (i != 0)
	return i;
    }

  return 0;
}


int
file_lookup (char *path, void *context)
{
  int len;
  FileLookupCtx *ctx = context;
  struct stat stat_st;
  int i;

  MESSAGE (2, (stderr, "file_lookup(): %s/%s%s\t", path, ctx->name,
	       ctx->suffix));

  len = strlen (path);
  if (len && path[len - 1] == '/')
    len--;

  buffer_clear (ctx->fullname);
  buffer_append_len (ctx->fullname, path, len);
  buffer_append (ctx->fullname, "/");
  buffer_append (ctx->fullname, ctx->name);
  buffer_append (ctx->fullname, ctx->suffix);

  i = stat (buffer_ptr (ctx->fullname), &stat_st) == 0;

  MESSAGE (2, (stderr, "#%c\n", i ? 't' : 'f'));

  return i;
}


char *
tilde_subst (char *fname)
{
  char *cp;
  int i;
  struct passwd *pswd;
  Buffer buffer;
  char *result;

  if (fname[0] != '~')
    return xstrdup (fname);

  if (fname[1] == '/' || fname[1] == '\0')
    {
      /* The the user's home directory from the `HOME' environment
         variable. */
      cp = getenv ("HOME");
      if (cp == NULL)
	return xstrdup (fname);

      buffer_init (&buffer);
      buffer_append (&buffer, cp);
      buffer_append (&buffer, fname + 1);

      result = buffer_copy (&buffer);
      buffer_uninit (&buffer);

      return result;
    }

  /* Get user's login name. */
  for (i = 1; fname[i] && fname[i] != '/'; i++)
    ;

  buffer_init (&buffer);
  buffer_append_len (&buffer, fname + 1, i - 1);

  pswd = getpwnam (buffer_ptr (&buffer));
  buffer_uninit (&buffer);

  if (pswd)
    {
      /* Found passwd entry. */
      buffer_init (&buffer);
      buffer_append (&buffer, pswd->pw_dir);
      buffer_append (&buffer, fname + i);

      result = buffer_copy (&buffer);
      buffer_uninit (&buffer);

      return result;
    }

  /* No match found. */
  return xstrdup (fname);
}


double
parse_float (char *string, int units, int horizontal)
{
  double val;
  char *end;

  val = strtod (string, &end);
  if (end == string)
  malformed_float:
    ERROR ((stderr, _("malformed float dimension: \"%s\""), string));

  if (units)
    {
      switch (*end)
	{
	case 'c':
	  val *= 72 / 2.54;
	  break;

	case 'p':
	  break;

	case 'i':
	  val *= 72;
	  break;

	case '\0':
	  /* FALLTHROUGH */

	case 'l':
	  if (horizontal)
	    val *= CHAR_WIDTH ('m');
	  else
	    val *= LINESKIP;
	  break;

	default:
	  goto malformed_float;
	  break;
	}
    }
  else
    {
      if (*end != '\0')
	goto malformed_float;
    }

  return val;
}


/*
 * InputStream functions.
 */

int
is_open (InputStream *is, FILE *fp, char *fname, char *input_filter)
{
  /* Init stream variables. */
  is->data_in_buf = 0;
  is->bufpos = 0;
  is->nreads = 0;
  is->unget_ch = NULL;
  is->unget_pos = 0;
  is->unget_alloc = 0;

  /* Input filter? */
  if (input_filter)
    {
      char *cmd = NULL;
      int cmdlen;
      int i, pos;
      char *cp;

      is->is_pipe = 1;

      if (fname == NULL)
	fname = input_filter_stdin;

      /*
       * Count the initial command length, this will grow dynamically
       * when file specifier `%s' is encountered from <input_filter>.
       */
      cmdlen = strlen (input_filter) + 1;
      cmd = xmalloc (cmdlen);

      /* Create filter command. */
      pos = 0;
      for (i = 0; input_filter[i]; i++)
	{
	  if (input_filter[i] == '%')
	    {
	      switch (input_filter[i + 1])
		{
		case 's':
		  /* Expand cmd-buffer. */
		  if ((cp = shell_escape (fname)) != NULL)
		    {
		      cmdlen += strlen (cp);
		      cmd = xrealloc (cmd, cmdlen);

		      /* Paste filename. */
		      strcpy (cmd + pos, cp);
		      pos += strlen (cp);
		      free (cp);
		    }

		  i++;
		  break;

		case '%':
		  cmd[pos++] = '%';
		  i++;
		  break;

		default:
		  cmd[pos++] = input_filter[i];
		  break;
		}
	    }
	  else
	    cmd[pos++] = input_filter[i];
	}
      cmd[pos++] = '\0';

      is->fp = popen (cmd, "r");
      xfree (cmd);

      if (is->fp == NULL)
	{
	  ERROR ((stderr,
		  _("couldn't open input filter \"%s\" for file \"%s\": %s"),
		  input_filter, fname ? fname : "(stdin)",
		  strerror (errno)));
	  return 0;
	}
    }
  else
    {
      /* Just open the stream. */
      is->is_pipe = 0;
      if (fp)
	is->fp = fp;
      else
	{
	  is->fp = fopen (fname, "rb");
	  if (is->fp == NULL)
	    {
	      ERROR ((stderr, _("couldn't open input file \"%s\": %s"), fname,
		      strerror (errno)));
	      return 0;
	    }
	}
    }

  return 1;
}


void
is_close (InputStream *is)
{
  if (is->is_pipe)
    pclose (is->fp);
  else
    fclose (is->fp);

  if (is->unget_ch)
    xfree (is->unget_ch);
}


int
is_getc (InputStream *is)
{
  int ch;

  if (is->unget_pos > 0)
    {
      ch = is->unget_ch[--is->unget_pos];
      return ch;
    }

 retry:

  /* Do we have any data left? */
  if (is->bufpos >= is->data_in_buf)
    {
      /* At the EOF? */
      if (is->nreads > 0 && is->data_in_buf <= 0)
	/* Yes. */
	return EOF;

      /* Read more data. */
      memset (is->buf, 0, sizeof (is->buf));
      is->data_in_buf = fread (is->buf, 1, sizeof (is->buf)-1, is->fp);
      is->bufpos = 0;
      is->nreads++;

      goto retry;
    }

  return is->buf[is->bufpos++];
}


int
is_ungetc (int ch, InputStream *is)
{
  if (is->unget_pos >= is->unget_alloc)
    {
      is->unget_alloc += 1024;
      is->unget_ch = xrealloc (is->unget_ch, is->unget_alloc);
    }

  is->unget_ch[is->unget_pos++] = ch;

  return 1;
}


/*
 * Buffer Functions.
 */

void
buffer_init (Buffer *buffer)
{
  buffer->allocated = 128;
  buffer->data = xmalloc (buffer->allocated);
  buffer->data[0] = '\0';
  buffer->len = 0;
}


void
buffer_uninit (Buffer *buffer)
{
  xfree (buffer->data);
}


Buffer *
buffer_alloc ()
{
  Buffer *buffer = (Buffer *) xcalloc (1, sizeof (Buffer));

  buffer_init (buffer);

  return buffer;
}


void
buffer_free (Buffer *buffer)
{
  buffer_uninit (buffer);
  xfree (buffer);
}


void
buffer_append (Buffer *buffer, const char *data)
{
  buffer_append_len (buffer, data, strlen (data));
}


void
buffer_append_len (Buffer *buffer, const char *data, size_t len)
{
  if (buffer->len + len + 1 >= buffer->allocated)
    {
      buffer->allocated = buffer->len + len + 1024;
      buffer->data = xrealloc (buffer->data, buffer->allocated);
    }

  memcpy (buffer->data + buffer->len, data, len);
  buffer->len += len;

  buffer->data[buffer->len] = '\0';
}


char *
buffer_copy (Buffer *buffer)
{
  char *copy = xmalloc (buffer->len + 1);

  memcpy (copy, buffer->data, buffer->len + 1);

  return copy;
}


void
buffer_clear (Buffer *buffer)
{
  buffer->len = 0;
  buffer->data[0] = '\0';
}


char *
buffer_ptr (Buffer *buffer)
{
  return buffer->data;
}


size_t
buffer_len (Buffer *buffer)
{
  return buffer->len;
}

/*
 * Escapes the name of a file so that the shell groks it in 'single'
 * quotation marks.  The resulting pointer has to be free()ed when not
 * longer used.
*/
char *
shell_escape(const char *fn)
{
  size_t len = 0;
  const char *inp;
  char *retval, *outp;

  for(inp = fn; *inp; ++inp)
    switch(*inp)
    {
      case '\'': len += 4; break;
      default:   len += 1; break;
    }

  outp = retval = malloc(len + 1);
  if(!outp)
    return NULL; /* perhaps one should do better error handling here */
  for(inp = fn; *inp; ++inp)
    switch(*inp)
    {
      case '\'': *outp++ = '\''; *outp++ = '\\'; *outp++ = '\'', *outp++ = '\''; break;
      default:   *outp++ = *inp; break;
    }
  *outp = 0;

  return retval;
}
