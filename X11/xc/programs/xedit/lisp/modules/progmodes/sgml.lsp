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
;; $XFree86: xc/programs/xedit/lisp/modules/progmodes/sgml.lsp,v 1.3 2004/01/08 21:29:12 paulo Exp $
;;

(require "syntax")
(in-package "XEDIT")

;; Default property the text is shown.
(defsynprop *prop-sgml-default*
    "default"
    :font	"-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-1"
    :foreground	"Gray10"
)

(defsynprop *prop-sgml-default-short*
    "default-short"
    :font	"-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-1"
    :foreground	"Gray10"
    :underline	t
)

;; Large font.
(defsynprop *prop-sgml-sect*
    "sect"
    :font	"-*-helvetica-bold-r-*-*-17-*-*-*-*-*-*-1"
    :foreground	"Gray20"
)

;; Monospaced property.
(defsynprop *prop-sgml-tt*
    "tt"
    :font	"-*-courier-medium-r-*-*-12-*-*-*-*-*-*-1"
    :foreground	"Black"
)

;; Italic property.
(defsynprop *prop-sgml-it*
    "it"
    :font	"-*-helvetica-medium-o-*-*-12-*-*-*-*-*-*-1"
    :foreground	"Black"
)

;; Bold font property.
(defsynprop *prop-sgml-bf*
    "bf"
    :font	"-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-1"
    :foreground	"Gray10"
)

;; Looks like a link...
(defsynprop *prop-sgml-link*
    "link"
    :font	"-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-1"
    :foreground	"blue"
    :underline	t
)

;; Monospaced, also looks like a link...
(defsynprop *prop-sgml-email*
    "email"
    :font	"-*-courier-medium-r-*-*-12-*-*-*-*-*-*-1"
    :foreground	"blue"
    :underline	t
)

;; Another monospaced property,
(defsynprop *prop-sgml-screen*
    "screen"
    :font	"-*-fixed-*-*-*-*-*-*-*-*-*-*-*-1"
    :foreground	"Gray10"
)

(defsynprop *prop-sgml-maybe-entity*
    "maybe-entity"
    :font	"*lucidatypewriter-medium-r*-12-*"
    :foreground	"VioletRed4"
    :background	"LightYellow"
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  The macros sgml-syntoken and sgml-syntable allows creating rules for
;; matching text inside tags in the format:
;;	<tag> or <tag arg=value> or <tag arg1=value ... argn=value>
;;		any-text
;;	</tag>
;;  The generated rules don't allow things like: < tag> or </tag >
;;
;;  This could also be done as a normal definition, with a starting rule like:
;;	"<(tag1|tag2|tag3)\\>"
;; and an ending rule like:
;;	"</(tag1|tag2|tag3)>"
;;  But is implemented in way that will fail on purpose for things like:
;;	<tag1>any text</tag3></tag1>
;;
;; NOTE: These definitions aren't cheap in the time required to process the
;;	file, and are just adaptations/tests with the syntax-highlight code,
;;	probably it is better to avoid using it in other syntax definitions.
;; NOTE2: It cannot be defined as a single macro because it is required to
;;	  generate 2 entries in the main SGML syntax highlight definition,
;;	  or, should generate the entire definition from a macro; you will
;;	  need to type the tag name twice, but shouldn't be a problem if
;;	  you are using sgml :-)
;; XXX: Maybe the syntax-highlight code could save the starting match and
;;	apply a regex generated at run-time to check for the ending tag,
;;	but this probably would make the parser too slow, better to have
;;	a specialized parser if that is required...
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro sgml-syntoken (name)
    `(syntoken (string-concat "<" ,name "\\>")
	:icase t
	:contained t
	:begin (intern (string-concat ,name "$") 'keyword))
)
(defmacro sgml-syntable (name property)
    `(let
	(
	(label (intern (string-concat ,name "$") 'keyword))
	(nested-label (intern (string (gensym)) 'keyword))
	)
	(syntable label *prop-preprocessor* nil
	    ;; tag is still open, process any options
	    (synaugment :generic-tag)
	    (syntoken ">"
		:nospec t
		:property *prop-preprocessor*
		:begin nested-label)
	    ;;	Generate a nested table that includes everything, and only
	    ;; returns when the closing tag is found.
	    (syntable nested-label ,property nil
		(syntoken (string-concat "</" ,name ">")
		    :icase t
		    :nospec t
		    :property *prop-preprocessor*
		    :switch -2)
		(synaugment :main)
	    )
	)
    )
)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Generate tokens for tags that don't require and ending tag.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro sgml-syntable-simple (name property)
    `(let
	(
	(label (intern (string-concat ,name "$") 'keyword))
	(nested-label (intern (string (gensym)) 'keyword))
	)
	(syntable label *prop-preprocessor* nil
	    ;; tag is still open, process any options
	    (synaugment :generic-tag)
	    (syntoken ">"
		:nospec t
		:property *prop-preprocessor*
		:begin nested-label)
	    ;;	Generate a nested table that finishes whenever an unmatched
	    ;; start or end tag is found.
	    (syntable nested-label ,property nil
		(syntoken "</"
		    :icase t
		    :nospec t
		    :contained t
		    :begin :simple-nested-tag)
		;;  These will take precedence over other rules
		(syntoken "<"
		    :icase t
		    :nospec t
		    :contained t
		    :begin :simple-nested-tag)
		(syntoken "<p>"
		    :icase t
		    :nospec t
		    :property *prop-preprocessor*
		    :switch :main)
		(synaugment :main)
	    )
	)
    )
)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Define some macros to generate tokens for tags in the format:
;;	<tag/  ... /
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro sgml-syntoken-short (name)
    `(syntoken (string-concat "<" ,name "/")
	:icase t
	:property *prop-preprocessor*
	:begin (intern (string-concat ,name "/") 'keyword))
)
(defmacro sgml-syntable-short (name property)
    `(syntable (intern (string-concat ,name "/") 'keyword) ,property nil
	(syntoken "/"
	    :nospec t
	    :property *prop-preprocessor*
	    :switch -1)
	(syntoken "</?\\w+>"
	    :property *prop-control*
	    :switch :main)
    )
)


;; The main SGML syntax table
(defsyntax *sgml-mode* :main *prop-sgml-default* nil nil
    ;; Comments
    (syntoken "<!--"
	:nospec t
	:contained t
	:begin :comment)
    (syntable :comment *prop-comment* nil
	;; Only one rule, to finish the comment.
	(syntoken "-->"
	    :nospec t
	    :switch -1)
    )

    ;; Entities
    (syntoken "&[a-zA-Z0-9_.-]+;"
	:property *prop-constant*)
    ;; Probably an entity, missing ending `;'
    (syntoken "&[a-zA-Z0-9_.-]+"
	:property *prop-sgml-maybe-entity*)

    ;; Strings
    (syntable :string *prop-string* nil
	;;  Ignore escaped characters.
	(syntoken "\\\\.")
	;;  Rule to finish the string.
	(syntoken "\""
	    :nospec t
	    :switch -1)
    )

    ;; Links
    (syntable :link *prop-preprocessor* nil
	;; No link string following "url="
	(syntoken ">"
	    :nospec t
	    :property *prop-control*
	    :switch -1)
	(syntoken "\""
	    :nospec t
	    :contained t
	    :begin :link-string)
	(syntable :link-string *prop-sgml-link* nil
	    ;; Ignore escaped characters.
	    (syntoken "\\\\.")
	    ;; Rule to finish the link, note that returns two levels.
	    (syntoken "\""
		:nospec t
		:switch -2)
	)
    )

    ;; "Special" tag
    (syntoken "<!"
	:nospec t
	:contained t
	:begin :special-tag)
    ;;  Rules for "special" tags
    (syntable :special-tag *prop-preprocessor* nil
	(syntoken "["
	    :nospec t
	    :property *prop-preprocessor*
	    :begin :brackets)
	;; Finish the "special" tag
	(syntoken ">"
	    :nospec t
	    :switch -1)
	(syntable :brackets *prop-sgml-default* nil
	    (syntoken "]"
		:nospec t
		:property *prop-preprocessor*
		:switch -1)
	    ;; Allow nesting.
	    (syntoken "["
		:nospec t
		:property *prop-preprocessor*
		:begin :brackets)
	    ;; Entities.
	    (syntoken "%[a-zA-Z0-9_.-]+;?"
		:property *prop-annotation*)
	    ;;  Allow everything inside the brackets
	    (synaugment :main)
	)
	;; Don't use generic tag tokens, only create a rule for strings
	(syntoken "\""
	    :nospec t
	    :begin :string
	    :contained t)
	;; Allow everything inside the "special" tag
	(synaugment :main)
    )

    ;; Some "short" tags
    (sgml-syntoken-short "tt")
    (sgml-syntable-short "tt" *prop-sgml-tt*)
    (sgml-syntoken-short "it")
    (sgml-syntable-short "it" *prop-sgml-it*)
    (sgml-syntoken-short "bf")
    (sgml-syntable-short "bf" *prop-sgml-bf*)
    (sgml-syntoken-short "em")
    (sgml-syntable-short "em" *prop-sgml-bf*)

    ;; Short tag
    (syntoken "<\\w+/"
	:property *prop-preprocessor*
	:begin :short-tag)
    (syntable :short-tag *prop-sgml-default-short* nil
	(syntoken "/"
	    :nospec t
	    :property *prop-preprocessor*
	    :switch -1)
	(syntoken "</?\\w+>"
	    :property *prop-control*
	    :switch -1)
    )

    ;;  Don't allow spaces, this may and may not be the start of a tag,
    ;; but the syntax-highlight definition is not specialized...
    (syntoken "<([^/a-zA-Z]|$)"
	:property *prop-control*)

    ;; Some tags that require an end tag
    (sgml-syntoken "tt")
    (sgml-syntable "tt" *prop-sgml-tt*)
    (sgml-syntoken "code")
    (sgml-syntable "code" *prop-sgml-tt*)
    (sgml-syntoken "tag")
    (sgml-syntable "tag" *prop-sgml-tt*)
    (sgml-syntoken "verb")
    (sgml-syntable "verb" *prop-sgml-tt*)
    (sgml-syntoken "programlisting")
    (sgml-syntable "programlisting" *prop-sgml-tt*)
    (sgml-syntoken "it")
    (sgml-syntable "it" *prop-sgml-it*)
    (sgml-syntoken "bf")
    (sgml-syntable "bf" *prop-sgml-bf*)
    (sgml-syntoken "em")
    (sgml-syntable "em" *prop-sgml-bf*)
    (sgml-syntoken "mail")
    (sgml-syntable "mail" *prop-sgml-email*)
    (sgml-syntoken "email")
    (sgml-syntable "email" *prop-sgml-email*)
    (sgml-syntoken "screen")
    (sgml-syntable "screen" *prop-sgml-screen*)
    (sgml-syntoken "tscreen")
    (sgml-syntable "tscreen" *prop-sgml-screen*)


    ;;  Helper for tags that don't need an ending one.
    ;;  NOTE: Since the parser is not specialized, if the tag is
    ;;	      folowed by one that has a special property defined here,
    ;;	      it may not be detected, i.e. put a <p> after the <sect>
    ;;	      and it will work.
    (syntable :simple-nested-tag *prop-preprocessor* nil
	;; tag is still open, process any options
	(synaugment :generic-tag)
	(syntoken ">"
	    :nospec t
	    :property *prop-preprocessor*
	    :switch -3)
    )
    (sgml-syntoken "sect")
    (sgml-syntable-simple "sect" *prop-sgml-sect*)
    (sgml-syntoken "sect1")
    (sgml-syntable-simple "sect1" *prop-sgml-sect*)
    (sgml-syntoken "sect2")
    (sgml-syntable-simple "sect2" *prop-sgml-sect*)

    ;; Generic tags
    (syntoken "<"
	:nospec t
	:contained t
	:begin :tag)
    ;; Table :generic-tag is defined to be augmented, no rule to finish it.
    (syntable :generic-tag *prop-preprocessor* nil
	;; Start string
	(syntoken "\""
	    :nospec t
	    :begin :string
	    :contained t)
	;; Start url link
	(syntoken "url="
	    :nospec t
	    :begin :link)
	;; Cannot nest
	(syntoken "<"
	    :nospec t
	    :property *prop-control*)
    )
    (syntable :tag *prop-preprocessor* nil
	;; Finish the tag
	(syntoken ">"
	    :nospec t
	    :switch -1)
	;; Import generic definitions
	(synaugment :generic-tag)
    )
)
