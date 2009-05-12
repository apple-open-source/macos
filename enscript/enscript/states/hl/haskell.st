/**
 * Name: haskell
 * Description: Haskell programming language.
 *
 * Simple highlighting treating keywords, comments, strings and type
 * expressions specially.
 *
 * Author: Hans-Wolfgang Loidl <hwloidl@dcs.gla.ac.uk>
 * Date: 27/2/97
 */

state haskell extends HighlightEntry
{
  /* Comments. */
  /\{\-/ {
    comment_face (true);
    language_print ($0);
    call (haskell_comment);
    comment_face (false);
  }

  /* One line comments. */
  /\-\-/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* Built-in beasts (GHC specific). */
  /\b\_/ {
    keyword_face (true);
    language_print ($0);
    call (haskell_builtin);
    keyword_face (false);
  }

  /* Type declarations. */
  /::/ {
    type_face (true);
    language_print ($0);
    call (eat_one_line);
    /* call (haskell_type); */
    type_face (false);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (haskell_string);
    string_face (false);
  }

  /* Pre-processor lines. */
  /^#/ {
    reference_face (true);
    language_print ($0);
    call (eat_one_line);
    /* call (haskell_ppline); */
    reference_face (false);
  }

  /* Character constants. */
  /'.'|'\\.'/ {
    string_face (true);
    language_print ($0);
    string_face (false);
  }

  /* Keywords.
     I took that from haskell.el. The True Way to do it would be to grab it
     out of the on-line haskell report (actually, The Real True Way would
     be to write a Haskell program that extracts different kinds of
     keywords and to partially evaluate it wrt the Haskell report; but that
     might be a wee overkill).
     (let ((strings
	'("case" "class" "data" "default" "deriving" "else" "hiding" "if" "import" "in" "\
	  infix" "infixl" "infixr" "instance" "interface" "let" "module" "of" "renaming" "\
	  then" "to" "type" "where" )))
      (make-regexp strings)
     )
     ==>
     \(infix\|then\)\|c\(ase\|lass\)\|d\(ata\|e\(fault\|riving\)\)\|else\|hiding\|i\([fn]\|mport\|n\(fix[lr]\|stance\|terface\)\)\|let\|module\|of\|renaming\|t\(o\|ype\)\|where
   */
  /\b((infix|then)|c(ase|lass)|d(ata|e(fault|riving))|else|hiding|i([fn]|mport|n(fix[lr]|stance|terface))|let|module|of|renaming|t(o|ype)|where)\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }
}

state haskell_comment extends Highlight
{
  /\-\}/ {
    language_print ($0);
    return;
  }
}

/* HWL: for GHC builtin objects like _parGlobal_ i.e. not std Haskell */
state haskell_builtin extends Highlight
{
  /(\_\b)| / {
    language_print ($0);
    return;
  }
}

state haskell_type extends Highlight
{
  /* ToDo: Implement type continuation lines:
	   If the line ends in a -> or the next starts with a -> then we
	   are still in a type expression
  */
  /\n/ {
    language_print ($0);
    return;
  }
}

state haskell_string extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /\"/ {
    language_print ($0);
    return;
  }
}

state haskell_ppline extends Highlight
{
  /\/\*/ {
    /* Comment within a pre-processor line. */
    reference_face (false);
    comment_face (true);
    language_print ($0);
    call (c_comment);
    comment_face (false);
    reference_face (true);
  }
  /\n/ {
    language_print ($0);
    return;
  }
}


/*
Local variables:
mode: c
End:
*/
