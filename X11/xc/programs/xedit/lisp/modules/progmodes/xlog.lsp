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
;; $XFree86: xc/programs/xedit/lisp/modules/progmodes/xlog.lsp,v 1.1 2003/01/16 06:25:51 paulo Exp $
;;

(require "syntax")
(in-package "XEDIT")

(defsynprop *prop-xlog-probe*
    "xlog-probe"
    :font	"*courier-medium-r*12*"
    :background	"rgb:c/f/c")

(defsynprop *prop-xlog-config*
    "xlog-config"
    :font	"*courier-medium-r*12*"
    :background	"rgb:c/e/f")

(defsynprop *prop-xlog-default*
    "xlog-default"
    :font	"*courier-medium-r*12*"
    :background	"rgb:e/c/f")

(defsynprop *prop-xlog-warning*
    "xlog-warning"
    :font	"*courier-bold-r*12*"
    :foreground	"Red4"
    :background	"Yellow1"
)

(defsynprop *prop-xlog-error*
    "xlog-error"
    :font	"*courier-bold-r*12*"
    :foreground	"Yellow2"
    :background	"Red3"
)

(defsyntax *xlog-mode* :main nil nil nil
    ;; highlight version
    (syntoken "^XFree86 Version \\S+" :property *prop-annotation*)

    ;; release date
    (syntoken "^Release Date: " :property *prop-keyword* :begin :note)

    ;; highlight operating system description
    (syntoken "^Build Operating System: " :property *prop-keyword* :begin :note)

    (syntable :note *prop-annotation* nil (syntoken "$" :switch -1))

    ;; don't highlight info lines
    (syntoken "^\\(II\\) " :property *prop-keyword*)

    ;; default lines
    (syntoken "^\\(==\\) " :property *prop-keyword* :begin :default)
    (syntable :default *prop-xlog-default* nil (syntoken "$" :switch -1))

    ;; probe lines
    (syntoken "^\\(--\\) " :property *prop-keyword* :begin :probe)
    (syntable :probe *prop-xlog-probe* nil (syntoken "$" :switch -1))

    ;; config lines
    (syntoken "^\\(\\*\\*\\) " :property *prop-keyword* :begin :config)
    (syntable :config *prop-xlog-config* nil (syntoken "$" :switch -1))

    ;; warnings
    (syntoken "^\\(WW\\) " :property *prop-keyword* :begin :warning)
    (syntable :warning *prop-xlog-warning* nil (syntoken "$" :switch -1))

    ;; errors
    (syntoken "^\\(EE\\) " :property *prop-keyword* :begin :error)
    (syntable :error *prop-xlog-error* nil (syntoken "$" :switch -1))

    ;; command line and "uncommon" messages
    (syntoken "^\\(..\\) " :property *prop-control* :begin :warning)
)
