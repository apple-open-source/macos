/**
 * Name: wmlscript
 * Description: WMLScript language.
 * Author: Markku Rossi <mtr@iki.fi>
 */

state wmlscript_string extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /[\']/ {
    language_print ($0);
    return;
  }
}

state wmlscript extends HighlightEntry
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

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* '' strings. */
  /[\']/ {
    string_face (true);
    language_print ($0);
    call (wmlscript_string);
    string_face (false);
  }

  /* Function definitions. */
  /\b(function)([ \t]+)([A-Za-z\$_][A-Za-z\$_0-9]*)([ \t]*\()/ {
    keyword_face (true);
    language_print ($1);
    keyword_face (false);

    language_print ($2);

    function_name_face (true);
    language_print ($3);
    function_name_face (false);

    language_print ($4);
  }

  /* This is actually punctuation.  We define it here so the `div'
     part won't get accidentally highlighted. */
  /\bdiv=/ {
    language_print($0);
  }

  /* Keywords.
     (build-re '(access agent break case catch class const continue
     debugger default delete div do domain else enum equiv export
     extends extern false finally for function header http if import
     in invalid isvalid lib meta name new null path private public
     return sizeof struct super switch this throw true try typeof use
     user var void with ))
  */
  /\b(a(ccess|gent)|break|c(a(se|tch)|lass|on(st|tinue))\
|d(e(bugger|fault|lete)|iv|o(|main))\
|e(lse|num|quiv|x(port|te(nds|rn)))|f(alse|inally|or|unction)\
|h(eader|ttp)|i(f|mport|n(|valid)|svalid)|lib|meta|n(ame|ew|ull)\
|p(ath|rivate|ublic)|return|s(izeof|truct|uper|witch)\
|t(h(is|row)|r(ue|y)|ypeof)|use(|r)|v(ar|oid)|with)\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* Built-in libraries.
     (build-re '(Lang Float String URL WmlBrowser Dialogs))
   */
  /\b(Dialogs|Float|Lang|String|URL|WmlBrowser)(\.)\
([a-zA-Z_][a-zA-Z_0-9]*)\b/ {
    builtin_face (true);
    language_print ($1);
    builtin_face (false);

    language_print($2);

    builtin_face (true);
    language_print ($3);
    builtin_face (false);
  }
}


/*
Local variables:
mode: c
End:
*/
