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
;; $XFree86: xc/programs/xedit/lisp/modules/progmodes/c.lsp,v 1.27 2004/01/12 17:53:20 paulo Exp $
;;

(require "syntax")
(require "indent")
(in-package "XEDIT")

(defsynprop *prop-format*
    "format"
    :font	"*lucidatypewriter-medium-r*-12-*"
    :foreground	"RoyalBlue2"
    :underline	t
)

(defsynoptions *c-DEFAULT-style*
    ;; Positive number. Basic indentation.
    (:indentation		.	4)

    ;; Boolean. Support for GNU style indentation.
    (:brace-indent		.	nil)

    ;; Boolean. Add one indentation level to case and default?
    (:case-indent		.	t)

    ;; Boolean. Remove one indentation level for labels?
    (:label-dedent		.	t)

    ;; Boolean. Add one indentation level to continuations?
    (:cont-indent		.	t)

    ;; Boolean. Move cursor to the indent column after pressing <Enter>?
    (:newline-indent		.	t)

    ;; Boolean. Set to T if tabs shouldn't be used to fill indentation.
    (:emulate-tabs		.	nil)

    ;; Boolean. Force a newline before braces?
    (:newline-before-brace	.	nil)

    ;; Boolean. Force a newline after braces?
    (:newline-after-brace	.	nil)

    ;; Boolean. Force a newline after semicolons?
    (:newline-after-semi	.	nil)

    ;; Boolean. Only calculate indentation after pressing <Enter>?
    ;;		This may be useful if the parser does not always
    ;;		do what the user expects...
    (:only-newline-indent	.	nil)

    ;; Boolean. Remove extra spaces from previous line.
    ;;		This should default to T when newline-indent is not NIL.
    (:trim-blank-lines		.	t)

    ;; Boolean. If this hash-table entry is set, no indentation is done.
    ;;		Useful to temporarily disable indentation.
    (:disable-indent		.	nil)
)

;; BSD like style
(defsynoptions *c-BSD-style*
    (:indentation		.	8)
    (:brace-indent		.	nil)
    (:case-indent		.	nil)
    (:label-dedent		.	t)
    (:cont-indent		.	t)
    (:newline-indent		.	t)
    (:emulate-tabs		.	nil)
    (:newline-before-brace	.	nil)
    (:newline-after-brace	.	t)
    (:newline-after-semi	.	t)
    (:trim-blank-lines		.	t)
)

;; GNU like style
(defsynoptions *c-GNU-style*
    (:indentation		.	2)
    (:brace-indent		.	t)
    (:case-indent		.	nil)
    (:label-dedent		.	t)
    (:cont-indent		.	t)
    (:newline-indent		.	nil)
    (:emulate-tabs		.	nil)
    (:newline-before-brace	.	t)
    (:newline-after-brace	.	t)
    (:newline-after-semi	.	t)
    (:trim-blank-lines		.	nil)
)

;; K&R like style
(defsynoptions *c-K&R-style*
    (:indentation		.	5)
    (:brace-indent		.	nil)
    (:case-indent		.	nil)
    (:label-dedent		.	t)
    (:cont-indent		.	t)
    (:newline-indent		.	t)
    (:emulate-tabs		.	t)
    (:newline-before-brace	.	t)
    (:newline-after-brace	.	t)
    (:newline-after-semi	.	t)
    (:trim-blank-lines		.	t)
)

(defvar *c-styles* '(
    ("xedit"	.	*c-DEFAULT-style*)
    ("BSD"	.	*c-BSD-style*)
    ("GNU"	.	*c-GNU-style*)
    ("K&R"	.	*c-K&R-style*)
))

(defvar *c-mode-options* *c-DEFAULT-style*)
; (setq *c-mode-options* *c-gnu-style*)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; This is a very lazy "pattern matcher" for the C language.
;; If the syntax in the code is not correct, it may get confused, and
;; because it is "lazy" some wrong constructs will be recognized as
;; correct when reducing patterns.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defindent *c-mode-indent* :main
    ;; this must be the first token
    (indtoken "^\\s*"		:start-of-line)
    (indtoken "\\<case\\>"	:c-case)
    (indtoken "\\<default\\>"	:c-default)
    (indtoken "\\<do\\>"	:do)
    (indtoken "\\<if\\>"	:c-if)
    (indtoken "\\<else\\>"	:c-else)
    (indtoken "\\<for\\>"	:c-for)
    (indtoken "\\<switch\\>"	:c-switch)
    (indtoken "\\<while\\>"	:c-while)
    ;; Match identifiers and numbers as an expression
    (indtoken "\\w+"		:expression)
    (indtoken ";"		:semi		:nospec t)
    (indtoken ","		:comma		:nospec t)
    (indtoken ":"		:collon		:nospec t)
    ;;  Ignore spaces before collon, this avoids dedenting ternary
    ;; and bitfield definitions as the parser does not distinguish
    ;; labels from those, another option would be to use the pattern
    ;; "\\w+:", but this way should properly handle labels generated
    ;; by macros, example: `MACRO_LABEL(value):'
    (indtoken "\\s+:"		nil)

    (indinit			(c-braces 0))
    (indtoken "{"
	:obrace
	:nospec t
	:code	(decf c-braces)
    )
    (indtoken "}"
	:cbrace
	:nospec t
	:begin	:braces
	:code	(incf c-braces)
    )
    (indtable :braces
	(indtoken "{"
	    :obrace
	    :nospec t
	    :switch -1
	    :code   (decf c-braces)
	)
	(indtoken "}"
	    :cbrace
	    :nospec t
	    :begin  :braces
	    :code   (incf c-braces)
	)
    )

    (indinit			(c-bra 0))
    (indtoken ")"		:cparen		:nospec t :code (incf c-bra))
    (indtoken "("		:oparen		:nospec t :code (decf c-bra))
    (indtoken "]"		:cbrack		:nospec t :code (incf c-bra))
    (indtoken "["		:obrack		:nospec t :code (decf c-bra))
    (indtoken "\\\\$"		:continuation)

    ;; C++ style comment, disallow other tokens to match inside comment
    (indtoken "//.*$"		nil)

    (indtoken "#"		:hash		:nospec t)

    ;; if in the same line, reduce now, this must be done because the
    ;; delimiters are identical
    (indtoken "'([^\\']|\\\\.)*'"	:expression)
    (indtoken "\"([^\\\"]|\\\\.)*\""	:expression)

    (indtoken "\""		:cstring	:nospec t	:begin :string)

    (indtoken "'"		:cconstant	:nospec t	:begin :constant)

    (indtoken "*/"		:ccomment	:nospec t	:begin :comment)
    ;; this must be the last token
    (indtoken "$"		:end-of-line)

    (indtable :string
	;; Ignore escaped characters
	(indtoken "\\." 	nil)
	;; Return to the toplevel when the start of the string is found
	(indtoken "\""		:ostring	:nospec t	:switch -1)
    )
    (indtable :constant
	;; Ignore escaped characters
	(indtoken "\\." 	nil)
	;; Return to the toplevel when the start of the character is found
	(indtoken "'"		:oconstant	:nospec t	:switch -1)
    )
    (indtable :comment
	(indtoken "/*"		:ocomment	:nospec t	:switch -1)
    )

    ;; "Complex" statements
    (indinit		(c-complex 0) (c-cases 0))

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Order of reduce rules here is important, process comment,
    ;; continuations, preprocessor and set states when an eol is found.
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    (indinit	(c-offset (point-max))
		(c-prev-offset c-offset)
    )
    (indreduce :indent
	t
	((:start-of-line))
	(and (= *ind-start* *ind-offset*)
	    (setq
		*offset* (+ *ind-offset* *ind-length*)
	    )
	)
	(setq
	    c-prev-offset   c-offset
	    c-offset	    *ind-offset*
	)
    )

    ;; Delete comments
    (indreduce nil
	t
	((:ocomment nil :ccomment))
    )

    ;; Join in a single token to simplify removal of possible multiline
    ;; preprocessor directives
    (indinit			c-continuation)
    (indreduce :continuation
	t
	((:continuation :end-of-line))
	(setq c-continuation t)
    )

    (indreduce :eol
	t
	((:end-of-line))
	;; Anything after the eol offset is safe to parse now
	(setq c-continuation nil)
    )

    ;; Delete blank lines
    (indreduce nil
	t
	((:indent :eol))
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Preprocessor
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indreduce nil
	(>= *ind-offset* *ind-start*)
	((:indent :hash))
	(setq *indent* 0)
	(indent-macro-reject-left)
    )
    (indreduce nil
	t
	((:indent :hash nil :eol))
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Expressions
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indreduce :expression
	t
	;; Reduce to a single expression
	((:expression :parens)
	 (:expression :bracks)
	 (:expression :expression)
	;; These may be multiline
	 (:ostring (not :ostring) :cstring)
	 (:oconstant (not :oconstant) :cconstant)
	)
    )

    (indreduce :expression
	t
	((:expression :eol :indent :expression)
	 (:expression :eol :expression)
	)
    )

    (indreduce :exp-comma
	t
	((:expression :comma)
	)
    )

    ;; A semicollon, start a statement
    (indreduce :stat
	t
	((:semi))
    )

    ;; Expression following (possibly empty) statement
    (indreduce :stat
	t
	(((or :expression :exp-comma) :stat))
    )

    ;; Multiline statements
    (indreduce :stat
	t
	(((or :expression :exp-comma) :eol :indent :stat)
	 ;; rule below may have removed the :indent
	 ((or :expression :exp-comma) :eol :stat)
	)
    )

    (indinit	c-exp-indent)
    ;; XXX This rule avoids parsing large amounts of code
    (indreduce :stat
	t
	;; Eat eol if following expression
	((:indent :stat :eol)
	 (:indent :stat)
	)
	(if
	    (or
		(null c-exp-indent)
		(/= (cdar c-exp-indent) (+ *ind-offset* *ind-length*))
	    )
	    ;; A new statement, i.e. not just joining a multiline one
	    (push
		(cons
		    (offset-indentation *ind-offset* :resolve t)
		    (+ *ind-offset* *ind-length*)
		)
		c-exp-indent
	    )
	    ;; Update start of statement
	    (rplaca
		(car c-exp-indent)
		(offset-indentation *ind-offset* :resolve t)
	    )
	)
	(when (consp (cdr c-exp-indent))
	    (if (and
		    (zerop c-complex)
		    (zerop c-cases)
		    (zerop c-bra)
		    (= (caar c-exp-indent) (caadr c-exp-indent))
		)
		;; Two statements with the same indentation
		(progn
		    (setq *indent* (caar c-exp-indent))
		    (indent-macro-reject-left)
		)
		;; Different indentation or complex state
		(progn
		    (rplacd c-exp-indent nil)
		    (setq c-complex 0)
		)
	    )
	)
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Handle braces
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indreduce :stat
	;; If block finishes before current line, group as a statement
	(< (+ *ind-offset* *ind-length*) *ind-start*)
	((:obrace (not :obrace) :cbrace))
    )
    (indreduce :obrace
	;; If not in the first line
	(< *ind-offset* *ind-start*)
	;; If the opening { is the first non blank char in the line
	((:indent :obrace))
	(setq *indent* (offset-indentation (+ *ind-offset* *ind-length*)))

	;; XXX This may be the starting brace of a switch
	(setq c-case-flag nil)
	(indent-macro-reject-left)
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Labels
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; XXX this frequently doesn't do what is expected, should redefine
    ;; some rules, as it frequently will dedent while typing something
    ;; like  test ? exp1 : exp2
    ;;                   ^ dedents here because it reduces everything
    ;;			   before ':' to a single :expression token.
    (indreduce :label
	t
	((:indent :expression :collon :eol))
	(when (and *label-dedent* (>= *ind-offset* *ind-start*))
	    (setq
		*indent*
		(- (offset-indentation *ind-offset* :resolve t) *base-indent*)
	    )
	    (indent-macro-reject-left)
	)
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Handle if
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indreduce :if
	t
	((:c-if :parens)
	)
	(incf c-complex)
    )

    (indreduce :else
	t
	((:c-else))
	(incf c-complex)
    )

    ;; Join
    (indreduce :else-if
	t
	((:else :if)
	 (:else :eol :indent :if)
	)
	(incf c-complex)
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Handle for
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Join with the parentheses
    (indreduce :for
	t
	((:c-for :parens)
	)
	(incf c-complex)
    )
    ;; Before current line, simplify
    (indreduce :stat
	(< (+ *ind-offset* *ind-length*) *ind-point*)
	((:for :stat)
	)
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Handle while and do
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indreduce :while
	t
	((:c-while :parens)
	)
	(incf c-complex)
    )
    (indreduce :stat
	t
	((:do :stat :while)
	 (:while :stat)
	)
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Handle switch
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indinit			c-case-flag)

    (indreduce :switch
	t
	((:c-switch :parens)
	)
    )
    ;; Transform in a statement
    (indreduce :stat
	(< (+ *ind-offset* *ind-length*) *ind-start*)
	((:switch :stat)
	 ;; Do it now or some rule may stop parsing, and calculate
	 ;; a wrong indentation for nested switches
	 (:switch :eol :indent :stat)
	)
    )
    ;; An open switch
    (indreduce :obrace
	(and
	    (<= c-braces 0)
	    (> *ind-start* *ind-offset*)
	)
	((:indent :switch :obrace)
	)
	(setq
	    *indent* (offset-indentation *ind-offset* :resolve t)
	    c-case-flag nil
	)
	(indent-macro-reject-left)
    )
    (indreduce :obrace
	(and
	    (<= c-braces 0)
	    (> *ind-start* *ind-offset*)
	)
	((:indent :switch :eol :indent :obrace)
	)
	(setq
	    *indent* (- (offset-indentation *ind-offset* :resolve t) *base-indent*)
	    c-case-flag nil
	)
	(and *brace-indent* (incf *indent* *base-indent*))
	(indent-macro-reject-left)
    )
    ;; Before current line
    (indreduce :case
	(and
	    (or
		(not *case-indent*)
		(prog1 c-case-flag (setq c-case-flag t))
	    )
	    (<= c-braces 0)
	    (< *ind-offset* *ind-start*)
	)
	((:indent :case)
	)
	(setq
	    *indent* (offset-indentation *ind-offset* :resolve t)
	    c-case-flag nil
	)
	(indent-macro-reject-left)
    )
    (indreduce :case
	t
	((:c-case :expression :collon)
	 (:c-default :collon)
	 ;; Assume that it is yet being edited, or adjusting indentation
	 (:c-case)
	 (:c-default)
	)
	(and (>= *ind-offset* *ind-start*)
	    (incf c-cases)
	)
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Handle parentheses and brackets
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Reduce matches
    (indreduce :parens
	t
	((:oparen (not :oparen) :cparen))
	(when
	    (and
		(< *ind-offset* *ind-start*)
		(> (+ *ind-offset* *ind-length*) *ind-start*)
	    )
	    (setq *indent* (1+ (offset-indentation *ind-offset* :align t)))
	    (indent-macro-reject-left)
	)
    )
    (indreduce :bracks
	t
	((:obrack (not :obrack) :cbrack))
	(when
	    (and
		(< *ind-offset* *ind-start*)
		(> (+ *ind-offset* *ind-length*) *ind-start*)
	    )
	    (setq *indent* (1+ (offset-indentation *ind-offset* :align t)))
	    (indent-macro-reject-left)
	)
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Assuming previous lines have correct indentation, this allows
    ;; resolving the indentation fastly
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Line ended with an open brace
    (indreduce :obrace
	(< *ind-offset* *ind-start*)
	((:indent (or :for :while :if :else-if :else :do) :obrace)
	)
	(setq *indent* (offset-indentation *ind-offset* :resolve t))
	(indent-macro-reject-left)
    )
    ;; Adjust indentation level if current line starts with an open brace
    (indreduce nil
	(< *ind-offset* *ind-start* (+ *ind-offset* *ind-length*))
	 ;; Just set initial indentation
	((:indent (or :for :while :if :else-if :else :do) :eol :indent :obrace)
	)
	(setq
	    *indent*
	    (- (offset-indentation *ind-offset* :resolve t) *base-indent*)
	)
	(and *brace-indent* (incf *indent* *base-indent*))
	(indent-macro-reject-left)
    )
    ;; Previous rule failed, current line does not start with an open brace
    (indreduce :flow
	;; first statement is in current line
	(and
	    (<= c-braces 0)
	    (> (+ *ind-offset* *ind-length*) *ind-start* *ind-offset*)
	)
	((:indent (or :for :while :if :else-if :else :do) :eol :indent)
	)
	(setq *indent* (offset-indentation *ind-offset* :resolve t))
	(indent-macro-reject-left)
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Simplify, remove old (:eol :indent)
    ;; This must be the last rule, to avoid not matching the
    ;; rules for fast calculation of indentation above
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indreduce nil
	(> *ind-offset* c-prev-offset)
	((:eol :indent))
    )


    (indinit			(c-flow 0))

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; If
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indinit			c-if-flow)
    (indresolve :if
	(and (< *ind-offset* *ind-start*)
	    (push c-flow c-if-flow)
	    (incf *indent* *base-indent*)
	    (incf c-flow)
	)
    )
    (indresolve (:else-if :else)
	(when c-if-flow
	    (while (< c-flow (car c-if-flow))
		(incf *indent* *base-indent*)
		(incf c-flow)
	    )
	    (or (eq *ind-token* :else-if) (pop c-if-flow))
	)
	(and (< *ind-offset* *ind-start*)
	    (incf *indent* *base-indent*)
	    (incf c-flow)
	)
    )


    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; For/while/do
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indinit			c-do-flow)
    (indresolve (:for :while :do)
	(if (eq *ind-token* :do)
	    (and (< *ind-offset* *ind-start*) (push c-flow c-do-flow))
	    (when (and c-do-flow (eq *ind-token* :while))
		(while (< c-flow (car c-do-flow))
		    (incf *indent* *base-indent*)
		    (incf c-flow)
		)
		(pop c-do-flow)
	    )
	)
	(and (< *ind-offset* *ind-start*)
	    (incf *indent* *base-indent*)
	    (incf c-flow)
	)
    )


    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Switch
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indresolve :switch
	(setq c-case-flag nil)
    )
    (indresolve (:case :c-case)
	(if (< *ind-offset* *ind-start*)
	    (or c-case-flag
		(setq
		    *indent*
		    (+ (offset-indentation *ind-offset* :resolve t)
			*base-indent*
		    )
		)
	    )
	    (if c-case-flag
		(and (= (decf c-cases) 0)
		    (decf *indent* *base-indent*)
		)
		(or *case-indent*
		    (decf *indent* *base-indent*)
		)
	    )
	)
	(setq c-case-flag t)
    )


    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Braces/flow control
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indresolve :flow
	(incf *indent* *base-indent*)
    )
    (indresolve :obrace
	(and (< *ind-offset* *ind-start*)
	    (incf *indent* *base-indent*)
	)
    )
    (indresolve :cbrace
	(decf *indent* *base-indent*)
	(and *case-indent* c-case-flag
	    (decf *indent* *base-indent*)
	    (setq c-case-flag nil)
	)
	(and (not *offset*) (>= *ind-offset* *ind-start*)
	    (setq *offset* *ind-offset*)
	)
    )


    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Statements
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indresolve :stat
	(when (< *ind-offset* *ind-start*)
	    (while (> c-flow 0)
		(setq
		    *indent*	(- *indent* *base-indent*)
		    c-flow	(1- c-flow)
		)
	    )
	)
	(and
	    *cont-indent*
	    (< *ind-offset* *ind-start*)
	    (> (+ *ind-offset* *ind-length*) *ind-start*)
	    (incf *indent* *base-indent*)
	)
    )

    (indresolve :expression
	(and
	    *cont-indent*
	    (zerop c-bra)
	    (> *indent* 0)
	    (< *ind-offset* *ind-start*)
	    (> (+ *ind-offset* *ind-length*) *ind-start*)
	    (incf *indent* *base-indent*)
	)
    )

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Open
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (indresolve (:oparen :obrack)
	(and (< *ind-offset* *ind-start*)
	    (setq *indent* (1+ (offset-indentation *ind-offset* :align t)))
	)
    )
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Find a "good" offset to start parsing backwards, so that it should
;; always generate the same results.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun c-offset-indent (&aux char (point (point)))
    ;; Skip spaces forward
    (while (member (setq char (char-after point)) indent-spaces)
	(incf point)
    )
    (or (characterp char) (return-from c-offset-indent point))

    ;;	Skip word chars
    (when (alphanumericp char)
	(while (and (setq char (char-after point)) (alphanumericp char))
	    (incf point)
	)
	(or (characterp char) (return-from c-offset-indent point))

	;; Skip spaces forward
	(while (member (setq char (char-after point)) indent-spaces)
	    (incf point)
	)
	(or (characterp char) (return-from c-offset-indent point))
    )

    ;; don't include " or ' to avoid parsing strings "inverted"
    (if (member char '(#\Newline #\" #\')) point (1+ point))
)
(compile 'c-offset-indent)

(defun c-should-indent (options)
    (when (hash-table-p options)
	;; check if previous line has extra spaces
	(and (gethash :trim-blank-lines options)
	    (indent-clear-empty-line)
	)

	;; indentation disabled?
	(and (gethash :disable-indent options)
	    (return-from c-should-indent)
	)

	(let*
	    (
	    (point (point))
	    (start (scan point :eol :left))
	    (char (char-before point))
	    offset
	    match
	    text
	    )

	    ;; at the start of an empty file
	    (or (characterp char)
		(return-from c-should-indent)
	    )

	    ;; if at bol and should indent only when starting a line
	    (and (gethash :only-newline-indent options)
		(return-from c-should-indent (= point start))
	    )

	    (and
		(char= char #\;)
		(gethash :newline-after-semi options)
		(return-from c-should-indent t)
	    )

	    ;; if one of these was typed, must check indentation
	    (and (member char '(#\{ #\} #\: #\] #\) #\#))
		(return-from c-should-indent t)
	    )

	    ;; at the start of a line
	    (and (= point start)
		(return-from c-should-indent (gethash :newline-indent options))
	    )

	    ;; if first character
	    (and (= point (1+ start))
		(return-from c-should-indent t)
	    )

	    ;; check if is the first non-blank character in a new line
	    (when
		(and
		    (gethash :cont-indent options)
		    (= point (scan point :eol :right))
		    (alphanumericp char)
		)
		(setq offset (1- point))
		(while
		    (and
			(> offset start)
			(member (char-before offset) indent-spaces)
		    )
		    (decf offset)
		)
		;; line has only one character with possible spaces before it
		(and (<= offset start)
		    (return-from c-should-indent t)
		)
	    )

	    ;; check for keywords that change indentation
	    (when (alphanumericp char)
		(setq offset (1- point))
		(while
		    (and
			(alphanumericp (char-before offset))
			(> offset start)
		    )
		    (decf offset)
		)
		(setq
		    text	(read-text offset (- point offset))
		    match	(re-exec #.(re-comp "(case|else|while)\\w?\\>")
				    text)
		)
		(and
		    (consp match)
		    (return-from c-should-indent (<= (- (caar match) offset) 2))
		)
	    )
	)
    )
    ;; Should not indent
    nil
)
(compile 'c-should-indent)


(defun c-indent-check (syntax syntable options
		       &aux start point char left brace change)
    (setq
	point	(point)
	char	(char-before point)
	left	point
	brace	(member char '(#\{ #\}))
    )

    (when
	(and brace (gethash :newline-before-brace options))
	(setq start (scan point :eol :left))
	(while
	    (and
		(> (decf left) start)
		(member (char-before left) indent-spaces)
	    )
	    ;; skip blanks
	)
	(when (> left start)
	    (replace-text left left (string #\Newline))
	    (c-indent syntax syntable)
	    (setq change t)
	)
    )

    (when
	(or
	    (and brace (not change) (gethash :newline-after-brace options))
	    (and (char= char #\;) (gethash :newline-after-semi options))
	)
	(setq left (point))
	(replace-text left left (string #\Newline))
	(goto-char (1+ left))
	(c-indent syntax syntable)
    )
)

(defun c-indent (syntax syntable)
    (let*
	(
	(options (syntax-options syntax))
	*base-indent*
	*brace-indent*
	*case-indent*
	*label-dedent*
	*cont-indent*
	)

	(or (c-should-indent options) (return-from c-indent))

	(setq
	    *base-indent*	(gethash :indentation options 4)
	    *brace-indent*	(gethash :brace-indent options nil)
	    *case-indent*	(gethash :case-indent options t)
	    *label-dedent*	(gethash :label-dedent options t)
	    *cont-indent*	(gethash :cont-indent options t)
	)

	(indent-macro
	    *c-mode-indent*
	    (c-offset-indent)
	    (gethash :emulate-tabs options)
	)

	(c-indent-check syntax syntable options)
    )
)
(compile 'c-indent)

(defsyntax *c-mode* :main nil #'c-indent *c-mode-options*
    ;;  All recognized C keywords.
    (syntoken
	(string-concat
	    "\\<("
	    "asm|auto|break|case|catch|char|class|const|continue|default|"
	    "delete|do|double|else|enum|extern|float|for|friend|goto|if|"
	    "inline|int|long|new|operator|private|protected|public|register|"
	    "return|short|signed|sizeof|static|struct|switch|template|this|"
	    "throw|try|typedef|union|unsigned|virtual|void|volatile|while"
	    ")\\>")
	:property *prop-keyword*)

    ;; Numbers, this is optional, comment this rule if xedit is
    ;; too slow to load c files.
    (syntoken
	(string-concat
	    "\\<("
	    ;; Integers
	    "(\\d+|0x\\x+)(u|ul|ull|l|ll|lu|llu)?|"
	    ;; Floats
	    "\\d+\\.?\\d*(e[+-]?\\d+)?[lf]?"
	    ")\\>")
	:icase t
	:property *prop-number*
    )

    ;; String start rule.
    (syntoken "\"" :nospec t :begin :string :contained t)

    ;; Character start rule.
    (syntoken "'" :nospec t :begin :character :contained t)

    ;; Preprocessor start rule.
    (syntoken "^\\s*#\\s*\\w+" :begin :preprocessor :contained t)

    ;; Comment start rule.
    (syntoken "/*" :nospec t :begin :comment :contained t)

    ;; C++ style comments.
    (syntoken "//.*" :property *prop-comment*)

    ;; Punctuation, this is also optional, comment this rule if xedit is
    ;; too slow to load c files.
    (syntoken "[][(){}/*+:;=<>,&.!%|^~?-][][(){}*+:;=<>,&.!%|^~?-]?"
	:property *prop-punctuation*)


    ;; Rules for comments.
    (syntable :comment *prop-comment* #'default-indent
	;; Match nested comments as an error.
	(syntoken "/*" :nospec t :property *prop-error*)

	(syntoken "XXX|TODO|FIXME" :property *prop-annotation*)

	;;  Rule to finish a comment.
	(syntoken "*/" :nospec t :switch -1)
    )

    ;; Rules for strings.
    (syntable :string *prop-string* #'default-indent
	;; Ignore escaped characters, this includes \".
	(syntoken "\\\\.")

	;; Match, most, printf arguments.
	(syntoken "%%|%([+-]?\\d+)?(l?[deEfgiouxX]|[cdeEfgiopsuxX])"
	    :property *prop-format*)

	;; Ignore continuation in the next line.
	(syntoken "\\\\$")

	;; Rule to finish a string.
	(syntoken "\"" :nospec t :switch -1)

	;; Don't allow strings continuing in the next line.
	(syntoken ".?$" :begin :error)
    )

    ;; Rules for characters.
    (syntable :character *prop-constant* nil
	;; Ignore escaped characters, this includes \'.
	(syntoken "\\\\.")

	;; Ignore continuation in the next line.
	(syntoken "\\\\$")

	;; Rule to finish a character constant.
	(syntoken "'" :nospec t :switch -1)

	;; Don't allow constants continuing in the next line.
	(syntoken ".?$" :begin :error)
    )

    ;;  Rules for preprocessor.
    (syntable :preprocessor *prop-preprocessor* #'default-indent
	;;  Preprocessor includes comments.
	(syntoken "/*" :nospec t :begin :comment :contained t)

	;;  Ignore lines finishing with a backslash.
	(syntoken "\\\\$")

	;; Return to previous state if end of line found.
	(syntoken ".?$" :switch -1)
    )

    (syntable :error *prop-error* nil
	(syntoken "^.*$" :switch -2)
    )

    ;;  You may also want to comment this rule if the parsing is
    ;; noticeably slow.
    (syntoken "\\c" :property *prop-control*)
)
