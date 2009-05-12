/**
 * Name: sh
 * Description: Bourne shell programming language.
 * Author: Juergen Kahrs <Juergen.Kahrs@t-online.de>
 */

state sh extends HighlightEntry
{
  /* Comments. */
  /#/ {
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

  /* Excutable script. */
  /^#!/ {
    reference_face (true);
    language_print ($0);
    call (eat_one_line);
    reference_face (false);
  }

  /* Keywords. */
  /\b(CDPATH|HOME|IFS|MAIL((CHECK)?|(PATH)?)|P(ATH|S(1|2))|SH(ACCT|ELL)|\
break|c(ase|d|ontinue)|do(ne)?|e(cho|lse|sac|val|x(ec|it|port))|f(i|or)|\
getopts|hash|i[fn]|limit|newgrp|pwd|re(ad(only)?|turn)|s(et|hift)|\
t(est|hen|imes|rap|ype)|u(limit|mask|n(limit|set))|w(ait|hile))\b/ {
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
