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
;; $XFree86: xc/programs/xedit/lisp/modules/progmodes/xrdb.lsp,v 1.2 2002/10/06 17:11:48 paulo Exp $
;;

(require "syntax")
(in-package "XEDIT")

(defsynprop *prop-xrdb-comment*
    "xrdb-comment"
    :font	"*courier-medium-o*-12-*"
    :foreground	"sienna"
)

(defsynprop *prop-xrdb-special*
    "format"
    :font	"*lucidatypewriter-medium-r*-12-*"
    :foreground	"RoyalBlue4"
)

(defsynprop *prop-xrdb-punctuation*
    "punctuation"
    :font	"-*-courier-bold-r-*-*-14-*-*-*-*-*-*-1"
    :foreground	"OrangeRed4"
)

(defsyntax *xrdb-mode* :main nil nil nil
    (syntoken "^\\s*!.*"
	:property *prop-xrdb-comment*)
    (syntoken "^\\s*#.*"
	:property *prop-preprocessor*)
    (syntoken "\\*|\\.|\\?"
	:property *prop-xrdb-punctuation*
	:begin :resource)
    (syntoken "."
	:nospec t
	:begin :resource)

    ;; Extra comments
    (syntoken "/*" :nospec t :begin :comment :contained t)
    (syntable :comment *prop-comment* nil
	(syntoken "/*" :nospec t :property *prop-error*)
	;;  Rule to finish a comment.
	(syntoken "*/" :nospec t :switch -1)
    )

    (syntable :resource nil nil
	(syntoken "\\*|\\.|\\?" :property *prop-xrdb-punctuation*)
	(syntoken ":\\s*" :property *prop-xrdb-punctuation* :begin :value)
    )

    (syntable :value *prop-string* nil
	(syntoken "\\\\$" :property *prop-constant*)


	;; If the pattern ends at a newline, must switch to the previous state.
	;; Not sure yet how to better handle this. The parser does not detect
	;; eol because it is a match to the empty string. A possible hack
	;; would be to check if the pattern string ends in a "$", but probably
	;; better in this case to have a syntoken option, to tell the parser
	;; an eol may exist.
	(syntoken
	    (string-concat
		"("
		"\\d+|"				;; numbers
		"(#\\x+|rgb:\\x+/\\x+/\\x+)|"	;; color spec
		"#\\w+"				;; translation table
		")$")
	    :property *prop-xrdb-special* :switch -2)
	(syntoken "(\\\\n?|\")$"
	    :property *prop-constant* :switch -2)

	;; XXX Cut&paste of the above, only without the match to eol
	(syntoken
	    (string-concat
		"("
		"\\d+|"
		"(#\\x+|rgb:\\x+/\\x+/\\x+)|"
		"#\\w+"
		")")
	    :property *prop-xrdb-special*)
	(syntoken "(\\\\n?|\")"
	    :property *prop-constant*)

	(syntoken "/*" :nospec t :begin :comment :contained t)
	(syntoken ".?$" :switch -2)
    )
)
