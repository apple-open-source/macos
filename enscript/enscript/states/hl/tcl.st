/**
 * Name: tcl
 * Description: Tcl programming language.
 * Author: Markku Rossi <mtr@iki.fi>
 */

state tcl_comment extends Highlight
{
  /[^\\\\]\n/ {
    language_print ($0);
    return;
  }
}

state tcl_proc_arglist extends Highlight
{
  /* List of arguments. */
  /{/ {
    language_print ($0);
    variable_name_face (true);
    str = match_balanced_block (/{/, /}/);
    variable_name_face (false);
    language_print (str);
    return;
  }
  /* Only one argument. */
  /[A-Za-z0-9]+/ {
    variable_name_face (true);
    language_print ($0);
    variable_name_face (false);
    return;
  }
  /* No idea what this is??? */
  /[.\n]/ {
    language_print ($0);
    return;
  }
}

state tcl extends HighlightEntry
{
  /* Comments. */
  /#/ {
    comment_face (true);
    language_print ($0);
    call (tcl_comment);
    comment_face (false);
  }
  /#\n/ {
    comment_face (true);
    language_print ($0);
    comment_face (false);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* Procedure definitions. */
  /\b(proc)([ \t]+)([A-Za-z_0-9]+)([ \t]+)/ {
    /* Keyword `proc'. */
    keyword_face (true);
    language_print ($1);
    keyword_face (false);

    /* Middle garbage. */
    language_print ($2);

    /* Function name. */
    function_name_face (true);
    language_print ($3);
    function_name_face (false);

    /* Second middle garbage. */
    language_print ($4);

    /* Function argument list. */
    call (tcl_proc_arglist);
  }

  /* Simple variable reference. */
  /(\$)([A-Za-z_0-9]+)/ {
    language_print ($1);
    variable_name_face (true);
    language_print ($2);
    variable_name_face (false);
  }

  /* {}-enclosed variable reference. */
  /\${/ {
    language_print ($0);
    variable_name_face (true);
    str = match_balanced_block (/{/, /}/);
    variable_name_face (false);
    language_print (str);
  }

  /* Keywords.
     (build-re '(
     ;; Tcl:
     Http Tcl after append array bgerror break case catch cd clock
     concat continue eof error eval exec exit expr fblocked fconfigure file
     fileevent filename flush for foreach format gets glob global history
     if incr info interp join lappend library lindex linsert list llength
     load lose lrange lreplace lsearch lsort open package pid pkg_mkIndex
     proc puts pwd read regexp regsub rename return scan seek set socket
     source split string subst switch tclvars tell time trace unknown unset
     update uplevel upvar vwait while

     ;; Tk:
     bell bind bindtags bitmap button canvas checkbutton clipboard destroy
     entry event focus font frame grab grid image label listbox lower menu
     menubutton message option options pack photo place radiobutton raise
     scale scrollbar selection send text tk tk_bindForTraversal tk_bisque
     tk_chooseColor tk_dialog tk_focusFollowsMouse tk_focusNext
     tk_focusPrev tk_getOpenFile tk_getSaveFile tk_menuBar tk_messageBox
     tk_optionMenu tk_popup tk_setPalette tkerror tkvars tkwait toplevel
     winfo wm
     ))
   */
  /\b(Http|Tcl|a(fter|ppend|rray)\
|b(ell|gerror|i(nd(|tags)|tmap)|reak|utton)\
|c(a(nvas|se|tch)|d|heckbutton|l(ipboard|ock)|on(cat|tinue))|destroy\
|e(ntry|of|rror|v(al|ent)|x(ec|it|pr))\
|f(blocked|configure|ile(|event|name)|lush|o(cus|nt|r(|each|mat))|rame)\
|g(ets|lob(|al)|r(ab|id))|history|i(f|mage|n(cr|fo|terp))|join\
|l(a(bel|ppend)|i(brary|n(dex|sert)|st(|box))|length|o(ad|se|wer)\
|r(ange|eplace)|s(earch|ort))\
|me(nu(|button)|ssage)|op(en|tion(|s))\
|p(ack(|age)|hoto|id|kg_mkIndex|lace|roc|uts|wd)\
|r(a(diobutton|ise)|e(ad|g(exp|sub)|name|turn))\
|s(c(a(le|n)|rollbar)|e(ek|lection|nd|t)|o(cket|urce)|plit|tring|ubst\
|witch)\
|t(clvars|e(ll|xt)|ime\
|k(\
|_(bi(ndForTraversal|sque)|chooseColor|dialog\
|focus(FollowsMouse|Next|Prev)|get(OpenFile|SaveFile)\
|me(nuBar|ssageBox)|optionMenu|popup|setPalette)\
|error|vars|wait)\
|oplevel|race)\
|u(n(known|set)|p(date|level|var))|vwait|w(hile|info|m))\b/ {
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
