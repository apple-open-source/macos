/**
 * Name: idl
 * Description: IDL (CORBA Interface Definition Language)
 */

state idl extends HighlightEntry
{
  /* Comments. */
  /\/\*/ {
    comment_face (true);
    language_print ($0);
    call (c_comment);
    comment_face (false);
  }
  /\/\// {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* Character constants. */
  /'.'|'\\\\.'/ {
    string_face (true);
    language_print ($0);
    string_face (false);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* Pre-processor lines. */
  /^#/ {
    reference_face (true);
    language_print ($0);
    call (c_ppline);
    reference_face (false);
  }

  /* Boolean literals */
  /\b(TRUE|FALSE)\b/ {
    string_face (true);
    language_print ($0);
    string_face (false);
  }

  /* Keywords.
    (build-re '(any attribute boolean case char const context default double
    enum exception fixed float in inout interface long module Object
    octet oneway out native raises readonly sequence short string struct switch
    typedef unsigned union void wchar wstring))
  */
  /\b(Object|a(ny|ttribute)|boolean|c(ase|har|on(st|text))|d(efault|ouble)\
|e(num|xception)|f(ixed|loat)|in(|out|terface)|long|module|native\
|o(ctet|neway|ut)|r(aises|eadonly)|s(equence|hort|tr(ing|uct)|witch)\
|typedef|un(ion|signed)|void|w(char|string))\b/ {
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
