;;
;; Copyright (c) 2001 by The XFree86 Project, Inc.
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
;; $XFree86: xc/programs/xedit/lisp/test/hello.lsp,v 1.3 2002/11/08 08:01:01 paulo Exp $
;;
(require "xaw")
(require "xt")

(defun quit-callback (widget user call) (quit))

(defun fix-shell-size (shell)
    (let ((size (xt-get-values shell '("width" "height"))))
	 (xt-set-values shell
	     (list (cons "minWidth" (cdar size))
		   (cons "maxWidth" (cdar size))
		   (cons "minHeight" (cdadr size))
		   (cons "maxHeight" (cdadr size)))
	 )
    )
)

(setq toplevel
    (xt-app-initialize 'appcontext "Hello"
	'(("title" . "Hello World!"))))

(setq form
    (xt-create-managed-widget "form" form-widget-class toplevel
	'(("background" . "gray85")
	  ("displayList" . "foreground rgb:7/9/7;lines 1,-1,-1,-1,-1,1;foreground gray90;lines -1,0,0,0,0,-1")
	)))

(setq button
    (xt-create-managed-widget "button" command-widget-class form
	'(("label" . "Goodbye world!")
	  ("tip" . "This sample uses some customizations")
	  ("foreground" . "gray10")
	  ("background" . "gray80")
	  ("displayList" . "foreground rgb:7/9/7;lines 1,-1,-1,-1,-1,1;foreground gray90;lines -1,0,0,0,0,-1")
	)))
(xt-add-callback button "callback" 'quit-callback)

(xt-realize-widget toplevel)

(fix-shell-size toplevel)

(xt-app-main-loop appcontext)
