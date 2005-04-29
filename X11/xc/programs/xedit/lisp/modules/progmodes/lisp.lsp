;;
;; Copyright (c) 2002 by The XFree86 Project, Inc.
;;
;; Permission is hereby granted, free of charge, to any person obtaining a
;; copy of this software and associated documentation files (the "Software"),
;; to deal in the Software without restriction, including without limitation
;; the rights to use, copy, modify, merge, publish, distribute, sublicense,
;; and/or sell copies of the Software, and to permit persons to whom the
;; Software is furnished to do so, subject to the following conditions:
;;
;; The above copyright notice and this permission notice shall be included in
;; all copies or substantial portions of the Software.
;;
;; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
;; THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
;; WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
;; OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
;; SOFTWARE.
;;
;; Except as contained in this notice, the name of the XFree86 Project shall
;; not be used in advertising or otherwise to promote the sale, use or other
;; dealings in this Software without prior written authorization from the
;; XFree86 Project.
;;
;; Author: Paulo César Pereira de Andrade
;;
;;
;; $XFree86: xc/programs/xedit/lisp/modules/progmodes/lisp.lsp,v 1.10 2004/01/08 21:29:12 paulo Exp $
;;

(require "syntax")
(require "indent")
(in-package "XEDIT")

(defsynprop *prop-special*
  "special"
  :font		"*courier-bold-r*-12-*"
  :foreground	"NavyBlue"
)

(defsynprop *prop-quote*
  "quote"
  :font		"*courier-bold-r*-12-*"
  :foreground	"Red3"
)

(defsynprop *prop-package*
  "package"
  :font		"*lucidatypewriter-medium-r*-12-*"
  :foreground	"Gold4"
)

(defsynprop *prop-unreadable*
  "unreadable"
  :font		"*courier-medium-r*-12-*"
  :foreground	"Gray25"
  :underline	t
)

(defsynoptions *lisp-DEFAULT-style*
  ;; Positive number. Basic indentation.
  (:indentation			.	2)

  ;; Boolean. Move cursor to the indent column after pressing <Enter>?
  (:newline-indent		.	t)

  ;; Boolean. Use spaces instead of tabs to fill indentation?
  (:emulate-tabs		.	nil)

  ;; Boolean. Remove extra spaces from previous line.
  ;;		This should default to T when newline-indent is not NIL.
  (:trim-blank-lines		.	t)

  ;; Boolean. If this hash-table entry is set, no indentation is done.
  ;;		Useful to temporarily disable indentation.
  (:disable-indent		.	nil)
)

(defvar *lisp-mode-options* *lisp-DEFAULT-style*)

(defindent *lisp-mode-indent* :main
  ;; this must be the first token
  (indtoken "^\\s*"		:indent
    :code (or *offset* (setq *offset* (+ *ind-offset* *ind-length*))))
  ;; ignore single line comments
  (indtoken ";.*$"		nil)
  ;; multiline comments
  (indtoken "|#"		:comment	:nospec t	:begin :comment)
  ;; characters
  (indtoken "#\\\\(\\W|\\w+(-\\w+)?)"		:character)
  ;; numbers
  (indtoken
    (string-concat
      "(\\<|[+-])\\d+("
      ;; integers
      "(\\>|\\.(\\s|$))|"
      ;; ratios
      "/\\d+\\>|"
      ;;floats
      "\\.?\\d*([SsFfDdLlEe][+-]?\\d+)?\\>"
      ")")
    :number)
  ;; symbols, with optional package
  (indtoken
    (string-concat
      ;; optional package name and ending ':'
      "([A-Za-z_0-9%-]+:)?"
      ;; internal symbol if after package name, or keyword
      ":?"
      ;; symbol name
      "[][{}A-Za-z_0-9!$%&/<=>^~*+-]+")
    :symbol)
  ;; strings in the same line
  (indtoken "\"([^\\\"]|\\\\.)*\""		:string)
  ;; multiline strings
  (indtoken "\""		:cstring	:nospec t	:begin :string)
  ;; "quoted" symbols in the same line
  (indtoken "\\|([^\\|]|\\\\.)*\\|"		:symbol)
  ;; multiline
  (indtoken "|"			:csymbol	:nospec t	:begin :symbol)
  (indtoken "#"			:hash		:nospec t)

  (indinit	(parens 0))
  (indtoken "("			:oparen		:nospec t :code (incf parens))
  (indtoken ")"			:cparen		:nospec t :code (decf parens))

  (indtable :comment
    ;; multiline comments can nest
    (indtoken "|#"		nil		:nospec t	:begin :comment)
    (indtoken "#|"		nil		:nospec t	:switch -1))

  (indtable :string
    ;; Ignore escaped characters
    (indtoken "\\." 		nil)
    ;; Return to the toplevel when the start of the string is found
    (indtoken "\""		:ostring	:nospec t	:switch -1))

  (indtable :symbol
    ;; Ignore escaped characters
    (indtoken "\\." 		nil)
    ;; Return to the toplevel when the start of the symbol is found
    (indtoken "|"		:osymbol	:nospec t	:switch -1))

  ;; ignore comments
  (indreduce nil
    t
    ((:comment)))

  ;; reduce multiline strings
  (indreduce :string
    t
    ((:ostring (not :ostring) :cstring)))

  ;; reduce multiline symbols
  (indreduce :symbol
    t
    ((:osymbol (not :osymbol) :csymbol)))

  ;; reduce basic types, don't care if inside list or not
  (indreduce :element
    t
    ((:number)
      (:string)
      (:character)
      (:element :element)
      (:indent :element)))

  (indreduce :symbol
    t
    ((:symbol :symbol)
      (:symbol :element)
      (:indent :symbol)))

  ;; the "real" indentation value, to make easier parsing code like:
  ;;  (foo (bar (baz (blah
  ;;        ^         ^
  ;;        |         |
  ;;        indent    |
  ;;                  effective indentation to be used
  (indinit	(indent 0))

  ;; indentation values of opening parenthesis.
  (indinit	stack)

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ;; if before current line and open parenthesis >= 0, use indentation
  ;; of current line to calculate relative indentation.
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  (indreduce :oparen	;; simple list?
    (and (>= parens 0) (< *ind-offset* *ind-start*))
    ((:indent :oparen))
    (setq
      *indent*	(offset-indentation (+ *ind-offset* *ind-length*) :resolve t)
      indent	*indent*)
    (indent-macro-reject-left))

  ;; reduce list if there isn't indentation change
  (indreduce :element
    t
    ((:oparen (not :oparen) :cparen)))

  (indresolve :oparen
    (setq
      *indent*
      (offset-indentation
	(+ *ind-offset* *ind-length* -1 *base-indent*) :align t))
    (push *indent* stack)
    (incf indent *base-indent*)
    (if (< *indent* indent) (setq *indent* indent)))

  (indresolve :cparen
    (decf indent *base-indent*)
    (setq *indent* (pop stack))
    (if (null stack)
      (setq *indent* indent)
      (setq *indent* (car stack))))
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Find a "good" offset to start parsing backwards, so that it should
;; always generate the same results.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun lisp-offset-indent (&aux char (point (scan (point) :eol :left)))
  ;; skip spaces
  (while (member (setq char (char-after point)) indent-spaces)
    (incf point))
  (if (member char '(#\))) (1+ point) point))

(defun lisp-should-indent (options &aux char point start)
  (when (hash-table-p options)
    ;; check if previous line has extra spaces
    (and (gethash :trim-blank-lines options)
      (indent-clear-empty-line))

    ;; indentation disabled?
    (and (gethash :disable-indent options)
      (return-from lisp-should-indent))

    (setq
      point	(point)
      char	(char-before (point))
      start	(scan point :eol :left))

    ;; at the start of a line
    (and (= point start)
      (return-from lisp-should-indent (gethash :newline-indent options)))

    ;; if first character
    (and (= point (1+ start)) (return-from lisp-should-indent t))

    ;; if closing parenthesis and first nonblank char
    (when (and (characterp char) (char= char #\)))
      (decf point)
      (while
	(and (> point start) (member (char-before point) indent-spaces))
	(decf point))
      (return-from lisp-should-indent (<= point start)))
  )
  ;; should not indent
  nil)

(defun lisp-indent (syntax syntable)
  (let*
    ((options (syntax-options syntax))
      *base-indent*)

    (or (lisp-should-indent options) (return-from lisp-indent))

    (setq *base-indent* (gethash :indentation options 2))

    (indent-macro
      *lisp-mode-indent*
      (lisp-offset-indent)
      (gethash :emulate-tabs options))))

(compile 'lisp-indent)

(defsyntax *lisp-mode* :main nil #'lisp-indent *lisp-mode-options*
  ;; highlight car and parenthesis
  (syntoken "\\(+\\s*[][{}A-Za-z_0-9!$%&/<=>?^~*:+-]*\\)*"
    :property *prop-keyword*)
  (syntoken "\\)+" :property *prop-keyword*)

  ;; nil and t
  (syntoken "\\<(nil|t)\\>" :icase t :property *prop-special*)

  (syntoken "|" :nospec t :begin :unreadable :contained t)

  ;; keywords
  (syntoken ":[][{}A-Za-z_0-9!$%&/<=>^~+-]+" :property *prop-constant*)

  ;; special symbol.
  (syntoken "\\*[][{}A-Za-z_0-9!$%&7=?^~+-]+\\*"
    :property *prop-special*)

  ;; special identifiers
  (syntoken "&(aux|key|optional|rest)\\>" :icase t :property *prop-constant*)

  ;; numbers
  (syntoken
    ;; since lisp is very liberal in what can be a symbol, this pattern
    ;; will not always work as expected, since \< and \> will not properly
    ;; work for all characters that may be in a symbol name
    (string-concat
      "(\\<|[+-])\\d+("
      ;; integers
      "(\\>|\\.(\\s|$))|"
      ;; ratios
      "/\\d+\\>|"
      ;;floats
      "\\.?\\d*([SsFfDdLlEe][+-]?\\d+)?\\>"
      ")")
    :property *prop-number*)

  ;; characters
  (syntoken "#\\\\(\\W|\\w+(-\\w+)?)" :property *prop-constant*)

  ;; quotes
  (syntoken "[`'.]|,@?" :property *prop-quote*)

  ;; package names
  (syntoken "[A-Za-z_0-9%-]+::?" :property *prop-package*)

  ;; read time evaluation
  (syntoken "#\\d+#" :property *prop-preprocessor*)
  (syntoken "#([+'cCsS-]|\\d+[aA=])?" :begin :preprocessor :contained t)

  (syntoken "\\c" :property *prop-control*)

  ;; symbols, do nothing, just resolve conflicting matches
  (syntoken "[][{}A-Za-z_0-9!$%&/<=>^~*+-]+")

  (syntable :simple-comment *prop-comment* nil
    (syntoken "$" :switch -1)
    (syntoken "XXX|FIXME|TODO" :property *prop-annotation*))

  (syntable :comment *prop-comment* nil
    ;; comments can nest
    (syntoken "#|" :nospec t :begin :comment)
    ;;  return to previous state
    (syntoken "|#" :nospec t :switch -1)
    (syntoken "XXX|FIXME|TODO" :property *prop-annotation*))

  (syntable :unreadable *prop-unreadable* nil
    ;; ignore escaped characters
    (syntoken "\\\\.")
    (syntoken "|" :nospec t :switch -1))

  (syntable :string *prop-string* nil
    ;; ignore escaped characters
    (syntoken "\\\\.")
    (syntoken "\"" :nospec t :switch -1))

  (syntable :preprocessor *prop-preprocessor* nil
    ;; a symbol
    (syntoken "[][{}A-Za-z_0-9!$%&/<=>^~:*+-]+" :switch -1)

    ;; conditional expression
    (syntoken "(" :nospec t :begin :preprocessor-expression :contained t)

    (syntable :preprocessor-expression *prop-preprocessor* nil
      ;; recursive
      (syntoken "(" :nospec t :begin :preprocessor-recursive :contained t)
      (syntoken ")" :nospec t :switch -2)

      (syntable :preprocessor-recursive *prop-preprocessor* nil
	(syntoken "(" :nospec t
	  :begin :preprocessor-recursive
	  :contained t)
	(syntoken ")" :nospec t :switch -1)
	(synaugment :comments-and-strings))
      (synaugment :comments-and-strings))
    (synaugment :comments-and-strings))

  (syntable :comments-and-strings nil nil
    (syntoken "\"" :nospec t :begin :string :contained t)
    (syntoken "#|" :nospec t :begin :comment :contained t)
    (syntoken ";" :begin :simple-comment :contained t))

  (synaugment :comments-and-strings)
)
