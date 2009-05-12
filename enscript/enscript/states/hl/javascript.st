/**
 * Name: javascript
 * Description: JavaScript language.
 * Author: Markku Rossi <mtr@iki.fi>
 */

from_vrml = 0;

state javascript_string extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /[\']/ {
    language_print ($0);
    return;
  }
}

state javascript_internal extends Highlight
{
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
    if (from_vrml)
      {
	reference_face (true);
	language_print ($0);
	reference_face (false);
	return;
      }
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* '' strings. */
  /[\']/ {
    string_face (true);
    language_print ($0);
    call (javascript_string);
    string_face (false);
  }

  /* Function definitions. */
  /\b(function)([ \t]+)([A-Za-z\$_][A-Za-z\$_0-9]*)([ \t]*\()/ {
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
     (build-re '(
     abstract boolean break byte case catch char class const continue
     default do double else extends false final finally float for function
     goto if implements import in instanceof int interface long native new
     null package private protected public return short static super switch
     synchronized this throw throws transient true try var void while with
     ))
   */
  /\b(abstract|b(oolean|reak|yte)|c(a(se|tch)|har|lass|on(st|tinue))\
|d(efault|o(|uble))|e(lse|xtends)|f(alse|inal(|ly)|loat|or|unction)\
|goto|i(f|mp(lements|ort)|n(|stanceof|t(|erface)))|long\
|n(ative|ew|ull)|p(ackage|r(ivate|otected)|ublic)|return\
|s(hort|tatic|uper|witch|ynchronized)|t(h(is|row(|s))|r(ansient|ue|y))\
|v(ar|oid)|w(hile|ith))\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* Built-in objects.
     (build-re '(Math Date eval parseInt parseFloat))
   */
  /\b(Date|Math|eval|parse(Float|Int))\b/ {
    builtin_face (true);
    language_print ($0);
    builtin_face (false);
  }

  /* Terminator for nested JavaScript programs. */
  /<\/[sS][cC][rR][iI][pP][tT]>/ {
    from_html_terminator = $0;
    return;
  }
}

state javascript extends HighlightEntry
{
  BEGIN {
    call (javascript_internal);
    return;
  }
}


/*
Local variables:
mode: c
End:
*/
