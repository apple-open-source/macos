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
;; $XFree86: xc/programs/xedit/lisp/test/list.lsp,v 1.6 2002/12/06 03:25:29 paulo Exp $
;;

;; basic lisp function tests

;; Most of the tests are just the examples from the
;;
;;	Common Lisp HyperSpec (TM)
;;	Copyright 1996-2001, Xanalys Inc. All rights reserved.
;;
;; Some tests are hand crafted, to test how the interpreter treats
;; uncommon arguments or special conditions


#|
 MAJOR PROBLEMS:

 o NIL and T should be always treated as symbols, actually it is
   legal to say (defun nil (...) ...)
 o There aren't true uninterned symbols, there are only symbols that
   did not yet establish the home package, but once one is created, an
   interned symbol is always returned.
|#

(defun compare-test (test expect function arguments
		     &aux result (error t) unused error-value)
    (multiple-value-setq
	(unused error-value)
	(ignore-errors
	    (setq result (apply function arguments))
	    (setq error nil)
	)
    )
    (if error
	(format t "ERROR: (~S~{ ~S~}) => ~S~%" function arguments error-value)
	(or (funcall test result expect)
	    (format t "(~S~{ ~S~}) => should be ~S not ~S~%"
		function arguments expect result
	    )
	)
    )
)

(defun compare-eval (test expect form
		     &aux result (error t) unused error-value)
    (multiple-value-setq
	(unused error-value)
	(ignore-errors
	    (setq result (eval form))
	    (setq error nil)
	)
    )
    (if error
	(format t "ERROR: ~S => ~S~%" form error-value)
	(or (funcall test result expect)
	    (format t "~S => should be ~S not ~S~%"
		form expect result
	    )
	)
    )
)

(defun error-test (function &rest arguments &aux result (error t))
    (ignore-errors
	(setq result (apply function arguments))
	(setq error nil)
    )
    (or error
	(format t "ERROR: no error for (~S~{ ~S~}), result was ~S~%"
	    function arguments result)
    )
)

(defun error-eval (form &aux result (error t))
    (ignore-errors
	(setq result (eval form))
	(setq error nil)
    )
    (or error
	(format t "ERROR: no error for ~S, result was ~S~%" form result)
    )
)

(defun eq-test (expect function &rest arguments)
    (compare-test #'eq expect function arguments))

(defun eql-test (expect function &rest arguments)
    (compare-test #'eql expect function arguments))

(defun equal-test (expect function &rest arguments)
    (compare-test #'equal expect function arguments))

(defun equalp-test (expect function &rest arguments)
    (compare-test #'equalp expect function arguments))


(defun eq-eval (expect form)
    (compare-eval #'eq expect form))

(defun eql-eval (expect form)
    (compare-eval #'eql expect form))

(defun equal-eval (expect form)
    (compare-eval #'equal expect form))

(defun equalp-eval (expect form)
    (compare-eval #'equalp expect form))

;; clisp treats strings loaded from a file as constants
(defun xseq (sequence)
    #+clisp (if *load-pathname* (copy-seq sequence) sequence)
    #-clisp sequence
)

;; apply				- function
(equal-test '((+ 2 3) . 4) #'apply 'cons '((+ 2 3) 4))
(eql-test -1 #'apply #'- '(1 2))
(eql-test 7 #'apply #'max 3 5 '(2 7 3))
(error-test #'apply #'+ 1)
(error-test #'apply #'+ 1 2)
(error-test #'apply #'+ 1 . 2)
(error-test #'apply #'+ 1 2 3)
(error-test #'apply #'+ 1 2 . 3)
(eql-test 6 #'apply #'+ 1 2 3 ())

;; eq					- function
(eq-eval t '(let* ((a #\a) (b a)) (eq a b)))
(eq-test t #'eq 'a 'a)
(eq-test nil #'eq 'a 'b)
(eq-eval t '(eq #1=1 #1#))
(eq-test nil #'eq "abc" "abc")
(setq a '('x #c(1 2) #\z))
(eq-test nil #'eq a (copy-seq a))

;; eql					- function
(eq-test t #'eql 1 1)
(eq-test t #'eql 1.3d0 1.3d0)
(eq-test nil #'eql 1 1d0)
(eq-test t #'eql #c(1 -5) #c(1 -5))
(eq-test t #'eql 'a 'a)
(eq-test nil #'eql :a 'a)
(eq-test t #'eql #c(5d0 0) 5d0)
(eq-test nil #'eql #c(5d0 0d0) 5d0)
(eq-test nil #'eql "abc" "abc")
(equal-eval '(1 5/6 #p"test" #\#) '(setq a '(1 5/6 #p"test" #\#)))
(eq-test nil #'eql a (copy-seq a))

(setf
    hash0 (make-hash-table)
    hash1 (make-hash-table)
    (gethash 1 hash0) 2
    (gethash 1 hash1) 2
    (gethash :foo hash0) :bar
    (gethash :foo hash1) :bar
)
(defstruct test a b c)
(setq
    struc0 (make-test :a 1 :b 2 :c #\c)
    struc1 (make-test :a 1 :b 2 :c #\c)
)

;; equal				- function
(eq-test t #'equal "abc" "abc")
(eq-test t #'equal 1 1)
(eq-test t #'equal #c(1 2) #c(1 2))
(eq-test nil #'equal #c(1 2) #c(1 2d0))
(eq-test t #'equal #\A #\A)
(eq-test nil #'equal #\A #\a)
(eq-test nil #'equal "abc" "Abc")
(equal-eval '(1 2 3/5 #\a) '(setq a '(1 2 3/5 #\a)))
(eq-test t #'equal a (copy-seq a))
(eq-test nil #'equal hash0 hash1)
(eq-test nil #'equal struc0 struc1)
(eq-test nil #'equal #(1 2 3 4) #(1 2 3 4))

;; equalp				- function
(eq-test t #'equalp hash0 hash1)
(setf
    (gethash 2 hash0) "FoObAr"
    (gethash 2 hash1) "fOoBaR"
)
(eq-test t #'equalp hash0 hash1)
(setf
    (gethash 3 hash0) 3
    (gethash 3d0 hash1) 3
)
(eq-test nil #'equalp hash0 hash1)
(eq-test t #'equalp struc0 struc1)
(setf
    (test-a struc0) #\a
    (test-a struc1) #\A
)
(eq-test t #'equalp struc0 struc1)
(setf
    (test-b struc0) 'test
    (test-b struc1) :test
)
(eq-test nil #'equalp struc0 struc1)
(eq-test t #'equalp #c(1/2 1d0) #c(0.5d0 1))
(eq-test t #'equalp 1 1d0)
(eq-test t #'equalp #(1 2 3 4) #(1 2 3 4))
(eq-test t #'equalp #(1 #\a 3 4d0) #(1 #\A 3 4))

;; acons				- function
(equal-test '((1 . "one")) #'acons 1 "one" nil)
(equal-test '((2 . "two") (1 . "one")) #'acons 2 "two" '((1 . "one")))

;; adjoin				- function
(equal-test '(nil) #'adjoin nil nil)
(equal-test '(a) #'adjoin 'a nil)
(equal-test '(1 2 3) #'adjoin 1 '(1 2 3))
(equal-test '(1 2 3) #'adjoin 2 '(1 2 3))
(equal-test '((1) (1) (2) (3)) #'adjoin '(1) '((1) (2) (3)))
(equal-test '((1) (2) (3)) #'adjoin '(1) '((1) (2) (3)) :key #'car)
(error-test #'adjoin nil 1)

;; alpha-char-p				- function
(eq-test t #'alpha-char-p #\a)
(eq-test nil #'alpha-char-p #\5)
(error-test #'alpha-char-p 'a)

;; alphanumericp			- function
(eq-test t #'alphanumericp #\Z)
(eq-test t #'alphanumericp #\8)
(eq-test nil #'alphanumericp #\#)

;; and					- macro
(eql-eval 1 '(setq temp1 1 temp2 1 temp3 1))
(eql-eval 2 '(and (incf temp1) (incf temp2) (incf temp3)))
(eq-eval t '(and (eql 2 temp1) (eql 2 temp2) (eql 2 temp3)))
(eql-eval 1 '(decf temp3))
(eq-eval nil '(and (decf temp1) (decf temp2) (eq temp3 'nil) (decf temp3)))
(eq-eval t '(and (eql temp1 temp2) (eql temp2 temp3)))
(eq-eval t '(and))
(equal-eval '(1 2 3) '(multiple-value-list (and (values 'a) (values 1 2 3))))
(equal-eval nil '(and (values) t))

;; append				- function
(equal-test '(a b c d e f g) #'append '(a b c) '(d e f) '() '(g))
(equal-test '(a b c . d) #'append '(a b c) 'd)
(eq-test nil #'append)
(eql-test 'a #'append nil 'a)
(error-test #'append 1 2)

;; assoc				- function
(equal-test '(1 . "one") #'assoc 1 '((2 . "two") (1 . "one")))
(equal-test '(2 . "two") #'assoc 2 '((1 . "one") (2 . "two")))
(eq-test nil #'assoc 1 nil)
(equal-test '(2 . "two") #'assoc-if #'evenp '((1 . "one") (2 . "two")))
(equal-test '(3 . "three") #'assoc-if-not #'(lambda(x) (< x 3))
	'((1 . "one") (2 . "two") (3 . "three")))
(equal-test '("two" . 2) #'assoc #\o '(("one" . 1) ("two" . 2) ("three" . 3))
	:key #'(lambda (x) (char x 2)))
(equal-test '(a . b) #'assoc 'a '((x . a) (y . b) (a . b) (a . c)))

;; atom					- function
(eq-test t #'atom 1)
(eq-test t #'atom '())
(eq-test nil #'atom '(1))
(eq-test t #'atom 'a)

;; block				- special operator
(eq-eval nil '(block empty))
(eql-eval 2 '(let ((x 1))
		(block stop (setq x 2) (return-from stop) (setq x 3)) x))
(eql-eval 2 '(block twin (block twin (return-from twin 1)) 2))

;; both-case-p				- function
(eq-test t #'both-case-p #\a)
(eq-test nil #'both-case-p #\1)

;; boundp				- function
(eql-eval 1 '(setq x 1))
(eq-test t #'boundp 'x)
(makunbound 'x)
(eq-test nil #'boundp 'x)
(eq-eval nil '(let ((x 1)) (boundp 'x)))
(error-test #'boundp 1)

;; butlast, nbutlast			- function
(setq x '(1 2 3 4 5 6 7 8 9))
(equal-test '(1 2 3 4 5 6 7 8) #'butlast x)
(equal-eval '(1 2 3 4 5 6 7 8 9) 'x)
(eq-eval nil '(nbutlast x 9))
(equal-test '(1) #'nbutlast x 8)
(equal-eval '(1) 'x)
(eq-test nil #'butlast nil)
(eq-test nil #'nbutlast '())
(error-test #'butlast 1 2)
(error-test #'butlast -1 '(1 2))

;; car, cdr, caar ...			- function
(eql-test 1 #'car '(1 2))
(eql-test 2 #'cdr '(1 . 2))
(eql-test 1 #'caar '((1 2)))
(eql-test 2 #'cadr '(1 2))
(eql-test 2 #'cdar '((1 . 2)))
(eql-test 3 #'cddr '(1 2 . 3))
(eql-test 1 #'caaar '(((1 2))))
(eql-test 2 #'caadr '(1 (2 3)))
(eql-test 2 #'cadar '((1 2) 2 3))
(eql-test 3 #'caddr '(1 2 3 4))
(eql-test 2 #'cdaar '(((1 . 2)) 3))
(eql-test 3 #'cdadr '(1 (2 . 3) 4))
(eql-test 3 #'cddar '((1 2 . 3) 3))
(eql-test 4 #'cdddr '(1 2 3 . 4))
(eql-test 1 #'caaaar '((((1 2)))))
(eql-test 2 #'caaadr '(1 ((2))))
(eql-test 2 #'caadar '((1 (2)) 3))
(eql-test 3 #'caaddr '(1 2 (3 4)))
(eql-test 2 #'cadaar '(((1 2)) 3))
(eql-test 3 #'cadadr '(1 (2 3) 4))
(eql-test 3 #'caddar '((1 2 3) 4))
(eql-test 4 #'cadddr '(1 2 3 4 5))
(eql-test 2 #'cdaaar '((((1 . 2))) 3))
(eql-test 3 #'cdaadr '(1 ((2 . 3)) 4))
(eql-test 3 #'cdadar '((1 (2 . 3)) 4))
(eql-test 4 #'cdaddr '(1 2 (3 . 4) 5))
(eql-test 3 #'cddaar '(((1 2 . 3)) 4))
(eql-test 4 #'cddadr '(1 (2 3 . 4) 5))
(eql-test 4 #'cdddar '((1 2 3 . 4) 5))
(eql-test 5 #'cddddr '(1 2 3 4 . 5))

;; first ... tenth, rest		- function
(eql-test 2 #'rest '(1 . 2))
(eql-test 1 #'first '(1 2))
(eql-test 2 #'second '(1 2 3))
(eql-test 2 #'second '(1 2 3))
(eql-test 3 #'third '(1 2 3 4))
(eql-test 4 #'fourth '(1 2 3 4 5))
(eql-test 5 #'fifth '(1 2 3 4 5 6))
(eql-test 6 #'sixth '(1 2 3 4 5 6 7))
(eql-test 7 #'seventh '(1 2 3 4 5 6 7 8))
(eql-test 8 #'eighth '(1 2 3 4 5 6 7 8 9))
(eql-test 9 #'ninth '(1 2 3 4 5 6 7 8 9 10))
(eql-test 10 #'tenth '(1 2 3 4 5 6 7 8 9 10 11))
(error-test #'car 1)
(error-test #'car #c(1 2))
(error-test #'car #(1 2))

;; case					- macro
(eql-eval t '(let ((a 1)) (case a ((4 5 6) nil) ((3 2 1) t) (otherwise :error))))
(eql-eval t '(let ((a 1)) (case a ((3 2) nil) (1 t) (t :error))))
(error-eval '(let ((a 1)) (case a (2 :error) (t nil) (otherwise t))))
(error-eval '(let ((a 1)) (case a (2 :error) (otherwise t) (t nil))))

;; catch				- special operator
(eql-eval 3 '(catch 'dummy-tag 1 2 (throw 'dummy-tag 3) 4))
(eql-eval 4 '(catch 'dummy-tag 1 2 3 4))
(eq-eval 'throw-back '(defun throw-back (tag) (throw tag t)))
(eq-eval t '(catch 'dummy-tag (throw-back 'dummy-tag) 2))

;; char					- function
(eql-test #\a #'char "abc" 0)
(eql-test #\b #'char "abc" 1)
(error-test #'char "abc" 3)

;; char-*				- function
(eq-test nil #'alpha-char-p #\3)
(eq-test t #'alpha-char-p #\y)
(eql-test #\a #'char-downcase #\a)
(eql-test #\a #'char-downcase #\a)
(eql-test #\1 #'char-downcase #\1)
(error-test #'char-downcase 1)
(eql-test #\A #'char-upcase #\a)
(eql-test #\A #'char-upcase #\A)
(eql-test #\1 #'char-upcase #\1)
(error-test #'char-upcase 1)
(eq-test t #'lower-case-p #\a)
(eq-test nil #'lower-case-p #\A)
(eq-test t #'upper-case-p #\W)
(eq-test nil #'upper-case-p #\w)
(eq-test t #'both-case-p #\x)
(eq-test nil #'both-case-p #\%)
(eq-test t #'char= #\d #\d)
(eq-test t #'char-equal #\d #\d)
(eq-test nil #'char= #\A #\a)
(eq-test t #'char-equal #\A #\a)
(eq-test nil #'char= #\d #\x)
(eq-test nil #'char-equal #\d #\x)
(eq-test nil #'char= #\d #\D)
(eq-test t #'char-equal #\d #\D)
(eq-test nil #'char/= #\d #\d)
(eq-test nil #'char-not-equal #\d #\d)
(eq-test nil #'char/= #\d #\d)
(eq-test nil #'char-not-equal #\d #\d)
(eq-test t #'char/= #\d #\x)
(eq-test t #'char-not-equal #\d #\x)
(eq-test t #'char/= #\d #\D)
(eq-test nil #'char-not-equal #\d #\D)
(eq-test t #'char= #\d #\d #\d #\d)
(eq-test t #'char-equal #\d #\d #\d #\d)
(eq-test nil #'char= #\d #\D #\d #\d)
(eq-test t #'char-equal #\d #\D #\d #\d)
(eq-test nil #'char/= #\d #\d #\d #\d)
(eq-test nil #'char-not-equal #\d #\d #\d #\d)
(eq-test nil #'char/= #\d #\d #\D #\d)
(eq-test nil #'char-not-equal #\d #\d #\D #\d)
(eq-test nil #'char= #\d #\d #\x #\d)
(eq-test nil #'char-equal #\d #\d #\x #\d)
(eq-test nil #'char/= #\d #\d #\x #\d)
(eq-test nil #'char-not-equal #\d #\d #\x #\d)
(eq-test nil #'char= #\d #\y #\x #\c)
(eq-test nil #'char-equal #\d #\y #\x #\c)
(eq-test t #'char/= #\d #\y #\x #\c)
(eq-test t #'char-not-equal #\d #\y #\x #\c)
(eq-test nil #'char= #\d #\c #\d)
(eq-test nil #'char-equal #\d #\c #\d)
(eq-test nil #'char/= #\d #\c #\d)
(eq-test nil #'char-not-equal #\d #\c #\d)
(eq-test t #'char< #\d #\x)
(eq-test t #'char-lessp #\d #\x)
(eq-test t #'char-lessp #\d #\X)
(eq-test t #'char-lessp #\D #\x)
(eq-test t #'char-lessp #\D #\X)
(eq-test t #'char<= #\d #\x)
(eq-test t #'char-not-greaterp #\d #\x)
(eq-test t #'char-not-greaterp #\d #\X)
(eq-test t #'char-not-greaterp #\D #\x)
(eq-test t #'char-not-greaterp #\D #\X)
(eq-test nil #'char< #\d #\d)
(eq-test nil #'char-lessp #\d #\d)
(eq-test nil #'char-lessp #\d #\D)
(eq-test nil #'char-lessp #\D #\d)
(eq-test nil #'char-lessp #\D #\D)
(eq-test t #'char<= #\d #\d)
(eq-test t #'char-not-greaterp #\d #\d)
(eq-test t #'char-not-greaterp #\d #\D)
(eq-test t #'char-not-greaterp #\D #\d)
(eq-test t #'char-not-greaterp #\D #\D)
(eq-test t #'char< #\a #\e #\y #\z)
(eq-test t #'char-lessp #\a #\e #\y #\z)
(eq-test t #'char-lessp #\a #\e #\y #\Z)
(eq-test t #'char-lessp #\a #\E #\y #\z)
(eq-test t #'char-lessp #\A #\e #\y #\Z)
(eq-test t #'char<= #\a #\e #\y #\z)
(eq-test t #'char-not-greaterp #\a #\e #\y #\z)
(eq-test t #'char-not-greaterp #\a #\e #\y #\Z)
(eq-test t #'char-not-greaterp #\A #\e #\y #\z)
(eq-test nil #'char< #\a #\e #\e #\y)
(eq-test nil #'char-lessp #\a #\e #\e #\y)
(eq-test nil #'char-lessp #\a #\e #\E #\y)
(eq-test nil #'char-lessp #\A #\e #\E #\y)
(eq-test t #'char<= #\a #\e #\e #\y)
(eq-test t #'char-not-greaterp #\a #\e #\e #\y)
(eq-test t #'char-not-greaterp #\a #\E #\e #\y)
(eq-test t #'char> #\e #\d)
(eq-test t #'char-greaterp #\e #\d)
(eq-test t #'char-greaterp #\e #\D)
(eq-test t #'char-greaterp #\E #\d)
(eq-test t #'char-greaterp #\E #\D)
(eq-test t #'char>= #\e #\d)
(eq-test t #'char-not-lessp #\e #\d)
(eq-test t #'char-not-lessp #\e #\D)
(eq-test t #'char-not-lessp #\E #\d)
(eq-test t #'char-not-lessp #\E #\D)
(eq-test t #'char> #\d #\c #\b #\a)
(eq-test t #'char-greaterp #\d #\c #\b #\a)
(eq-test t #'char-greaterp #\d #\c #\b #\A)
(eq-test t #'char-greaterp #\d #\c #\B #\a)
(eq-test t #'char-greaterp #\d #\C #\b #\a)
(eq-test t #'char-greaterp #\D #\C #\b #\a)
(eq-test t #'char>= #\d #\c #\b #\a)
(eq-test t #'char-not-lessp #\d #\c #\b #\a)
(eq-test t #'char-not-lessp #\d #\c #\b #\A)
(eq-test t #'char-not-lessp #\D #\c #\b #\a)
(eq-test t #'char-not-lessp #\d #\C #\B #\a)
(eq-test nil #'char> #\d #\d #\c #\a)
(eq-test nil #'char-greaterp #\d #\d #\c #\a)
(eq-test nil #'char-greaterp #\d #\d #\c #\A)
(eq-test nil #'char-greaterp #\d #\D #\c #\a)
(eq-test nil #'char-greaterp #\d #\D #\C #\a)
(eq-test t #'char>= #\d #\d #\c #\a)
(eq-test t #'char-not-lessp #\d #\d #\c #\a)
(eq-test t #'char-not-lessp #\d #\D #\c #\a)
(eq-test t #'char-not-lessp #\D #\d #\c #\a)
(eq-test t #'char-not-lessp #\D #\D #\c #\A)
(eq-test nil #'char> #\e #\d #\b #\c #\a)
(eq-test nil #'char-greaterp #\e #\d #\b #\c #\a)
(eq-test nil #'char-greaterp #\E #\d #\b #\c #\a)
(eq-test nil #'char-greaterp #\e #\D #\b #\c #\a)
(eq-test nil #'char-greaterp #\E #\d #\B #\c #\A)
(eq-test nil #'char>= #\e #\d #\b #\c #\a)
(eq-test nil #'char-not-lessp #\e #\d #\b #\c #\a)
(eq-test nil #'char-not-lessp #\e #\d #\b #\c #\A)
(eq-test nil #'char-not-lessp #\E #\d #\B #\c #\a)

;; char-code				- function
;; XXX assumes ASCII
(eql-test 49 #'char-code #\1)
(eql-test 90 #'char-code #\Z)
(eql-test 127 #'char-code #\Delete)
(eql-test 27 #'char-code #\Escape)
(eql-test 13 #'char-code #\Return)
(eql-test 0 #'char-code #\Null)
(eql-test 10 #'char-code #\Newline)
(error-test #'char-code 65)

;; character				- function
(eql-test #\a #'character #\a)
(eql-test #\a #'character "a")
(eql-test #\A #'character 'a)

;; XXX assumes ASCII, and should be allowed to fail?
(eql-test #\A #'character 65)

(error-test #'character 1/2)
(error-test #'character "abc")
(error-test #'character :test)
(eq-test #\T #'character t)
(error-test #'character nil)

;; characterp				- function
(eq-test t #'characterp #\a)
(eq-test nil #'characterp 1)
(eq-test nil #'characterp 1/2)
(eq-test nil #'characterp 'a)
(eq-test nil #'characterp '`a)




;; TODO coerce




;; cond					- macro
(eql-eval 2 '(let ((a 1)) (cond ((= a 2) 1) ((= a 1) 2) ((= a 0) 1) (t nil))))
(eql-eval nil '(let ((a 1)) (cond ((= a 2) 1) (t nil) ((= a 1) 2) ((= a 0) 1))))

;; consp				- function (predicate)
(eq-test t #'consp '(1 2))
(eq-test t #'consp '(1 . 2))
(eq-test nil #'consp nil)
(eq-test nil #'consp 1)

;; constantp				- function (predicate)
(eq-test t #'constantp 1)
(eq-test t #'constantp #\x)
(eq-test t #'constantp :test)
(eq-test nil #'constantp 'test)
(eq-test t #'constantp ''1)
(eq-test t #'constantp '(quote 1))
(eq-test t #'constantp "string")
(eq-test t #'constantp #c(1 2))
(eq-test t #'constantp #(1 2))
(eq-test nil #'constantp #p"test")
(eq-test nil #'constantp '(1 2))
(eq-test nil #'constantp (make-hash-table))
(eq-test nil #'constantp *package*)
(eq-test nil #'constantp *standard-input*)

;; copy-list, copy-alist and copy-tree	- function
(equal-test '(1 2) #'copy-list '(1 2))
(equal-test '(1 . 2) #'copy-list '(1 . 2))
(eq-test nil #'copy-list nil)
(error-test #'copy-list 1)
(equal-eval '(1 (2 3)) '(setq x '(1 (2 3))))
(equal-eval x '(setq y (copy-list x)))
(equal-test '("one" (2 3)) #'rplaca x "one")
(eql-test 1 #'car y)
(equal-test '("two" 3) #'rplaca (cadr x) "two")
(eq-test (caadr x) #'caadr y)
(equal-eval '(1 (2 3) 4) '(setq a '(1 (2 3) 4) b (copy-list a)))
(eq-eval t '(eq (cadr a) (cadr b)))
(eq-eval t '(eq (car a) (car b)))
(setq a '(1 (2 3) 4) b (copy-alist a))
(eq-eval nil '(eq (cadr a) (cadr b)))
(eq-eval t '(eq (car a) (car b)))
(eq-test nil #'copy-alist nil)
(eq-test nil #'copy-list nil)
(error-test #'copy-list 1)
(setq a '(1 (2 (3))))
(setq as-list (copy-list a))
(setq as-alist (copy-alist a))
(setq as-tree (copy-tree a))
(eq-eval t '(eq (cadadr a) (cadadr as-list)))
(eq-eval t '(eq (cadadr a) (cadadr as-alist)))
(eq-eval nil '(eq (cadadr a) (cadadr as-tree)))

;; decf					- macro
(setq n 2)
(eql-eval 1 '(decf n))
(eql-eval 1 'n)
(setq n -2147483648)
(eql-eval -2147483649 '(decf n))
(eql-eval -2147483649 'n)
(setq n 0)
(eql-eval -0.5d0 '(decf n 0.5d0))
(eql-eval -0.5d0 'n)
(setq n 1)
(eql-eval 1/2 '(decf n 1/2))
(eql-eval 1/2 'n)

;; delete and remove			- function
(setq a '(1 3 4 5 9) b a)
(equal-test '(1 3 5 9) #'remove 4 a)
(eq-eval t '(eq a b))
(setq a (delete 4 a))
(equal-eval '(1 3 5 9) 'a)
(setq a '(1 2 4 1 3 4 5) b a)
(equal-test '(1 2 1 3 5) #'remove 4 a)
(eq-eval t '(eq a b))
(equal-test '(1 2 1 3 4 5) #'remove 4 a :count 1)
(eq-eval t '(eq a b))
(equal-test '(1 2 4 1 3 5) #'remove 4 a :count 1 :from-end t)
(eq-eval t '(eq a b))
(equal-test '(4 3 4 5) #'remove 3 a :test #'>)
(eq-eval t '(eq a b))
(setq a (delete 4 '(1 2 4 1 3 4 5)))
(equal-eval '(1 2 1 3 5) 'a)
(setq a (delete 4 '(1 2 4 1 3 4 5) :count 1))
(equal-eval '(1 2 1 3 4 5) 'a)
(setq a (delete 4 '(1 2 4 1 3 4 5) :count 1 :from-end t))
(equal-eval '(1 2 4 1 3 5) 'a)
(equal-test "abc" #'delete-if #'digit-char-p "a1b2c3")
(equal-test "123" #'delete-if-not #'digit-char-p "a1b2c3")
(eq-test nil #'delete 1 nil)
(eq-test nil #'remove 1 nil)
(setq a '(1 2 3 4 :test 5 6 7 8) b a)
(equal-test '(1 2 :test 7 8) #'remove-if #'numberp a :start 2 :end 7)
(eq-eval t '(eq a b))
(setq a (delete-if #'numberp a :start 2 :end 7))
(equal-eval '(1 2 :test 7 8) 'a)

;; digit-char				- function
(eql-test #\0 #'digit-char 0)
(eql-test #\A #'digit-char 10 11)
(eq-test nil #'digit-char 10 10)
(eql-test 35 #'digit-char-p #\z 36)
(error-test #'digit-char #\a)
(error-test #'digit-char-p 1/2)



;; TODO directory (known to have problems with parameters like "../*/../*/")



;; elt					- function
(eql-test #\a #'elt "xabc" 1)
(eql-test 3 #'elt '(0 1 2 3) 3)
(error-test #'elt nil 0)

;; endp					- function
(eql-test t #'endp nil)
(error-test #'endp t)
(eql-test nil #'endp '(1 . 2))
(error-test #'endp #(1 2))

;; every				- function
(eql-test t #'every 'not-used ())
(eql-test t #'every #'characterp "abc")
(eql-test nil #'every #'< '(1 2 3) '(4 5 6) #(7 8 -1))
(eql-test t #'every #'< '(1 2 3) '(4 5 6) #(7 8))

;; fboundp and fmakunbound		- function
(eq-test t #'fboundp 'car)
(eq-eval 'test '(defun test ()))
(eq-test t #'fboundp 'test)
(eq-test 'test #'fmakunbound 'test)
(eq-test nil #'fboundp 'test)
(eq-eval 'test '(defmacro test (x) x))
(eq-test t #'fboundp 'test)
(eq-test 'test #'fmakunbound 'test)

;; fill					- function
(setq x (list 1 2 3 4))
(equal-test '((4 4 4 4) (4 4 4 4) (4 4 4 4) (4 4 4 4)) #'fill x '(4 4 4 4))
(eq-eval t '(eq (car x) (cadr x)))
(equalp-test '#(a z z d e) #'fill '#(a b c d e) 'z :start 1 :end 3)
(equal-test "012ee" #'fill (xseq "01234") #\e :start 3)
(error-test #'fill 1 #\a)

;; find					- function
(eql-test #\Space #'find #\d "here are some letters that can be looked at" :test #'char>)
(eql-test 3 #'find-if #'oddp '(1 2 3 4 5) :end 3 :from-end t)
(eq-test nil #'find-if-not #'complexp '#(3.5 2 #C(1.0 0.0) #C(0.0 1.0)) :start 2)
(eq-test nil #'find 1 "abc")
(error-test #'find 1 #c(1 2))

;; find-symbol				- function
(equal-eval '(nil nil)
    '(multiple-value-list (find-symbol "NEVER-BEFORE-USED")))
(equal-eval '(nil nil)
    '(multiple-value-list (find-symbol "NEVER-BEFORE-USED")))
(setq test (multiple-value-list (intern "NEVER-BEFORE-USED")))
(equal-eval test '(read-from-string "(never-before-used nil)"))
(equal-eval '(never-before-used :internal)
    '(multiple-value-list (intern "NEVER-BEFORE-USED")))
(equal-eval '(never-before-used :internal)
    '(multiple-value-list (find-symbol "NEVER-BEFORE-USED")))
(equal-eval '(nil nil)
    '(multiple-value-list (find-symbol "never-before-used")))
(equal-eval '(car :inherited)
    '(multiple-value-list (find-symbol "CAR" 'common-lisp-user)))
(equal-eval '(car :external)
   '(multiple-value-list  (find-symbol "CAR" 'common-lisp)))
;; XXX these will generate wrong results, NIL is not really a symbol
;; currently in the interpreter
(equal-eval '(nil :inherited)
    '(multiple-value-list (find-symbol "NIL" 'common-lisp-user)))
(equal-eval '(nil :external)
    '(multiple-value-list (find-symbol "NIL" 'common-lisp)))
(setq test (multiple-value-list
     (find-symbol "NIL" (prog1 (make-package "JUST-TESTING" :use '())
			       (intern "NIL" "JUST-TESTING")))))
(equal-eval (read-from-string "(just-testing::nil :internal)") 'test)
(eq-eval t '(export 'just-testing::nil 'just-testing))
(equal-eval '(just-testing:nil :external)
    '(multiple-value-list (find-symbol "NIL" 'just-testing)))

#+xedit (equal-eval '(nil nil)
	'(multiple-value-list (find-symbol "NIL" "KEYWORD")))
#|
;; optional result of previous form:
(equal-eval '(:nil :external)
    '(multiple-value-list (find-symbol "NIL" "KEYWORD")))
|#



;; funcall				- function
(eql-test 6 #'funcall #'+ 1 2 3)
(eql-test 1 #'funcall #'car '(1 2 3))
(equal-test '(1 2 3) #'funcall #'list 1 2 3)



;; TODO properly implement ``function''



;; functionp				- function (predicate)
(eq-test nil #'functionp 'append)
(eq-test t #'functionp #'append)
(eq-test nil #'functionp '(lambda (x) (* x x)))
(eq-test t #'functionp #'(lambda (x) (* x x)))
(eq-test t #'functionp (symbol-function 'append))
(eq-test nil #'functionp 1)
(eq-test nil #'functionp nil)

;; gensym				- function
(setq sym1 (gensym))
(eq-test nil #'symbol-package sym1)
(setq sym1 (gensym 100))
(setq sym2 (gensym 100))
(eq-test nil #'eq sym1 sym2)
(eq-test nil #'equalp (gensym) (gensym))

;; get					- accessor
(defun make-person (first-name last-name)
  (let ((person (gensym "PERSON")))
    (setf (get person 'first-name) first-name)
    (setf (get person 'last-name) last-name)
    person))
(eq-eval '*john* '(defvar *john* (make-person "John" "Dow")))
(eq-eval '*sally* '(defvar *sally* (make-person "Sally" "Jones")))
(equal-eval "John" '(get *john* 'first-name))
(equal-eval "Jones" '(get *sally* 'last-name))
(defun marry (man woman married-name)
  (setf (get man 'wife) woman)
  (setf (get woman 'husband) man)
  (setf (get man 'last-name) married-name)
  (setf (get woman 'last-name) married-name)
  married-name)
(equal-eval "Dow-Jones" '(marry *john* *sally* "Dow-Jones"))
(equal-eval "Dow-Jones" '(get *john* 'last-name))
(equal-eval "Sally" '(get (get *john* 'wife) 'first-name))
(equal-eval `(wife ,*sally* last-name "Dow-Jones" first-name "John")
    '(symbol-plist *john*))
(eq-eval 'age
    '(defmacro age (person &optional (default ''thirty-something))
      `(get ,person 'age ,default)))
(eq-eval 'thirty-something '(age *john*))
(eql-eval 20 '(age *john* 20))
(eql-eval 25 '(setf (age *john*) 25))
(eql-eval 25 '(age *john*))
(eql-eval 25 '(age *john* 20))

;; graphic-char-p			- function
(eq-test t #'graphic-char-p #\a)
(eq-test t #'graphic-char-p #\Space)
(eq-test nil #'graphic-char-p #\Newline)
(eq-test nil #'graphic-char-p #\Tab)
(eq-test nil #'graphic-char-p #\Rubout)

;; if					- special operator
(eq-eval nil '(if nil t))
(eq-eval nil '(if t nil t))
(eq-eval nil '(if nil t nil))
(eq-eval nil '(if nil t (if nil (if nil t) nil)))

;; incf					- macro
(setq n 1)
(eql-eval 2 '(incf n))
(eql-eval 2 'n)
(setq n 2147483647)
(eql-eval 2147483648 '(incf n))
(eql-eval 2147483648 'n)
(setq n 0)
(eql-eval 0.5d0 '(incf n 0.5d0))
(eql-eval 0.5d0 'n)
(setq n 1)
(eql-eval 3/2 '(incf n 1/2))
(eql-eval 3/2 'n)

;; intersection				- function
(setq list1 (list 1 1 2 3 4 'a 'b 'c "A" "B" "C" "d")
      list2 (list 1 4 5 'b 'c 'd "a" "B" "c" "D"))
(equal-test '(1 1 4 b c) #'intersection list1 list2)
(equal-test '(1 1 4 b c "B") #'intersection list1 list2 :test 'equal)
(equal-test '(1 1 4 b c "A" "B" "C" "d")
    #'intersection list1 list2 :test #'equalp)
(setq list1 (nintersection list1 list2))
(equal-eval '(1 1 4 b c) 'list1)
(setq list1 (copy-list '((1 . 2) (2 . 3) (3 . 4) (4 . 5))))
(setq list2 (copy-list '((1 . 3) (2 . 4) (3 . 6) (4 . 8))))
(equal-test '((2 . 3) (3 . 4)) #'nintersection list1 list2 :key #'cdr)

;; keywordp				- function (predicate)
(eq-test t #'keywordp :test)
(eq-test nil #'keywordp 'test)
(eq-test nil #'keywordp '#:test)
(eq-test nil #'keywordp 1)
(eq-test nil #'keywordp #'keywordp)
(eq-test nil #'keywordp nil)

;; last					- function
(equal-test '(3) #'last '(1 2 3))
(equal-test '(2 . 3) #'last '(1 2 . 3))
(eq-test nil #'last nil)
(eql-test () #'last '(1 2 3) 0)
(setq a '(1 . 2))
(eql-test 2 #'last a 0)
(eq-test a #'last a 1)
(eq-test a #'last a 2)
(eq-test t #'last t)
(equal-test #c(1 2) #'last #c(1 2))
(equalp-test #(1 2 3) #'last #(1 2 3))

;; length				- function
(eql-test 3 #'length "abc")
(eql-test 0 #'length nil)
(eql-test 1 #'length '(1 . 2))
(eql-test 2 #'length #(1 2))
(error-test #'length #c(1 2))
(error-test #'length t)

;; let					- special operator
(eql-eval 2 '(setq a 1 b 2))
(eql-eval 2 '(let ((a 2)) a))
(eql-eval 1 'a)
(eql-eval 1 '(let ((a 3) (b a)) b))
(eql-eval 2 'b)

;; let*					- special operator
(setq a 1 b 2)
(eql-eval 2 '(let* ((a 2)) a))
(eql-eval 1 'a)
(eql-eval 3 '(let* ((a 3) (b a)) b))
(eql-eval 2 'b)

;; list					- function
(equal-test '(1) #'list 1)
(equal-test '(3 4 a b 4) #'list 3 4 'a (car '(b . c)) (+ 6 -2))
(eq-test nil #'list)

;; list-length				- function
(eql-test 4 #'list-length '(a b c d))
(eql-test 3 #'list-length '(a (b c) d))
(eql-test 0 #'list-length '())
(eql-test 0 #'list-length nil)
(defun circular-list (&rest elements)
  (let ((cycle (copy-list elements))) 
    (nconc cycle cycle)))
(eq-test nil #'list-length (circular-list 'a 'b))
(eq-test nil #'list-length (circular-list 'a))
(eql-test 0 #'list-length (circular-list))

;; list*				- function
(eql-test 1 #'list* 1)
(equal-test '(a b c . d) #'list* 'a 'b 'c 'd)
(error-test #'list*)
(setq a '(1 2))
(eq-test a #'list* a)

;; listp				- function (predicate)
(eq-test t #'listp nil)
(eq-test t #'listp '(1 . 2))
(eq-test nil #'listp t)
(eq-test nil #'listp #'listp)
(eq-test nil #'listp #(1 2))
(eq-test nil #'listp #c(1 2))

;; lower-case-p				- function
(eq-test t #'lower-case-p #\a)
(eq-test nil #'lower-case-p #\1)
(eq-test nil #'lower-case-p #\Newline)
(error-test #'lower-case-p 1)



;; TODO make-array	(will be rewritten)



;; make-list				- function
(equal-test '(nil nil nil) #'make-list 3)
(equal-test '((1 2) (1 2)) #'make-list 2 :initial-element '(1 2))
(eq-test nil #'make-list 0)
(eq-test nil #'make-list 0 :initial-element 1)

;; make-package				- function
(setq pack1 (make-package "PACKAGE-1" :nicknames '("PACK-1" "PACK1")))
(setq pack2 (make-package "PACKAGE-2" :nicknames '("PACK-2" "PACK2") :use '("PACK1")))
(equal-test (list pack2) #'package-used-by-list pack1)
(equal-test (list pack1) #'package-use-list pack2)
(eq-test pack1 #'symbol-package 'pack1::test)
(eq-test pack2 #'symbol-package 'pack2::test)

;; make-string				- function
(equal-test "55555" #'make-string 5 :initial-element #\5)
(equal-test "" #'make-string 0)
(error-test #'make-string 10 :initial-element t)
(error-test #'make-string 10 :initial-element nil)
(error-test #'make-string 10 :initial-element 1)
(eql-test 10 #'length (make-string 10))

;; make-symbol				- function
(setq a "TEST")
;; This will fail
(eq-test nil #'eq (make-symbol a) (make-symbol a))
(equal-test a #'symbol-name (make-symbol a))
(setq temp-string "temp")
(setq temp-symbol (make-symbol temp-string))
(equal-test temp-string #'symbol-name temp-symbol)
(equal-eval '(nil nil) '(multiple-value-list (find-symbol temp-string)))

;; makunbound				- function
(eq-eval 1 '(setf (symbol-value 'a) 1))
(eq-test t #'boundp 'a)
(eql-eval 1 'a)
(eq-test 'a #'makunbound 'a)
(eq-test nil #'boundp 'a)
(error-test #'makunbound 1)

;; mapc					- function
(setq dummy nil)
(equal-test '(1 2 3 4)
    #'mapc #'(lambda (&rest x) (setq dummy (append dummy x)))
   '(1 2 3 4)
   '(a b c d e)
   '(x y z))
(equal-eval '(1 a x 2 b y 3 c z) 'dummy)

;; mapcan				- function
(equal-test '(d 4 e 5)
    #'mapcan #'(lambda (x y) (if (null x) nil (list x y)))
    '(nil nil nil d e)
    '(1 2 3 4 5 6))
(equal-test '(1 3 4 5)
    #'mapcan #'(lambda (x) (and (numberp x) (list x)))
    '(a 1 b c 3 4 d 5))

;; mapcar				- function
(equal-test '(1 2 3) #'mapcar #'car '((1 a) (2 b) (3 c)))
(equal-test '(3 4 2 5 6) #'mapcar #'abs '(3 -4 2 -5 -6))
(equal-test '((a . 1) (b . 2) (c . 3)) #'mapcar #'cons '(a b c) '(1 2 3))
(equal-test '((1 3 5)) #'mapcar #'list* '(1 2) '(3 4) '((5)))
(equal-test '((1 3 5) (2 4 6)) #'mapcar #'list* '(1 2) '(3 4) '((5) (6)))

;; mapcon				- function
(equal-test '(1 a 2 b (3) c) #'mapcon #'car '((1 a) (2 b) ((3) c)))
(equal-test '((1 2 3 4) (2 3 4) (3 4) (4)) #'mapcon #'list '(1 2 3 4))

;; mapl					- function
(setq dummy nil)
(equal-test '(1 2 3 4) #'mapl #'(lambda (x) (push x dummy)) '(1 2 3 4))
(equal-eval '((4) (3 4) (2 3 4) (1 2 3 4)) 'dummy)

;; maplist				- function
(equal-test '((1 2 3 4 1 2 1 2 3) (2 3 4 2 2 3))
    #'maplist #'append '(1 2 3 4) '(1 2) '(1 2 3))
(equal-test '((foo a b c d) (foo b c d) (foo c d) (foo d))
    #'maplist #'(lambda (x) (cons 'foo x)) '(a b c d))
(equal-test '(0 0 1 0 1 1 1)
    #'maplist #'(lambda (x) (if (member (car x) (cdr x)) 0 1)) '(a b a c d b c))

;; member				- function
(setq a '(1 2 3))
(eq-test (cdr a) #'member 2 a)
(setq a '((1 . 2) (3 . 4)))
(eq-test (cdr a) #'member 2 a :test-not #'= :key #'cdr)
(eq-test nil #'member 'e '(a b c d))
(eq-test nil #'member 1 nil)
(error-test #'member 2 '(1 . 2))
(setq a '(a b nil c d))
(eq-test (cddr a) #'member-if #'listp a)
(setq a '(a #\Space 5/3 foo))
(eq-test (cddr a) #'member-if #'numberp a)
(setq a '(3 6 9 11 . 12))
(eq-test (cdddr a) #'member-if-not #'zerop a :key #'(lambda (x) (mod x 3)))

;; multiple-value-bind			- macro
(equal-eval '(11 9) '(multiple-value-bind (f r) (floor 130 11) (list f r)))

;; multiple-value-call			- special operator
(equal-eval '(1 / 2 3 / / 2 0.5)
    '(multiple-value-call #'list 1 '/ (values 2 3) '/ (values) '/ (floor 2.5)))
(eql-eval 10 '(multiple-value-call #'+ (floor 5 3) (floor 19 4)))

;; multiple-value-list			- macro
(equal-eval '(-1 1) '(multiple-value-list (floor -3 4)))
(eql-eval nil '(multiple-value-list (values)))
(equal-eval '(nil) '(multiple-value-list (values nil)))

;; multiple-value-prog1			- special operator
(setq temp '(1 2 3))
(equal-eval temp
    '(multiple-value-list
	(multiple-value-prog1
	    (values-list temp)
	    (setq temp nil)
	    (values-list temp))))

;; multiple-value-setq			- macro
(eql-eval 1 '(multiple-value-setq (quotient remainder) (truncate 3.5d0 2)))
(eql-eval 1 quotient)
(eql-eval 1.5d0 'remainder)
(eql-eval 1 '(multiple-value-setq (a b c) (values 1 2)))
(eql-eval 1 'a)
(eql-eval 2 'b)
(eq-eval nil 'c)
(eql-eval 4 '(multiple-value-setq (a b) (values 4 5 6)))
(eql-eval 4 'a)
(eql-eval 5 'b)
(setq a 1)
(eql-eval nil '(multiple-value-setq (a) (values)))
(eql-eval nil 'a)

;; nconc				- function
(eq-test nil #'nconc)
(setq x '(a b c))
(setq y '(d e f))
(equal-test '(a b c d e f) #'nconc x y)
(equal-eval '(a b c d e f) 'x)
(eq-test y #'cdddr x)
(equal-test '(1 . 2) #'nconc (list 1) 2)
(error-test #'nconc 1 2 3)
(equal-eval '(k l m)
   '(setq foo (list 'a 'b 'c 'd 'e)
	  bar (list 'f 'g 'h 'i 'j)
	  baz (list 'k 'l 'm)))
(equal-test '(a b c d e f g h i j k l m) #'nconc foo bar baz)
(equal-eval '(a b c d e f g h i j k l m) 'foo)
(equal-eval (nthcdr 5 foo) 'bar)
(equal-eval (nthcdr 10 foo) 'baz)
(setq foo (list 'a 'b 'c 'd 'e)
      bar (list 'f 'g 'h 'i 'j)
      baz (list 'k 'l 'm))
(equal-eval '(a b c d e f g h i j k l m) '(setq foo (nconc nil foo bar nil baz)))
(equal-eval '(a b c d e f g h i j k l m) 'foo)
(equal-eval (nthcdr 5 foo) 'bar)
(equal-eval (nthcdr 10 foo) 'baz)

;; notany				- function
(eql-test t #'notany #'> '(1 2 3 4) '(5 6 7 8) '(9 10 11 12))
(eql-test t #'notany 'not-used ())
(eql-test nil #'notany #'characterp #(1 2 3 4 5 #\6 7 8))

;; notevery				- function
(eql-test nil #'notevery #'< '(1 2 3 4) '(5 6 7 8) '(9 10 11 12))
(eql-test nil #'notevery 'not-used ())
(eql-test t #'notevery #'numberp #(1 2 3 4 5 #\6 7 8))

;; nth					- accessor (function)
(eql-test 'foo #'nth 0 '(foo bar baz))
(eql-test 'bar #'nth 1 '(foo bar baz))
(eq-test nil #'nth 3 '(foo bar baz))
(error-test #'nth 0 #c(1 2))
(error-test #'nth 0 #(1 2))
(error-test #'nth 0 "test")

;; nth-value				- macro
(equal-eval 'a '(nth-value 0 (values 'a 'b)))
(equal-eval 'b '(nth-value 1 (values 'a 'b)))
(eq-eval nil '(nth-value 2 (values 'a 'b)))
(equal-eval '(3332987528 3332987528 t)
    '(multiple-value-list
	(let* ((x 83927472397238947423879243432432432)
	       (y 32423489732)
	       (a (nth-value 1 (floor x y)))
	       (b (mod x y)))
	  (values a b (= a b)))))

;; nthcdr				- function
(eq-test nil #'nthcdr 0 '())
(eq-test nil #'nthcdr 3 '())
(equal-test '(a b c) #'nthcdr 0 '(a b c))
(equal-test '(c) #'nthcdr 2 '(a b c))
(eq-test () #'nthcdr 4 '(a b c))
(eql-test 1 #'nthcdr 1 '(0 . 1))
(error-test #'nthcdr -1 '(1 2))
(error-test #'nthcdr #\Null '(1 2))
(error-test #'nthcdr 1 t)
(error-test #'nthcdr 1 #(1 2 3))

;; or					- macro
(eq-eval nil '(or))
(setq temp0 nil temp1 10 temp2 20 temp3 30)
(eql-eval 10 '(or temp0 temp1 (setq temp2 37)))
(eql-eval 20 'temp2)
(eql-eval 11 '(or (incf temp1) (incf temp2) (incf temp3)))
(eql-eval 11 'temp1)
(eql-eval 20 temp2)
(eql-eval 30 'temp3)
(eql-eval 11 '(or (values) temp1))
(eql-eval 11 '(or (values temp1 temp2) temp3))
(equal-eval '(11 20) '(multiple-value-list (or temp0 (values temp1 temp2))))
(equal-eval '(20 30)
    '(multiple-value-list (or (values temp0 temp1) (values temp2 temp3))))

;; packagep				- function (predicate)
(eq-test t #'packagep *package*)
(eq-test nil #'packagep 10)
(eq-test t #'packagep (make-package "TEST-PACKAGE"))
(eq-test nil #'packagep 'keyword)
(eq-test t #'packagep (find-package 'keyword))

;; pairlis				- function
#+xedit	;; order of result may vary
(progn
    (equal-test '((one . 1) (two . 2) (three . 3) (four . 19))
	#'pairlis '(one two) '(1 2) '((three . 3) (four . 19)))
    (setq keys '(1 2 3)
	  data '("one" "two" "three")
	  alist '((4 . "four")))
    (equal-test '((1 . "one") (2 . "two") (3 . "three"))
	#'pairlis keys data)
    (equal-test '((1 . "one") (2 . "two") (3 . "three") (4 . "four"))
	#'pairlis keys data alist)
    (equal-eval '(1 2 3) 'keys)
    (equal-eval '("one" "two" "three") 'data)
    (equal-eval '((4 . "four")) 'alist)
    (eq-test nil #'pairlis 1 2)
    (error-test #'pairlis '(1 2 3) '(4 5))
)

;; pop					- macro
(setq stack '(a b c) test stack)
(eq-eval 'a '(pop stack))
(eq-eval (cdr test) 'stack)
(setq llst '((1 2 3 4)) test (car llst))
(eq-eval 1 '(pop (car llst)))
(eq-eval (cdr test) '(car llst))
(error-eval '(pop 1))
(error-eval '(pop nil))
;; dotted list
(setq stack (cons 1 2))
(eq-eval 1 '(pop stack))
(error-eval '(pop stack))
;; circular list
(setq stack '#1=(1 . #1#) *print-circle* t)
(eql-eval 1 '(pop stack))
(eql-eval 1 '(pop stack))
(eql-eval 1 '(pop (cdr stack)))

;; position				- function
(eql-test 4 #'position #\a "baobab" :from-end t)
(eql-test 2 #'position-if #'oddp '((1) (2) (3) (4)) :start 1 :key #'car)
(eq-test nil #'position 595 '())
(eq-test 4 #'position-if-not #'integerp '(1 2 3 4 5.0))
(eql-test 1 #'position (char-int #\1) "0123" :key #'char-int)

;; prog					- macro
(eq-eval nil '(prog () :error))
(eq-eval 'ok
    '(prog ((a 0))
	l1 (if (< a 10) (go l3) (go l2))
	(return 'failed)
	l2 (return 'ok)
	(return 'failed)
	l3 (incf a) (go l1)
	(return 'failed)
    ))
(setq a 1)
(eq-eval '/= '(prog ((a 2) (b a)) (return (if (= a b) '= '/=))))

;; prog*				- macro
(setq a 1)
(eq-eval nil '(prog* () :error))
(eq-eval 'ok
    '(prog* ((a 0) (b 0))
	l1 (if (< a 10) (go l3) (go l2))
	(return 'failed)
	l2 (if (< b 10) (go l4) (return 'ok))
	(return 'failed)
	l3 (incf a) (go l1)
	(return 'failed)
	l4 (incf b) (setq a 0) (go l1)
	(return 'failed)
    ))
(eq-eval '= '(prog* ((a 2) (b a)) (return (if (= a b) '= '/=))))

;; prog1				- macro
(setq temp 1)
(eql-eval 1 '(prog1 temp (incf temp) (eql-eval 2 'temp) temp))
(eql-eval 2 'temp)
(eql-eval 2 '(prog1 temp (setq temp nil) (eql-eval nil 'temp) temp))
(eq-eval nil 'temp)
(eql-eval 1 '(prog1 (values 1 2 3) 4))
(setq temp (list 'a 'b 'c))
(eq-eval 'a '(prog1 (car temp) (setf (car temp) 'alpha)))
(equal-eval '(alpha b c) 'temp)
(equal-eval '(1)
    '(multiple-value-list (prog1 (values 1 2) (values 4 5))))

;; prog2				- macro
(setq temp 1)
(eql-eval 3 '(prog2 (incf temp) (incf temp) (incf temp)))
(eql-eval 4 'temp)
(eql-eval 2 '(prog2 1 (values 2 3 4) 5))
(equal-eval '(3)
    '(multiple-value-list (prog2 (values 1 2) (values 3 4) (values 5 6))))

;; progn				- special operator
(eq-eval nil '(progn))
(eql-eval 3 '(progn 1 2 3))
(equal-eval '(1 2 3) '(multiple-value-list (progn (values 1 2 3))))
(setq a 1)
(eq-eval 'here '(if a (progn (setq a nil) 'here) (progn (setq a t) 'there)))
(eq-eval nil 'a)

;; progv				- special operator
(makunbound '*x*)	;; make sure it is not bound
(setq *x* 1)
(eql-eval 2 '(progv '(*x*) '(2) *x*))
(eql-eval 1 '*x*)
(equal-eval '(3 4)
    '(let ((*x* 3)) (progv '(*x*) '(4) (list *x* (symbol-value '*x*)))))
(makunbound '*x*)
(defvar *x* 1)
(equal-eval '(4 4)
    '(let ((*x* 3)) (progv '(*x*) '(4) (list *x* (symbol-value '*x*)))))
(equal-eval '(4 4)
    '(multiple-value-list
	(let ((*x* 3))
	     (progv '(*x*) '(4) (values-list (list *x* (symbol-value '*x*)))))))

;; push					- macro
(setq llst '(nil))
(equal-eval '(1) '(push 1 (car llst)))
(equal-eval '((1)) 'llst)
(equal-eval '(1 1) '(push 1 (car llst)))
(equal-eval '((1 1)) 'llst)
(setq x '(a (b c) d))
(equal-eval '(5 B C) '(push 5 (cadr x)))
(equal-eval '(a (5 b c) d) 'x)

;; pushnew				- macro
(setq x '(a (b c) d))
(equal-eval '(5 b c) '(pushnew 5 (cadr x)))
(equal-eval '(a (5 b c) d) 'x)
(equal-eval '(5 b c) '(pushnew 'b (cadr x)))
(equal-eval '(a (5 b c) d) 'x)
(setq lst '((1) (1 2) (1 2 3)))
(equal-eval '((2) (1) (1 2) (1 2 3)) '(pushnew '(2) lst))
(equal-eval '((1) (2) (1) (1 2) (1 2 3)) '(pushnew '(1) lst))
(equal-eval '((1) (2) (1) (1 2) (1 2 3)) '(pushnew '(1) lst :test 'equal))
(equal-eval '((1) (2) (1) (1 2) (1 2 3)) '(pushnew '(1) lst :key #'car))

;; remove-duplicates			- function
(equal-test "aBcD" #'remove-duplicates "aBcDAbCd" :test #'char-equal :from-end t)
(equal-test '(a c b d e) #'remove-duplicates '(a b c b d d e))
(equal-test '(a b c d e) #'remove-duplicates '(a b c b d d e) :from-end t)
(equal-test '((bar #\%) (baz #\A))
    #'remove-duplicates '((foo #\a) (bar #\%) (baz #\A))
     :test #'char-equal :key #'cadr)
(equal-test '((foo #\a) (bar #\%))
    #'remove-duplicates '((foo #\a) (bar #\%) (baz #\A))
     :test #'char-equal :key #'cadr :from-end t)
(setq tester (list 0 1 2 3 4 5 6))
(equal-test '(0 4 5 6) #'delete-duplicates tester :key #'oddp :start 1 :end 6)

;; replace				- function
(equal-test "abcd456hij"
    #'replace (copy-seq "abcdefghij") "0123456789" :start1 4 :end1 7 :start2 4)
(setq lst (xseq "012345678"))
(equal-test "010123456" #'replace lst lst :start1 2 :start2 0)
(equal-eval "010123456" 'lst)

;; rest					- accessor
(equal-eval '(2) '(rest '(1 2)))
(eql-eval 2 '(rest '(1 . 2)))
(eq-eval nil '(rest '(1)))
(setq *cons* '(1 . 2))
(equal-eval "two" '(setf (rest *cons*) "two"))
(equal-eval '(1 . "two") '*cons*)

;; return				- macro
(eq-eval nil '(block nil (return) 1))
(eql-eval 1 '(block nil (return 1) 2))
(equal-eval '(1 2) '(multiple-value-list (block nil (return (values 1 2)) 3)))
(eql-eval 1 '(block nil (block alpha (return 1) 2)))
(eql-eval 2 '(block alpha (block nil (return 1)) 2))
(eql-eval 1 '(block nil (block nil (return 1) 2)))

;; return-from				- special operator
(eq-eval nil '(block alpha (return-from alpha) 1))
(eql-eval 1 '(block alpha (return-from alpha 1) 2))
(equal-eval '(1 2)
    '(multiple-value-list (block alpha (return-from alpha (values 1 2)) 3)))
(eql-eval 2
    '(let ((a 0)) (dotimes (i 10) (incf a) (when (oddp i) (return))) a))
(eq-eval 'temp '(defun temp (x) (if x (return-from temp ''dummy)) 44))
(eql-eval 44 '(temp nil))
(eq-eval 'dummy (temp t))
(eql-eval 2 (block nil (unwind-protect (return-from nil 1) (return-from nil 2))))
(error-eval '(funcall (block nil #'(lambda () (return-from nil)))))

;; reverse				- function
(setq str (xseq "abc") test str)
(equal-test "cba" #'reverse str)
(eq-eval test 'str)
(equal-eval "cba" '(setq test (nreverse str)))
(equal-eval "cba" 'test)
(setq l (list 1 2 3) test l)
(equal-eval '(3 2 1) '(setq test (nreverse l)))
(equal-eval '(3 2 1) 'test)

;; rplac?				- function
(eql-eval '*some-list*
    '(defparameter *some-list* (list* 'one 'two 'three 'four)))
(equal-eval '(one two three . four) '*some-list*)
(equal-test '(uno two three . four) #'rplaca *some-list* 'uno)
(equal-eval '(uno two three . four) '*some-list*)
(equal-test '(three iv) #'rplacd (last *some-list*) (list 'iv))
(equal-eval '(uno two three iv) '*some-list*)

;; search				- function
(eql-test 7 #'search "dog" "it's a dog's life")
(eql-test 2 #'search '(0 1) '(2 4 6 1 3 5) :key #'oddp)
(eql-test 8 #'search "foo" "foooobarfooooobarfo" :from-end t)
(eql-test 5
    #'search "123"
	(mapcar #'(lambda (x) (+ x (char-code #\0)))
	'(1 2 34 3 2 1 2 3 4 3 2 1)) :from-end t
	:key #'(lambda (x) (if (integerp x) (code-char x) x)))
(eql-test 0 #'search "abc" "abcd" :from-end t)
(eql-test 3 #'search "bar" "foobar")

;; set					- function
(eql-eval 1 '(setf (symbol-value 'n) 1))
(eql-test 2 #'set 'n 2)
(eql-test 2 #'symbol-value 'n)
(eql-eval 4
   '(let ((n 3))
	(setq n (+ n 1))
	(setf (symbol-value 'n) (* n 10))
	(set 'n (+ (symbol-value 'n) n))
	n))
(eql-eval 44 'n)
(defvar *n* 2)
(eql-eval 80
   '(let ((*n* 3))
	(setq *n* (+ *n* 1))
	(setf (symbol-value '*n*) (* *n* 10))
	(set '*n* (+ (symbol-value '*n*) *n*))
	*n*))
(eql-eval 2 '*n*)
(eq-eval '*even-count* '(defvar *even-count* 0))
(eq-eval '*odd-count* '(defvar *odd-count* 0))
(eql-eval 'tally-list
   '(defun tally-list (list)
      (dolist (element list)
	(set (if (evenp element) '*even-count* '*odd-count*)
	     (+ element (if (evenp element) *even-count* *odd-count*))))))
(eq-eval nil '(tally-list '(1 9 4 3 2 7)))
(eql-eval 6 '*even-count*)
(eql-eval 20 '*odd-count*)

;; set-difference			- function
(setq lst1 (list "A" "b" "C" "d") lst2 (list "a" "B" "C" "d"))
(equal-test '("A" "b" "C" "d") #'set-difference lst1 lst2)
(equal-test '("A" "b") #'set-difference lst1 lst2 :test 'equal)
(eq-test nil #'set-difference lst1 lst2 :test #'equalp)
(equal-test '("A" "b") #'nset-difference lst1 lst2 :test #'string=)
(setq lst1 '(("a" . "b") ("c" . "d") ("e" . "f"))
      lst2 '(("c" . "a") ("e" . "b") ("d" . "a")))
(equal-test '(("c" . "d") ("e" . "f"))
    #'nset-difference lst1 lst2 :test #'string= :key #'cdr)
(equal-eval '(("c" . "a") ("e" . "b") ("d" . "a")) 'lst2)
(equal-test '("banana" "lemon" "rhubarb")
   #'set-difference
	'("strawberry" "chocolate" "banana" "lemon" "pistachio" "rhubarb")
	'(#\c #\w) :test #'(lambda (s c) (find c s)))

;; set-exclusive-or			- function
(setq lst1 (list 1 "a" "b") lst2 (list 1 "A" "b"))
(equal-test '("a" "b" "A" "b") #'set-exclusive-or lst1 lst2)
(equal-test '("a" "A") #'set-exclusive-or lst1 lst2 :test #'equal)
(eq-test nil #'set-exclusive-or lst1 lst2 :test 'equalp)
(equal-test '("a" "b" "A" "b") #'nset-exclusive-or lst1 lst2)
(setq lst1 '(("a" . "b") ("c" . "d") ("e" . "f"))
      lst2 '(("c" . "a") ("e" . "b") ("d" . "a")))
(equal-test '(("c" . "d") ("e" . "f") ("c" . "a") ("d" . "a"))
    #'nset-exclusive-or lst1 lst2 :test #'string= :key #'cdr)

;; setf					- macro
(setq x (cons 'a 'b) y (list 1 2 3))
(equal-eval '(1 x 3) '(setf (car x) 'x (cadr y) (car x) (cdr x) y))
(equal-eval '(x 1 x 3) 'x)
(equal-eval '(1 x 3) 'y)
(setq x (cons 'a 'b) y (list 1 2 3))
(eq-eval nil '(psetf (car x) 'x (cadr y) (car x) (cdr x) y))
(equal-eval '(x 1 a 3) 'x)
(equal-eval '(1 a 3) 'y)
(error-eval '(setf x))
(error-eval '(psetf x))

;; setq					- special form
(eql-eval 3 '(setq a 1 b 2 c 3))
(eql-eval 1 'a)
(eql-eval 2 'b)
(eql-eval 3 'c)
(eql-eval 7 '(setq a (1+ b) b (1+ a) c (+ a b)))
(eql-eval 3 'a)
(eql-eval 4 'b)
(eql-eval 7 'c)
(eq-eval nil '(psetq a 1 b 2 c 3))
(eql-eval 1 'a)
(eql-eval 2 'b)
(eql-eval 3 'c)
(equal-eval '(2 1)
    '(multiple-value-list (let ((a 1) (b 2)) (psetq a b  b a) (values a b))))
(error-eval '(setq x))
(error-eval '(setq x 1 y))

;; some					- function
(eq-test t #'some #'= '(1 2 3 4 5) '(5 4 3 2 1))

;; sort					- function
(setq tester (copy-seq "lkjashd"))
(equal-test "adhjkls" #'sort tester #'char-lessp)
(setq tester (list '(1 2 3) '(4 5 6) '(7 8 9)))
(equal-test '((7 8 9) (4 5 6) (1 2 3)) #'sort tester #'> :key #'car)
(setq tester (list 1 2 3 4 5 6 7 8 9 0))
(equal-test '(1 3 5 7 9 2 4 6 8 0)
    #'stable-sort tester #'(lambda (x y) (and (oddp x) (evenp y))))
(equalp-test
  #((("Kathy" "Chapman") "Editorial")
    (("Dick" "Gabriel") "Objects")
    (("Gregor" "Kiczales") "Objects")
    (("Sandra" "Loosemore") "Compiler")
    (("Larry" "Masinter") "Cleanup")
    (("David" "Moon") "Objects")
    (("Kent" "Pitman") "Conditions")
    (("Dick" "Waters") "Iteration")
    (("JonL" "White") "Iteration"))
   #'sort (setq committee-data
	    (vector (list (list "JonL" "White") "Iteration")
		    (list (list "Dick" "Waters") "Iteration")
		    (list (list "Dick" "Gabriel") "Objects")
		    (list (list "Kent" "Pitman") "Conditions")
		    (list (list "Gregor" "Kiczales") "Objects")
		    (list (list "David" "Moon") "Objects")
		    (list (list "Kathy" "Chapman") "Editorial")
		    (list (list "Larry" "Masinter") "Cleanup")
		    (list (list "Sandra" "Loosemore") "Compiler")))
      #'string-lessp :key #'cadar)
(equalp-eval
  #((("Larry" "Masinter") "Cleanup")
    (("Sandra" "Loosemore") "Compiler")
    (("Kent" "Pitman") "Conditions")
    (("Kathy" "Chapman") "Editorial")
    (("Dick" "Waters") "Iteration")
    (("JonL" "White") "Iteration")
    (("Dick" "Gabriel") "Objects")
    (("Gregor" "Kiczales") "Objects")
    (("David" "Moon") "Objects"))
    '(setq committee-data
	(stable-sort committee-data #'string-lessp :key #'cadr)))
(error-test #'sort #c(1 2))

;; string				- function
(setq a "already a string")
(eq-test a #'string a)
(equal-test "ELM" #'string 'elm)
(equal-test "c" #'string #\c)

;; string-*				- function
(eq-test t #'string= "foo" "foo")
(eq-test nil #'string= "foo" "Foo")
(eq-test nil #'string= "foo" "bar")
(eq-test t #'string= "together" "frog" :start1 1 :end1 3 :start2 2)
(eq-test t #'string-equal "foo" "Foo")
(eq-test t #'string= "abcd" "01234abcd9012" :start2 5 :end2 9)
(eql-test 3 #'string< "aaaa" "aaab")
(eql-test 4 #'string>= "aaaaa" "aaaa")
(eql-test 5 #'string-not-greaterp "Abcde" "abcdE")
(eql-test 6 #'string-lessp "012AAAA789" "01aaab6" :start1 3 :end1 7
						  :start2 2 :end2 6)
(eq-test nil #'string-not-equal "AAAA" "aaaA")
(error-test #'string= #(1 2 3) '(1 2 3))
(eql-test 0 #'string< "abcd" "efg")
(eql-test 1 #'string< "abcd" "afg")
(eql-test 0 #'string/= "foo" "baar")
(eql-test nil #'string/= "foobar" "foobar")

;; string-{upcase,downcase,capitalize}	- function
(equal-test "ABCDE" #'string-upcase "abcde")
(equal-test "aBCDe" #'string-upcase "abcde" :start 1 :end 4)
(equal-test "aBCDe" #'nstring-upcase (xseq "abcde") :start 1 :end 4)
(equal-test "DR. LIVINGSTON, I PRESUME?"
    #'string-upcase "Dr. Livingston, I presume?")
(equal-test "Dr. LIVINGSTON, I Presume?"
    #'string-upcase "Dr. Livingston, I presume?" :start 4 :end 19)
(equal-test "Dr. LIVINGSTON, I Presume?"
    #'nstring-upcase (xseq "Dr. Livingston, I presume?") :start 4 :end 19)
(equal-test "Dr. LiVINGston, I presume?"
    #'string-upcase "Dr. Livingston, I presume?" :start 6 :end 10)
(equal-test "Dr. LiVINGston, I presume?"
    #'nstring-upcase (xseq "Dr. Livingston, I presume?") :start 6 :end 10)
(equal-test "dr. livingston, i presume?"
    #'string-downcase "Dr. Livingston, I presume?")
(equal-test "Dr. livingston, i Presume?"
    #'string-downcase "Dr. Livingston, I Presume?" :start 1 :end 17)
(equal-test "Dr. livingston, i Presume?"
    #'nstring-downcase (xseq "Dr. Livingston, I Presume?") :start 1 :end 17)
(equal-test "Elm 13c Arthur;Fig Don'T"
    #'string-capitalize "elm 13c arthur;fig don't")
(equal-test "elm 13C Arthur;Fig Don't"
    #'string-capitalize "elm 13c arthur;fig don't" :start 6 :end 21)
(equal-test "elm 13C Arthur;Fig Don't"
    #'nstring-capitalize (xseq "elm 13c arthur;fig don't") :start 6 :end 21)
(equal-test " Hello " #'string-capitalize " hello ")
(equal-test " Hello " #'nstring-capitalize (xseq " hello "))
(equal-test "Occluded Casements Forestall Inadvertent Defenestration"
   #'string-capitalize "occlUDeD cASEmenTs FOreSTAll iNADVertent DEFenestraTION")
(equal-test "Don'T!" #'string-capitalize "DON'T!")
(equal-test "Pipe 13a, Foo16c" #'string-capitalize "pipe 13a, foo16c")
(setq str (copy-seq "0123ABCD890a"))
(equal-test "0123AbcD890a" #'nstring-downcase str :start 5 :end 7)
(equal-eval "0123AbcD890a" 'str)
(error-test #'nstring-capitalize 1)
(error-test #'string-capitalize "foobar" :start 4 :end 2)
(equal-test "foobar" #'string-capitalize "foobar" :start 0 :end 0)

;; string-{,left-,right-}trim		- function
(equal-test "kaaak" #'string-trim "abc" "abcaakaaakabcaaa")
#+xedit (equal-test "kaaak" #'nstring-trim "abc" "abcaakaaakabcaaa")
(equal-test "garbanzo beans"
    #'string-trim '(#\Space #\Tab #\Newline) " garbanzo beans
        ")
#+xedit (equal-test "garbanzo beans"
    #'nstring-trim '(#\Space #\Tab #\Newline) " garbanzo beans
        ")
(equal-test "three (silly) words"
    #'string-trim " (*)" " ( *three (silly) words* ) ")
#+xedit (equal-test "three (silly) words"
    #'nstring-trim " (*)" " ( *three (silly) words* ) ")
(equal-test "labcabcabc" #'string-left-trim "abc" "labcabcabc")
#+xedit (equal-test "labcabcabc" #'nstring-left-trim "abc" "labcabcabc")
(equal-test "three (silly) words* ) "
    #'string-left-trim " (*)" " ( *three (silly) words* ) ")
#+xedit (equal-test "three (silly) words* ) "
    #'nstring-left-trim " (*)" " ( *three (silly) words* ) ")
(equal-test " ( *three (silly) words"
    #'string-right-trim " (*)" " ( *three (silly) words* ) ")
#+xedit (equal-test " ( *three (silly) words"
    #'nstring-right-trim " (*)" " ( *three (silly) words* ) ")
(error-test #'string-trim 123 "123")
(error-test #'string-left-trim 123 "123")

;; stringp				- function (predicate)
(eq-test t #'stringp "abc")
(eq-test nil #'stringp #\a)
(eq-test nil #'stringp 1)
(eq-test nil #'stringp #(#\a #\b #\c))

;; subseq				- accessor
(setq str (xseq "012345"))
(equal-test "2345" #'subseq str 2)
(equal-test "34" #'subseq str 3 5)
(equal-eval "abc" '(setf (subseq str 4) "abc"))
(equal-eval "0123ab" 'str)
(equal-eval "A" '(setf (subseq str 0 2) "A"))
(equal-eval "A123ab" 'str)

;; subsetp				- function
(setq cosmos '(1 "a" (1 2)))
(eq-test t #'subsetp '(1) cosmos)
(eq-test nil #'subsetp '((1 2)) cosmos)
(eq-test t #'subsetp '((1 2)) cosmos :test 'equal)
(eq-test t #'subsetp '(1 "A") cosmos :test #'equalp)
(eq-test nil #'subsetp '((1) (2)) '((1) (2)))
(eq-test t #'subsetp '((1) (2)) '((1) (2)) :key #'car)

;; svref				- function
;; XXX vectors will be reimplemented, just a test for the current implementation
(setq v (vector 1 2 'sirens))
(eql-eval 1 '(svref v 0))
(eql-eval 'sirens '(svref v 2))
(eql-eval 'newcomer '(setf (svref v 1) 'newcomer))
(equalp-eval #(1 newcomer sirens) 'v)

;; symbol-name				- function
(equal-test "TEMP" #'symbol-name 'temp)
(equal-test "START" #'symbol-name :start)
(error-test #'symbol-name 1)

;; symbol-package			- function
(eq-test (find-package "LISP") #'symbol-package 'car)
(eql-test *package* #'symbol-package 'bus)
(eq-test (find-package "KEYWORD") #'symbol-package :optional)
;; Gensyms are uninterned, so have no home package.
(eq-test nil #'symbol-package (gensym))
(setq pk1 (make-package 'pk1))
(intern "SAMPLE1" "PK1")
(eq-eval t '(export (find-symbol "SAMPLE1" "PK1") "PK1"))
(setq pk2 (make-package 'pk2 :use '(pk1)))
(equal-eval '(pk1:sample1 :inherited)
    '(multiple-value-list (find-symbol "SAMPLE1" "PK2")))
(eq-test pk1 #'symbol-package 'pk1::sample1)
(eq-test pk1 #'symbol-package 'pk2::sample1)
(eq-test pk1 #'symbol-package 'pk1::sample2)
(eq-test pk2 #'symbol-package 'pk2::sample2)
;; The next several forms create a scenario in which a symbol
;; is not really uninterned, but is "apparently uninterned",
;; and so SYMBOL-PACKAGE still returns NIL.
(setq s3 'pk1::sample3)
(eq-eval t '(import s3 'pk2))
(eq-eval t '(unintern s3 'pk1))		;; XXX unintern not yet implemented
(eq-test nil #'symbol-package s3)	;; fail due to unintern not implemented
(eq-test t #'eq s3 'pk2::sample3)

;; symbol-plist				- accessor
(setq sym (gensym))
(eq-eval () '(symbol-plist sym))
(eq-eval 'val1 '(setf (get sym 'prop1) 'val1))
(equal-eval '(prop1 val1) '(symbol-plist sym))
(eq-eval 'val2 '(setf (get sym 'prop2) 'val2))
(equal-eval '(prop2 val2 prop1 val1) '(symbol-plist sym))
(setq sym-plist (list 'prop3 'val3))
(eq-eval sym-plist '(setf (symbol-plist sym) sym-plist))
(eq-eval sym-plist '(symbol-plist sym))

;; symbol-value				- accessor
(eql-eval 1 '(setf (symbol-value 'a) 1))
(eql-eval 1 '(symbol-value 'a))
;; SYMBOL-VALUE cannot see lexical variables.
(eql-eval 1 '(let ((a 2)) (symbol-value 'a)))
(eql-eval 1 '(let ((a 2)) (setq a 3) (symbol-value 'a)))

#+xedit	;; incorrect...
(progn
    ;; SYMBOL-VALUE can see dynamic variables.
	    ;; declare not yet implemented
	    (proclaim '(special a))
    (eql-eval 2 '(let ((a 2)) (symbol-value 'a)))
    (eql-eval 1 'a)
    (eql-eval 3 '(let ((a 2)) (setq a 3) (symbol-value 'a)))
    (eql-eval 1 'a)
	    ;; declare not yet implement
	    (makunbound 'a)
    (eql-eval 2 '(let ((a 2)) (setf (symbol-value 'a) 3) a))
    (eql-eval 3 'a)
    (eql-eval 3 '(symbol-value 'a))
	    ;; declare not yet implement
	    (makunbound 'a)
    (equal-eval '(5 4)
	'(multiple-value-list
	    (let ((a 4))

			;; declare not yet implemented
			(defparameter a 3)

	      (let ((b (symbol-value 'a)))
		(setf (symbol-value 'a) 5)
		(values a b)))))
    (eql-eval 3 'a)
)
(eq-eval :any-keyword '(symbol-value :any-keyword))
;; XXX these will fail
(eq-eval nil '(symbol-value 'nil))
(eq-eval nil '(symbol-value '()))

;; symbolp				- function (predicate)
(eq-test t #'symbolp 'elephant)
(eq-test nil #'symbolp 12)
;; XXX these will fail
(eq-test t #'symbolp nil)
(eq-test t #'symbolp '())
(eq-test t #'symbolp :test)
(eq-test nil #'symbolp "hello")

;; remprop				- function
(setq test (make-symbol "PSEUDO-PI"))
(eq-eval () '(symbol-plist test))
(eq-eval t '(setf (get test 'constant) t))
(eql-eval 3.14 '(setf (get test 'approximation) 3.14))
(eql-eval 'noticeable '(setf (get test 'error-range) 'noticeable))
(equal-eval '(error-range noticeable approximation 3.14 constant t)
    '(symbol-plist test))
(eq-eval nil '(setf (get test 'approximation) nil))
(equal-eval '(error-range noticeable approximation nil constant t)
    '(symbol-plist test))
(eq-eval nil (get test 'approximation))
(eq-test t #'remprop test 'approximation)
(eq-eval nil '(get test 'approximation))
(equal-eval '(error-range noticeable constant t) '(symbol-plist test))
(eq-test nil #'remprop test 'approximation)
(equal-eval '(error-range noticeable constant t) '(symbol-plist test))
(eq-test t #'remprop test 'error-range)
(eql-eval 3 '(setf (get test 'approximation) 3))
(equal-eval '(approximation 3 constant t) '(symbol-plist test))

;; throw				- special operator
(equal-eval '(3 9)
    '(multiple-value-list
	(catch 'result
	    (setq i 0 j 0)
	    (loop (incf j 3) (incf i)
		  (if (= i 3) (throw 'result (values i j)))))))
(eql-eval 2 '(catch nil (unwind-protect (throw nil 1) (throw nil 2))))

;; XXX undefined consequences
(eql-eval 2
   '(catch 'a
      (catch 'b
	(unwind-protect (throw 'a 1)
	  (throw 'b 2)))))
(eq-eval :outer-catch
   '(catch 'foo
	(setq string (format nil "The inner catch returns ~s."
	    (catch 'foo
		(unwind-protect (throw 'foo :first-throw)
		    (throw 'foo :second-throw)))))
         :outer-catch))
(equal-eval "The inner catch returns :SECOND-THROW." 'string)

;; tree-equal				- function
(setq tree1 '(1 (1 2))
      tree2 '(1 (1 2)))
(eq-test t #'tree-equal tree1 tree2)
(eq-test nil #'eql tree1 tree2)
(setq tree1 '('a ('b 'c))
      tree2 '('a ('b 'c)))
(eq-test t #'tree-equal tree1 tree2 :test 'eq)
(eq-test t #'tree-equal 1 1)
(eq-test nil #'tree-equal (list 1 2) (cons 1 2))
(eq-test nil #'tree-equal 1 2)

;; union				- function
(equal-test '(b c f a d) #'union '(a b c) '(f a d))
(equal-test '((y 6) (z 2) (x 4))
    #'union '((x 5) (y 6)) '((z 2) (x 4)) :key #'car)
(setq lst1 (list 1 2 '(1 2) "a" "b")
      lst2 (list 2 3 '(2 3) "B" "C"))
(equal-test '(1 (1 2) "a" "b" 2 3 (2 3) "B" "C") #'nunion lst1 lst2)

;; unless				- macro
(eq-eval 'hello '(when t 'hello))
(eq-eval nil '(unless t 'hello))
(eq-eval nil (when nil 'hello))
(eq-eval 'hello '(unless nil 'hello))
(eq-eval nil (when t))
(eql-eval nil '(unless nil))
(setq test nil)
(equal-eval '(3 2 1) '(when t (push 1 test) (push 2 test) (push 3 test)))
(equal-eval '(3 2 1) 'test)
(setq test nil)
(eq-eval nil '(unless t (push 1 test) (push 2 test) (push 3 test)))
(eq-eval nil 'test)
(eq-eval nil '(when nil (push 1 test) (push 2 test) (push 3 test)))
(eq-eval nil 'test)
(equal-eval '(3 2 1) '(unless nil (push 1 test) (push 2 test) (push 3 test)))
(equal-eval '(3 2 1) 'test)
(equal-eval '((4) nil (5) nil 6 (6) 7 (7))
   '(let ((x 3))
      (list (when (oddp x) (incf x) (list x))
	    (when (oddp x) (incf x) (list x))
	    (unless (oddp x) (incf x) (list x))
	    (unless (oddp x) (incf x) (list x))
	    (if (oddp x) (incf x) (list x))
	    (if (oddp x) (incf x) (list x))
	    (if (not (oddp x)) (incf x) (list x))
	    (if (not (oddp x)) (incf x) (list x)))))

;; unwind-protect			- special operator
(defun dummy-function (x)
   (setq state 'running)
   (unless (numberp x) (throw 'abort 'not-a-number))
   (setq state (1+ x)))
(eql-eval 2 '(catch 'abort (dummy-function 1)))
(eql-eval 2 'state)
(eq-eval 'not-a-number '(catch 'abort (dummy-function 'trash)))
(eq-eval 'running 'state)
(eq-eval 'not-a-number
    '(catch 'abort (unwind-protect (dummy-function 'trash)
		   (setq state 'aborted))))
(eq-eval 'aborted 'state)
(eql-eval 2 '(block nil (unwind-protect (return 1) (return 2))))
;; XXX undefined consequences
(eql-eval 2
   '(block a
	(block b
	    (unwind-protect (return-from a 1)
			    (return-from b 2)))))
(eql-eval 2 '(catch nil (unwind-protect (throw nil 1) (throw nil 2))))
;; XXX undefined consequences
(eql-eval 2
   '(catch 'a (catch 'b (unwind-protect (throw 'a 1) (throw 'b 2)))))
(eq-eval ':outer-catch
   '(catch 'foo
	(setq string
	    (format nil "The inner catch returns ~s."
		(catch 'foo
		    (unwind-protect (throw 'foo :first-throw)
		    (throw 'foo :second-throw)))))
         :outer-catch))
(equal-eval "The inner catch returns :SECOND-THROW." 'string)
(eql-eval 10
   '(catch 'a
	(catch 'b
	    (unwind-protect (1+ (catch 'a (throw 'b 1)))
		(throw 'a 10)))))
;; XXX undefined consequences
(eql-eval 4
   '(catch 'foo
       (catch 'bar
	   (unwind-protect (throw 'foo 3)
	     (throw 'bar 4)
	     (print 'xxx)))))
(eql-eval 4
   '(catch 'bar
       (catch 'foo
	   (unwind-protect (throw 'foo 3)
	     (throw 'bar 4)
	     (print 'xxx)))))
(eql-eval 5
   '(block nil
       (let ((x 5))
	 (unwind-protect (return)
	   (return x)))))

;; upper-case-p				- function
(eq-test t #'upper-case-p #\A)
(eq-test nil #'upper-case-p #\a)
(eq-test nil #'upper-case-p #\5)
(error-test #'upper-case-p 1)

;; values				- accessor
(eq-eval () '(multiple-value-list (values)))
(equal-eval '(1) '(multiple-value-list (values 1)))
(equal-eval '(1 2) '(multiple-value-list (values 1 2)))
(equal-eval '(1 2 3) '(multiple-value-list (values 1 2 3)))
(equal-eval '(1 4 5) '(multiple-value-list (values (values 1 2 3) 4 5)))

;; values-list				- function
(eq-eval nil '(multiple-value-list (values-list nil)))
(equal-eval '(1) '(multiple-value-list (values-list '(1))))
(equal-eval '(1 2) '(multiple-value-list (values-list '(1 2))))
(equal-eval '(1 2 3) '(multiple-value-list (values-list '(1 2 3))))
