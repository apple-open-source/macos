/**
 * Name: fortran
 * Description: Fortran77 programming language.
 * Author: Keith Refson <Keith.Refson@earth.ox.ac.uk>
 *    Markku Rossi <mtr@iki.fi>
 *    edited by Joachim Kaiser <jhk@cmpnetmail.com>:
 *       - relational/logical operators and io specifiers (named added) with
 *         changed from keyword_face to builtin_face
 *       - type statement keywords changed from keyword_face to type_face
 *       - 'print' moved from keywords to io statements
 *       - distinguish between 'real' type statement and 'real' type conversion
 *         function
 *       - 'endfile', 'enddo' and 'while' removed from keyword list (not
 *         defined in FORTRAN 77), 'assign' added, 'else if' and 'end if' with
 *         optional blank before 'if'
 *       - comments only as complete lines starting with [cC\*] (FORTRAN 77)
 *       - case-insensitive regexp with the `i' option at the end of the regexp
 */

state fortran_string extends Highlight
{
  /[\']/ {
    language_print ($0);
    debug ("Finishing fortran_string state.");
    return;
  }
}

state fortran_io extends Highlight
{
  /\(/ {
    language_print ($0);
    parentheses_level++;
    debug (concat("Parenthesis_level = ",string(parentheses_level)));
  }

  /\)/ {
    language_print ($0);
    parentheses_level--;
    debug (concat("Parenthesis_level = ",string(parentheses_level)));
    if (parentheses_level == 0) {
      debug ("Finishing fortran_io state.");
      return;
    }
  }

  /* IO Specifiers.
     (build-re '(FMT UNIT REC END ERR FILE STATUS ACCESS FORM RECL BLANK
     IOSTAT EXIST OPENED NUMBER NAME NAMED SEQUENTIAL DIRECT FORMATTED
     UNFORMATTED NEXTREC))
   */
  /\b(ACCESS|BLANK|DIRECT|E(ND|RR|XIST)|F(ILE|MT|ORM(|ATTED))|IOSTAT\
|N(AMED?|EXTREC|UMBER)|OPENED|REC(|L)|S(EQUENTIAL|TATUS)\
|UN(FORMATTED|IT))\b/i {
    debug (concat("This is an io specifier: ",$0));
    builtin_face (true);
    language_print ($0);
    builtin_face (false);
  }

  /* String within io statement */
  /[\']/ {
    debug ("String in io statement found.");
    string_face (true);
    language_print ($0);
    call (fortran_string);
    string_face (false);
  }
}

state fortran extends HighlightEntry
{
  BEGIN {
    parentheses_level = 0;
    debug ("Starting fortran state.");
  }

  END {
    debug ("Finishing fortran state.");
  }

  /* Comments. */
  /^[cC\*]/ {
    debug ("Comment line found.");
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* String constants. */
  /[\']/ {
    debug ("String constant found.");
    string_face (true);
    language_print ($0);
    call (fortran_string);
    string_face (false);
  }

  /* Relational/logical operators.  We have to roll by hand because of the
     dots - "\b" doesn't delimit here. */
  /\.(AND|EQV?|G(E|T)|L(E|T)|NE(QV)?|NOT|OR)\./i {
    debug (concat("This is an rel/log operator: ",$0));
    builtin_face (true);
    language_print ($0);
    builtin_face (false);
  }

  /* IO Statement (build-re '(OPEN CLOSE READ PRINT
     WRITE INQUIRE BACKSPACE ENDFILE REWIND )) */
  /\b(BACKSPACE|CLOSE|ENDFILE|INQUIRE|OPEN|PRINT|RE(AD|WIND)|WRITE)(\(?)/i {
    debug (concat("This is an io statement: ",$1));
    keyword_face (true);
    language_print ($1);
    keyword_face (false);
    if (strcmp ($3, "") != 0) {
        language_print ($3);
        parentheses_level=1;
        call (fortran_io);
    }
  }

  /* Type statements. */
  /\b((CHARACTER|COMPLEX|INTEGER|LOGICAL|REAL)(\*([0-9]+|\())?\
|(DOUBLE *PRECISION))( *\(?[a-zA-Z0-9]+)/i {
    debug (concat("This is a type statement: ",$2,$5));
    type_face (true);
    language_print ($2);
    language_print ($5);
    type_face (false);
    language_print ($3);
    next_word = $6;
    if (regmatch (next_word, / *FUNCTION/i)) {
        debug ("... for a function");
        keyword_face (true);
        language_print (next_word);
        keyword_face (false);
    } else {
        language_print (next_word);
    }
  }

  /* Keywords other than type and io statements.
     (build-re '(ASSIGN BLOCK_sDATA CALL COMMON CONTINUE DATA DIMENSION DO
     ELSE ELSE_sIF END END_sIF ENTRY EQUIVALENCE EXTERNAL FORMAT FUNCTION
     GO_sTO IF IMPLICIT INCLUDE INTRINSIC PARAMETER PAUSE PROGRAM RETURN
     SAVE STOP SUBROUTINE THEN ))
   */
  /\b(ASSIGN|BLOCK \*DATA|C(ALL|O(MMON|NTINUE))|D(ATA|IMENSION|O)\
|E(LSE(| \*IF)|N(D(| \*IF)|TRY)|QUIVALENCE|XTERNAL)|F(ORMAT|UNCTION)\
|GO \*TO|I(F|MPLICIT|N(CLUDE|TRINSIC))|P(A(RAMETER|USE)|ROGRAM)|RETURN\
|S(AVE|TOP|UBROUTINE)|THEN)\b/i {
    debug (concat("Other keyword found: ",$0));
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
