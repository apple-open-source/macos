/**
 * Name: fortran_pp
 * Description: Fortran77 programming language with cpp-statements
 * Author: Keith Refson <Keith.Refson@earth.ox.ac.uk>
 *	   Markku Rossi <mtr@iki.fi>
 *	   Tobias Buchal <buch@ifh.bau-verm.uni-karlsruhe.de>
 */

state fortran_string extends Highlight
{
  /[\']/ {
    language_print ($0);
    return;
  }
}

state fortran_io extends Highlight
{
  /\)/ {
    language_print ($0);
    return;
  }



  /* IO Keywords.  (build-re '(FMT UNIT REC END ERR FILE STATUS
     ACCESS FORM RECL BLANK IOSTAT EXIST OPENED NUMBER NAME
     SEQUENTIAL DIRECT FORMATTED UNFORMATTED NEXTREC)) */
  /\BbACCESS|BLANK|DIRECT|E(ND|RR|XIST)|F(ILE|MT|ORM(|ATTED))|IOSTAT\
|N(AME|EXTREC|UMBER)|OPENED|REC(|L)|S(EQUENTIAL|TATUS)\
|UN(FORMATTED|IT))\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* IO Keywords.  (build-re '(fmt unit rec end err file status
     access form recl blank iostat exist opened number name
     sequential direct formatted unformatted nextrec)) */
  /\b(access|blank|direct|e(nd|rr|xist)|f(ile|mt|orm(|atted))|iostat\
|n(ame|extrec|umber)|opened|rec(|l)|s(equential|tatus)\
|un(formatted|it))\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* Strings in the io statement */
  /[\']/ {
    string_face (true);
    language_print ($0);
    call (fortran_string);
    string_face (false);
  }
}

state fortran_pp extends HighlightEntry
{
  /* Comments. */
  /(^[cC!\*]|!)/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* String constants. */
  /[\']/ {
    string_face (true);
    language_print ($0);
    call (fortran_string);
    string_face (false);
  }

  /* Pre-processor lines. */
  /^#/ {
    reference_face (true);
    language_print ($0);
    call (eat_one_line);
    reference_face (false);
  }

  /* Comparators.  We have to roll by hand because of the
     dots - "\b" doesn't delimit here. */
  /\.(and|eqv?|g(e|t)|l(e|t)|ne(qv)?|not|or)\./ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* Comparators.  We have to roll by hand because of the
     dots - "\b" doesn't delimit here. */
  /\.(AND|EQV?|G(E|T)|L(E|T)|NE(QV)?|NOT|OR)\./ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* IO Statement (build-re '(open close read
  write inquire backspace endfile rewind )) */
  /\b(backspace|close|endfile|inquire|open|re(ad|wind)|write)\b/ {

    keyword_face (true);
    language_print ($0);
    keyword_face (false);
    call (fortran_io);
  }

  /* IO Statement (build-re '(OPEN CLOSE READ
  WRITE INQUIRE BACKSPACE ENDFILE REWIND )) */
  /\b(BACKSPACE|CLOSE|ENDFILE|INQUIRE|OPEN|RE(AD|WIND)|WRITE)\b/ {

    keyword_face (true);
    language_print ($0);
    keyword_face (false);
    call (fortran_io);
  }

  /* Keywords.  (build-re '( block\ *data call
     character\\*\[0-9\]+ common complex\\*\[0-9\]+ continue
     data dimension do double\ *precision else elseif end enddo
     endfile endif entry equivalence external format function
     go\ *to if implicit include integer\\*\[0-9\]+ intrinsic
     logical\\*\[0-9\]+ parameter pause print program
     real\\*\[0-9\]+ return  save stop subroutine then while )) */
  /\b(block *data|c(all|haracter(\*[0-9]+)?|o(m(mon|plex(\*[0-9]+)?)|ntinue))\
|d(ata|imension|o(|uble *precision))\
|e(lse(|if)|n(d(|do|file|if)|try)|quivalence|xternal)|f(ormat|unction)\
|go *to|i(f|mplicit|n(clude|t(eger(\*[0-9]+)?|rinsic)))|logical(\*[0-9]+)?\
|p(a(rameter|use)|r(int|ogram))|re(al(\*[0-9]+)?|turn)\
|s(ave|top|ubroutine)|then|while)\b/ {

    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }
  /* Keywords.  (build-re '( block\ *data call
     character\\*\[0-9\]+ COMMON COMPLEX\\*\[0-9\]+ CONTINUE
     DATA DIMENSION DO DOUBLE\ *PRECISION ELSE ELSEIF END ENDDO
     ENDFILE ENDIF ENTRY EQUIVALENCE EXTERNAL FORMAT FUNCTION
     GO\ *TO IF IMPLICIT INCLUDE INTEGER\\*\[0-9\]+ INTRINSIC
     LOGICAL\\*\[0-9\]+ PARAMETER PAUSE PRINT PROGRAM
     REAL\\*\[0-9\]+ RETURN  SAVE STOP SUBROUTINE THEN WHILE )) */
  /\b(BLOCK *DATA|C(ALL|HARACTER(\*[0-9]+)?|O(M(MON|PLEX(\*[0-9]+)?)|NTINUE))\
|D(ATA|IMENSION|O(|UBLE *PRECISION))\
|E(LSE(|IF)|N(D(|DO|FILE|IF)|TRY)|QUIVALENCE|XTERNAL)|F(ORMAT|UNCTION)\
|GO *TO|I(F|MPLICIT|N(CLUDE|T(EGER(\*[0-9]+)?|RINSIC)))|LOGICAL(\*[0-9]+)?\
|P(A(RAMETER|USE)|R(INT|OGRAM))|RE(AL(\*[0-9]+)?|TURN)\
|S(AVE|TOP|UBROUTINE)|THEN|WHILE)\b/ {

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
