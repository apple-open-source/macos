/**
 * Name: m4
 * Description: macro processor
 * Author: Jean-Marc Calvez <jean-marc.calvez@st.com>
 */

state m4 extends HighlightEntry
{
  BEGIN {
    require_state (pascal);
  }

  /* Comments. */
  /dnl|#/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* String constants. */
  /\`/ {
    string_face (true);
    language_print ($0);
    call (pascal_string);
    string_face (false);
  }

  /* Keywords:
     (build-re '(changequote changecom decr define defn divert divnum dumpdef
     errprint eval ifdef ifelse include incr index len m4exit m4wrap maketemp
     popdef pushdef shift sinclude substr syscmd sysval translit traceon
     traceoff undefine undivert))
  */
  /\b(change(com|quote)|d(e(cr|f(ine|n))|iv(ert|num)|umpdef)|e(rrprint|val)\
|i(f(def|else)|n(c(lude|r)|dex))|len|m(4(exit|wrap)|aketemp)\
|p(opdef|ushdef)|s(hift|include|ubstr|ys(cmd|val))\
|tra(ceo(ff|n)|nslit)|und(efine|ivert))\b/ {
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
