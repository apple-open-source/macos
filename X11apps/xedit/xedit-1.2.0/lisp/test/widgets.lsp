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
;; $XFree86: xc/programs/xedit/lisp/test/widgets.lsp,v 1.1 2001/08/31 15:00:15 paulo Exp $
;;
(require "xaw")
(require "xt")

(defun quit-callback (widget user call) (quit))

(setq toplevel
    (xt-app-initialize 'appcontext "Widgets"
	'(("title" . "Widgets (without customization)"))))

(setq vpane
    (xt-create-managed-widget "vpane" paned-widget-class toplevel))
(setq form
    (xt-create-managed-widget "form" form-widget-class vpane))
(xt-create-managed-widget "command" command-widget-class form
    '(("label" . "Command Widget")))
(xt-create-managed-widget "label" label-widget-class form
    '(("label" . "Label Widget") ("fromVert" . "command")))
(xt-create-managed-widget "button" menu-button-widget-class form
    '(("label" . "MenuButton Widget") ("fromVert" . "label")))

(setq popup
    (xt-create-managed-widget "menu" simple-menu-widget-class toplevel))
(xt-create-managed-widget "smebsb" sme-bsb-object-class popup
    '(("label" . "SmeBSB Object")))
(xt-create-managed-widget "smeline" sme-line-object-class popup)
(xt-create-managed-widget "smebsb2" sme-bsb-object-class popup
    '(("label" . "SmeBSB Object two")))

(xt-create-managed-widget "toggle" toggle-widget-class form
    '(("label" . "Toggle Widget") ("fromVert" . "button")))
(xt-create-managed-widget "repeater" repeater-widget-class form
    '(("label" . "Repeater Widget") ("fromVert" . "toggle")))

(setq quit
    (xt-create-managed-widget "quit" command-widget-class vpane
	'(("label" . "Quit"))))
(xt-add-callback quit "callback" 'quit-callback)

(xt-realize-widget toplevel)
(xt-app-main-loop appcontext)
