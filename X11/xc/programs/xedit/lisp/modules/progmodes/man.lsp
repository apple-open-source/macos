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
;; $XFree86: xc/programs/xedit/lisp/modules/progmodes/man.lsp,v 1.2 2002/10/06 17:11:48 paulo Exp $
;;

(require "syntax")
(in-package "XEDIT")

(defsynprop *prop-man-b*
    "b"
    :font	"*courier-bold-r*12*"
    :foreground	"gray12"
)

(defsynprop *prop-man-i*
    "i"
    :font	"*courier-medium-o*12*"
    :foreground	"black"
)

(defsynprop *prop-man-bi*
    "bi"
    :font	"*courier-bold-o*12*"
    :foreground	"gray20"
)

(defsynprop *prop-man-th*
    "th"
    :font	"-*-courier-*-*-*-*-18-*-*-*-*-*-*-1"
    :foreground	"Red3"
)

(defsynprop *prop-man-sh*
    "sh"
    :font	"-*-courier-*-*-*-*-14-*-*-*-*-*-*-1"
    :foreground	"OrangeRed3"
)

(defsynprop *prop-man-ss*
    "ss"
    :font	"-*-courier-*-*-*-*-12-*-*-*-*-*-*-1"
    :foreground	"Gold4"
)

(defsynprop *prop-man-escape*
    "escape"
    :font	"*lucidatypewriter-medium-r*12*"
    :foreground	"RoyalBlue4"
)

(defsynprop *prop-man-string*
    "string"
    :font	"*lucidatypewriter-bold-r*12*"
    :foreground	"RoyalBlue3"
;    :underline	t
)

(defmacro man-syntoken (pattern)
    `(syntoken (string-concat "^\\.(" ,pattern ")(\\s+|$)")
	:icase t
;	:contained t
	:property *prop-preprocessor*
	:begin (intern (string ,pattern) 'keyword)))

(defmacro man-syntable (pattern property)
    `(syntable (intern (string ,pattern) 'keyword) ,property nil
	(syntoken "$" :switch -1)
	(synaugment :extras)))


(defsyntax *man-mode* :main nil nil nil
    (syntoken "^\\.\\\\\".*"
	:property *prop-comment*)

    (man-syntoken "b|br|nm")
    (man-syntable "b|br|nm" *prop-man-b*)

    (man-syntoken "i|ir|ri|ip")
    (man-syntable "i|ir|ri|ip" *prop-man-i*)

    (man-syntoken "th|dt")
    (man-syntable "th|dt" *prop-man-th*)

    (man-syntoken "sh")
    (man-syntable "sh" *prop-man-sh*)

    (man-syntoken "ss")
    (man-syntable "ss" *prop-man-ss*)

    (man-syntoken "bi")
    (man-syntable "bi" *prop-man-bi*)

    ;; Anything not matched...
    (syntoken "^\\.[a-z][a-z](\\s+|$)"
	:icase t
	:property *prop-preprocessor*)

    (syntable :extras nil nil
	(syntoken "\\<__\\l+__\\>"
	    :property *prop-constant*)
	(syntoken "\\\\fB"
	    :property *prop-preprocessor*
	    :begin :b)
	(syntoken "\\\\fI"
	    :property *prop-preprocessor*
	    :begin :i)
	(syntoken "\\\\f\\u"
	    :property *prop-preprocessor*)

	(syntoken "\\\\\\*?."
	    :property *prop-man-escape*)

	(syntoken "\""
	    :property *prop-man-string*)

	(syntable :i *prop-man-i* nil
	    (syntoken "$"
		:switch :main)
	    (syntoken "\\\\f\\u"
		:property *prop-preprocessor*
		:switch -1)
	)
	(syntable :b *prop-man-b* nil
	    (syntoken "$"
		:switch :main)
	    (syntoken "\\\\f\\u"
		:property *prop-preprocessor*
		:switch -1)
	)
    )

    (synaugment :extras)
)
