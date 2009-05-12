/**
 * Name: delphi
 * Description: Delphi programming language.
 * Author: Michael Van Canneyt <michael@tfdec1.fys.kuleuven.ac.be>
 */

state delphi extends HighlightEntry
{
  BEGIN {
    require_state (pascal);
  }

  /* comments */
  /(\{|\(\*)/ {
    comment_face (true);
    language_print ($0);
    call (pascal_comment);
    comment_face (false);
  }
  /* C++ -style comments */
  /\/\// {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
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
     (build-re '(and asm array begin case const constructor destructor
     div do downto else end file for function goto if implementation
     in inline interface label mod nil not object of or packed
     procedure program record repeat set shlr string then to type
     unit until uses var while with xor As class except exports
     finalization finally inherited initialization is library property
     raise threAdvar try absolute abstract assembler automated cdecl
     default dispid dynamic export external far forward index message
     name near nodefault override pascal private protected public
     published read register resident stdcall stored virtual write)
     t)
   */
  /\b(A[sS]\
|[aA]([bB][sS]([oO][lL][uU][tT][eE]|[tT][rR][aA][cC][tT])|[nN][dD]\
|[rR][rR][aA][yY]|[sS]([mM]|[sS][eE][mM][bB][lL][eE][rR])\
|[uU][tT][oO][mM][aA][tT][eE][dD])\
|[bB][eE][gG][iI][nN]\
|[cC]([aA][sS][eE]|[dD][eE][cC][lL]|[lL][aA][sS][sS]\
|[oO][nN][sS][tT](|[rR][uU][cC][tT][oO][rR]))\
|[dD]([eE]([fF][aA][uU][lL][tT]|[sS][tT][rR][uU][cC][tT][oO][rR])\
|[iI]([sS][pP][iI][dD]|[vV])|[oO](|[wW][nN][tT][oO])\
|[yY][nN][aA][mM][iI][cC])\
|[eE]([lL][sS][eE]|[nN][dD]\
|[xX]([cC][eE][pP][tT]|[pP][oO][rR][tT](|[sS])|[tT][eE][rR][nN][aA][lL]))\
|[fF]([aA][rR]\
|[iI]([lL][eE]|[nN][aA][lL]([iI][zZ][aA][tT][iI][oO][nN]|[lL][yY]))\
|[oO][rR](|[wW][aA][rR][dD])|[uU][nN][cC][tT][iI][oO][nN])\
|[gG][oO][tT][oO]\
|[iI]([fF]|[mM][pP][lL][eE][mM][eE][nN][tT][aA][tT][iI][oO][nN]\
|[nN](|[dD][eE][xX]|[hH][eE][rR][iI][tT][eE][dD]\
|[iI][tT][iI][aA][lL][iI][zZ][aA][tT][iI][oO][nN]|[lL][iI][nN][eE]\
|[tT][eE][rR][fF][aA][cC][eE])\
|[sS])\
|[lL]([aA][bB][eE][lL]|[iI][bB][rR][aA][rR][yY])\
|[mM]([eE][sS][sS][aA][gG][eE]|[oO][dD])\
|[nN]([aA][mM][eE]|[eE][aA][rR]|[iI][lL]\
|[oO]([dD][eE][fF][aA][uU][lL][tT]|[tT]))\
|[oO]([bB][jJ][eE][cC][tT]|[fF]|[rR]|[vV][eE][rR][rR][iI][dD][eE])\
|[pP]([aA]([cC][kK][eE][dD]|[sS][cC][aA][lL])\
|[rR]([iI][vV][aA][tT][eE]\
|[oO]([cC][eE][dD][uU][rR][eE]|[gG][rR][aA][mM]|[pP][eE][rR][tT][yY]\
|[tT][eE][cC][tT][eE][dD]))\
|[uU][bB][lL][iI]([cC]|[sS][hH][eE][dD]))\
|[rR]([aA][iI][sS][eE]\
|[eE]([aA][dD]|[cC][oO][rR][dD]|[gG][iI][sS][tT][eE][rR]|[pP][eE][aA][tT]\
|[sS][iI][dD][eE][nN][tT]))\
|[sS]([eE][tT]|[hH][lL][rR]\
|[tT]([dD][cC][aA][lL][lL]|[oO][rR][eE][dD]|[rR][iI][nN][gG]))\
|[tT]([hH]([eE][nN]|[rR][eE]A[dD][vV][aA][rR])|[oO]|[rR][yY]|[yY][pP][eE])\
|[uU]([nN]([iI][tT]|[tT][iI][lL])|[sS][eE][sS])\
|[vV]([aA][rR]|[iI][rR][tT][uU][aA][lL])\
|[wW]([hH][iI][lL][eE]|[iI][tT][hH]|[rR][iI][tT][eE])|[xX][oO][rR])\b/ {
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
