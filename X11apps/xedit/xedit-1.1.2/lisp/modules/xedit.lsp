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
;; $XdotOrg: xc/programs/xedit/lisp/modules/xedit.lsp,v 1.2 2004/04/23 19:54:45 eich Exp $
;; $XFree86: xc/programs/xedit/lisp/modules/xedit.lsp,v 1.9 2003/01/16 03:50:46 paulo Exp $
;;

(provide "xedit")

#+debug	(make-package "XEDIT" :use '("LISP" "EXT"))
(in-package "XEDIT")


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  TODO The user should be able to define *auto-modes* prior to the
;; initialization here in a configuration file, since defvar only binds
;; the variable if it is unbound or doesn't have a value defined.
;;  *auto-modes* is a list of conses where every car is compiled
;; to a regexp to match the name of the file being loaded. The caddr is
;; either a string, a pathname, or a syntax-p.
;;  When loading a file, if the regexp in the car matches, it will check
;; the caddr value, and if it is a:
;;	string:		executes (load "progmodes/<the-string>.lsp")
;;	pathname:	executes (load <the-pathhame>)
;;	syntax-p:	does nothing, already loaded
;;
;;  If it fails to load the file, or the returned value is not a
;; syntax-p, the entry is removed.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defvar *auto-modes* '(
    ("\\.(c|cc|C|cxx|cpp|h|hpp|bm|xbm|xpm|y|h\\.in)$"
	"C/C++"		"c"	. *c-mode*)
    ("\\.(l|li?sp|scm)$"
	"Lisp/Scheme"	"lisp"	. *lisp-mode*)
    ("\\.sh$"
	"Unix shell"	"sh"	. *sh-mode*)
    ("\\.(diff|patch)"
	"Patch file"	"patch"	. *patch-mode*)
    ("/[Mm]akefile.*|\\.mk$"
	"Makefile"	"make"	. *make-mode*)
    ("\\.(ac|in|m4)$"
	"Autotools"	"auto"	. *auto-mode*)
    ("\\.spec$"
	"RPM spec"	"rpm"	. *rpm-mode*)
    ("\\.(pl|pm|ph)$"
	"Perl"		"perl"	. *perl-mode*)
    ("\\.(py)$"
	"Python"	"python". *python-mode*)
    ("\\.(sgml?|dtd)$"
	"SGML"		"sgml"	. *sgml-mode*)
    ("\\.html?$"
	"HTML"		"html"	. *html-mode*)
    ("\\.(man|\\d)$"
	"Man page"	"man"	. *man-mode*)
    ("app-defaults/\\w+|\\u[A-Za-z0-9_-]+\\.ad"
	"X resource"	"xrdb"	. *xrdb-mode*)
    ("\\<(XF86Config|xorg.conf)[^/]*"
	"XF86Config"	"xconf"	. *xconf-mode*)
    ("\\<(XFree86|Xorg)\\.\\d+\\.log(\\..*|$)"
	"XFree86 log"	"xlog"	. *xlog-mode*)
    ("Imakefile|(\\.(cf|rules|tmpl|def)$)"
	"X imake"	"imake"	. *imake-mode*)
))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Compile the regexps in the *auto-modes* list.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(dolist (mode *auto-modes*)
    (rplaca mode (re-comp (car mode) :nosub t))
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Find the progmode associated with the given filename.
;; Returns nil if nothing matches.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun auto-mode (filename &optional symbol &aux syntax)
    (if (and symbol (symbolp symbol))
	(if (boundp symbol)
	    (return-from auto-mode (symbol-value symbol))
	    (setq syntax (cddr (find symbol *auto-modes* :key #'cdddr)))
	)
	;; symbol optional argument is not a symbol
	(do*
	    (
	    (mode   *auto-modes*    (cdr mode))
	    (regex  (caar mode)     (caar mode))
	    )
	    ((endp mode))

	    ;; only wants to know if the regex match.
	    (when (listp (re-exec regex filename :count 0))
		(setq syntax (cddar mode) symbol (cdr syntax))
		(return)
	    )
	)
    )

    ;; if file was already loaded
    (if (and symbol (boundp symbol))
	(return-from auto-mode (symbol-value symbol))
    )

    (when (consp syntax)
	;; point to the syntax file specification
	(setq syntax (car syntax))

	;; try to load the syntax definition file
	(if (stringp syntax)
	    (load
		(string-concat
		    (namestring *default-pathname-defaults*)
		    "progmodes/"
		    syntax
		    ".lsp"
		)
	    )
	    (load syntax)
	)

	(and symbol (boundp symbol) (symbol-value symbol))
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Data types.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  The main syntax structure, normally, only one should exist per
;; syntax highlight module.
;;  The structure is defined here so it is not required to load all
;; the extra data associated with syntax-highlight at initialization
;; time, and will never be loaded if no syntax-highlight mode is
;; defined to the files being edited.
(defstruct syntax
    name		;;  A unique string to identify the syntax mode.
			;; Should be the name of the language/file type.
    options		;;  A hash table of options specified for the
			;; language.

    ;; Field(s) defined at "compile time"
    labels		;;  Not exactly a list of labels, but all syntax
			;; tables for the module.
    quark		;;  A XrmQuark associated with the XawTextPropertyList
			;; used by this syntax mode.
    token-count		;;  Number of distinct syntoken structures in
			;; the syntax table.
)

;;  Xlfd description, used when combining properties.
;;  Field names are self descriptive.
;;	XXX Fields should be initialized as strings, but fields
;;	    that have an integer value should be allowed to
;;	    be initialized as such.
;;  Combining properties in supported in Xaw, but not yet in the
;; syntax highlight code interface. Combining properties allow easier
;; implementation for markup languages, for example:
;;	<b>bold<i>italic</i></b>
;;	would render "bold" using a bold version of the default font,
;;	and "italic" using a bold and italic version of the default font
(defstruct xlfd
    foundry
    family
    weight
    slant
    setwidth
    addstyle
    pixel-size
    point-size
    res-x
    res-y
    spacing
    avgwidth
    registry
    encoding
)


;;   At some time this structure should also hold information for at least:
;;	o fontset
;;	o foreground pixmap
;;	o background pixmap
;;   XXX This is also a TODO in Xaw.
(defstruct synprop
    quark	;;   XrmQuark identifier of the XawTextProperty
		;; structure. This field is filled when "compiling"
		;; the syntax-table.

    name	;;   String name of property, must be unique per
		;; property list.
    font	;; Optional font string name of property.
    foreground	;; Optional string representation of foreground color.
    background	;; Optional string representation of background color.
    xlfd	;;   Optional xlfd structure, when combining properties.
		;; Currently combining properties logic not implemented,
		;; but fonts may be specified using the xlfd definition.

    ;; Boolean properties.
    underline	;; Draw a line below the text.
    overstrike	;; Draw a line over the text.

    ;; XXX Are these working in Xaw?
    subscript	;; Align text to the bottom of the line.
    superscript	;; Align text to the top of the line.
    ;;  Note: subscript and superscript only have effect when the text
    ;; line has different height fonts displayed.
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Utility macro, to create a "special" variable holding
;; a synprop structure.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro defsynprop (variable name
		      &key font foreground background xlfd underline
			   overstrike subscript superscript)
    `(progn
	(proclaim '(special ,variable))
	(setq ,variable
	    (make-synprop
		:name		,name
		:font		,font
		:foreground	,foreground
		:background	,background
		:xlfd		,xlfd
		:underline	,underline
		:overstrike	,overstrike
		:subscript	,subscript
		:superscript	,superscript
	    )
	)
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Convert a synprop structure  to a string in the format
;; expected by Xaw.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun synprop-to-string (synprop &aux values booleans xlfd)
    (if (setq xlfd (synprop-xlfd synprop))
	(dolist
	    (element
	       `(
		("foundry"	    ,(xlfd-foundry xlfd))
		("family"	    ,(xlfd-family xlfd))
		("weight"	    ,(xlfd-weight xlfd))
		("slant"	    ,(xlfd-slant xlfd))
		("setwidth"	    ,(xlfd-setwidth xlfd))
		("addstyle"	    ,(xlfd-addstyle xlfd))
		("pixelsize"	    ,(xlfd-pixel-size xlfd))
		("pointsize"	    ,(xlfd-point-size xlfd))
		("resx" 	    ,(xlfd-res-x xlfd))
		("resy" 	    ,(xlfd-res-y xlfd))
		("spacing"	    ,(xlfd-spacing xlfd))
		("avgwidth"	    ,(xlfd-avgwidth xlfd))
		("registry"	    ,(xlfd-registry xlfd))
		("encoding"	    ,(xlfd-encoding xlfd))
		)
	    )
	    (if (cadr element)
		(setq values (append values element))
	    )
	)
    )
    (dolist
	(element
	   `(
	    ("font"		,(synprop-font synprop))
	    ("foreground"	,(synprop-foreground synprop))
	    ("background"	,(synprop-background synprop))
	    )
	)
	(if (cadr element)
	    (setq values (append values element))
	)
    )

    ;;  Boolean attributes. These can be specified in the format
    ;; <name>=<anything>, but do a nicer output as the format
    ;; <name> is accepted.
    (dolist
	(element
	    `(
	    ("underline"	,(synprop-underline synprop))
	    ("overstrike"	,(synprop-overstrike synprop))
	    ("subscript"	,(synprop-subscript synprop))
	    ("superscript"	,(synprop-superscript synprop))
	    )
	)
	(if (cadr element)
	    (setq booleans (append booleans element))
	)
    )

    ;;  Play with format conditionals, list iteration, and goto, to
    ;; make resulting string.
    (format
	nil
	"~A~:[~;?~]~:[~3*~;~A=~A~{&~A=~A~}~]~:[~;&~]~:[~2*~;~A~{&~A~*~}~]"

	(synprop-name synprop)				;; ~A
	(or values booleans)				;; ~:[~;?~]
	values						;; ~:[
	    (car values) (cadr values) (cddr values)	;; ~A=~A~{&~A=~A~}
	(and values booleans)				;; ~:[~;&~]
	booleans					;; ~:[
	    (car booleans) (cddr booleans)		;; ~A~{&~A~*~}
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Use xedit protocol to create a XawTextPropertyList with the
;; given arguments.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun compile-syntax-property-list (name properties
				     &aux string-properties quark)

    ;; Create a string representation of the properties.
    (dolist (property properties)
	(setq
	    string-properties
	    (append
		string-properties
		(list (synprop-to-string property))
	    )
	)
    )

    (setq
	string-properties
	(case (length string-properties)
	    (0	"")
	    (1	(car string-properties))
	    (t	(format nil "~A~{,~A~}"
		    (car string-properties)
		    (cdr string-properties)
		)
	    )
	)
    )

#+debug
    (format *output* "~Cconvert-property-list ~S ~S~%"
	*escape*
	name
	string-properties
    )
    (setq quark #-debug (convert-property-list name string-properties)
		#+debug 0)

    ;; Store the quark for properties not yet "initialized".
    ;; XXX This is just a call to Xrm{Perm,}StringToQuark, and should
    ;;     be made available if there were a wrapper/interface to
    ;;     that Xlib function.
    (dolist (property properties)
	(unless (integerp (synprop-quark property))
#+debug
	    (format *output* "~Cxrm-string-to-quark ~S~%"
		*escape*
		(synprop-name property)
	    )
	    (setf
		(synprop-quark property)
#-debug		(xrm-string-to-quark (synprop-name property))
#+debug		0
	    )
	)
    )

    quark
)




#+debug
(progn
    (defconstant *escape* #\$)

    (defconstant *output* *standard-output*)

    ;; Recognized identifiers for wrap mode.
    (defconstant *wrap-modes* '(:never :line :word))

    ;; Recognized identifiers for justification.
    (defconstant *justifications* '(:left :right :center :full))

    ;; XawTextScanType
    (defconstant *scan-type*
	'(:positions :white-space :eol :paragraph :all :alpha-numeric))

    ;; XawTextScanDirection
    (defconstant *scan-direction* '(:left :right))

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    ;; Debugging version of xedit functions.
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (defun clear-entities (left right)
	(format *output* "~Cclear-entities ~D ~D~%"
	    *escape* left right))

    (defun add-entity (offset length identifier)
	(format *output* "~Cadd-entity ~D ~D ~D~%"
	    *escape* offset length identifier))

    (defun background (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-background ~S~%" *escape* value)
	    (format *output* "~Cget-background~%" *escape*)))

    (defun foreground (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-foreground ~S~%" *escape* value)
	    (format *output* "~Cget-foreground~%" *escape*)))

    (defun font (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-font ~S~%" *escape* value)
	    (format *output* "~Cget-font~%" *escape*)))

    (defun point (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-point ~D~%" *escape* value)
	    (format *output* "~Cget-point~%" *escape*)))

    (defun point-min ()
	(format *output* "~Cpoint-min~%" *escape*))

    (defun point-max ()
	(format *output* "~Cpoint-max~%" *escape*))

    (defun property-list (&optional (quark nil specified))
	(format *output* "~property-list ~D~%" *escape* quark))

    (defun insert (string)
	(format *output* "~Cinsert ~S~%" *escape* string))

    (defun read-text (offset length)
	(format *output* "~Cread-text ~D ~D~%"
	    *escape* offset length))

    (defun replace-text (left right string)
	(format *output* "~Creplace-text ~D ~D ~S~%"
	    *escape* left right string))

    (defun scan (offset type direction &key (count 1) include)
	(unless (setq type (position type *scan-type*))
	    (error "SCAN: type must be one of ~A, not ~A"
		*scan-type* type))
	(unless (setq direction (position direction *scan-direction*))
	    (error "SCAN: direction must be one of ~A, not ~A"
		*scan-direction* direction))
	(format *output* "~Cscan ~D ~D ~D ~D ~D~%"
	    *escape* offset type direction count (if include 1 0)))

    (defun search-forward (string &optional case-sensitive)
	(format *output* "~Csearch-forward ~S ~D~%"
	    *escape* string (if case-sensitive 1 0)))

    (defun search-backward (string &optional case-sensitive)
	(format *output* "~Csearch-backward ~S ~D~%"
	    *escape* string (if case-sensitive 1 0)))

    (defun wrap-mode (&optional (value nil specified))
	(if specified
	    (progn
		(unless (member value *wrap-modes*)
		    (error "WRAP-MODE: argument must be one of ~A, not ~A"
			*wrap-modes* value))
		(format *output* "~Cset-wrap-mode ~S~%"
		    *escape* (string value)))
	    (format *output* "~Cget-wrap-mode~%" *escape*)))

    (defun auto-fill (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-auto-fill ~S~%"
		*escape* (if value "true" "false"))
	    (format *output* "~Cget-auto-fill~%" *escape*)))

    (defun justification (&optional (value nil specified))
	(if specified
	    (progn
		(unless (member value *justifications*)
		    (error "JUSTIFICATION: argument must be one of ~A, not ~A"
			*justifications* value))
		(format *output* "~Cset-justification ~S~%"
		    *escape* (string value)))
	    (format *output* "~Cget-justification~%" *escape*)))

    (defun left-column (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-left-column ~D~%" *escape* value)
	    (format *output* "~Cget-left-column~%" *escape*)))

    (defun right-column (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-right-column ~D~%" *escape* value)
	    (format *output* "~Cget-right-column~%" *escape*)))

    (defun vertical-scrollbar (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-vert-scrollbar ~S~%"
		*escape* (if value "always" "never"))
	    (format *output* "~Cget-vert-scrollbar~%" *escape*)))

    (defun horizontal-scrollbar (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-horiz-scrollbar ~S~%"
		*escape* (if value "always" "never"))
	    (format *output* "~Cget-horiz-scrollbar~%" *escape*)))

    #|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
    (defun create-buffer (name)
	(format *output* "~Ccreate-buffer ~S~%" *escape* name))

    (defun remove-buffer (name)
	(format *output* "~Cremove-buffer ~S~%" *escape* name))

    (defun buffer-name (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-buffer-name ~S~%" *escape* value)
	    (format *output* "~Cget-buffer-name~%" *escape*)))

    (defun buffer-filename (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-buffer-filename ~S~%"
		*escape* (namestring value))
	    (format *output* "~Cget-buffer-filename~%" *escape*)))

    (defun current-buffer (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-current-buffer ~S~%" *escape* value)
	    (format *output* "~Cget-current-buffer~%" *escape*)))

    (defun other-buffer (&optional (value nil specified))
	(if specified
	    (format *output* "~Cset-other-buffer ~S~%" *escape* value)
	    (format *output* "~Cget-other-buffer~%" *escape*)))
    |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||#
)
