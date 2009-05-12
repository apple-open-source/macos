/**
 * Name: vhdl
 * Description: VHSIC Hardware Description Language (VHDL)
 *		Highlights keywords, comments and special vhdl
 *		constructs.  Please send comments or suggestions.
 *
 * Author: Brian Silveira (brian@nortel.ca)
 *
 */

state vhdl extends HighlightEntry
{
  /* Comments. */
  /--/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* Keywords.
     (build-re '(abs access after alias all and architecture array
     assert attribute begin begin block body body buffer bus case
     component configuration configuration constant disconnect downto
     else elsif end entity exit file for function function generate
     generic guarded if in inout is label library linkage loop map
     mod nand new next nor not null of on open or others out package
     port procedure process range record register rem report return
     reverse select severity signal subtype then to transport type
     units until use variable wait when while with with xor)
     t)
   */
  /\b([aA]([bB][sS]|[cC][cC][eE][sS][sS]|[fF][tT][eE][rR]\
|[lL]([iI][aA][sS]|[lL])|[nN][dD]\
|[rR]([cC][hH][iI][tT][eE][cC][tT][uU][rR][eE]|[rR][aA][yY])\
|[sS][sS][eE][rR][tT]|[tT][tT][rR][iI][bB][uU][tT][eE])\
|[bB]([eE][gG][iI][nN]()|[lL][oO][cC][kK]|[oO][dD][yY]()\
|[uU]([fF][fF][eE][rR]|[sS]))\
|[cC]([aA][sS][eE]\
|[oO]([mM][pP][oO][nN][eE][nN][tT]\
|[nN]([fF][iI][gG][uU][rR][aA][tT][iI][oO][nN]()|[sS][tT][aA][nN][tT])))\
|[dD]([iI][sS][cC][oO][nN][nN][eE][cC][tT]|[oO][wW][nN][tT][oO])\
|[eE]([lL][sS]([eE]|[iI][fF])|[nN]([dD]|[tT][iI][tT][yY])|[xX][iI][tT])\
|[fF]([iI][lL][eE]|[oO][rR]|[uU][nN][cC][tT][iI][oO][nN]())\
|[gG]([eE][nN][eE][rR]([aA][tT][eE]|[iI][cC])|[uU][aA][rR][dD][eE][dD])\
|[iI]([fF]|[nN](|[oO][uU][tT])|[sS])\
|[lL]([aA][bB][eE][lL]|[iI]([bB][rR][aA][rR][yY]|[nN][kK][aA][gG][eE])\
|[oO][oO][pP])\
|[mM]([aA][pP]|[oO][dD])\
|[nN]([aA][nN][dD]|[eE]([wW]|[xX][tT])|[oO]([rR]|[tT])|[uU][lL][lL])\
|[oO]([fF]|[nN]|[pP][eE][nN]|[rR]|[tT][hH][eE][rR][sS]|[uU][tT])\
|[pP]([aA][cC][kK][aA][gG][eE]|[oO][rR][tT]\
|[rR][oO][cC][eE]([dD][uU][rR][eE]|[sS][sS]))\
|[rR]([aA][nN][gG][eE]\
|[eE]([cC][oO][rR][dD]|[gG][iI][sS][tT][eE][rR]|[mM]|[pP][oO][rR][tT]\
|[tT][uU][rR][nN]|[vV][eE][rR][sS][eE]))\
|[sS]([eE]([lL][eE][cC][tT]|[vV][eE][rR][iI][tT][yY])|[iI][gG][nN][aA][lL]\
|[uU][bB][tT][yY][pP][eE])\
|[tT]([hH][eE][nN]|[oO]|[rR][aA][nN][sS][pP][oO][rR][tT]|[yY][pP][eE])\
|[uU]([nN]([iI][tT][sS]|[tT][iI][lL])|[sS][eE])\
|[vV][aA][rR][iI][aA][bB][lL][eE]\
|[wW]([aA][iI][tT]|[hH]([eE][nN]|[iI][lL][eE])|[iI][tT][hH]())\
|[xX][oO][rR])\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /<=|=>/ {
    reference_face (true);
    language_print ($0);
    reference_face(false);
  }
}


/*
Local variables:
mode: c
End:
*/
