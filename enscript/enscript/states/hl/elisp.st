/**
 * Name: elisp
 * Description: Emacs LISP.
 */

state elisp extends HighlightEntry
{
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
  /(\([ \t]*)(defun)([ \t]+\(?)([!\$%&\*\/:<=>\?~_^a-zA-Z][!\$%&\*\/:<=>\?~_^a-zA-Z0-9.+\-]*)/ {
    /* Starting garbage. */
    language_print ($1);

    /* Keyword `defun'. */
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

  /* Keywords taken out of fond-lock.el.  Added: and, or, lambda.
     (build-re '(and or lambda cond if while let let* prog progn
     progv prog1 prog2 prog* inline catch throw save-restriction
     save-excursion save-window-excursion save-selected-window
     save-match-data unwind-protect condition-case track-mouse
     eval-after-load eval-and-compile eval-when-compile when
     unless do flet labels return return-from with-output-to-temp-buffer
     with-timeout))
   */
  /\b(and|c(atch|ond(|ition-case))|do\
|eval-(a(fter-load|nd-compile)|when-compile)|flet|i(f|nline)\
|l(a(bels|mbda)|et(|*))|or|prog(|*|1|2|n|v)|return(|-from)\
|save-(excursion|match-data|restriction|selected-window|window-excursion)\
|t(hrow|rack-mouse)|un(less|wind-protect)\
|w(h(en|ile)|ith-(output-to-temp-buffer|timeout)))\b/ {
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
