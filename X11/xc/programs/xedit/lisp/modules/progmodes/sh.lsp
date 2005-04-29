;;
;; Copyright (c) 2003 by The XFree86 Project, Inc.
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
;; $XFree86: xc/programs/xedit/lisp/modules/progmodes/sh.lsp,v 1.2 2004/01/08 21:29:13 paulo Exp $
;;

(require "syntax")
(require "indent")
(in-package "XEDIT")

(defsynprop *prop-escape*
    "escape"
    :font	"*lucidatypewriter-medium-r*-12-*"
    :foreground	"Red3")

(defsynprop *prop-variable*
    "variable"
    :font	"*lucidatypewriter-medium-r*-12-*"
    :foreground	"Gold4")

(defsynprop *prop-backslash*
    "backslash"
    :font	"*courier-bold-r*-12-*"
    :foreground	"green4")

;; XXX it would be interesting if "here-documents" could be parsed
;; just searching for "<<\s*EOF\\>" and then for "^EOF\\>" should
;; handle most cases, but would be a hack...
(defsyntax *sh-mode* :main nil #'default-indent nil
    ;; keywords and common commands/builtins
    (syntoken "\\<(if|then|elif|else|fi|case|in|esac|for|do|done|while|until|break|continue|eval|exit|exec|test|echo|cd|shift|local|return)\\>"
	:property *prop-keyword*)

    ; comments
    (syntoken "#.*$" :property *prop-comment*)

    ;; punctuation
    (syntoken "[][;:*?(){}<>&!|$#]+" :property *prop-punctuation*)

    ;; variable declaration
    (syntoken "\\w+=" :property *prop-preprocessor*)

    ;; numbers
    (syntoken "\\<\\d+\\>" :property *prop-number*)

    ;; escaped characters at toplevel
    (syntoken "\\\\." :property *prop-backslash*)

    ;; single quote
    (syntoken "'" :nospec t :contained t :begin :single)
    (syntable :single *prop-constant* nil
	;; do nothing, escaped characters
	(syntoken "\\\\.")
	(syntoken "'" :nospec t :switch -1)
    )

    ;; double quote
    (syntoken "\"" :nospec t :contained t :begin :double)
    (syntable :double *prop-string* #'default-indent
	;; escaped characters
	(syntoken "\\\\." :property *prop-backslash*)
	(syntoken "\"" :nospec t :switch -1)
	;; rule to start escape
	(syntoken "`" :nospec t :contained t :begin :escape)
	;; ignore single quote, required because escape is augmented
	(syntoken "'" :nospec t)
	(synaugment :escape :variable)
    )

    ;; escaped commands
    (syntoken "`" :nospec t :contained t :begin :escape)
    (syntable :escape *prop-escape* #'default-indent
	;; escaped characters
	(syntoken "\\\\." :property *prop-backslash*)
	(syntoken "`" :nospec t :switch -1)
	;; rule to start double quote inside escape
	(syntoken "\"" :nospec t :contained t :begin :double)
	;; rule to start single quote
	(syntoken "'" :nospec t :contained t :begin :single)
	(synaugment :double :variable)
    )

    (syntable :variable nil nil
	(syntoken "\\$\\w+" :property *prop-variable*)
    )
    (synaugment :variable)
)
