/**
 * Name: pascal
 * Description: Pascal programming language
 * Author: Michael Van Canneyt <michael@tfdec1.fys.kuleuven.ac.be>
 */

state pascal_comment extends Highlight
{
  /(\}|\*\))/ {
    language_print ($0);
    return;
  }
}

state pascal_string extends Highlight
{
  /[\']/ {
    language_print ($0);
    return;
  }
}

state pascal extends HighlightEntry
{
  /* comments */
  /(\{|\(\*)/ {
    comment_face (true);
    language_print ($0);
    call (pascal_comment);
    comment_face (false);
  }
  /* strings */
  /[\']/ {
    string_face (true);
    language_print ($0);
    call (pascal_string);
    string_face (false);
  }
  /* Keywords.
     (build-re '(and asm array begin case const constructor destructor div
     do downto else end file for function goto if implementation in inline
     interface label mod nil not object of or packed procedure program record
     repeat set shlr string then to type unit until uses var while with xor)
     t)
   */
  /\b([aA]([nN][dD]|[rR][rR][aA][yY]|[sS][mM])|[bB][eE][gG][iI][nN]\
|[cC]([aA][sS][eE]|[oO][nN][sS][tT](|[rR][uU][cC][tT][oO][rR]))\
|[dD]([eE][sS][tT][rR][uU][cC][tT][oO][rR]|[iI][vV]|[oO](|[wW][nN][tT][oO]))\
|[eE]([lL][sS][eE]|[nN][dD])\
|[fF]([iI][lL][eE]|[oO][rR]|[uU][nN][cC][tT][iI][oO][nN])\
|[gG][oO][tT][oO]\
|[iI]([fF]|[mM][pP][lL][eE][mM][eE][nN][tT][aA][tT][iI][oO][nN]\
|[nN](|[lL][iI][nN][eE]|[tT][eE][rR][fF][aA][cC][eE]))\
|[lL][aA][bB][eE][lL]|[mM][oO][dD]|[nN]([iI][lL]|[oO][tT])\
|[oO]([bB][jJ][eE][cC][tT]|[fF]|[rR])\
|[pP]([aA][cC][kK][eE][dD]\
|[rR][oO]([cC][eE][dD][uU][rR][eE]|[gG][rR][aA][mM]))\
|[rR][eE]([cC][oO][rR][dD]|[pP][eE][aA][tT])\
|[sS]([eE][tT]|[hH][lL][rR]|[tT][rR][iI][nN][gG])\
|[tT]([hH][eE][nN]|[oO]|[yY][pP][eE])\
|[uU]([nN]([iI][tT]|[tT][iI][lL])|[sS][eE][sS])|[vV][aA][rR]\
|[wW]([hH][iI][lL][eE]|[iI][tT][hH])|[xX][oO][rR])\b/ {
  keyword_face (true);
  language_print ($0);
  keyword_face (false);
  }
}


/*
Local variables:
mode: c
End:
*/
