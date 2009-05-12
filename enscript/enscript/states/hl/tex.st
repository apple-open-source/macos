/**
 * Name: tex
 * Description: TeX/LaTeX command, comment and math highlighting.
 * Author: Toni Giorgino <toni@pcape2.pi.infn.it>
 */

/* Syntactically highlights _every_ TeX command.  A more specialized
 * LaTeX mode might instead operate by keyword, eg. reproducing text
 * emphasis and boldface, perhaps cluttering the output too much.
 * Label and reference highlighting is a better candidate for a To Do
 * list.  Please report modifications.     TG
 */

state tex extends HighlightEntry
{
  /* escaped comment mark */
  /\\\\%/ {
    language_print ($0);
  }

  /* comment */
  /%/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* one non-letter commands */
  /\\\\[^[:alpha:]]/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* multi letter commands */
  /\\\\[[:alpha:]]+/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* display math environment */
  /\$\$[^$]+\$\$/ {
    string_face (true);
    language_print ($0);
    string_face (false);
  }

  /* inline math environment */
  /\$[^$]+\$/ {
    variable_name_face (true);
    language_print ($0);
    variable_name_face (false);
   }
}


/*
Local variables:
mode: c
End:
*/
