/**
 * Name: tcsh
 * Description: TC-Shell script language
 * Author: Jean-Marc Calvez <jean-marc.calvez@st.com>
 */

state tcsh extends HighlightEntry
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
     (build-re '(% : @ alias alloc bg bindkey break breaksw builtins bye case
     case cd chdir complete continue default dirs echo echotc else end endif
     endsw eval exec exit fg filetest foreach getspath getxvers glob goto
     hashstat history hup if inlib jobs kill limit log login logout ls-F
     migrate newgrp nice nohup notify onintr popd printenv pushd rehash repeat
     rootnode sched set setenv setpath setspath settc setty setxvers shift
     source stop suspend switch telltc time umask unalias uncomplete unhash
     universe unlimit unset unsetenv ver wait warp watchlog where which while))
  */
  /\b(%|:|@|al(ias|loc)|b(g|indkey|reak(|sw)|uiltins|ye)\
|c(ase()|d|hdir|o(mplete|ntinue))|d(efault|irs)\
|e(cho(|tc)|lse|nd(|if|sw)|val|x(ec|it))|f(g|iletest|oreach)\
|g(et(spath|xvers)|lob|oto)|h(ashstat|istory|up)|i(f|nlib)|jobs|kill\
|l(imit|og(|in|out)|s-F)|migrate|n(ewgrp|ice|o(hup|tify))|onintr\
|p(opd|rintenv|ushd)|r(e(hash|peat)|ootnode)\
|s(ched|et(|env|path|spath|t(c|y)|xvers)|hift|ource|top|uspend|witch)\
|t(elltc|ime)|u(mask|n(alias|complete|hash|iverse|limit|set(|env)))\
|ver|w(a(it|rp|tchlog)|h(ere|i(ch|le))))\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* Special aliases:
     (build-re '(beepcmd cwdcmd periodic precmd shell))

     Predefined variables:
     (build-re '(addsuffix afsuser ampm argv autocorrect autoexpand autolist
     autologout backslash_quote cdpath correct cwd dextract dirsfile dirstack
     dunique echo echo_style edit ellipsis fignore filec gid histchars histdup
     histfile histlist history home ignoreeof inputmode listjobs listlinks
     listmax listmaxrows loginsh mail matchbeep nobeep noclobber noglob
     nokanji nonomatch nostat notify oid owd path printexitvalue prompt
     prompt2 prompt3 pushdtohome pushdsilent recexact
     recognize_only_executables rmstar savedirs savehist sched shell shlvl
     showdots status symlinks tcsh term tperiod tty uid user verbose version
     visiblebell watch whow wordchars)) */
  /\b(a(ddsuffix|fsuser|mpm|rgv|uto(correct|expand|l(ist|ogout)))\
|backslash_quote|c(dpath|orrect|wd)|d(extract|irs(file|tack)|unique)\
|e(cho(|_style)|dit|llipsis)|fi(gnore|lec)|gid\
|h(ist(chars|dup|file|list|ory)|ome)|i(gnoreeof|nputmode)\
|l(ist(jobs|links|max(|rows))|oginsh)|ma(il|tchbeep)\
|no(beep|clobber|glob|kanji|nomatch|stat|tify)|o(id|wd)\
|p(ath|r(intexitvalue|ompt(|2|3))|ushd(silent|tohome))\
|r(ec(exact|ognize_only_executables)|mstar)\
|s(ave(dirs|hist)|ched|h(ell|lvl|owdots)|tatus|ymlinks)\
|t(csh|erm|period|ty)|u(id|ser)|v(er(bose|sion)|isiblebell)\
|w(atch|how|ordchars))\b/ {
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
