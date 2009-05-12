/**
 * Name: skill
 * Description: Cadence Design Systems lispy language.
 * Author: Jean-Marc Calvez <jean-marc.calvez@st.com>
 */

state skill extends HighlightEntry
{
  /* Line Comments (lispish). */
  /;/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* Block Comments (C-style) */
  /\/\*/ {
    comment_face (true);
    language_print ($0);
    call (c_comment);
    comment_face (false);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* Definitions. */
  /(\([ \t]*)(procedure|defmacro|mprocedure|nprocedure|defun)([ \t]+\(?)([!\$%&\*\/:<=>\?~_^a-zA-Z][!\$%&\*\/:<=>\?~_^a-zA-Z0-9.+\-]*)/ {
    /* Starting garbage. */
    language_print ($1);

    /* Keyword. */
    keyword_face (true);
    language_print ($2);
    keyword_face (false);

    /* Middle garbage. */
    language_print ($3);

    /* Function name. */
    function_name_face (true);
    language_print ($4);
    function_name_face (false);
  }

  /* Keywords. Taken from Skill Language in the Quick Ref.  + operators
     (build-re '(@rest @optional @key abs acos add1 alias alphalessp
     alphaNumCmp and append append1 apply arglist arrayp arrayref asin assoc
     assq atan atom band bcdp bitfield bitfield bnand bnor bnot bor boundp
     break breakpt buildString bxnor bxor caaar caadr caar cadr callInitProc
     car case caseq cdar cddr cdr changeWorkingDir clear clearExitProcs close
     compareTime compress concat cond cons cont continue copy
     copyDefstructDeep cos count createDir csh debugQuit debugStatus declare
     declareLambda declareNLambda defCapDepends defCapPrefixes defInitProc
     defmacro defprop defstruct defstructp defun defUserInitProc defvar
     deleteDir deleteFile difference drain dtpr dump ed edi edit edl encrypt
     eq equal err error errset errsetstring eval evalstring evenp exists exit
     exp expandMacro expt fileLength fileSeek fileTell fix fixp float floatp
     for forall foreach fprintf fscanf gc gcsummary gensym get get_pname
     get_string getAllLoadedFiles getc getchar getCurrentTime getd getDirFiles
     getFnWriteProtect getFunctions getInstallPath getq getqq gets
     getShellEnvVar getSkillPath getVarWriteProtect getVersion getWarn
     getWorkingDir go geqp getq getqq greaterp if ilToolBox index infile
     installDebugger instring isCallable isDir isExecutable isFile
     isFileEncrypted isFileName isLink isReadable isWritable lambda last lconc
     leftshift length leqp lessp let lineread linereadstring list
     listFunctions listp listVariables load loadContext loadi loadstring log
     lowerCase makeTable makeTempFileName map mapc mapcan mapcar maplist max
     measureTime member memq min minus minusp mod mprocedure nconc ncons
     needCells neq nequal newline next nil nindex nlambda nprocedure nth
     nthcdr nthelem null numberp numOpenFiles oddp onep or otherp outfile
     parseString plist plus plusp portp postdecrement postincrement pp pprint
     predecrement preincrement prependInstallPath print printf printFunctions
     printlev println printstruct printVariables procedure profile
     profileReset profileSummary prog prog1 prog2 progn putd putprop putpropq
     putpropqq quote quotient random range readTable regExitAfter
     regExitBefore remd remdq remExitProc remove remprop remq return reverse
     rexCompile rexExecute rexMagic rexMatchAssocList rexMatchList rexMatchp
     rexReplace rexSubstitute rightshift rindex round rplaca rplacd
     saveContext set setarray setContext setFnWriteProtect setof setplist setq
     setqbitfield steqbitfield1 setShellEnvVar setSkillPath setVarWriteProtect
     sh shell simplifyFilename sin skDisableMessage skIgnoreMessage
     skillDebugger sklint skUnignoreMessage sort sortcar sprintf sqrt srandom
     sstatus stacktrace status step stepout strcat strcmp stringp
     stringToFunction strlen strncat strncmp sub1 subst substring sxtd symbolp
     symeval symstrp tablep tableToList tailp tan tconc terpri times tracef
     tracep tracev type typep unalias unbreakpt uncount uninstallDebugger
     unless unprofile untrace untracep untracev upperCase warn when where
     whereis while writeTable xcons zerop zxtd))
   */
  /\b(@(key|optional|rest)\
|a(bs|cos|dd1|l(ias|pha(NumCmp|lessp))|nd|pp(end(|1)|ly)\
|r(glist|ray(p|ref))|s(in|s(oc|q))|t(an|om))\
|b(and|cdp|itfield()|n(and|o(r|t))|o(r|undp)|reak(|pt)|uildString\
|x(nor|or))\
|c(a(a(ar|dr|r)|dr|llInitProc|r|se(|q))|d(ar|dr|r)|hangeWorkingDir\
|l(ear(|ExitProcs)|ose)\
|o(mp(areTime|ress)|n(cat|d|s|t(|inue))|py(|DefstructDeep)|s|unt)\
|reateDir|sh)\
|d(e(bug(Quit|Status)|clare(|Lambda|NLambda)\
|f(Cap(Depends|Prefixes)|InitProc|UserInitProc|macro|prop|struct(|p)|un\
|var)\
|lete(Dir|File))\
|ifference|rain|tpr|ump)\
|e(d(|i(|t)|l)|ncrypt|q(|ual)|rr(|or|set(|string))|v(al(|string)|enp)\
|x(i(sts|t)|p(|andMacro|t)))\
|f(i(le(Length|Seek|Tell)|x(|p))|loat(|p)|or(|all|each)|printf|scanf)\
|g(c(|summary)\
|e(nsym|qp\
|t(|AllLoadedFiles|CurrentTime|DirFiles|F(nWriteProtect|unctions)\
|InstallPath|S(hellEnvVar|killPath)|V(arWriteProtect|ersion)\
|W(arn|orkingDir)|_(pname|string)|c(|har)|d|q(|q())|s))\
|o|reaterp)\
|i(f|lToolBox|n(dex|file|st(allDebugger|ring))\
|s(Callable|Dir|Executable|File(|Encrypted|Name)|Link|Readable|Writable))\
|l(a(mbda|st)|conc|e(ftshift|ngth|qp|ssp|t)\
|i(neread(|string)|st(|Functions|Variables|p))\
|o(ad(|Context|i|string)|g|werCase))\
|m(a(keT(able|empFileName)|p(|c(|a(n|r))|list)|x)|e(asureTime|m(ber|q))\
|in(|us(|p))|od|procedure)\
|n(con(c|s)|e(edCells|q(|ual)|wline|xt)|i(l|ndex)|lambda|procedure\
|th(|cdr|elem)|u(ll|m(OpenFiles|berp)))\
|o(ddp|nep|r|therp|utfile)\
|p(arseString|l(ist|us(|p))|o(rtp|st(decrement|increment))|p(|rint)\
|r(e(decrement|increment|pendInstallPath)\
|int(|Functions|Variables|f|l(ev|n)|struct)\
|o(cedure|file(|Reset|Summary)|g(|1|2|n)))\
|ut(d|prop(|q(|q))))\
|quot(e|ient)\
|r(an(dom|ge)\
|e(adTable|gExit(After|Before)|m(ExitProc|d(|q)|ove|prop|q)|turn|verse\
|x(Compile|Execute|Ma(gic|tch(AssocList|List|p))|Replace|Substitute))\
|i(ghtshift|ndex)|ound|plac(a|d))\
|s(aveContext\
|et(|Context|FnWriteProtect|S(hellEnvVar|killPath)|VarWriteProtect|array\
|of|plist|q(|bitfield))\
|h(|ell)|i(mplifyFilename|n)\
|k(DisableMessage|IgnoreMessage|UnignoreMessage|illDebugger|lint)\
|ort(|car)|printf|qrt|random|status\
|t(a(cktrace|tus)|e(p(|out)|qbitfield1)\
|r(c(at|mp)|ing(ToFunction|p)|len|nc(at|mp)))\
|ub(1|st(|ring))|xtd|ym(bolp|eval|strp))\
|t(a(ble(ToList|p)|ilp|n)|conc|erpri|imes|race(f|p|v)|ype(|p))\
|u(n(alias|breakpt|count|installDebugger|less|profile|trace(|p|v))\
|pperCase)\
|w(arn|h(e(n|re(|is))|ile)|riteTable)|xcons|z(erop|xtd))\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* Skill functions. Use prefix to match, they are too many to
     enumerate. Used bold-italic... */
  /(cdf|dag|db|de|df|dl|dm|enter|fm|ge|hi|mif|tc|tfc|sch)[A-Z][^ \t]*/ {
    bold_italic (true);
    language_print ($0);
    bold_italic (false);
  }
}


/*
Local variables:
mode: c
End:
*/
