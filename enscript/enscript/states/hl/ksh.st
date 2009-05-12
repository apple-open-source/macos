/**
 * Name: ksh
 * Description: Korn shell programming language.
 * Author: Jean-Marc Calvez <jean-marc.calvez@st.com>
 */

state ksh extends HighlightEntry
{
  /(\${*#[a-zA-Z0-9_]*})/ {
    language_print ($0);
  }

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

  /* Keywords:
     (build-re '(: alias bg break continue cd command eval exec exit export
     fc fg getopts hash jobs kill let login newgrp print pwd read readonly
     return set shift stop suspend test times trap type typeset ulimit umask
     unalias unset wait whence for in do done select case esac if then elif
     else fi while until function time))
  */
  /\b(:|alias|b(g|reak)|c(ase|d|o(mmand|ntinue))|do(|ne)\
|e(l(if|se)|sac|val|x(ec|it|port))|f(c|g|i|or|unction)|getopts|hash\
|i(f|n)|jobs|kill|l(et|ogin)|newgrp|p(rint|wd)|re(ad(|only)|turn)\
|s(e(lect|t)|hift|top|uspend)|t(est|hen|ime(|s)|rap|ype(|set))\
|u(limit|mask|n(alias|set|til))|w(ait|h(ence|ile)))\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* exported aliases (built-in)
     (build-re '(autoload false functions hash history integer nohup r true
     type))
  */
  /\b(autoload|f(alse|unctions)|h(ash|istory)|integer|nohup|r|t(rue|ype))\b/ {
    builtin_face (true);
    language_print ($0);
    builtin_face (false);
  }
}


/*
Local variables:
mode: c
End:
*/
