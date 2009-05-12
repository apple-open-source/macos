/**
 * Name: scheme
 * Description: Scheme programming language.
 * Author: Markku Rossi <mtr@iki.fi>
 */

state scheme extends HighlightEntry
{
  BEGIN {
    /*
     * Modify regexp character syntax so that we can distinguish all
     * scheme symbols.
     */
    extras = list ('!', '$', '%', '&', '*', '/', ':', '<',
		   '=', '>', '?', '~', '^', '.', '+', '-');
    for (i = 0; i < length (extras); i = i + 1)
      regexp_syntax (extras[i], 'w');
  }

  /* Comments. */
  /;/ {
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

  /* Definitions. */
  /(\([ \t]*)(define)([ \t]+\(?)([!\$%&\*\/:<=>\?~_^a-zA-Z][!\$%&\*\/:<=>\?~_^a-zA-Z0-9.+\-]*)/ {
    /* Starting garbage. */
    language_print ($1);

    /* Keyword `define'. */
    keyword_face (true);
    language_print ($2);
    keyword_face (false);

    /* Middle garbage. */
    language_print ($3);

    /* Function name. */
    function_name_face (true);
    language_print ($4);
    function_name_face (false);
  }

  /* ':'-names, Emacs highlights these, so do we. */
  /([ \t])(:[!\$%&\*\/:<=>\?~_^a-zA-Z0-9.+\-]*)/ {
    language_print ($1);
    reference_face (true);
    language_print ($2);
    reference_face (false);
  }

  /* Keywords.
     "=>" + "set!" +
     (build-re '(else define unquote unquote-splicing quote lambda
     if begin cond and or case let let* letrec do delay quasiquote))
   */
  /=>|\bset!|\b(and|begin|c(ase|ond)|d(e(fine|lay)|o)|else|if\
|l(ambda|et(|\*|rec))|or|qu(asiquote|ote)|unquote(|-splicing))\b/ {
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
