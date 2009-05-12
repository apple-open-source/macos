/**
 * Name: csh
 * Description: C-Shell script language
 * Author: Jean-Marc Calvez <jean-marc.calvez@st.com>
 */

state csh extends HighlightEntry
{
  /* Comments. From sh description */
  /#/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* String constants. From sh */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* Excutable script. From sh */
  /^#!/ {
    reference_face (true);
    language_print ($0);
    call (eat_one_line);
    reference_face (false);
  }

  /* Keywords. :
     (build-re '(: alias bg break breaksw case cd chdir continue default dirs
     echo eval exec exit fg foreach end glob goto hashstat history if then
     else endif jobs kill limit login logout nice nohup notify onintr popd
     pushd rehash repeat set setenv shift source stop suspend switch case
     endsw time umask unalias unhash unlimit unset wait while % @))
  */
  /\b(%|:|@|alias|b(g|reak(|sw))|c(ase()|d|hdir|ontinue)|d(efault|irs)\
|e(cho|lse|nd(|if|sw)|val|x(ec|it))|f(g|oreach)|g(lob|oto)\
|h(ashstat|istory)|if|jobs|kill|l(imit|og(in|out))|n(ice|o(hup|tify))\
|onintr|p(opd|ushd)|re(hash|peat)\
|s(et(|env)|hift|ource|top|uspend|witch)|t(hen|ime)\
|u(mask|n(alias|hash|limit|set))|w(ait|hile))\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* Predefined variables:
     (build-re '(argv cdpath cwd echo fignore filec hardpaths histchars
     history home ignoreeof mail nobeep noclobber noglob nonomatch notify path
     prompt savehist shell status verbose)) */
  /\b(argv|c(dpath|wd)|echo|fi(gnore|lec)|h(ardpaths|ist(chars|ory)|ome)\
|ignoreeof|mail|no(beep|clobber|glob|nomatch|tify)|p(ath|rompt)\
|s(avehist|hell|tatus)|verbose)\b/ {
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
