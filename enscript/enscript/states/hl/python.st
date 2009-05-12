/**
 * Name: python
 * Description: Python programming language.
 * Author: Andy Eskilsson <Andy.Eskilsson@telelogic.se>
 */

state python_string extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  python_string_end {
    language_print ($0);
    return;
  }
}

state python extends HighlightEntry
{
  /* Comments. */
  /#/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }
  /* Python strings */
  /(\"\"\"|[\'][\'][\'])/ {
    python_string_end = regexp($0);
    string_face (true);
    language_print ($0);
    call (python_string);
    string_face (false);
  }

  /(\"|[\'])/ {
    python_string_end = regexp( $0 );
    string_face (true);
    language_print ($0);
    call (python_string);
    string_face (false);
  }

  /* Function */
  /([ \t]*)(def)([ \t]+)([^(]*)/ {
    /* indentation */
    language_print ($1);

    /* def */
    keyword_face (true);
    language_print ($2);
    keyword_face (false);

    /* middle */
    language_print ($3);

    /* Function name. */
    function_name_face (true);
    language_print ($4);
    function_name_face (false);
  }

    /* Keywords
       (build-re '(and assert break class continue def del elif else
       else: except except: exec finally for from global if import in
       is lambda not or pass print raise return try try: while
       yield)) */
  /\b(a(nd|ssert)|break|c(lass|ontinue)|de(f|l)\
|e(l(if|se(|:))|x(cept(|:)|ec))|f(inally|or|rom)|global|i(f|mport|n|s)\
|lambda|not|or|p(ass|rint)|r(aise|eturn)|try(|:)|while|yield)\b/ {
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
