/**
 * Name: c
 * Description: C programming language.
 * Author: Markku Rossi <mtr@iki.fi>
 */

c_type_re =
/* Types.
   (build-re '(auto char const double enum extern float int long
   register short signed static struct typedef union unsigned void
   volatile))
 */
  /\b(auto|c(har|onst)|double|e(num|xtern)|float|int|long|register\
|s(hort|igned|t(atic|ruct))|typedef|un(ion|signed)|vo(id|latile))\b/;

/* The super state of all C highlightings. */
state CHighlight extends Highlight
{
  BEGIN {
    if (verbose_highlighting)
      verbose_re = /(->|<=|>=|==|!=|\&\&|\|\||!)/;
    else
      verbose_re = 0;
  }

  verbose_re {
    match = $0;
    if (strcmp (match, "->") == 0)
      str = "rightarrow";
    else if (strcmp (match, "<=") == 0)
      str = "le";
    else if (strcmp (match, ">=") == 0)
      str = "ge";
    else if (strcmp (match, "==") == 0)
      str = "equiv";
    else if (strcmp (match, "&&") == 0)
      str = "land";
    else if (strcmp (match, "||") == 0)
      str = "lor";
    else if (strcmp (match, "!") == 0)
      str = "lnot";
    else if (strcmp (match, "!=") == 0)
      str = "ne";
    else
      str = 0;

    if (!str || !language_symbol (str))
      language_print ($0);
  }
}

/*
 * The highlight entry for C highlightings.  It is a bit ugly to
 * re-implement the BEGIN and END because they must be kept in sync
 * with the code in the `HighlightEntry'.  But, since we don't support
 * multiple heritance, we have no choice.
 */
state CHighlightEntry extends CHighlight
{
  BEGIN {
    if (highlight_entry_nesting++ == 0)
      header ();
  }
  END {
    if (--highlight_entry_nesting == 0)
      trailer ();
  }
}

state c extends CHighlightEntry
{
  BEGIN {
    /*
     * Set the regular expression that is used to highlight types from
     * the beginning of the on-line function definition.  This
     * variable should be overwritten by each state that extends the
     * `c' state.
     */
    type_re = c_type_re;
  }

  /* Comments. */
  /\/\*/ {
    comment_face (true);
    language_print ($0);
    call (c_comment);
    comment_face (false);
  }
  /\/\// {
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

  /* Pre-processor lines. */
  /^#/ {
    language_print ($0);
    call (c_ppline);
  }

  /* Character constants. */
  /'.'|'\\\\.'/ {
    string_face (true);
    language_print ($0);
    string_face (false);
  }

  /* Keywords, but not types, goto, or case.
     (build-re '(break continue default do else for if return sizeof
     switch while))
  */
  /\b(break|continue|d(efault|o)|else|for|if|return|s(izeof|witch)|while)\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* Types. */
  c_type_re {
    type_face (true);
    language_print ($0);
    type_face (false);
  }

  /* Labels.  Emacs accepts also bare numbers. */
  /^([ \t]*)([a-zA-Z0-9_]+)(:)/ {
    language_print ($1);

    reference_face (true);
    language_print ($2);
    reference_face (false);

    language_print ($3);
  }

  /* Goto, case and the target. */
  /\<(goto|case)\>([ \t]+)(-?[A-Za-z_0-9]+)?/ {
    keyword_face (true);
    language_print ($1);
    keyword_face (false);

    language_print ($2);

    if (length ($3) > 0)
      {
	reference_face (true);
	language_print ($3);
	reference_face (false);
      }
  }

  /*
   * Function definitions, but only if you code with the one and only
   * usable indentation style (GNU).
   */
  /^([a-zA-Z_][a-zA-Z_0-9]*)([ \t]*\()/ {
    function_name_face (true);
    language_print ($1);
    function_name_face (false);

    language_print ($2);
  }

  /* Function definitions and prototypes for other (loser) coding styles. */
  /^([A-Za-z][a-zA-Z0-9_\* ]+)([ \*])([a-zA-Z_][a-zA-Z_0-9]*)([ \t]*\()/ {
    garbage = $1;
    middle_garbage = $2;
    function_name = $3;
    tail_garbage = $4;

    highlight_types (garbage, type_re);

    language_print (middle_garbage);

    function_name_face (true);
    language_print (function_name);
    function_name_face (false);

    language_print (tail_garbage);
  }
}


/*
Local variables:
mode: c
End:
*/
