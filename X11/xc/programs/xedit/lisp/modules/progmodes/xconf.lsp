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
;; $XFree86: xc/programs/xedit/lisp/modules/progmodes/xconf.lsp,v 1.1 2003/01/16 03:50:46 paulo Exp $
;;

(require "syntax")
(require "indent")
(in-package "XEDIT")

(defsyntax *xconf-mode* :main nil #'default-indent nil
    ;; section start
    (syntoken "\\<(Section|SubSection)\\>"
	:property *prop-keyword* :icase t :begin :section)
    ;; just for fun, highlight the section name differently
    (syntable :section *prop-constant* #'default-indent
	(syntoken "\"" :nospec t :begin :name)
	(syntable :name *prop-constant* nil
	    ;; ignore escaped characters
	    (syntoken "\\\\.")
	    (syntoken "\"" :nospec t :switch -2)
	)
    )

    ;; section end
    (syntoken "\\<(EndSection|EndSubSection)\\>"
	:property *prop-keyword* :icase t)

    ;; numeric options
    (syntoken "\\<\\d+(\\.\\d+)?\\>" :property *prop-number*)

    ;; comments
    (syntoken "#.*$" :property *prop-comment*)

    ;; strings
    (syntoken "\"" :nospec t :begin :string :contained t)
    (syntable :string *prop-string* #'default-indent
	;; ignore escaped characters
	(syntoken "\\\\.")
	(syntoken "\"" :nospec t :switch -1)
    )
)
