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
;; $XFree86: xc/programs/xedit/lisp/modules/progmodes/html.lsp,v 1.2 2002/09/22 18:41:27 paulo Exp $
;;

(require "syntax")
(in-package "XEDIT")

#|
  This is not a validation tool for html.

  It is possible to, using macros generate all combinations of text attributes,
  to properly handle <b>...<i>...</i>...</b> etc, as well as generating macros
  to automatically closing tags, but for now this file was built to work as an
  experience with the syntax highlight code.
|#

(defsynprop *prop-html-default*
    "default"
    :font	"-*-lucida-medium-r-*-*-14-*-*-*-*-*-*-1"
    :foreground	"Gray10")

(defsynprop *prop-html-bold*
    "bold"
    :font	"-*-lucida-bold-r-*-*-14-*-*-*-*-*-*-1"
    :foreground	"Gray15")

(defsynprop *prop-html-italic*
    "italic"
    :font	"-*-lucida-medium-i-*-*-14-*-*-*-*-*-*-1"
    :foreground	"Gray10")

(defsynprop *prop-html-pre*
    "pre"
    :font	"-*-courier-medium-r-*-*-14-*-*-*-*-*-*-1"
    :foreground	"Gray10")

(defsynprop *prop-html-link*
    "link"
    :font	"-*-lucida-medium-r-*-*-14-*-*-*-*-*-*-1"
    :foreground	"Blue"
    :underline "t")

(defsynprop *prop-html-small*
    "small"
    :font	"-*-lucida-medium-r-*-*-10-*-*-*-*-*-*-1"
    :foreground	"Gray10")

(defsynprop *prop-html-big*
    "big"
    :font	"-*-lucida-medium-r-*-*-20-*-*-*-*-*-*-1"
    :foreground	"Gray15")

(defsynprop *prop-html-name*
    "name"
    :font	"-*-lucida-bold-r-*-*-14-*-*-*-*-*-*-1"
    :foreground	"Black"
    :background "rgb:e/f/e")

(defsynprop *prop-html-h1*
    "h1"
    :font	"-*-lucida-bold-r-*-*-20-*-*-*-*-*-*-1"
    :foreground	"Gray15")

(defsynprop *prop-html-h2*
    "h2"
    :font	"-*-lucida-bold-r-*-*-17-*-*-*-*-*-*-1"
    :foreground	"Gray15")

(defsynprop *prop-html-h4*
    "h4"
    :font	"-*-lucida-bold-r-*-*-12-*-*-*-*-*-*-1"
    :foreground	"Gray15")

(defsynprop *prop-html-h5*
    "h5"
    :font	"-*-lucida-bold-r-*-*-10-*-*-*-*-*-*-1"
    :foreground	"Gray15")

(defsynprop *prop-html-li*
    "li"
    :font	"-*-lucida-bold-r-*-*-8-*-*-*-*-*-*-1"
    :foreground	"rgb:0/5/0"
    :underline	t)

(defsynprop *prop-html-hr*
    "hr"
    :font	"-*-courier-bold-r-*-*-12-*-*-*-*-*-*-1"
    :foreground	"rgb:0/5/0"
    :overstrike	t)

(defsynprop *prop-html-title*
    "title"
    :font	"-*-lucida-medium-r-*-*-14-*-*-*-*-*-*-1"
    :foreground	"Red3"
    :underline "t")

(defsynprop *prop-html-tag*
    "tag"
    :font	"-*-courier-medium-r-*-*-10-*-*-*-*-*-*-1"
    :foreground	"green4")

(defsynprop *prop-html-string*
    "string"
    :font	"-*-lucida-medium-r-*-*-10-*-*-*-*-*-*-1"
    :foreground	"RoyalBlue2")

(defsynprop *prop-html-comment*
    "comment"
    :font	"-*-courier-medium-o-*-*-10-*-*-*-*-*-*-1"
    :foreground	"SlateBlue3")

(defsynprop *prop-html-entity*
    "entity"
    :font	"-*-lucida-medium-r-*-*-12-*-*-*-*-*-*-1"
    :foreground	"Red4")

(defsynprop *prop-html-unknown*
    "unknown"
    :font	"-*-courier-bold-r-*-*-10-*-*-*-*-*-*-1"
    :foreground	"yellow"
    :background "red")

(defmacro html-syntoken (name)
    `(syntoken (string-concat "<" ,name "\\>")
	:icase t :contained t
	:begin (intern (string-concat ,name "$") 'keyword)))
(defmacro html-syntable (name property)
    `(let
	((label (intern (string-concat ,name "$") 'keyword))
	 (nested-label (intern (string (gensym)) 'keyword)))
	(syntable label *prop-html-tag* nil
	    (synaugment :generic-tag)
	    (syntoken ">" :nospec t :property *prop-html-tag* :begin nested-label)
	    (syntable nested-label ,property nil
		(syntoken (string-concat "</" ,name ">")
		    :icase t :nospec t :property *prop-html-tag* :switch -2)
		(syntoken (string-concat "</" ,name "\\s*$")
		    :icase t :contained t :begin :continued-end-tag)
		(synaugment :main)))))


(defsyntax *html-mode* :main *prop-html-default* nil nil
    (syntoken "<!--" :nospec t :contained t :begin :comment)
    (syntable :comment *prop-html-comment* nil
	(syntoken "-->" :nospec t :switch -1))
    (syntoken "&([a-zA-Z0-9_.-]+|#\\x\\x?);?" :property *prop-html-entity*)
    (syntoken "<li>" :nospec t :icase t :property *prop-html-li*)
    (syntoken "<hr>" :nospec t :icase t :property *prop-html-hr*)

    (syntoken "<img\\>" :icase t :contained t :begin :tag)
    (syntoken "<(p|br)>" :icase t :property *prop-html-tag*)

    ;; If in the toplevel, unbalanced!
    ;; XXX When adding new nested tables, don't forget to update this pattern.
    (syntoken
	(string-concat
	    "</("
	    "b|strong|i|em|address|pre|code|tt|small|big|a|span|div|"
	    "h1|h2|h3|h4|h5|title|font|ol|ul|dl|dt|dd|menu"
	    ")\\>")
	:icase t :property *prop-html-unknown* :begin :unbalanced)
    (syntable :unbalanced *prop-html-unknown* nil
	(syntoken ">" :nospec t :switch :main)
	(synaugment :generic-tag)
    )

    #||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
    ;; XXX ONLY add a rule for "html", "head" and "body" if you want to do a
    ;; more complete check for common errors. If you add those rules, it will
    ;; reparse the entire file at every character typed (unless there are
    ;; errors in which case the parser resets the state).
    ;; For visualization only that would be OK...
    ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||#

    (html-syntoken "b")
    (html-syntable "b" *prop-html-bold*)
    (html-syntoken "strong")
    (html-syntable "strong" *prop-html-bold*)

    (html-syntoken "i")
    (html-syntable "i" *prop-html-italic*)
    (html-syntoken "em")
    (html-syntable "em" *prop-html-italic*)
    (html-syntoken "address")
    (html-syntable "address" *prop-html-italic*)

    (html-syntoken "pre")
    (html-syntable "pre" *prop-html-pre*)
    (html-syntoken "code")
    (html-syntable "code" *prop-html-pre*)
    (html-syntoken "tt")
    (html-syntable "tt" *prop-html-pre*)

    (html-syntoken "small")
    (html-syntable "small" *prop-html-small*)

    (html-syntoken "big")
    (html-syntable "big" *prop-html-big*)

    ;; Cannot hack html-syntoken and html-syntable to handle this,
    ;; as the option to <a may be in the next line.
    (syntoken "<a\\>" :icase t :contained t :begin :a)
    (syntable :a *prop-html-tag* nil
	;; Tag is open
	(syntoken "\\<href\\>" :icase t :begin :a-href)
	(syntoken "\\<name\\>" :icase t :begin :a-name)
	(syntoken "<" :nospec t :property *prop-html-unknown* :switch -2)
	(synaugment :generic-tag)
	(syntoken ">" :nospec t :begin :a-generic-text)
	(syntable :a-href *prop-html-tag* nil
	    (syntoken ">" :nospec t :begin :a-href-text)
	    (synaugment :generic-tag)
	    (syntable :a-href-text *prop-html-link* nil
		(syntoken "</a>"
		    :icase t :nospec t :property *prop-html-tag* :switch -3)
		(syntoken "</a\\s*$" :icase t :begin :continued-nested-end-tag)
		(synaugment :main)
	    )
	)
	(syntable :a-name *prop-html-tag* nil
	    (syntoken ">" :nospec t :begin :a-name-text)
	    (synaugment :generic-tag)
	    (syntable :a-name-text *prop-html-name* nil
		(syntoken "</a>"
		    :icase t :nospec t :property *prop-html-tag* :switch -3)
		(syntoken "</a\\s*$" :icase t :begin :continued-nested-end-tag)
		(synaugment :main)
	    )
	)
	(syntable :a-generic-text nil nil
	    (syntoken "</a>"
		:icase t :nospec t :property *prop-html-tag* :switch -2)
	    (syntoken "<a/\\s$" :icase t :begin :continued-end-tag)
	    (synaugment :main)
	)
    )

    ;; Do nothing, just check start/end tags
    (html-syntoken "ol")
    (html-syntable "ol" nil)
    (html-syntoken "ul")
    (html-syntable "ul" nil)
    (html-syntoken "dl")
    (html-syntable "dl" nil)
    ;; Maybe <dt> and <dd> should be in a special table, to not require
    ;; and ending tag.
    ;; XXX Maybe should also add a table for <p>.
    (html-syntoken "dt")
    (html-syntable "dt" nil)
    (html-syntoken "dd")
    (html-syntable "dd" nil)

    (html-syntoken "span")
    (html-syntable "span" nil)
    (html-syntoken "div")
    (html-syntable "div" nil)
    (html-syntoken "menu")
    (html-syntable "menu" nil)

    (html-syntoken "h1")
    (html-syntable "h1" *prop-html-h1*)
    (html-syntoken "h2")
    (html-syntable "h2" *prop-html-h2*)
    (html-syntoken "h3")
    (html-syntable "h3" *prop-html-bold*)
    (html-syntoken "h4")
    (html-syntable "h4" *prop-html-h4*)
    (html-syntoken "h5")
    (html-syntable "h5" *prop-html-h5*)
    (html-syntoken "title")
    (html-syntable "title" *prop-html-title*)

    (html-syntoken "font")
    (html-syntable "font" *prop-control*)

    (syntoken "<" :nospec t :contained t :begin :tag)
    (syntable :generic-tag *prop-html-tag* nil
	(syntoken "\"" :nospec t :contained t :begin :string)
	(syntoken "<" :nospec t :property *prop-html-unknown*)
    )
    (syntable :tag *prop-html-tag* nil
	(syntoken ">" :nospec t :switch -1)
	(synaugment :generic-tag)
    )
	;; Tag ended in a newline, common practice...
    (syntable :continued-end-tag *prop-html-tag* nil
	(syntoken ">" :nospec t :switch -3)
	(synaugment :generic-tag)
    )
    (syntable :continued-nested-end-tag *prop-html-tag* nil
	(syntoken ">" :nospec t :switch -4)
	(synaugment :generic-tag)
    )

    (syntable :string *prop-html-string* nil
	(syntoken "\\\\.")
	(syntoken "\"" :nospec t :switch -1)
    )
)
