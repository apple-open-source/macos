/**
 * Name: bash
 * Description: Bourne-Again shell programming language.
 * Author: Jean-Marc Calvez <jean-marc.calvez@st.com>
 */

state bash extends HighlightEntry
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
     (build-re '(! case do done elif else esac fi for function if in select
     then until while { } time))
  */
  /\b(!|case|do(|ne)|e(l(if|se)|sac)|f(i|or|unction)|i(f|n)|select\
|t(hen|ime)|until|while|{|})\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* shell built-in commands
     (build-re '(: source alias bg bind break builtin cd command continue
     declare typeset dirs disown echo enable eval exec exit export fc fg
     getopts hash help history jobs kill let local logout popd pushd pwd read
     readonly return set shift shopt suspend test times trap type ulimit umask
     unalias unset wait))
  */
  /\b(:|alias|b(g|ind|reak|uiltin)|c(d|o(mmand|ntinue))\
|d(eclare|i(rs|sown))|e(cho|nable|val|x(ec|it|port))|f(c|g)|getopts\
|h(ash|elp|istory)|jobs|kill|l(et|o(cal|gout))|p(opd|ushd|wd)\
|re(ad(|only)|turn)|s(et|h(ift|opt)|ource|uspend)\
|t(est|imes|rap|ype(|set))|u(limit|mask|n(alias|set))|wait)\b/ {
    builtin_face (true);
    language_print ($0);
    builtin_face (false);
  }

  /* shell variables (built-in)
     (build-re '(PPID PWD OLDPWD REPLY UID EUID BASH BASH_VERSION BASH_VERSINFO
     SHLVL RANDOM SECONDS LINENO HISTCMD DIRSTACK PIPESTATUS OPTARG OPTIND
     HOSTNAME HOSTTYPE OSTYPE MACHTYPE SHELLOPTS IFS PATH HOME CDPATH ENV MAIL
     MAILCHECK MAILPATH PS1 PS2 PS3 PS4 TIMEFORMAT HISTSIZE HISTFILE
     HISTFILESIZE OPTERR LANG LC_ALL LC_COLLATE LC_MESSAGES PROMPT_COMMAND
     IGNOREEOF TMOUT FCEDIT FIGNORE GLOBIGNORE INPUTRC HISTCONTROL HISTIGNORE
     histchars HOSTFILE auto_resume))
  */
  /\b(BASH(|_VERSI(NFO|ON))|CDPATH|DIRSTACK|E(NV|UID)|F(CEDIT|IGNORE)\
|GLOBIGNORE\
|H(IST(C(MD|ONTROL)|FILE(|SIZE)|IGNORE|SIZE)|O(ME|ST(FILE|NAME|TYPE)))\
|I(FS|GNOREEOF|NPUTRC)|L(ANG|C_(ALL|COLLATE|MESSAGES)|INENO)\
|MA(CHTYPE|IL(|CHECK|PATH))|O(LDPWD|PT(ARG|ERR|IND)|STYPE)\
|P(ATH|IPESTATUS|PID|ROMPT_COMMAND|S(1|2|3|4)|WD)|R(ANDOM|EPLY)\
|S(ECONDS|H(ELLOPTS|LVL))|T(IMEFORMAT|MOUT)|UID|auto_resume|histchars)\b/ {
    variable_name_face (true);
    language_print ($0);
    variable_name_face (false);
  }
}


/*
Local variables:
mode: c
End:
*/
