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
;; $XFree86: xc/programs/xedit/lisp/modules/progmodes/make.lsp,v 1.2 2002/10/06 17:11:48 paulo Exp $
;;

(require "syntax")
(in-package "XEDIT")

(defsynprop *prop-shell*
    "shell"
    :font	"*courier-bold-r*12*"
    :foreground	"Red4"
)

(defsynprop *prop-variable*
    "variable"
    :font	"*courier-medium-r*12*"
    :foreground	"Red3"
)

;;  The syntax-highlight definition does not try to flag errors, just show
;; tabs in the start of lines for better visualization.
(defsynprop *prop-tabulation*
    "tabulation"
    :font	"*courier-medium-r*12*"
    :background	"Gray90"
)


(defsyntax *make-mode* :main nil nil nil
    (syntoken "^\\t+" :property *prop-tabulation*)

    (syntoken "^\\.\\w+" :property *prop-keyword*)

    (syntoken "$("
	:nospec t
	:begin :shell
	:property *prop-shell*)

    (syntoken "[][(){};$<=>&@/\\,.:~!|*?'`+-]"
	:property *prop-shell*)

    ;;  Preprocessor start rule.
    (syntoken "#.*"
	:property *prop-comment*)

    ;;  String start rule.
    (syntoken "\""
	:begin :string
	:nospec t
	:contained t)

    ;;  Quoted string start rule.
    (syntoken "\\\""
	:begin :quoted-string
	:nospec t
	:contained t)

    (syntable :shell *prop-variable* nil
	(syntoken ")"
	    :nospec t
	    :property *prop-shell*
	    :switch -1)
    )

    ;;  Rules for strings.
    (syntable :string *prop-string* nil

	;;  Ignore escaped characters, this includes \".
	(syntoken "\\\\.")

	;;  Ignore continuation in the next line.
	(syntoken "\\\\$")

	;;  Rule to finish a string.
	(syntoken "\""
	    :nospec t
	    :switch -1)

	;;  Don't allow strings continuing in the next line.
	(syntoken ".?$"
	    :begin :error)
    )

    ;;  Rules for quoted strings.
    (syntable :quoted-string *prop-constant* nil

	;;  Rule to finish the quoted string.
	(syntoken "\\\""
	    :nospec t
	    :switch -1)

	;;  Ignore escaped characters
	(syntoken "\\\\.")

	;;  Ignore continuation in the next line.
	(syntoken "\\\\$")

	;;  Don't allow strings continuing in the next line.
	(syntoken ".?$"
	    :begin :error)
    )

    (syntable :error *prop-error* nil
	(syntoken "^.*$"
	    :switch -2)
    )
)
