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
;; $XFree86: xc/programs/xedit/lisp/modules/lisp.lsp,v 1.10 2002/12/20 04:32:47 paulo Exp $
;;
(provide "lisp")

(in-package "LISP")

(export '(
    second third fourth fifth sixth seventh eighth ninth tenth
    pathname merge-pathnames
    logtest signum
    alphanumericp copy-seq push pop prog prog*
    with-open-file with-output-to-string
))

(defun second (a)	(nth 1 a))
(defun third (a)	(nth 2 a))
(defun fourth (a)	(nth 3 a))
(defun fifth (a)	(nth 4 a))
(defun sixth (a)	(nth 5 a))
(defun seventh (a)	(nth 6 a))
(defun eighth (a)	(nth 7 a))
(defun ninth (a)	(nth 8 a))
(defun tenth (a)	(nth 9 a))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; pathnames
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun pathname (filename)
    (values (parse-namestring filename)))

(defun merge-pathnames (pathname &optional defaults default-version)
    (if (null default-version)
	(parse-namestring pathname nil defaults)
	(parse-namestring pathname nil
	    (make-pathname :defaults defaults :version default-version))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; math
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun logtest (integer1 integer2)
    (not (zerop (logand integer1 integer2))))

(defun signum (number)
    (if (zerop number) number (/ number (abs number))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; misc functions/macros
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun alphanumericp (char)
    (or (alpha-char-p char) (not (null (digit-char-p char)))))

(defun copy-seq (sequence)
    (subseq sequence 0))

(defmacro prog (init &rest body)
    `(block nil (let ,init (tagbody ,@body))))

(defmacro prog* (init &rest body)
    `(block nil (let* ,init (tagbody ,@body))))

(defmacro with-open-file (file &rest body)
    `(let ((,(car file) (open ,@(cdr file))))
	(unwind-protect
	    (progn ,@body)
	    (if ,(car file) (close ,(car file))))))

(defmacro with-output-to-string (stream &rest body)
    `(let ((,(car stream) (make-string-output-stream)))
	(unwind-protect
	    (progn ,@body (get-output-stream-string ,(car stream)))
	    (and ,(car stream) (close ,(car stream))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; setf
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defsetf car (list) (value)	`(progn (rplaca ,list ,value) ,value))
(defsetf cdr (list) (value)	`(progn (rplacd ,list ,value) ,value))

(defsetf caar (list) (value)	`(progn (rplaca (car ,list) ,value) ,value))
(defsetf cadr (list) (value)	`(progn (rplaca (cdr ,list) ,value) ,value))
(defsetf cdar (list) (value)	`(progn (rplacd (car ,list) ,value) ,value))
(defsetf cddr (list) (value)	`(progn (rplacd (cdr ,list) ,value) ,value))
(defsetf caaar (list) (value)	`(progn (rplaca (caar ,list) ,value) ,value))
(defsetf caadr (list) (value)	`(progn (rplaca (cadr ,list) ,value) ,value))
(defsetf cadar (list) (value)	`(progn (rplaca (cdar ,list) ,value) ,value))
(defsetf caddr (list) (value)	`(progn (rplaca (cddr ,list) ,value) ,value))
(defsetf cdaar (list) (value)	`(progn (rplacd (caar ,list) ,value) ,value))
(defsetf cdadr (list) (value)	`(progn (rplacd (cadr ,list) ,value) ,value))
(defsetf cddar (list) (value)	`(progn (rplacd (cdar ,list) ,value) ,value))
(defsetf cdddr (list) (value)	`(progn (rplacd (cddr ,list) ,value) ,value))
(defsetf caaaar (list) (value)	`(progn (rplaca (caaar ,list) ,value) ,value))
(defsetf caaadr (list) (value)	`(progn (rplaca (caadr ,list) ,value) ,value))
(defsetf caadar (list) (value)	`(progn (rplaca (cadar ,list) ,value) ,value))
(defsetf caaddr (list) (value)	`(progn (rplaca (caddr ,list) ,value) ,value))
(defsetf cadaar (list) (value)	`(progn (rplaca (cdaar ,list) ,value) ,value))
(defsetf cadadr (list) (value)	`(progn (rplaca (cdadr ,list) ,value) ,value))
(defsetf caddar (list) (value)	`(progn (rplaca (cddar ,list) ,value) ,value))
(defsetf cadddr (list) (value)	`(progn (rplaca (cdddr ,list) ,value) ,value))
(defsetf cdaaar (list) (value)	`(progn (rplacd (caaar ,list) ,value) ,value))
(defsetf cdaadr (list) (value)	`(progn (rplacd (caadr ,list) ,value) ,value))
(defsetf cdadar (list) (value)	`(progn (rplacd (cadar ,list) ,value) ,value))
(defsetf cdaddr (list) (value)	`(progn (rplacd (caddr ,list) ,value) ,value))
(defsetf cddaar (list) (value)	`(progn (rplacd (cdaar ,list) ,value) ,value))
(defsetf cddadr (list) (value)	`(progn (rplacd (cdadr ,list) ,value) ,value))
(defsetf cdddar (list) (value)	`(progn (rplacd (cddar ,list) ,value) ,value))
(defsetf cddddr (list) (value)	`(progn (rplacd (cdddr ,list) ,value) ,value))

(defsetf first (list) (value)	`(progn (rplaca ,list ,value) ,value))
(defsetf second (list) (value)	`(progn (rplaca (nthcdr 1 ,list) ,value) ,value))
(defsetf third (list) (value)	`(progn (rplaca (nthcdr 2 ,list) ,value) ,value))
(defsetf fourth (list) (value)	`(progn (rplaca (nthcdr 3 ,list) ,value) ,value))
(defsetf fifth (list) (value)	`(progn (rplaca (nthcdr 4 ,list) ,value) ,value))
(defsetf sixth (list) (value)	`(progn (rplaca (nthcdr 5 ,list) ,value) ,value))
(defsetf seventh (list) (value)	`(progn (rplaca (nthcdr 6 ,list) ,value) ,value))
(defsetf eighth (list) (value)	`(progn (rplaca (nthcdr 7 ,list) ,value) ,value))
(defsetf ninth (list) (value)	`(progn (rplaca (nthcdr 8 ,list) ,value) ,value))
(defsetf tenth (list) (value)	`(progn (rplaca (nthcdr 9 ,list) ,value) ,value))

(defsetf rest (list) (value)	`(progn (rplacd ,list ,value) ,value))

(defun lisp::nth-store (index list value)
    (rplaca (nthcdr index list) value) value)
(defsetf nth lisp::nth-store)

(defsetf aref (array &rest indices) (value)
    `(lisp::vector-store ,array ,@indices ,value))

(defsetf get (symbol key &optional default) (value)
    `(lisp::put ,symbol ,key ,value))

(defsetf symbol-plist lisp::set-symbol-plist)

(defsetf gethash (key hash-table &optional default) (value)
    `(lisp::puthash ,key ,hash-table ,value))

(defsetf char lisp::char-store)
(defsetf schar lisp::char-store)
(defsetf elt lisp::elt-store)
(defsetf svref lisp::elt-store)
(defsetf documentation lisp::documentation-store)

(defsetf symbol-value set)

(defsetf subseq (sequence start &optional end) (value)
    `(progn (replace ,sequence ,value :start1 ,start :end1 ,end) ,value))
