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
;; $XFree86: xc/programs/xedit/lisp/modules/progmodes/rpm.lsp,v 1.1 2003/01/16 03:50:46 paulo Exp $
;;

(require "syntax")
(in-package "XEDIT")

;; Only for testing, unifinished, good for viewing but too slow for real use...
#|
(defsynprop *prop-rpm-special*
    "rpm-special"
    :font	"*courier-bold-r*12*"
    :foreground	"NavyBlue"
)

(defsynprop *prop-rpm-escape*
    "rpm-escape"
    :font	"*lucidatypewriter-medium-r*12*"
    :foreground	"Red3")

;; main package is implicit
(defsyntax *rpm-mode* :package nil nil nil
    (syntable :sections nil nil
	(syntoken "^%package"
	    :icase t
	    :switch :package
	    ;; XXX :begin :package was added just to test finishing and
	    ;; starting a new syntax-table, unfortunately if using it
	    ;; this way, frequently the entire file will be reparsed
	    ;; at every character typed.
	    ;; TODO study these cases and implement code to avoid it,
	    ;; the easiest way is limiting the number of backtracked lines,
	    ;; the screen contents sometimes could not correctly reflect
	    ;; file contents in this case...
	    :begin :package
	    :property *prop-rpm-special*)
	(syntoken "^%(build|setup|install|pre|preun|post|postun)\\>"
	    :icase t
	    :property *prop-rpm-special*
	    :switch :package
	    :begin :shell)
	;; %changelog, XXX no rules to return to the toplevel
	(syntoken "^%changelog\\>"
	    :icase t
	    :switch :package
	    :begin :changelog
	    :property *prop-rpm-special*)
	(syntable :changelog nil nil
	    ;; ignore if escaped
	    (syntoken "%%")
	    ;; "warn" if not escaped
	    (syntoken "%" :property *prop-control*)
	    ;; emails
	    (syntoken "<[a-z0-9_-]+@[a-z0-9_-]+\\.\\w+(\\.\\w+)?>"
		:icase t
		:property *prop-string*)
	)
	;; comments
	(syntoken "#" :contained t :nospec t :begin :comment)
	(syntable :comment *prop-comment* nil
	    ;; some macros are expanded even when inside comments, and may
	    ;; cause surprises, "warn" about it
	    (syntoken "%\\{?\\w+\\}?" :property *prop-rpm-special*)
	    (syntoken "$" :switch -1)
	)
	(synaugment :global)
    )

    ;; may appear anywhere
    (syntable :global nil nil
	;; preprocessor like commands
	(syntoken "^%(define|if|ifarch|else|endif)\\>"
	    :icase t
	    :property *prop-preprocessor*)
	;; variables
	(syntoken "%\\{.*\\}" :property *prop-constant*)
    )

    ;; example: "Group: ..." or "Group(pt_BR): ..."
    (syntoken "^\\w+(\\(\\w+\\))?:" :property *prop-keyword*)

    ;; for sections with shell commands
    (syntable :shell nil nil
	(syntoken "\\<(if|then|elif|else|fi|for|do|done|case|esac|while|until)\\>"
	    :property *prop-keyword*)
	(syntable :strings nil nil
	    (syntoken "\"" :nospec t :begin :string :contained t)
	    (syntable :string *prop-string* nil
		(syntoken "\\$\\(?\\w+\\)?" :property *prop-constant*)
		(syntoken "\\\\.")
		(syntoken "\"" :nospec t :switch -1)
	    )
	    (syntoken "\'" :nospec t :begin :constant :contained t)
	    (syntable :constant *prop-constant* nil
		(syntoken "\\\\.")
		(syntoken "\'" :nospec t :switch -1)
	    )
	    (syntoken "\`" :nospec t :begin :escape :contained t)
	    (syntable :escape *prop-rpm-escape* nil
		(syntoken "\\$\\(?\\w+\\)?" :property *prop-constant*)
		(syntoken "\\\\.")
		(syntoken "\`" :nospec t :switch -1)
	    )
	)
	(synaugment :strings :sections)
    )
    (synaugment :sections)
)
|#


(defsyntax *rpm-mode* :package nil nil nil
    ;; commands, macro definitions, etc
    (syntoken "^\\s*%\\s*\\w+" :property *prop-keyword*)

    ;; rpm "variables"
    (syntoken "%\\{.*\\}" :property *prop-constant*)

    ;; package info, example: "Group: ...", "Group(pt_BR): ...", etc.
    (syntoken "^\\w+(\\(\\w+\\))?:" :property *prop-preprocessor*)

    ;; comments
    (syntoken "#" :contained t :nospec t :begin :comment)
    (syntable :comment *prop-comment* nil
	;; some macros are expanded even when inside comments, and may
	;; cause surprises, "warn" about it
	(syntoken "%define\\>" :property *prop-control*)
	(syntoken "%\\{?\\w+\\}?" :property *prop-string*)
	(syntoken "$" :switch -1)
    )

    ;; emails
    (syntoken "<?[a-z0-9_-]+@[a-z0-9_-]+\\.\\w+(\\.\\w+)*>?"
	:icase t
	:property *prop-string*)
    ;; links
    (syntoken "\\<(http|ftp)://\\S+" :property *prop-string*)
)
