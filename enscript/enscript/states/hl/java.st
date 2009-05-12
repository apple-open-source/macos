/**
 * Name: java
 * Description: Java programming language.
 * Author: Paul Fisher <pnfisher@eos.ncsu.edu>
 */

state java extends HighlightEntry
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
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* Character constants. */
  /'.'|'\\\\.'/ {
    string_face (true);
    language_print ($0);
    string_face (false);
  }

  /* Keywords.
     (build-re '(abstract boolean break byte case catch char class
     const continue default do double else extends false final finally
     float for goto if implements import instanceof int interface long
     native new null package private protected public return short static
     super switch synchronized this throw throws transient true try void
     volatile while))
  */
  /\b(abstract|b(oolean|reak|yte)|c(a(se|tch)|har|lass|on(st|tinue))\
|d(efault|o(|uble))|e(lse|xtends)|f(alse|inal(|ly)|loat|or)|goto\
|i(f|mp(lements|ort)|n(stanceof|t(|erface)))|long|n(ative|ew|ull)\
|p(ackage|r(ivate|otected)|ublic)|return\
|s(hort|tatic|uper|witch|ynchronized)|t(h(is|row(|s))|r(ansient|ue|y))\
|vo(id|latile)|while)\b/ {
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
