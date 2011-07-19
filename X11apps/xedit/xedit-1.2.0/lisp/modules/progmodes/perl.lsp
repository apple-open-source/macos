;; Copyright (c) 2007,2008 Paulo Cesar Pereira de Andrade
;;
;; Permission is hereby granted, free of charge, to any person obtaining a
;; copy of this software and associated documentation files (the "Software"),
;; to deal in the Software without restriction, including without limitation
;; the rights to use, copy, modify, merge, publish, distribute, sublicense,
;; and/or sell copies of the Software, and to permit persons to whom the
;; Software is furnished to do so, subject to the following conditions:
;;
;; The above copyright notice and this permission notice (including the next
;; paragraph) shall be included in all copies or substantial portions of the
;; Software.
;;
;; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
;; THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
;; FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
;; DEALINGS IN THE SOFTWARE.
;;
;; Author: Paulo Cesar Pereira de Andrade
;;

;;   Perl syntax and indentation mode
;;   Based on the C/C++ and Lisp modes. Attempting to make simple
;; syntax/indentation rules, that should work correctly with most
;; perl code.

;;  *cont-indent* is somewhat buggy, that if pressing C-A,Tab, will
;; not generate the same output as when normally typing the expression.
;;  This is because the parser doesn't search for a matching ';', '{',
;; '[' or '(' to know where the expression starts. The C mode has the
;; same problem. Example:
;;	a +
;;	    b;	<-- if pressing C-A,Tab will align "b;" with "a +"

;;  Maybe most of the code here, and some code in the C mode could be
;; merged to have a single "default mode" parser for languages that
;; basically only depend on { and } for indentation.

(require "syntax")
(require "indent")
(in-package "XEDIT")

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defsynprop *prop-string-escape*
  "string-escape"
  :font		"*lucidatypewriter-bold-r*-12-*"
  :foreground	"RoyalBlue2"
  :underline	t)

(defsynprop *prop-string-keyword-bold*
  "string-variable-bold"
  :font		"*lucidatypewriter-bold-r*-12-*"
  :foreground	"RoyalBlue4")

(defsynprop *prop-string-keyword*
  "string-variable"
  :font		"*lucidatypewriter-medium-r*-12-*"
  :foreground	"RoyalBlue4")

(defsynprop *prop-constant-escape*
  "constant-escape"
  :font		"*lucidatypewriter-medium-r*-12-*"
  :foreground	"VioletRed3"
  :underline	t)

(defsynprop *prop-regex*
  "regex"
  :font		"*courier-medium-o*-12-*"
  :foreground	"black")

(defsynprop *prop-shell*
  "shell"
  :font		"*lucidatypewriter-medium-r*-12-*"
  :foreground	"red3")

(defsynprop *prop-shell-escape*
  "shell-escape"
  :font		"*lucidatypewriter-bold-r*-12-*"
  :foreground	"red3"
  :underline	t)

(defsynprop *prop-documentation*
  "documentation"
  :font		"fixed"
  :foreground	"black"
  :background	"rgb:e/e/e"
)


;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defsynoptions *perl-DEFAULT-style*
  ;; Positive number. Basic indentation
  (:indentation		.	4)

  ;; Boolean. Add one indentation level to continuations?
  (:cont-indent		.	t)

  ;; Boolean. Move cursor to the indent column after pressing <Enter>?
  (:newline-indent	.	t)

  ;; Boolean. Set to T if tabs shouldn't be used to fill indentation.
  (:emulate-tabs	.	nil)

  ;; Boolean. Only calculate indentation after pressing <Enter>?
  ;;		This may be useful if the parser does not always
  ;;		do what the user expects...
  (:only-newline-indent	.	nil)

  ;; Boolean. Remove extra spaces from previous line.
  ;;		This should default to T when newline-indent is not NIL.
  (:trim-blank-lines	.	t)

  ;; Boolean. If this hash-table entry is set, no indentation is done.
  ;;		Useful to temporarily disable indentation.
  (:disable-indent	.	nil))



;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defvar *perl-mode-options* *perl-DEFAULT-style*)

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
;; Parenthesis are usually not required, just distinguish as:
;; expression:	code without an ending ';'
;; statement:	code ending in a ';'
;; block:	code enclosed in '{' and '}'
;; In Perl a simpler logic can be used, unlikely the C mode, as in
;; perl braces are mandatory
(defindent *perl-mode-indent* :main
  ;; this must be the first token
  (indtoken "^\\s*" :indent
    :code (or *offset* (setq *offset* (+ *ind-offset* *ind-length*))))
  ;; this may cause some other patterns to fail, due to matching single \'
  (indtoken "(&?(\\w+)|&(\\w+)?)'\\w+" :expression)
  ;; special variables
  (indtoken "\\$(\\d|^\\u|[][0-9!#$*()_@<>?/|,\"'])" :expression)
  ;; ignore comments
  (indtoken "#.*$" nil)
  ;; treat regex as expressions to avoid confusing parser
  (indtoken "m?/([^/]|\\\\/)+/\\w*" :expression)
  (indtoken "m\\{[^}]+\\}\\w*" :expression)
  (indtoken "m<[^>]+>\\w*" :expression)
  (indtoken "(s|tr)/[^/]+/([^/]|\\\\/)*/\\w*" :expression)
  (indtoken "//" :expression :nospec t)
  ;; fast resolve deferences to expressions
  (indtoken "[$@%&*]?\\{\\$?\\S+\\}" :expression)

  (indtoken "($%@*)?\\w+" :expression)
  (indtoken ";" :semi :nospec t)
  (indinit (braces 0))
  (indtoken "{" :obrace :nospec t
    :code (decf braces))
  (indtoken "}" :cbrace :nospec t
    :code (incf braces))
  (indinit (parens&bracks 0))
  (indtoken ")" :cparen :nospec t :code (incf parens&bracks))
  (indtoken "(" :oparen	:nospec t :code (decf parens&bracks))
  (indtoken "]" :cbrack	:nospec t :code (incf parens&bracks))
  (indtoken "[" :obrack	:nospec t :code (decf parens&bracks))
  ;; if in the same line, reduce now, this must be done because the
  ;; delimiters are identical
  (indtoken "'([^\\']|\\\\.)*'" :expression)
  (indtoken "\"([^\\\"]|\\\\.)*\"" :expression)
  (indtoken "\"" :cstring1 :nospec t :begin :string1)
  (indtoken "'" :cstring2 :nospec t :begin :string2)
  ;; This must be the last rule
  (indtoken "\\s*$"		:eol)

  (indtable :string1
    ;; Ignore escaped characters
    (indtoken "\\." nil)
    ;; Return to the toplevel when the start of the string is found
    (indtoken "\"" :ostring1 :nospec t :switch -1))
  (indtable :string2
    (indtoken "\\." nil)
    (indtoken "'" :ostring2 :nospec t :switch -1))

  ;; This avoids some problems with *cont-indent* adding an indentation
  ;; level to an expression after an empty line
  (indreduce nil
    t
    ((:indent :eol)))

  ;; Reduce to a single expression token
  (indreduce :expression
    t
    ((:indent :expression)
      (:expression :eol)
      (:expression :parens)
      (:expression :bracks)
      (:expression :expression)
      ;; multiline strings
      (:ostring1 (not :ostring1) :cstring1)
      (:ostring2 (not :ostring2) :cstring2)
      ;; parenthesis and brackets
      (:oparen (not :oparen) :cparen)
      (:obrack (not :obrack) :cbrack)))

  ;; Statements end in a semicollon
  (indreduce :statement
    t
    ((:semi)
      (:indent :semi)
      (:expression :statement)
      (:statement :eol)
      ;; Doesn't necessarily end in a semicollon
      (:expression :block)))

  (indreduce :block
    t
    ((:obrace (not :obrace) :cbrace)
      (:block :eol)))
  (indreduce :obrace
    (< *ind-offset* *ind-start*)
    ((:indent :obrace))
    (setq *indent* (offset-indentation (+ *ind-offset* *ind-length*) :resolve t))
    (indent-macro-reject-left))

  ;; Try to do an smart indentation on open parenthesis and brackets
  (indreduce :parens
    t
    ((:oparen (not :oparen) :cparen))
    (when (and
	    (< *ind-offset* *ind-start*)
	    (> (+ *ind-offset* *ind-length*) *ind-start*))
      (setq *indent* (1+ (offset-indentation *ind-offset* :align t)))
      (indent-macro-reject-left)))
  (indreduce :bracks
    t
    ((:obrack (not :obrack) :cbrack))
    (when (and
	    (< *ind-offset* *ind-start*)
	    (> (+ *ind-offset* *ind-length*) *ind-start*))
      (setq *indent* (1+ (offset-indentation *ind-offset* :align t)))
      (indent-macro-reject-left)))

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ;; Assuming previous lines have correct indentation, try to
  ;; fast resolve brace indentation
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ;; Line ended with an open brace
  (indreduce :obrace
    (< *ind-offset* *ind-start*)
    ((:expression :obrace))
    (setq *indent* (offset-indentation *ind-offset* :resolve t))
    (indent-macro-reject-left))
  ;; Line starts with an open brace
  (indreduce nil
    (< *ind-offset* *ind-start* (+ *ind-offset* *ind-length*))
    ;; Just set initial indentation
    ((:indent :obrace))
    (setq
      *indent* (- (offset-indentation *ind-offset* :resolve t) *base-indent*))
    (indent-macro-reject-left))

  (indresolve :statement
    (when (< *ind-offset* *ind-start*)
      (while (> braces 0)
	(setq
	  *indent*	(- *indent* *base-indent*)
	  braces	(1- braces)))))

  (indresolve :obrace
    (and (< *ind-offset* *ind-start*)
      (incf *indent* *base-indent*)))
  (indresolve :cbrace
    (decf *indent* *base-indent*))
  (indresolve :expression
    (and
      *cont-indent*
      (> *indent* 0)
      (zerop parens&bracks)
      (< *ind-offset* *ind-start*)
      (> (+ *ind-offset* *ind-length*) *ind-start*)
      (incf *indent* *base-indent*)))

  (indresolve (:oparen :obrack)
    (and (< *ind-offset* *ind-start*)
      (setq *indent* (1+ (offset-indentation *ind-offset* :align t)))))
)

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defun perl-offset-indent (&aux char (point (point)))
  ;; Skip spaces forward
  (while (member (setq char (char-after point)) indent-spaces)
    (incf point))
  (if (member char '(#\})) (1+ point) point))

(compile 'perl-offset-indent)

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defun perl-should-indent (options &aux char point start offset)
  (when (hash-table-p options)
    ;; check if previous line has extra spaces
    (and (gethash :trim-blank-lines options)
      (indent-clear-empty-line))

    ;; indentation disabled?
    (and (gethash :disable-indent options)
      (return-from perl-should-indent))

    (setq
      point	(point)
      char	(char-before point)
      start	(scan point :eol :left))

    ;; if at bol and should indent only when starting a line
    (and (gethash :only-newline-indent options)
      (return-from perl-should-indent (= point start)))

    ;; at the start of a line
    (and (= point start)
      (return-from perl-should-indent (gethash :newline-indent options)))

    ;; if first character
    (and (= point (1+ start))
      (return-from perl-should-indent t))

    ;; check if is the first non-blank character in a new line
    (when (and
	    (gethash :cont-indent options)
	    (= point (scan point :eol :right))
	    (alphanumericp char))
      (setq offset (1- point))
      (while (and
	       (> offset start)
	       (member (char-before offset) indent-spaces))
	(decf offset))
      ;; line has only one character with possible spaces before it
      (and (<= offset start)
	(return-from perl-should-indent t)))

    ;; if one of these was typed, should check indentation
    (if (member char '(#\})) (return-from perl-should-indent t))
  )
  ;; Should not indent
  nil)

(compile 'perl-should-indent)

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defun perl-indent (syntax syntable)
  (let*
    ((options (syntax-options syntax))
      *base-indent*
      *cont-indent*)

    (or (perl-should-indent options) (return-from perl-indent))
    (setq
      *base-indent* (gethash :indentation options 4)
      *cont-indent* (gethash :cont-indent options t))

    (indent-macro
      *perl-mode-indent*
      (perl-offset-indent)
      (gethash :emulate-tabs options))))

(compile 'perl-indent)

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
;; some example macros to easily add new patterns for strings and possibly
;; regex or other patterns
(defmacro perl-q-string-token (token)
  `(syntoken (string-concat "\\<q(q|w)?\\s*\\" ,token)
     :icase t :contained t :begin
     (intern (string-concat "string" ,token) 'keyword)))
(defmacro perl-q-string-table (start end)
  `(syntable (intern (string-concat "string" ,start) 'keyword)
     *prop-string* #'default-indent
     (syntoken ,end :nospec t :switch -1)
     (synaugment :inside-string)))

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defsyntax *perl-mode* :main nil #'perl-indent *perl-mode-options*
  ;; keywords
  (syntoken
    (string-concat
      "\\<("
      "and|for|foreach|gt|if|else|elsif|eq|goto|le|lt|last|ne|"
      "neg|next|not|or|return|shift|sub|unless|unshift|until|while"
      ")\\>")
    :property *prop-keyword*)

  ;; pseudo keywords
  (syntoken
    (string-concat
      "\\<("
      "BEGIN|END|bless|blessed|defined|delete|eval|local|my|our|"
      "package|require|undef|use"
      ")\\>")
    :property *prop-preprocessor*)
  ;; this may cause some other patterns to fail, due to matching single \'
  (syntoken "(&?(\\w+)|&(\\w+)?)'\\w+" :property *prop-preprocessor*)

  ;; numbers
  (syntoken
    (string-concat
      "\\<("
      ;; Integers
      "(\\d+|0x\\x+)|"
      ;; Floats
      "\\d+\\.?\\d*(e[+-]?\\d+)?"
      ")\\>")
    :icase t
    :property *prop-number*)

  ;; special variables
  (syntoken "\\$(\\d|^\\u|[][0-9!#$*()_@<>?/|,\"'])" :property *prop-keyword*)

  ;; also match variables
  (syntable :inside-string nil nil
    ;; escaped characters

    ;; XXX This pattern was matching the empty string and entering an
    ;; infinite loop in code like:
#|
---%<---
"		<-- *** if an backslash is added it fails. Inverting
a";                 *** the pattern fixed the problem, but was the wrong
---%<---	    *** solution. Note that C-G stops the interpreter, and
		    *** special care must be taken with patterns matching
		    *** empty strings.
|#

    (syntoken "\\\\\\d{3}|\\\\." :property *prop-string-escape*)
    (syntoken "(\\{\\$|\\$\\{)" :property *prop-string-keyword-bold* :begin :string-varbrace)
    (syntoken "[$@]" :property *prop-string-keyword-bold* :begin :string-variable)
    (syntoken "\\$(\\d|^\\u|[][0-9!#$*()_@<>?/|,\"'])" :property *prop-string-keyword-bold*))

  ;; variables insided strings
  (syntable :string-variable *prop-string-keyword* nil
    (syntoken "\\w+" :switch -1))
  (syntable :string-varbrace *prop-string-keyword* nil
    (syntoken		"}"
      :nospec		t
      :property	*prop-string-keyword-bold*
      :switch		-1)
    (synaugment :inside-string))

  ;; comments
  (syntoken "#.*$" :property *prop-comment*)

  ;; regex
  (syntoken "(\\<m)?/([^/]|\\\\/)+/\\w*"	:property *prop-regex*)
  (syntoken "\\<m\\{[^}]+\\}\\w*"		:property *prop-regex*)
  (syntoken "\\<m<[^>]+>\\w*"			:property *prop-regex*)
  (syntoken "\\<(s|tr)/[^/]+/([^/]|\\\\/)*/\\w*":property *prop-regex*)
  ;; just to avoid confusing the parser on something like split //, ...
  (syntoken "//" :nospec t			:property *prop-regex*)

  ;; strings
  (syntoken "\"" :nospec t :contained t :begin :string)
  (syntable :string *prop-string* #'default-indent
    (syntoken "\"" :nospec t :switch -1)
    (synaugment :inside-string))

  ;; more strings
  (perl-q-string-token "{")
  (perl-q-string-table "{" "}")
  (perl-q-string-token "[")
  (perl-q-string-table "[" "]")
  (perl-q-string-token "(")
  (perl-q-string-table "(" ")")
  (perl-q-string-token "/")
  (perl-q-string-table "/" "/")

  ;; yet more strings
  (syntoken "'" :nospec t :contained t :begin :constant)
  (syntable :constant *prop-constant* #'default-indent
    (syntoken "'"	:nospec t :switch -1)
    (syntoken "\\\\." :property *prop-string-escape*))

  ;; shell commands
  (syntoken "`" :nospec t :contained t :begin :shell)
  (syntable :shell *prop-shell* #'default-indent
    (syntoken "`"	:nospec t :switch -1)
    (synaugment :inside-string))

  ;; punctuation
  (syntoken "[][$@%(){}/*+:;=<>,&!|^~\\.?-]" :property *prop-punctuation*)
  (syntoken "\\<x\\>" :property *prop-punctuation*)

  ;; primitive faked heredoc support, doesn't match the proper string, just
  ;; expects an uppercase identifier in a single line
  (syntoken "<<\"[A-Z][A-Z0-9_]+\"" :property *prop-string* :begin :heredoc)
  (syntoken "<<'[A-Z][A-Z0-9_]+'" :property *prop-constant* :begin :heredoc)
  (syntoken "<<[A-Z][A-Z0-9_]+" :property *prop-preprocessor* :begin :heredoc)
  (syntable :heredoc *prop-documentation* #'default-indent
    (syntoken "^[A-Z][A-Z0-9_]+$" :switch -1))

  (syntoken "^=(pod|item|over|head\\d)\\>.*$" :property *prop-documentation* :begin :info)
  (syntable :info *prop-documentation* nil
    (syntoken "^=cut\\>.*$"	:switch -1)
    (syntoken "^.*$"))

  (syntoken "^(__END__|__DATA__)$" :property *prop-documentation*
    :begin :documentation)

  (syntoken "__\\u+__" :property *prop-preprocessor*)

  (syntable :documentation *prop-documentation* nil
    (syntoken "^.*$"))

)
