/**
 * Name: diffs
 * Description: side diff
 * Author: buchal@ifh.bau-verm.uni-karlsruhe.de
 */

state diffs extends HighlightEntry
{
  /^\t#/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /^.*>\t/ {
    function_name_face (true);
    language_print ($0);
    call (eat_one_line);
    function_name_face (false);
  }

  /^.*\|\t/ {
    reference_face (true);
    language_print ($0);
    call (eat_one_line);
    reference_face (false);
  }
  /^.*<$/ {
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
