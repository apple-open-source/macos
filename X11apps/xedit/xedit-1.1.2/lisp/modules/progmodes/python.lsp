;; Copyright (c) 2008 Paulo Cesar Pereira de Andrade
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

(require "syntax")
(require "indent")
(in-package "XEDIT")

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defsynprop *prop-indent*
  "indent"
  :font		"*courier-medium-r*-12-*"
  :background	"Gray92")

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defsynoptions *python-DEFAULT-options*
  ;; Positive number. Basic indentation
  (:indentation		.	4)

  ;; Boolean. Move cursor to the indent column after pressing <Enter>?
  (:newline-indent	.	t)

  ;; Boolean. Set to T if tabs shouldn't be used to fill indentation.
  (:emulate-tabs	.	t)

  ;; Boolean. Only calculate indentation after pressing <Enter>?
  ;;		This may be useful if the parser does not always
  ;;		do what the user expects...
  (:only-newline-indent	.	nil)

  ;; Boolean. Remove extra spaces from previous line.
  ;;		This should default to T when newline-indent is not NIL.
  (:trim-blank-lines	.	nil)

  ;; Boolean. If this hash-table entry is set, no indentation is done.
  ;;		Useful to temporarily disable indentation.
  (:disable-indent	.	nil))


;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
;;   Not doing "special" indentation of multiline ( because it is attempting
;; to do a "smart" indentation and usually don't read more then one line
;; back to resolve indentation.
;;   Code for multiline { and [, usually declaring vector/hash like variables
;; should be working properly.
;;   Note that the indent lisp hook is only run on character additions, so
;; it doesn't do a "smart" tabbing when pressing backspace, but it will
;; properly align to the "closest tab stop" when typping a character.
(defindent *python-mode-indent* :main
  ;; this must be the first token
  (indtoken "^\\s*" :indent
    :code (or *offset* (setq *offset* (+ *ind-offset* *ind-length*))))

  ;; ignore comments
  (indtoken "#.*$" nil)

  (indtoken ":" :collon :nospec t)

  ;; don't directly match {}, [], () strings, and :
  (indtoken "[a-zA-Z0-9+*/%^&<>=.,|!~-]+" :expression)

  ;; if in the same line, reduce now, as delimiters are identical
  (indtoken "'([^\\']|\\\\.)*'" :expression)
  (indtoken "\"([^\\\"]|\\\\.)*\"" :expression)
  ;; otherwise, use a table
  (indtoken "\"" :cstring :nospec t :begin :string)
  (indtoken "'" :cconstant :nospec t :begin :constant)
  (indtoken "\"\"\"" :cstring3 :nospec t :begin :string3)
  (indtoken "'''" :cconstant :nospec t :begin :constant3)

  (indinit (braces 0))
  (indtoken "}" :cbrace :nospec t :code (incf braces))
  (indtoken "{" :obrace	:nospec t :code (decf braces))
  (indtoken ")" :cparen :nospec t :code (incf braces))
  (indtoken "(" :oparen	:nospec t :code (decf braces))
  (indtoken "]" :cbrack	:nospec t :code (incf braces))
  (indtoken "[" :obrack	:nospec t :code (decf braces))

  ;; This must be the last token
  (indtoken "$"		:eol)

  (indtable :string
    ;; Ignore escaped characters
    (indtoken "\\." nil)
    ;; Return to the toplevel when the start of the string is found
    (indtoken "\"" :ostring :nospec t :switch -1))
  (indtable :constant
    (indtoken "\\." nil)
    (indtoken "'" :oconstant :nospec t :switch -1))

  (indtable :string3
    (indtoken "\"\"\"" :ostring3 :nospec t :switch -1))
  (indtable :constant3
    (indtoken "'''" :oconstant3 :nospec t :switch -1))

  ;; Reduce what isn't reduced in regex pattern match
  (indreduce :expression
    t
    ((:expression :expression)
      ;; multiline strings
      (:ostring (not :ostring) :cstring)
      (:oconstant (not :oconstant) :cconstant)
      (:ostring3 (not :ostring3) :cstring3)
      (:oconstant3 (not :oconstant3) :cconstant3)
      ;; braces, parenthesis and brackets
      (:obrace (not :obrace) :cbrace)
      (:oparen (not :oparen) :cparen)
      (:obrack (not :obrack) :cbrack)))

  ;; This should be the most common exit point;
  ;; just copy previous line indentation.
  (indreduce :align
    (< *ind-offset* *ind-start*)
    ((:indent :eol)
      (:indent :expression :eol))
    (setq *indent* (offset-indentation *offset* :resolve t))

    ;; If cursor is not in an indentation tab, assume user is trying to align
    ;; to another block, and just use the resolve code to round it down
    (unless (/= (mod *indent* *base-indent*) 0)
      ;; else use "previous-line" indentation.
      (setq *indent* (offset-indentation *ind-offset* :resolve t)))
    (indent-macro-reject-left))

  ;; This should be second most common exit point;
  ;; add one indentation level.
  (indreduce :align
    (< *ind-offset* *ind-start*)
    ((:indent :expression :collon :eol))
    (setq *indent* (+ *base-indent* (offset-indentation *ind-offset* :resolve t)))
    (indent-macro-reject-left))

  (indresolve :align
    (setq *indent* (- *indent* (mod *indent* *base-indent*))))

  ;; Calculate special indentation for [ and {
  (indresolve (:obrack :obrace)
    (and
      (< *ind-offset* *ind-start*)
      (setq *indent* (+ *base-indent*
		       (offset-indentation *ind-offset* :resolve t)))))
  (indresolve (:cbrack :cbrace)
    (setq *indent* (- (offset-indentation *ind-offset* :resolve t)
		     (if (>= *ind-offset* *ind-start*)
		       *base-indent* 0))))
)


;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defun python-offset-indent (&aux char (point (point)))
  ;; Skip spaces forward
  (while (member (setq char (char-after point)) indent-spaces)
    (incf point))
  point)

(compile 'python-offset-indent)

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defun python-should-indent (options &aux point start end offset)
  (when (hash-table-p options)
    ;; check if previous line has extra spaces
    (and (gethash :trim-blank-lines options)
      (indent-clear-empty-line))

    ;; indentation disabled?
    (and (gethash :disable-indent options)
      (return-from python-should-indent))

    (setq
      point	(point)
      start	(scan point :eol :left)
      end	(scan point :eol :right))

    ;; if at bol and should indent only when starting a line
    (and (gethash :only-newline-indent options)
      (return-from python-should-indent (= point start)))

    ;; at the start of a line
    (and (= point start)
      (return-from python-should-indent (gethash :newline-indent options)))

    ;; if first character
    (and (= point (1+ start))
      (return-from python-should-indent t))

    (setq offset start)
    (while (and
	     (< offset end)
	     (member (char-after offset) indent-spaces))
      (incf offset))

    ;; cursor is at first character in line, with possible spaces before it
    (return-from python-should-indent (or (= offset end) (= offset (1- point))))
  )
  ;; Should not indent
  nil)

(compile 'python-should-indent)

;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defun python-indent (syntax syntable)
  (let*
    ((options (syntax-options syntax))
      *base-indent*)

    (or (python-should-indent options) (return-from python-indent))
    (setq
      *base-indent* (gethash :indentation options 4))

    (indent-macro
      *python-mode-indent*
      (python-offset-indent)
      (gethash :emulate-tabs options))))

(compile 'python-indent)


;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defvar *python-mode-options* *python-DEFAULT-options*)


;=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
(defsyntax *python-mode* :main nil #'python-indent *python-mode-options*
  ;; keywords
  (syntoken
    (string-concat
      "\\<("
      "and|break|class|continue|def|del|enumerate|except|False|for|"
      "elif|else|if|in|is|len|None|not|or|pass|print|raise|range|"
      "return|self|True|try|type|while|yield"
      ")\\>")
    :property *prop-keyword*)

  (syntoken "^\\s+" :property *prop-indent*)

  ;; preprocessor like
  (syntoken
    (string-concat
      "\\<("
      "from|import"
      ")\\>")
    :property *prop-preprocessor*)

  ;; namespaces/accessors
  (syntoken "(\\w+\\.)+" :property *prop-preprocessor*)

  ;; more preprocessor like
  (syntoken "\\<__[a-zA-Z0-9]+__\\>" :property *prop-keyword*)

  ;; numbers
  (syntoken
    (string-concat
      "\\<("
      ;; Integers
      "(\\d+|0x\\x+)L?|"
      ;; Floats
      "\\d+\\.?\\d*(e[+-]?\\d+)?"
      ")\\>")
    :icase t
    :property *prop-number*)

  ;; comments
  (syntoken "#.*" :property *prop-comment*)

  ;; punctuation
  (syntoken "[][(){}+*/%^&<>=.,|!~:-]+" :property *prop-punctuation*)

  ;; constant or constant like
  (syntoken "'" :nospec t :property *prop-constant* :begin :constant)
  (syntoken "'''" :nospec t :property *prop-constant* :begin :constant3)

  ;; strings
  (syntoken "\"" :nospec t :property *prop-string* :begin :string)
  (syntoken "\"\"\"" :nospec t :property *prop-string* :begin :string3)

  (syntable :constant *prop-constant* nil
    (syntoken "\\\\.")
    (syntoken "'" :nospec t :switch -1))
  (syntable :constant3 *prop-constant* nil
    (syntoken "'''" :nospec t :switch -1))
  (syntable :string *prop-string* nil
    (syntoken "\\\\.")
    (syntoken "\"" :nospec t :switch -1))
  (syntable :string3 *prop-string* nil
    (syntoken "\"\"\"" :nospec t :switch -1))
)
