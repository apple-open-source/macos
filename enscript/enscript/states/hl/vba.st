/**
 * Name: vba
 * Description: Visual Basic (for Applications)
 * Author: Kevin Grover <grover@wizard.com>
 */

/* To Do
 * - ?Get parser to work for X.X.X.X and not hilight separate Xs
 * - ?recognize type declaration characters ($,%,@, etc.. on variables)
 * - ?Look for line numbers/labels (for Goto's)  /^\s*[0-9A-Za-z]+:/
 */

state vba extends HighlightEntry
{
  /* Comments. */
  /[\']|^\s*Rem/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* String constants. */
  /\".*\"/ {
    string_face (true);
    language_print ($0);
    string_face (false);
  }

  /* Keywords. From:
	       Excel for Windows 95 Power Programming with VGA
	       2nd Ed, John Walkenbach, pg 160

     (build-re '(
     Abs And Any As Boolean ByRef ByVal Call Case CBool CCur CDate
     CDbl CDecl CInt Circle CLng Close Const CSng CStr CurDirrase
     Currency CVar CVDate CVErr Date Debug Declare

     DefBool DefCur DefDate DefDbl DefInt DefLng DefObj DefSng DefStr
     DefVar Dim Dir Do Double Each Else ElsIf Empty End EndIf Eqv Erase
     Error Exit False Fix For Format FreeFile

     Function Get Global GoSub GoTo If Imp In Input InputB Instr InstrB Int
     Integer Is LBound Len LenB Let Like Line Load Local Lock Long Loop
     LSet Me Mid

     MidB Mod Name New Next Not Nothing Null Object On Open Option Optional
     Or Point Preserve Print Private Property PSet Public Put ReDim Rem
     Resume Return RSet Scale Seek

     Select Set Sgn Shared Single Spc Static Stop StrComp String Sub Tab
     Then TO True Type TypeOf UBound Unload Unlock Until Variant Wend While
     Width With Write Xor))
  */
  /\b(A(bs|n(d|y)|s)|B(oolean|y(Ref|Val))\
|C(Bool|Cur|D(ate|bl|ecl)|Int|Lng|S(ng|tr)|V(Date|Err|ar)|a(ll|se)|ircle\
|lose|onst|ur(Dirrase|rency))\
|D(ate|e(bug|clare|f(Bool|Cur|D(ate|bl)|Int|Lng|Obj|S(ng|tr)|Var))\
|i(m|r)|o(|uble))\
|E(ach|ls(If|e)|mpty|nd(|If)|qv|r(ase|ror)|xit)\
|F(alse|ix|or(|mat)|reeFile|unction)|G(et|lobal|o(Sub|To))\
|I(f|mp|n(|put(|B)|str(|B)|t(|eger))|s)\
|L(Bound|Set|e(n(|B)|t)|i(ke|ne)|o(ad|c(al|k)|ng|op))|M(e|id(|B)|od)\
|N(ame|e(w|xt)|ot(|hing)|ull)|O(bject|n|p(en|tion(|al))|r)\
|P(Set|oint|r(eserve|i(nt|vate)|operty)|u(blic|t))\
|R(Set|e(Dim|m|sume|turn))\
|S(cale|e(ek|lect|t)|gn|hared|ingle|pc|t(atic|op|r(Comp|ing))|ub)\
|T(O|ab|hen|rue|ype(|Of))|U(Bound|n(lo(ad|ck)|til))|Variant\
|W(end|hile|i(dth|th)|rite)|Xor)\b/ {
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
