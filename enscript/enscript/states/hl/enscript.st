/*
 * States definitions file for GNU Enscript.
 * Copyright (c) 1997-2003 Markku Rossi.
 * Author: Markku Rossi <mtr@iki.fi>
 *
 * The latest version of this file can be downloaded from URL:
 *
 *     http://www.iki.fi/~mtr/genscript/enscript.st
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * $Id: enscript.st,v 1.3 2003/03/05 08:31:31 mtr Exp $
 */

/*
 * Guildelines for writing new highlighting rules for the GNU Enscript.
 *
 * - all highlighting states should have a document comment like this:
 *
 *   /**
 *    * Name: c
 *    * Description: C programming language.
 *    * Author: Author Name <author@email.address>
 *    * ...
 *
 *   It is used by enscript's --help-pretty-print option to print
 *   description about supported highlighting modes.
 *
 * - the main entry state (for example, for the C, the state `c') must
 *   be inherited from state `HighlightEntry':
 *
 *   state c extends HighlightEntry
 *   {
 *     ...
 *
 * - all help-states smust be inherited from state `Highlight':
 *
 *   state c_string extends Highlight
 *   {
 *     ...
 *
 * - all printing should be done with the language_print() procedure
 *   instead of the print() primitive.
 *
 * - using enscript.el to build regular expressions:
 *
 *      M-x load-file RET enscript.el RET
 *
 *   Move in the buffer to the point where the (build-re '()) ends,
 *   that is, after the last closing parenthesis ')'.  Then, type:
 *
 *      C-x C-e
 *
 *   Magic should happen.
 *
 * These rules ensures that enscript's --help-pretty-print option and
 * the different output languages (HTML, RTF, etc.) work.
 */

/* This script needs at least this version of the States program. */
prereq ("1.6.2");

/*
 * Constants, fonts, etc.
 */

debug = "0";

/* Boolean values. */
true = 1;
false = 0;

font_spec = "Courier@10";

/* These components are resolved from <font_spec>. */
font = "";
ptsize = "";

/*
 * Generatel table of contents?  This is not supported by all output
 * languages.
 */
toc = "0";

/*
 * The Highlight style.  The default style is `emacs' to imitate
 * Emacs' font lock mode.
 */
style = "emacs";

/*
 * Create color outputs?
 */
color = "1";

/*
 * Use verbose highlighting rules?
 */
verbose_highlighting = false;

/*
 * Target language.  Possible values are:
 * - enscript		generate enscript special escapes
 * - html		generate HTML
 * - overstrike		generate overstrike (line printers, less)
 * - texinfo		generate Texinfo
 * - rtf                generate Rich Text Format (rtf - MS Word, WordPerfect)
 *                      This code can be souched into MS Word or PowerPoint
 *                      for a pretty version of the code
 */
language = "enscript";

/*
 * How many input files we have.
 */
num_input_files = "1";
current_input_file = 0;

/*
 * Document title.
 */
document_title = "Enscript Output";

/*
 * Global variables for nested highlighting `nested.st'.
 */
nested_start = "^-+(([ \t]*)([a-zA-Z_0-9]*)([ \t]*))-+$";
nested_start_re = 0;
nested_end = "^-+$";
nested_end_re_cached = 0;
nested_end_re = 0;
nested_default = "passthrough";

/*
 * Color definitions.
 */

cindex = 0;
rgb_values = list ();

sub define_color (name, r, g, b)
{
  rgb_values[cindex] = list (name, r, g, b);
  cindex = cindex + 1;
}

sub color_index (name)
{
  local i;

  for (i = 0; i < length (rgb_values); i = i + 1)
    if (strcmp (rgb_values[i][0], name) == 0)
      return i;

  return -1;
}

sub language_color (name)
{
  local idx;

  idx = color_index (name);
  if (idx < 0)
    panic ("unknown color `", name, "'");

  /*
   * The map_color() subroutine is language specific and defined in
   * *_faces() subroutine.
   */
  map_color (rgb_values[idx][1], rgb_values[idx][2], rgb_values[idx][3]);
}

/* RGB definitions for colors.	These are borrowed from X's rgb.txt file. */

define_color ("black",			0, 0, 0);
define_color ("gray25",			64, 64, 64);
define_color ("gray50",			127, 127, 127);
define_color ("gray75",			191, 191, 191);
define_color ("gray85",			217, 217, 217);
define_color ("gray90",			229, 229, 229);
define_color ("gray95",			242, 242, 242);
define_color ("blue",			0, 0, 255);
define_color ("cadet blue",		95, 158, 160);
define_color ("dark goldenrod",		184, 134, 11);
define_color ("dark olive green",	85, 107, 47);
define_color ("firebrick",		178, 34, 34);
define_color ("forest green",		34, 139, 34);
define_color ("green",			0, 255, 0);
define_color ("orchid",			218, 112, 214);
define_color ("purple",			160, 32, 240);
define_color ("red",			255, 0, 0);
define_color ("rosy brown",		188, 143, 143);

define_color ("DarkSeaGreen",		143, 188, 143);
define_color ("DarkSeaGreen1",		193, 255, 193);
define_color ("DarkSeaGreen2",		180, 238, 180);
define_color ("DarkSeaGreen3",		155, 205, 155);
define_color ("DarkSeaGreen4",		105, 139, 105);
define_color ("Goldenrod",		237, 218, 116);
define_color ("Aquamarine",		67, 183, 186);
define_color ("SeaGreen2",		100, 233, 134);
define_color ("Coral",			247, 101,  65);
define_color ("DarkSlateGray1",		154, 254, 255);
define_color ("LightGrey",		211, 211, 211);


/*
 * General helpers.
 */

sub debug (msg)
{
  if (debug_level)
    print ("DEBUG: ", msg, "\n");
}

sub is_prefix (prefix, string)
{
  return strncmp (prefix, string, length (prefix)) == 0;
}

sub strchr (string, ch)
{
  local len = length (string), i;

  for (i = 0; i < len; i = i + 1)
    if (string[i] == ch)
      return i;

  return -1;
}

sub need_version (major, minor, beta)
{
  local r, v, i;

  regmatch (version, (/([0-9]+)\.([0-9]+)\.([0-9]+)/));
  v = list (int ($1), int ($2), int ($3));
  r = list (major, minor, beta);

  for (i = 0; i < 3; i = i + 1)
    if (v[i] > r[i])
      return 1;
    else if (v[i] < r[i])
      return 0;

  /* Exact match. */
  return 1;
}

/* Highlight types which match expression <re> from string <data>. */
sub highlight_types (data, re)
{
  local match_len;

  while (regmatch (data, re))
    {
      language_print ($B);
      type_face (true);
      language_print ($0);
      type_face (false);

      match_len = length ($B, $0);

      data = substring (data, match_len, length (data));
    }

  language_print (data);
}


/*
 * The supported faces.  These functions are used in the highlighting
 * rules to mark different logical elements of the code.  The
 * different faces and their properties (face_*) are defined in the
 * style definition files.  The face_on() and face_off() functions are
 * defined in the output language definition files.
 */

sub bold (on)
{
  if (on)
    face_on (face_bold);
  else
    face_off (face_bold);
}

sub italic (on)
{
  if (on)
    face_on (face_italic);
  else
    face_off (face_italic);
}

sub bold_italic (on)
{
  if (on)
    face_on (face_bold_italic);
  else
    face_off (face_bold_italic);
}

sub comment_face (on)
{
  if (on)
    face_on (face_comment);
  else
    face_off (face_comment);
}

sub function_name_face (on)
{
  if (on)
    face_on (face_function_name);
  else
    face_off (face_function_name);
}

sub variable_name_face (on)
{
  if (on)
    face_on (face_variable_name);
  else
    face_off (face_variable_name);
}

sub keyword_face (on)
{
  if (on)
    face_on (face_keyword);
  else
    face_off (face_keyword);
}

sub reference_face (on)
{
  if (on)
    face_on (face_reference);
  else
    face_off (face_reference);
}

sub string_face (on)
{
  if (on)
    face_on (face_string);
  else
    face_off (face_string);
}

sub builtin_face (on)
{
  if (on)
    face_on (face_builtin);
  else
    face_off (face_builtin);
}

sub type_face (on)
{
  if (on)
    face_on (face_type);
  else
    face_off (face_type);
}

sub highlight_face (on)
{
  if (on)
    face_on (face_highlight);
  else
    face_off (face_highlight);
}


/*
 * Initializations.
 */

start
{
  /* Set debug level. */
  debug_level = int (debug);

  /* Use colors? */
  color = int (color);

  /* Increment input file count. */
  current_input_file = current_input_file + 1;

  /* Resolve fonts. */
  idx = strchr (font_spec, '@');
  if (idx < 0)
    panic ("malformed font spec: `", font_spec, "'");

  font = substring (font_spec, 0, idx);
  ptsize = substring (font_spec, idx + 1, length (font_spec));

  debug (concat ("start: ", font, "@", ptsize));

  /* Construct bold, italic, etc. fonts for our current body font. */
  if (is_prefix ("AvantGarde", font))
    {
      bold_font = "AvantGarde-Demi";
      italic_font = "AvantGarde-BookOblique";
      bold_italic_font = "AvantGarde-DemiOblique";
    }
  else if (regmatch (font, /^Bookman|Souvenir/))
    {
      bold_font = concat ($0, "-Demi");
      italic_font = concat ($0, "-LightItalic");
      bold_italic_font = concat ($0, "-DemiItalic");
    }
  else if (regmatch (font, /^Lucida(Sans-)?Typewriter/))
    {
      bold_font = concat ($0, "Bold");
      italic_font = concat ($0, "Oblique");
      bold_italic_font = concat ($0, "BoldOblique");
    }
  else if (regmatch (font, /^(.*)-Roman$/))
    {
      bold_font = concat ($1, "-Bold");
      italic_font = concat ($1, "-Italic");
      bold_italic_font = concat ($1, "-BoldItalic");
    }
  else
    {
      bold_font = concat (font, "-Bold");
      italic_font = concat (font, "-Oblique");
      bold_italic_font = concat (font, "-BoldOblique");
    }

  /* Create regular expressions for nested highlighting. */
  nested_start_re = regexp (nested_start);
  nested_end_re_cached = regexp (nested_end);

  /* Define output faces. */
  calln (concat ("lang_", language));

  /* Define our highlight style. */
  calln (concat ("style_", style));

  /* Resolve start state. */
  if (check_startrules ())
    debug ("startstate from startrules");
  if (check_namerules ())
    debug ("startstate from namerules");
}

namerules
{
  /\.(c|h)$/					c;
  /\.(c++|C|H|cpp|cc|cxx)$/			cpp;
  /\.m$/					matlab;
  /\.(mpl|mp|maple)$/				maple;
  /\.(scm|scheme)$/				scheme;
  /\b\.emacs$|\.el$/				elisp;
  /\.ad(s|b|a)$/				ada;
  /\.[Ss]$/					asm;
  /\.st$/					states;
  /(M|m)akefile.*/				makefile;
  /\.(MOD|DEF|mi|md)$/				modula_2;
  /\.tcl$/					tcl;
  /\.(v|vh)$/					verilog;
  /\.html?$/					html;
  /\bChangeLog$/				changelog;
  /\.(vhd|vhdl)$/				vhdl;
  /\.(scr|.syn|.synth)$/			synopsys;
  /\.idl$/					idl;
  /\.(hs|lhs|gs|lgs)$/				haskell;
  /\.(pm|pl)$/					perl;
  /\.(eps|EPS|ps|PS)$/				postscript;
  /\.py$/					python;
  /\.pyx$/					pyrex;
  /\.js$/					javascript;
  /\.java$/					java;
  /\.([Pp][Aa][Ss]|[Pp][Pp]|[Pp])$/		pascal;
  /\.[fF]$/					fortran;
  /\.awk$/					awk;
  /\.sh$/					sh;
  /\.vba$/					vba;
  /\.(cshrc|login|logout|history|csh)$/		csh;
  /\.tcshrc$/					tcsh;
  /\.(zshenv|zprofile|zshrc|zlogin|zlogout)$/	zsh;
  /\.(bash_profile|bashrc|inputrc)$/		bash;
  /\.m4$/					m4;
  /\.il$/					skill;
  /\.wrl$/					vrml;
  /\b(rfc.*\.txt|draft-.*\.txt)$/		rfc;
  /\.inf$/i					inf;
  /\.tex$/					tex;
  /\.wmlscript$/				wmlscript;
  /\.wmls$/					wmlscript;
  /^.*$/					passthrough;
}

startrules
{
  /.\010.\010.\010./					nroff;
  /-\*- [Cc] -\*-/					c;
  /-\*- [Cc]\+\+ -\*-/					cpp;
  /-\*- [Aa][Dd][Aa] -\*-/				ada;
  /-\*- [Aa][Ss][Mm] -\*-/				asm;
  /-\*- [Oo][Bb][Jj][Cc] -\*-/				objc;
  /-\*- [Ss][Cc][Hh][Ee][Mm][Ee] -\*-/			scheme;
  /-\*- [Ee][Mm][Aa][Cc][Ss] [Ll][Ii][Ss][Pp] -\*-/	elisp;
  /-\*- [Tt][Cc][Ll] -\*-/				tcl;
  /-\*- [Vv][Hh][Dd][Ll] -\*-/				vhdl;
  /-\*- [Hh][Aa][Ss][Kk][Ee][Ll][Ll] -\*-/		haskell;
  /-\*- [Ii][Dd][Ll] -\*-/				idl;
  /-\*- [Pp][Ee][Rr][Ll] -\*-/				perl;
  /^#![ \t]*\/.*\/perl/					perl;
  /^From:/						mail;
  /^#![ \t]*(\/usr)?\/bin\/[ngmt]?awk/			awk;
  /^#![ \t]*(\/usr)?\/bin\/sh/				sh;
  /^#![ \t]*(\/usr)?\/bin\/csh/				csh;
  /^#![ \t]*(\/usr)?(\/local)?\/bin\/tcsh/		tcsh;
  /^#![ \t]*(\/usr)?(\/local)?\/bin\/zsh/		zsh;
  /^#![ \t]*(\/usr)?(\/local)?\/bin\/bash/		bash;
  /^#![ \t]*(\/usr)?(\/ccs)?\/bin\/m4/			m4;
  /^#VRML/						vrml;
  /^\04?%!/						postscript;
}


/*
 * The global super states.
 */

state Highlight
{
  /* If you want to preserve enscript's escape sequences in the state
     highlighting, uncomment the following rule.  It passes all
     enscript's escape sequences to the output.

       /^\0[^{]+{[^}]+}/ {
        language_print ($0);
       }
  */

  /* If we are doing nested highlighting (same document can contain
     multiple highlighting styles), the variable `nested_end_re'
     specifies the end of the nesting highlight state. */
  nested_end_re {
    language_print($0);
    return;
  }

  /* Skip output language's special characters. */
  LANGUAGE_SPECIALS {
    language_print ($0);
  }
}

/* How many nesting HighlightEntry states are currently active.  The
   header and trailer will be printed at the nesting level 0. */
highlight_entry_nesting = 0;

state HighlightEntry extends Highlight
{
  BEGIN {
    if (highlight_entry_nesting++ == 0)
      header();
  }
  END {
    if (--highlight_entry_nesting == 0)
      trailer();
  }
}



/*
 * Helper subroutines and states.
 */

state match_balanced_block extends Highlight
{
  match_balanced_block_start {
    language_print ($0);
    match_balanced_block_count = match_balanced_block_count + 1;
  }

  match_balanced_block_end {
    match_balanced_block_count = match_balanced_block_count - 1;
    if (match_balanced_block_count == 0)
      return $0;

    language_print ($0);
  }
}

sub match_balanced_block (starter, ender)
{
  match_balanced_block_count = 1;
  match_balanced_block_start = starter;
  match_balanced_block_end = ender;
  return call (match_balanced_block);
}

state eat_one_line
{
  /.*\n/ {
    language_print ($0);
    return;
  }
}


/*
Local variables:
mode: c
End:
*/
