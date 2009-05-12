/**
 * Name: zsh
 * Description: Z-shell programming language.
 * Author: Jean-Marc Calvez <jean-marc.calvez@st.com>
 */

state zsh extends HighlightEntry
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

  /* Keywords:
     (build-re '(: noglob nocorrect exec command if then elif else fi for in do
     done while until repeat case esac select function time foreach end coproc
     return alias autoload bg bindkey break builtin bye cd chdir compctl
     continue declare dirs disable disown echo echotc emulate enabke eval exec
     exit export false fc fg functions getln getopts hash history integer jobs
     kill let limit local log logout popd print pushd pushln pwd read readonly
     rehash return sched set setopt shift source suspend test times trap
     ttyctl type typeset ulimit umask unalias unfunction unhash unlimit unset
     unsetopt vared wait whence where which))
  */
  /\b(:|a(lias|utoload)|b(g|indkey|reak|uiltin|ye)\
|c(ase|d|hdir|o(m(mand|pctl)|ntinue|proc))\
|d(eclare|i(rs|s(able|own))|o(|ne))\
|e(cho(|tc)|l(if|se)|mulate|n(abke|d)|sac|val|x(ec()|it|port))\
|f(alse|c|g|i|or(|each)|unction(|s))|get(ln|opts)|h(ash|istory)\
|i(f|n(|teger))|jobs|kill|l(et|imit|o(cal|g(|out)))|no(correct|glob)\
|p(opd|rint|ush(d|ln)|wd)|re(ad(|only)|hash|peat|turn())\
|s(ched|e(lect|t(|opt))|hift|ource|uspend)\
|t(est|hen|ime(|s)|rap|tyctl|ype(|set))\
|u(limit|mask|n(alias|function|hash|limit|set(|opt)|til))|vared\
|w(ait|h(e(nce|re)|i(ch|le))))\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* special functions (built-in)
     (build-re '(chpwd precmd periodic argv status signals cdpath fignore
     fpath histchars mailpath manpath path psvar prompt watch))
  */
  /\b(argv|c(dpath|hpwd)|f(ignore|path)|histchars|ma(ilpath|npath)\
|p(ath|eriodic|r(ecmd|ompt)|svar)|s(ignals|tatus)|watch)\b/ {
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
