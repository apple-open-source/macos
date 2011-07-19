;; Copyright (c) 2007,2008 Paulo Cesar Pereira de Andrade
;;
;; Permission is hereby granted, free of charge, to any person obtaining a
;; copy of this software and associated documentation files (the "Software"),
;; to deal in the Software without restriction, including without limitation
;; the rights to use, copy, modify, merge, publish, distribute, sublicense,
;; and/or sell copies of the Software, and to permit persons to whom the
;; Software is furnished to do so, subject to the following conditions:
;;
;; The above copyright notice and this permission notice (including the next
;; paragraph) shall be included in all copies or substantial portions of the
;; Software.
;;
;; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
;; THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
;; FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
;; DEALINGS IN THE SOFTWARE.
;;
;; Author: Paulo Cesar Pereira de Andrade
;;

;; Mode for editing autoconf/automake m4 files

(require "syntax")
(in-package "XEDIT")

(defsynprop *prop-macro*
    "macro"
    :font	"*courier-bold-r*-12-*"
    :foreground	"green4")

(defsynprop *prop-separator*
  "separator"
  :font		"*courier-bold-r*-12-*"
  :foreground	"Red3")

(defsynprop *prop-variable*
  "variable"
  :font		"*lucidatypewriter-medium-r*-12-*"
  :foreground	"Gold4")

(defsynprop *prop-escape*
  "escape"
  :font		"*lucidatypewriter-medium-r*-12-*"
  :foreground	"Red3")

(defsyntax *auto-mode* :main nil nil nil
  ;; dont consider dnl a macro call at top level
  (syntoken "(#.*|\\<dnl($|\\>.*))" :property *prop-comment*)

  ;; shell keywords
  (syntoken
    (string-concat
      "\\<("
      "if|then|else|elif|else|fi|case|in|esac|do|done"
      ")\\>") :property *prop-keyword*)

  ;; toplevel no arguments macro
  (syntoken "^[a-zA-Z0-9_]+$" :property *prop-macro*)

  (syntable :string *prop-string* nil
    ;; ignore escaped characters
    (syntoken "\\\\.")
    (syntoken "\"" :nospec t :switch -1)
    (synaugment :variables))
  (syntable :constant *prop-constant* nil
    (syntoken "\\\\.")
    (syntoken "'" :nospec t :switch -1)
    (synaugment :variables))
  (syntable :escape *prop-escape* nil
    (syntoken "\\\\.")
    (syntoken "`" :nospec t :switch -1)
    (synaugment :variables))

  (syntable :quoted-string *prop-string* nil
    (syntoken "\\\"" :nospec t :switch -1))

  (syntable :macro *prop-default* nil
    (syntoken "," :property *prop-separator*)
    (syntoken "[" :nospec t :property *prop-separator* :begin :quoted)
    (syntable :quoted *prop-default* nil
      ;; allow nesting
      (syntoken "[" :nospec t :property *prop-separator* :begin :quoted)
      (syntoken "]" :nospec t :property *prop-separator* :switch -1)
      (synaugment :shared :comments :variables))
    (syntoken ")" :nospec t :property *prop-macro* :switch -1)
    (synaugment :shared :quotes :variables :comments))

  (syntable :shared nil nil
    (syntoken "[a-zA-Z0-9_]+\\(" :property *prop-macro* :begin :macro)
    ;; variable assignment
    (syntoken "[a-zA-Z0-9_-]+=" :property *prop-keyword*))

  (syntable :quotes nil nil
    (syntoken "\"" :nospec t :begin :string :contained t)
    (syntoken "'" :nospec t :begin :constant :contained t)
    (syntoken "`" :nospec t :begin :escape :contained t)
    (syntoken "\\\"" :nospec t :begin :quoted-string :contained t))

  (syntable :variables nil nil
    (syntoken "\\$[a-zA-Z0-9_-]+" :property *prop-variable*)
    (syntoken "\\$\\{[a-zA-Z0-9_-]+\\}" :property *prop-variable*)
    (syntoken "\\$\\([a-zA-Z0-9_-]+\\)" :property *prop-variable*))

  (syntable :comments nil nil
    (syntoken "(#.*|\\<dnl($|\\>.*))" :property *prop-comment*))

  (synaugment :shared :quotes :variables))
