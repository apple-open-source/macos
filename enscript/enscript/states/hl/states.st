/**
 * Name: states
 * Description: States program's definition files.
 * Author: Markku Rossi <mtr@iki.fi>
 */

state states_regexp extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /\// {
    language_print ($0);
    return;
  }
}

state states extends HighlightEntry
{
  /* Comments. */
  /\/\*/ {
    comment_face (true);
    language_print ($0);
    call (c_comment);
    comment_face (false);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* Regular expressions. */
  /\\\// {
    string_face (true);
    language_print ($0);
    call (states_regexp);
    string_face (false);
  }

  /* Subroutine definitions. */
  /\b(sub)([ \t]+)([$a-zA-Z_][$a-zA-Z_0-9]*)([ \t]*\()/ {
    keyword_face (true);
    language_print ($1);
    keyword_face (false);

    language_print ($2);

    function_name_face (true);
    language_print ($3);
    function_name_face (false);

    language_print ($4);
  }

  /* Keywords.
     (build-re '(BEGIN END div else extends for if local namerules
     return start startrules state sub while))
   */
  /\b(BEGIN|END|div|e(lse|xtends)|for|if|local|namerules|return\
|s(ta(rt(|rules)|te)|ub)|while)\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* Build-ins:
     (build-re '(call calln check_namerules check_startrules concat
     getenv int length list panic prereq print regmatch regsub
     regsuball require_state sprintf strcmp string strncmp substring))
   */
  /\b(c(all(|n)|heck_(namerules|startrules)|oncat)|getenv|int|l(ength|ist)\
|p(anic|r(ereq|int))|re(g(match|sub(|all))|quire_state)\
|s(printf|tr(cmp|ing|ncmp)|ubstring))([ \t]*\()/ {
    builtin_face (true);
    language_print ($1);
    builtin_face (false);
    language_print (substring ($0, length ($1), length ($0)));
  }
}


/*
Local variables:
mode: c
End:
*/
