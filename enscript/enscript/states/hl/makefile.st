/**
 * Name: makefile
 * Description: Make program's definition files.
 * Author: Markku Rossi <mtr@iki.fi>
 */

state sh_eat_to_apostrophe extends Highlight
{
  /\'/ {
    language_print ($0);
    return;
  }
}

state sh_eat_to_grave extends Highlight
{
  /`/ {
    language_print ($0);
    return;
  }
}

state makefile extends HighlightEntry
{
  /* Comments. */
  /#/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }
  /* An escaped double quote, this doesn't start a string constant. */
  /\\\"/ {
    language_print ($0);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* Shell apostrophe quote. */
  /\'/ {
    string_face (true);
    language_print ($0);
    call (sh_eat_to_apostrophe);
    string_face (false);
  }

  /* Shell grave quote. */
  /`/ {
    string_face (true);
    language_print ($0);
    call (sh_eat_to_grave);
    string_face (false);
  }

  /* Variable references. */

  /\$\(/ {
    language_print ($0);
    reference_face (true);
    str = match_balanced_block (/\(/, /\)/);
    reference_face (false);
    language_print (str);
  }

  /\${/ {
    language_print ($0);
    reference_face (true);
    str = match_balanced_block (/{/, /}/);
    reference_face (false);
    language_print (str);
  }

  /* Targets. */
  /^[^ \t\r\n]+:/ {
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
