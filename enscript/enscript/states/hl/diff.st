/**
 * Name: diff
 * Description: normal diffs
 * Author: buchal@ifh.bau-verm.uni-karlsruhe.de
 */

state diff extends HighlightEntry
{
  /^[0-9]/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /^</ {
    function_name_face (true);
    language_print ($0);
    call (eat_one_line);
    function_name_face (false);
  }

  /^>/ {
    reference_face (true);
    language_print ($0);
    call (eat_one_line);
    reference_face (false);
  }
  /^[^\ ]/ {
    string_face (true);
    language_print ($0);
    call (eat_one_line);
    string_face (false);
  }
}


/*
Local variables:
mode: c
End:
*/
