/**
 * Name: perl
 * Description: Perl programming language.
 *
 * Author: Jim Villani, Logistics Management Institute (jvillani@lmi.org)
 *         perl_pod mods by Gregor Purdy (gregor@focusresearch.com)
 */

state perl_pod extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /^=cut\s*$/ {
    language_print ($0);
    return;
  }
}

state perl_comment extends Highlight
{
  /\*\\\// {
    language_print ($0);
    return;
  }
}

state perl_dquot_string extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /\"/ {
    language_print ($0);
    return;
  }
}

state perl_quot_string extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /[\']/ {
    language_print ($0);
    return;
  }
}

state perl_bquot_string extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /`/ {
    language_print ($0);
    return;
  }
}

state perl extends HighlightEntry
{
  /* Comments. */
  /#.*$/ {
    comment_face (true);
    language_print ($0);
    comment_face (false);
  }

  /* Ignore escaped quote marks */
  /\\\"/ {
    language_print ($0);
  }
  /\\\'/ {
    language_print ($0);
  }
  /\\\`/ {
    language_print ($0);
  }

  /* stuff after a -> is a method,
   * don't bold just because it looks like a keyword
   */
  /->\w+/ {
    language_print ($0);
  }

  /* stuff between a - and a => is a named parameter,
   * don't bold just because it looks like a keyword
   */
  /-\w+=>/ {
    language_print ($0);
  }

  /* In cgi files, JavaScript might be embedded, so we need to look out
   * for the JavaScript comments, because they might contain something
   * we don't like, like a contraction (don't, won't, etc.)
   * We won't put them in comment face, because they are not perl
   * comments.
   */
  /\/\// {
    language_print ($0);
    call (eat_one_line);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (perl_dquot_string);
    string_face (false);
  }
  /[\']/ {
    string_face (true);
    language_print ($0);
    call (perl_quot_string);
    string_face (false);
  }

  /* Backquoted command string */
  /`/ {
    string_face (true);
    language_print ($0);
    call (perl_bquot_string);
    string_face (false);
  }

  /* Variables */
  /[$%@&]+\w+/ {
    keyword_face (false);
    language_print ($0);
  }

  /* Keywords. From perl distribution's toke.c
     abs accept alarm and atan2 bind binmode bless caller chdir chmod
     chomp chop chown chr chroot close closedir cmp connect continue cos
     crypt dbmclose dbmopen defined delete die do dump each else elsif
     endgrent endhostent endnetent endprotoent endpwent endservent eof
     eq eval exec exists exit exp fcntl fileno flock for foreach fork
     format formline ge getc getgrent getgrgid getgrnam gethostbyaddr
     gethostbyname gethostent getlogin getnetbyaddr getnetbyname
     getnetent getpeername getpgrp getppid getpriority getprotobyname
     getprotobynumber getprotoent getpwent getpwnam getpwuid getservbyname
     getservbyport getservent getsockname getsockopt glob gmtime goto
     grep gt hex if index int ioctl join keys kill last lc lcfirst le
     length link listen local localtime log lstat lt m map mkdir msgctl
     msgget msgrcv msgsnd my ne new next no not oct open opendir or ord
     pack package pipe pop pos print printf prototype push q qq quotemeta
     qw qx rand read readdir readline readlink readpipe recv redo ref
     rename require reset return reverse rewinddir rindex rmdir s scalar
     seek seekdir select semctl semget semop send setgrent sethostent
     setnetent setpgrp setpriority setprotoent setpwent setservent
     setsockopt shift shmctl shmget shmread shmwrite shutdown sin sleep
     socket socketpair sort splice split sprintf sqrt srand stat study
     sub substr symlink syscall sysopen sysread sysseek system syswrite
     tell telldir tie tied time times tr truncate uc ucfirst umask undef
     unless unlink unpack unshift untie until use utime values vec wait
     waitpid wantarray warn while write x xor y
   */
  /\b(a(bs|ccept|larm|nd|tan2)|b(in(d|mode)|less)|c(aller|h(dir|mod\
|o(mp|p|wn)|r(|oot))|lose(|dir)|mp|o(n(nect|tinue)|s)|rypt)\
|d(bm(close|open)|e(fined|lete)|ie|o|ump)|e(ach|ls(e|if)|nd(gr|host|net|proto\
|pw|serv)ent|of|q|val|x(ec|i(sts|t)|p))|f(cntl|ileno|lock|or(|each|k\
|m(at|line)))|g(e(|t(c|gr(ent|gid|nam)|host(by(addr|name)|ent)|login\
|net(by(addr|name)|ent)|p(eername|grp|pid|riority|roto(by(addr|name|number)\
|ent)|w(ent|nam|uid))|s(erv(by(name|port)|ent)|ock(name|opt))))|lob|mtime\
|oto|rep|t)|hex|i(f|n(t|dex)|octl)|join|k(eys|ill)|l(ast|c(|first)|e(|ngth)\
|i(nk|sten)|o(cal(|time)|g)|stat|t)|m|m(ap|kdir|sg(ctl|get|rcv|snd)|y)\
|n(e(|w|xt)|o(|t))|o(ct|pen(|dir)|r(|d))|p(ack(|age)|ipe|o(p|s)|r(int(|f)\
|ototype)|ush)|q(|q|uotemeta|w|x)|r(and|e(a(d(|dir|lin(e|k)|pipe))|cv\
|do|f|name|quire|set|turn|verse|winddir)|index|mdir)|s(|calar|e(e(k|kdir)\
|lect|m(ctl|get|op)|nd|t((gr|host|net)ent|p(grp|r(iority|otoent)|went)\
|s(ervent|ockopt)))|h(ift|m(ctl|get|read|write)|utdown)|in|leep|o(cke(t|tpair)\
|rt)|p(li(ce|t)|rintf)|qrt|rand|t(at|udy)|u(b|bstr)|y(mlink|s(call|open|read\
|s(eek|tem)|write)))|t(ell(|dir)|i(e|ed|m(e|es))|r(|uncate))|u(c(|first)\
|mask|n(def|l(ess|ink)|pack|shift|ti(e|l))|se|time)|v(alues|ec)|w(a(i(t(|pid))\
|ntarray|rn)|hile|rite)|x(|or)|y)\b/ {

    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* POD. */
  /^=(pod|head1|head2|item|over|back|for|begin|end)\b.*$/ {
    language_print ($0);
    call (perl_pod);
  }
}


/*
Local variables:
mode: c
End:
*/
