/*
 * The default font.
 * Copyright (c) 1995, 1996, 1997 Markku Rossi.
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
 * Static variables.
 */

static AFMEncodingTable builtin_courier[] =
{
  {32, 	"space"},
  {33, 	"exclam"},
  {34, 	"quotedbl"},
  {35, 	"numbersign"},
  {36, 	"dollar"},
  {37, 	"percent"},
  {38, 	"ampersand"},
  {39, 	"quoteright"},
  {40, 	"parenleft"},
  {41, 	"parenright"},
  {42, 	"asterisk"},
  {43, 	"plus"},
  {44, 	"comma"},
  {45, 	"hyphen"},
  {46, 	"period"},
  {47, 	"slash"},
  {48, 	"zero"},
  {49, 	"one"},
  {50, 	"two"},
  {51, 	"three"},
  {52, 	"four"},
  {53, 	"five"},
  {54, 	"six"},
  {55, 	"seven"},
  {56, 	"eight"},
  {57, 	"nine"},
  {58, 	"colon"},
  {59, 	"semicolon"},
  {60, 	"less"},
  {61, 	"equal"},
  {62, 	"greater"},
  {63, 	"question"},
  {64, 	"at"},
  {65, 	"A"},
  {66, 	"B"},
  {67, 	"C"},
  {68, 	"D"},
  {69, 	"E"},
  {70, 	"F"},
  {71, 	"G"},
  {72, 	"H"},
  {73, 	"I"},
  {74, 	"J"},
  {75, 	"K"},
  {76, 	"L"},
  {77, 	"M"},
  {78, 	"N"},
  {79, 	"O"},
  {80, 	"P"},
  {81, 	"Q"},
  {82, 	"R"},
  {83, 	"S"},
  {84, 	"T"},
  {85, 	"U"},
  {86, 	"V"},
  {87, 	"W"},
  {88, 	"X"},
  {89, 	"Y"},
  {90, 	"Z"},
  {91, 	"bracketleft"},
  {92, 	"backslash"},
  {93, 	"bracketright"},
  {94, 	"asciicircum"},
  {95, 	"underscore"},
  {96, 	"quoteleft"},
  {97, 	"a"},
  {98, 	"b"},
  {99, 	"c"},
  {100,	"d"},
  {101, "e"},
  {102, "f"},
  {103, "g"},
  {104, "h"},
  {105, "i"},
  {106, "j"},
  {107, "k"},
  {108, "l"},
  {109, "m"},
  {110, "n"},
  {111, "o"},
  {112, "p"},
  {113, "q"},
  {114, "r"},
  {115, "s"},
  {116, "t"},
  {117, "u"},
  {118, "v"},
  {119, "w"},
  {120, "x"},
  {121, "y"},
  {122, "z"},
  {123, "braceleft"},
  {124, "bar"},
  {125, "braceright"},
  {126, "asciitilde"},
  {161, "exclamdown"},
  {162, "cent"},
  {163, "sterling"},
  {164, "fraction"},
  {165, "yen"},
  {166, "florin"},
  {167, "section"},
  {168, "currency"},
  {169, "quotesingle"},
  {170, "quotedblleft"},
  {171, "guillemotleft"},
  {172, "guilsinglleft"},
  {173, "guilsinglright"},
  {174, "fi"},
  {175, "fl"},
  {177, "endash"},
  {178, "dagger"},
  {179, "daggerdbl"},
  {180, "periodcentered"},
  {182, "paragraph"},
  {183, "bullet"},
  {184, "quotesinglbase"},
  {185, "quotedblbase"},
  {186, "quotedblright"},
  {187, "guillemotright"},
  {188, "ellipsis"},
  {189, "perthousand"},
  {191, "questiondown"},
  {193, "grave"},
  {194, "acute"},
  {195, "circumflex"},
  {196, "tilde"},
  {197, "macron"},
  {198, "breve"},
  {199, "dotaccent"},
  {200, "dieresis"},
  {202, "ring"},
  {203, "cedilla"},
  {205, "hungarumlaut"},
  {206, "ogonek"},
  {207, "caron"},
  {208, "emdash"},
  {225, "AE"},
  {227, "ordfeminine"},
  {232, "Lslash"},
  {233, "Oslash"},
  {234, "OE"},
  {235, "ordmasculine"},
  {241, "ae"},
  {245, "dotlessi"},
  {248, "lslash"},
  {249, "oslash"},
  {250, "oe"},
  {251, "germandbls"},
  {-1, 	"Aacute"},
  {-1, 	"Acircumflex"},
  {-1, 	"Adieresis"},
  {-1, 	"Agrave"},
  {-1, 	"Aring"},
  {-1, 	"Atilde"},
  {-1, 	"Ccedilla"},
  {-1, 	"Eacute"},
  {-1, 	"Ecircumflex"},
  {-1, 	"Edieresis"},
  {-1, 	"Egrave"},
  {-1, 	"Eth"},
  {-1, 	"Gcaron"},
  {-1, 	"IJ"},
  {-1, 	"Iacute"},
  {-1, 	"Icircumflex"},
  {-1, 	"Idieresis"},
  {-1, 	"Idot"},
  {-1, 	"Igrave"},
  {-1, 	"LL"},
  {-1, 	"Ntilde"},
  {-1, 	"Oacute"},
  {-1, 	"Ocircumflex"},
  {-1, 	"Odieresis"},
  {-1, 	"Ograve"},
  {-1, 	"Otilde"},
  {-1, 	"Scaron"},
  {-1, 	"Scedilla"},
  {-1, 	"Thorn"},
  {-1, 	"Uacute"},
  {-1, 	"Ucircumflex"},
  {-1, 	"Udieresis"},
  {-1, 	"Ugrave"},
  {-1, 	"Yacute"},
  {-1, 	"Ydieresis"},
  {-1, 	"Zcaron"},
  {-1, 	"aacute"},
  {-1, 	"acircumflex"},
  {-1, 	"adieresis"},
  {-1, 	"agrave"},
  {-1, 	"aring"},
  {-1, 	"arrowboth"},
  {-1, 	"arrowdown"},
  {-1, 	"arrowleft"},
  {-1, 	"arrowright"},
  {-1, 	"arrowup"},
  {-1, 	"atilde"},
  {-1, 	"brokenbar"},
  {-1, 	"ccedilla"},
  {-1, 	"center"},
  {-1, 	"copyright"},
  {-1, 	"dectab"},
  {-1, 	"degree"},
  {-1, 	"divide"},
  {-1, 	"down"},
  {-1, 	"eacute"},
  {-1, 	"ecircumflex"},
  {-1, 	"edieresis"},
  {-1, 	"egrave"},
  {-1, 	"eth"},
  {-1, 	"format"},
  {-1, 	"gcaron"},
  {-1, 	"graybox"},
  {-1, 	"iacute"},
  {-1, 	"icircumflex"},
  {-1, 	"idieresis"},
  {-1, 	"igrave"},
  {-1, 	"ij"},
  {-1, 	"indent"},
  {-1, 	"largebullet"},
  {-1, 	"left"},
  {-1, 	"lira"},
  {-1, 	"ll"},
  {-1, 	"logicalnot"},
  {-1, 	"merge"},
  {-1, 	"minus"},
  {-1, 	"mu"},
  {-1, 	"multiply"},
  {-1, 	"notegraphic"},
  {-1, 	"ntilde"},
  {-1, 	"oacute"},
  {-1, 	"ocircumflex"},
  {-1, 	"odieresis"},
  {-1, 	"ograve"},
  {-1, 	"onehalf"},
  {-1, 	"onequarter"},
  {-1, 	"onesuperior"},
  {-1, 	"otilde"},
  {-1, 	"overscore"},
  {-1, 	"plusminus"},
  {-1, 	"prescription"},
  {-1, 	"registered"},
  {-1, 	"return"},
  {-1, 	"scaron"},
  {-1, 	"scedilla"},
  {-1, 	"square"},
  {-1, 	"stop"},
  {-1, 	"tab"},
  {-1, 	"thorn"},
  {-1, 	"threequarters"},
  {-1, 	"threesuperior"},
  {-1, 	"trademark"},
  {-1, 	"twosuperior"},
  {-1, 	"uacute"},
  {-1, 	"ucircumflex"},
  {-1, 	"udieresis"},
  {-1, 	"ugrave"},
  {-1, 	"up"},
  {-1, 	"yacute"},
  {-1, 	"ydieresis"},
  {-1, 	"zcaron"},
  {0, NULL},
};

#define NUM_CHARACTER_METRICS \
  (sizeof (builtin_courier) / sizeof (AFMEncodingTable) - 1)

/*
 * Public functions.
 */

AFMError
afm_open_default_font (AFMHandle handle, AFMFont *font_return)
{
  AFMFont font;
  AFMIndividualCharacterMetrics *cm;
  int i;

  /* Alloc memory. */

  font = (AFMFont) calloc (1, sizeof (*font));
  if (font == NULL)
    goto error_out;
  font->private
    = (struct afm_font_private_data_st *) calloc (1, sizeof (*font->private));
  if (font->private == NULL)
    goto error_out;
  font->private->fontnames = strhash_init ();
  if (font->private->fontnames == NULL)
    goto error_out;

  /* Version. */
  font->version = 4.0;

  /* Global Font Info. */

  font->global_info.FontName = (char *) malloc (strlen ("Courier") + 1);
  if (font->global_info.FontName == NULL)
    goto error_out;
  strcpy (font->global_info.FontName, "Courier");

  font->global_info.FontBBox_llx = -40.0;
  font->global_info.FontBBox_lly = -290.0;
  font->global_info.FontBBox_urx = 640.0;
  font->global_info.FontBBox_ury = 795.0;

  /* Writing directions. */
  font->writing_direction_metrics[0].is_valid = AFMTrue;
  font->writing_direction_metrics[0].IsFixedPitch = AFMTrue;
  font->writing_direction_metrics[0].CharWidth_x = 600.0;
  font->writing_direction_metrics[0].CharWidth_y = 0.0;

  /* Character Metrics. */

  font->num_character_metrics = NUM_CHARACTER_METRICS;
  font->character_metrics
    = (AFMIndividualCharacterMetrics *)
      calloc (NUM_CHARACTER_METRICS, sizeof (AFMIndividualCharacterMetrics));
  if (font->character_metrics == NULL)
    goto error_out;

  for (i = 0; builtin_courier[i].character; i++)
    {
      cm = &font->character_metrics[i];
      cm->name = (char *) malloc (strlen (builtin_courier[i].character) + 1);
      if (cm->name == NULL)
	goto error_out;
      strcpy (cm->name, builtin_courier[i].character);

      if (!strhash_put (font->private->fontnames, cm->name,
			strlen (cm->name), cm, NULL))
	goto error_out;

      cm->character_code = builtin_courier[i].code;
      cm->w0x = 600.0;
      cm->w0y = 0.0;
    }

  *font_return = font;

  return AFM_SUCCESS;


 error_out:
  (void) afm_close_font (font);

  return AFM_ERROR_MEMORY;
}
