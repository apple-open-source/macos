/**
 * Name: inf
 * Description: GUI INF Script language
 * Author: Markku Rossi <mtr@iki.fi>
 */

state inf_string extends Highlight
{
  /\"\"/ {
    /* A quoted '"' character. */
    language_print($0);
  }
  /\"/ {
    /* End of the string. */
    language_print($0);
    return;
  }
}


state inf extends HighlightEntry
{
  /* Comments. */
  /;/ {
    comment_face(true);
    language_print($0);
    call(eat_one_line);
    comment_face(false);
  }

  /* Section names. */
  /^(\[)([^\]]+)(\][ \t]*)/ {
    language_print($1);

    function_name_face(true);
    language_print($2);
    function_name_face(false);

    language_print($3);
  }

  /* String constants. */
  /\"\"\"/ {
    /* A string starting with a '"' character. */
    string_face(true);
    language_print($0);
    call(inf_string);
    string_face(false);
  }
  /\"\"/ {
    /* An empty string. */
    string_face(true);
    language_print($0);
    string_face(false);
  }
  /\"/ {
    /* Start of a string. */
    string_face(true);
    language_print($0);
    call(inf_string);
    string_face(false);
  }

  /* Labels. */
  /^([a-zA-Z_][a-zA-Z_0-9]*)([ \t]*=[ \t]*\+)/ {
    reference_face(true);
    language_print($1);
    reference_face(false);
    language_print($2);
  }
  /* Goto statements and its target. */
  /(goto)([ \t]*)([a-zA-Z_][a-zA-Z_0-9]*)/ {
    keyword_face(true);
    language_print($1);
    keyword_face(false);

    language_print($2);

    reference_face(true);
    language_print($3);
    reference_face(false);
  }

  /* Shell section calls. */
  /\bshell\b/i {
    builtin_face(true);
    language_print($0);
    builtin_face(false);
  }

  /* The read-syms, install, and detect calls. */
  /^([ \t]*)(read-syms|detect|install)([ \t]+)([a-zA-Z_][a-zA-Z_0-9]*)/i {
    language_print($1);

    builtin_face(true);
    language_print($2);
    builtin_face(false);

    language_print($3);

    reference_face(true);
    language_print($4);
    reference_face(false);
  }

  /* The read-syms, detect, and install in other contexts. */
  /^([ \t]*)(read-syms|detect|install)\b/i {
    language_print($1);

    builtin_face(true);
    language_print($2);
    builtin_face(false);
  }

  /* Variable / constant definitions.  We catch these here so we won't
     conflict with any keywords, etc. */
  /([a-zA-Z_][a-zA-Z_0-9]*[ \t]*=)/ {
    language_print($0);
  }

  /* Keywords, sort of.

     (build-re '(return read-syms ifstr ifint ifcontains else
     else-ifstr else-ifint elseif-contains endif forlistdo
     endforlistdo goto set set-subst set-add set-sub set-mul set-div
     set-or set-hextodec set-dectohex exit))

  */
  /\b(e(lse(|-if(int|str)|if-contains)|nd(forlistdo|if)|xit)|forlistdo|goto\
|if(contains|int|str)|re(ad-syms|turn)\
|set(|-(add|d(ectohex|iv)|hextodec|mul|or|sub(|st))))\b(\(i\))?/i {
    keyword_face(true);
    language_print($0);
    keyword_face(false);
  }



}


/*
Local variables:
mode: c
End:
*/
