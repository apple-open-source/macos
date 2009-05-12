/**
 * Name: postscript
 * Description: PostScript programming language.
 *
 * Author: Dave Hylands (DHylands@creo.com)
 */

state ps_string extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /[\)]/ {
    language_print ($0);
    return;
  }
  /[\(]/ {
    /* Balanced ()'s in a string */
    language_print ($0);
    call (ps_string);
  }
}

state ps_encoded_string extends Highlight
{
  /[\>]/ {
    language_print ($0);
    return;
  }
}

state postscript extends HighlightEntry
{
  /* Comments. */
  /%/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* String constants. */
  /[\(]/ {
    string_face (true);
    language_print ($0);
    call (ps_string);
    string_face (false);
  }

  /[\<]/ {
    string_face (true);
    language_print ($0);
    call (ps_encoded_string);
    string_face (false);
  }

  /* Keywords.

     I built the keyword list using the following PostScript program

	/str 128 string def
	/outFile \(ps.txt)(w)file def
	/proc
	{
	    pop //str cvs outFile exch writestring outFile (\n) writestring
	} bind def

	systemdict /proc load forall
	statusdict /proc load forall
	errordict  /proc load forall

    I then ran:

	sort ps.txt | uniq | fmt > ps.key

    I then went through the list and removed << >> [ ] and obvious Harlequin
    extensions. Many of the keywords remaining are not documented in the Red
    Book but are implemented in some Adobe implementations (especially the
    ones from statusdict). I decided to leave them in.

    And since I don't have emacs (gasp), I wrote a program which takes the
    ps.key file and generates the regular expression. This was faster than
    trying to install emacs and figure out how it works.

    Also note that PostScript doesn't require whitespace in front of keywords.
    In particular, text can follow immediately after any of the following
    characters:

	> { } ) [ ] /

    and can be followed immediately by the following:

	< { } ( [ ] /

    in addition to white space.

    Contents of ps.key:

    $error .error 11x17tray 12x24tray 15x24tray = == =print =scratch =string
    FontDirectory GlobalFontDirectory ISOLatin1Encoding
    Run SharedFontDirectory SpecialRun StandardEncoding
    VMerror a2tray a3tray a4tray a5tray abs add aload
    anchorsearch and appletalktype arc arcn arct arcto array ashow astore
    atan authors awidthshow b5tray begin bind bitshift blackoverprint blink
    broadsheettray buildtime bytesavailable cachestatus ceiling cexec
    charpath checkpassword checkscreen clear cleardictstack clearinterrupt
    cleartomark clip clippath closefile closepath closescc colorimage concat
    concatmatrix configurationerror copy copypage cos count countdictstack
    countexecstack counttomark cshow currentblackgeneration
    currentcacheparams currentcmykcolor currentcolor currentcolorrendering
    currentcolorrenderingintent currentcolorscreen currentcolorspace
    currentcolortransfer currentdash currentdevparams currentdict
    currentdlhooks currentfile currentflat currentfont currentglobal
    currentgray currentgstate currenthalftone currenthalftonephase
    currenthsbcolor currentinterceptcolorspace currentlinecap
    currentlinejoin currentlinewidth currentmatrix currentmiterlimit
    currentobjectformat currentoverprint currentpacking currentpagedevice
    currentpoint currentrelativeneutrals currentreproductioncriteria
    currentrgbcolor currentscreen currentseparationcolornames
    currentseparationorder currentshared currentstrokeadjust
    currentsystemparams currenttransfer currenttrapintent
    currentundercolorremoval currentuserparams curveto cvi cvlit cvn cvr
    cvrs cvs cvx daytime def defaultblackoverprint
    defaulthandleerror defaultmatrix defaultmirrorprint defaultpagemargin
    defaultpageparams defaultprocesscolors defaulttimeouts definefont
    defineresource defineuserobject deletefile devdismount devforall
    devformat deviceinfo devmount devstatus dict dictfull dictstack
    dictstackoverflow dictstackunderflow disableinterrupt diskonline
    diskstatus div dlclearcaches dostartpage dosysstart dtransform dup echo
    eerom eescratch eexec enableinterrupt end endofjob eoclip eofill eq
    erasepage errorbeep errordict exch exec execdict execform execstack
    execstackoverflow execuserobject executeonly executive exit exp exposure
    false file filekind filelinenumber filename filenameforall fileposition
    fill filter findcharstrings findcolorrenderingintent findencoding
    findfont findpgfont findresource flattenpath floor flush flushcache
    flushfile for forall gcheck ge genericpaper get getinterval getknown
    getsccconfig gettopfile gettoprealfile globaldict glyphshow grestore
    grestoreall gsave gstate gt handleerror hardwareiomode
    identmatrix idiv idlefonts idtransform if ifelse
    image imagemask index ineofill infill initclip initgraphics initialized
    initializedisk initmatrix instroke internaldict interrupt
    interruptenabled inueofill inufill inustroke invalidaccess invalidexit
    invalidfileaccess invalidfont invalidrestore invertmatrix ioerror
    ismanualfeed itransform jobname jobsource jobstate jobstring jobtimeout
    known kshow languagelevel lastmode le ledgertray legaltray length
    lettertray limitcheck lineto listfilesinestack ln load
    loadcurrentpagedevice loadsetpagedevice log loop lt makefont makepattern
    manualfeedtimeout margins mark matchtemplate matrix maxlength
    medialength mediasize mediawidth mirrorprint mod moveto mul ne neg
    negativeprint newpath noaccess nocurrentpoint not null nulldevice
    openscc or packedarray pagecount pagemargin pageparams pagestackorder
    pagetype pathbbox pathforall pop print printererror printermessage
    printername printerstatus printerupset printobject processcolors product
    prompt pstack put putinterval quit ramsize rand rangecheck rcheck
    rcurveto read readhexstring readline readonly readstring realtime
    rectclip rectfill rectstroke renamefile repeat resetfile resetprinter
    resolution resourceforall resourcestatus restore reversepath revision
    rlineto rmoveto roll rootfont rotate round rrand run save scale
    scalefont sccbatch sccfiles sccinteractive scheck screenforall search
    selectfont sendctrld sendpcmd sendprinterstate serialnumber serverdict
    setaccuratescreens setbbox setblackgeneration setblackoverprint setblink
    setcachedevice setcachedevice2 setcachelimit setcacheparams setcharwidth
    setcmykcolor setcolor setcolorrendering setcolorrenderingintent
    setcolorscreen setcolorspace setcolortransfer setdash
    setdefaultblackoverprint setdefaultmirrorprint setdefaultpagemargin
    setdefaultpageparams setdefaultprocesscolors setdefaulttimeouts
    setdevparams setdlhooks setdostartpage setdosysstart seteescratch
    setexposure setfileposition setflat setfont setglobal setgray setgstate
    sethalftone sethalftonephase sethardwareiomode sethsbcolor setidlefonts
    setinterceptcolorspace setjobtimeout setlinecap setlinejoin setlinewidth
    setmargins setmatrix setmirrorprint setmiterlimit setnegativeprint
    setobjectformat setoverprint setpacking setpage setpagedevice
    setpagemargin setpageparams setpageseen setpagestackorder setpagetype
    setpassword setpattern setprintername setprocesscolors
    setrelativeneutrals setreproductioncriteria setresolution setrgbcolor
    setsccbatch setsccconfig setsccinteractive setscreen setshared
    setsoftwareiomode setstderr setstdio setstrokeadjust setsystemparams
    settransfer settrapintent setucacheparams setundercolorremoval
    setuserdiskpercent setuserparams setvmthreshold shareddict show showpage
    sin softwareiomode sqrt srand stack stackoverflow stackunderflow start
    startjob startpage status statuscommand statusdict stop stopped store
    string stringwidth stroke strokepath sub superstop superstopped
    switchsetting syntaxerror system systemdict tabloidtray timeout token
    transform translate true truncate type typecheck uappend ucache
    ucachestatus ueofill ufill undef undefined undefinedfilename
    undefinedresource undefinedresult undefinefont undefineresource
    undefineuserobject unmatchedmark unregistered upath userdict
    userdiskpercent usertime ustroke ustrokepath validatefont version
    vmreclaim vmstatus waittimeout wcheck where widthshow write
    writehexstring writeobject writestring xcheck xor xshow xyshow yshow
  */
  /(\b|[\>\{\}\)\[\]\/])\
(\$error|\.error|1(1x17tray|2x24tray|5x24tray)|=(|=|print|s(cratch|tring))\
|FontDirectory|GlobalFontDirectory|ISOLatin1Encoding|Run|S\
(haredFontDirectory|pecialRun|tandardEncoding)|VMerror|a(2tray|3tray\
|4tray|5tray|bs|dd|load|n(chorsearch|d)|ppletalktype|r(c(|n|t(|o))|ray)|s\
(how|tore)|tan|uthors|widthshow)|b(5tray|egin|i(nd|tshift)|l\
(ackoverprint|ink)|roadsheettray|uildtime|ytesavailable)|c(achestatus|e\
(iling|xec)|h(arpath|eck(password|screen))|l(ear(|dictstack|interrupt\
|tomark)|ip(|path)|ose(file|path|scc))|o(lorimage|n(cat(|matrix)\
|figurationerror)|py(|page)|s|unt(|dictstack|execstack|tomark))|show|ur\
(rent(blackgeneration|c(acheparams|mykcolor|olor(|rendering(|intent)|s\
(creen|pace)|transfer))|d(ash|evparams|ict|lhooks)|f(ile|lat|ont)|g(lobal\
|ray|state)|h(alftone(|phase)|sbcolor)|interceptcolorspace|line(cap|join\
|width)|m(atrix|iterlimit)|o(bjectformat|verprint)|p(a(cking|gedevice)\
|oint)|r(e(lativeneutrals|productioncriteria)|gbcolor)|s(creen|eparation\
(colornames|order)|hared|trokeadjust|ystemparams)|tra(nsfer|pintent)|u\
(ndercolorremoval|serparams))|veto)|v(i|lit|n|r(|s)|s|x))|d(aytime|e(f(\
|ault(blackoverprint|handleerror|m(atrix|irrorprint)|p(age(margin\
|params)|rocesscolors)|timeouts)|ine(font|resource|userobject))|letefile|v\
(dismount|for(all|mat)|iceinfo|mount|status))|i(ct(|full|stack(|overflow\
|underflow))|s(ableinterrupt|k(online|status))|v)|lclearcaches|os(tartpage\
|ysstart)|transform|up)|e(cho|e(rom|scratch|xec)|n(ableinterrupt|d(\
|ofjob))|o(clip|fill)|q|r(asepage|ror(beep|dict))|x(ch|ec(|dict|form|stack\
(|overflow)|u(serobject|t(eonly|ive)))|it|p(|osure)))|f(alse|i(l(e(|kind\
|linenumber|name(|forall)|position)|l|ter)|nd(c(harstrings\
|olorrenderingintent)|encoding|font|pgfont|resource))|l(attenpath|oor\
|ush(|cache|file))|or(|all))|g(check|e(|nericpaper|t(|interval|known\
|sccconfig|top(file|realfile)))|l(obaldict|yphshow)|restore(|all)|s(ave\
|tate)|t)|ha(ndleerror|rdwareiomode)|i(d(entmatrix|iv|lefonts|transform)\
|f(|else)|mage(|mask)|n(dex|eofill|fill|it(clip|graphics|ialized(|isk)\
|matrix)|stroke|ter(naldict|rupt(|enabled))|u(eofill|fill|stroke)|v(alid\
(access|exit|f(ileaccess|ont)|restore)|ertmatrix))|oerror|smanualfeed\
|transform)|job(name|s(ource|t(ate|ring))|timeout)|k(nown|show)|l(a\
(nguagelevel|stmode)|e(|dgertray|galtray|ngth|ttertray)|i(mitcheck|neto\
|stfilesinestack)|n|o(ad(|currentpagedevice|setpagedevice)|g|op)|t)|m(a\
(ke(font|pattern)|nualfeedtimeout|r(gins|k)|t(chtemplate|rix)|xlength)\
|edia(length|size|width)|irrorprint|o(d|veto)|ul)|n(e(|g(|ativeprint)\
|wpath)|o(access|currentpoint|t)|ull(|device))|o(penscc|r)|p(a(ckedarray\
|ge(count|margin|params|stackorder|type)|th(bbox|forall))|op|r(int(|er\
(error|message|name|status|upset)|object)|o(cesscolors|duct|mpt))|stack\
|ut(|interval))|quit|r(a(msize|n(d|gecheck))|c(heck|urveto)|e(a(d(\
|hexstring|line|only|string)|ltime)|ct(clip|fill|stroke)|namefile|peat|s\
(et(file|printer)|o(lution|urce(forall|status))|tore)|v(ersepath|ision))\
|lineto|moveto|o(ll|otfont|tate|und)|rand|un)|s(ave|c(ale(|font)|c(batch\
|files|interactive)|heck|reenforall)|e(arch|lectfont|nd(ctrld|p(cmd\
|rinterstate))|r(ialnumber|verdict)|t(accuratescreens|b(box|l(ack\
(generation|overprint)|ink))|c(ache(device(|2)|limit|params)|harwidth\
|mykcolor|olor(|rendering(|intent)|s(creen|pace)|transfer))|d(ash|e(fault\
(blackoverprint|mirrorprint|p(age(margin|params)|rocesscolors)|timeouts)\
|vparams)|lhooks|os(tartpage|ysstart))|e(escratch|xposure)|f(ileposition\
|lat|ont)|g(lobal|ray|state)|h(a(lftone(|phase)|rdwareiomode)|sbcolor)|i\
(dlefonts|nterceptcolorspace)|jobtimeout|line(cap|join|width)|m(a(rgins\
|trix)|i(rrorprint|terlimit))|negativeprint|o(bjectformat|verprint)|p(a\
(cking|ge(|device|margin|params|s(een|tackorder)|type)|ssword|ttern)|r\
(intername|ocesscolors))|r(e(lativeneutrals|productioncriteria|solution)\
|gbcolor)|s(c(c(batch|config|interactive)|reen)|hared|oftwareiomode|t(d\
(err|io)|rokeadjust)|ystemparams)|tra(nsfer|pintent)|u(cacheparams\
|ndercolorremoval|ser(diskpercent|params))|vmthreshold))|h(areddict|ow(\
|page))|in|oftwareiomode|qrt|rand|t(a(ck(|overflow|underflow)|rt(|job\
|page)|tus(|command|dict))|o(p(|ped)|re)|r(ing(|width)|oke(|path)))|u(b\
|perstop(|ped))|witchsetting|y(ntaxerror|stem(|dict)))|t(abloidtray|imeout\
|oken|r(ans(form|late)|u(e|ncate))|ype(|check))|u(append|cache(|status)\
|eofill|fill|n(def(|ine(d(|filename|res(ource|ult))|font|resource\
|userobject))|matchedmark|registered)|path|s(er(di(ct|skpercent)|time)\
|troke(|path)))|v(alidatefont|ersion|m(reclaim|status))|w(aittimeout|check\
|here|idthshow|rite(|hexstring|object|string))|x(check|or|show|yshow)\
|yshow)\
(\b|[\<\{\}\(\[\]\/])/ {
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
