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
;; $XFree86: xc/programs/xedit/lisp/test/stream.lsp,v 1.3 2002/12/06 03:25:29 paulo Exp $
;;

;; most format tests from the cltl second edition samples

;; basic io/format/pathname/stream tests

(defun do-format-test (error-test expect arguments
		    &aux result (error t) unused error-value)
    (multiple-value-setq
	(unused error-value)
	(ignore-errors
	    (setq result (apply #'format nil arguments))
	    (setq error nil)
	)
    )
    (if error-test
	(or error
	    (format t "ERROR: no error for (format nil~{ ~S~}), result was ~S~%"
		arguments result))
	(if error
	    (format t "ERROR: (format nil~{ ~S~}) => ~S~%" arguments error-value)
	    (or (string= result expect)
		(format t "(format nil~{ ~S~}) => should be ~S not ~S~%"
		    arguments expect result)))
    )
)

(defun format-test (expect &rest arguments)
    (do-format-test nil expect arguments))

(defun format-error (&rest arguments)
    (do-format-test t nil arguments))



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

(defun bool-test (expect function &rest arguments
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
	(or (eq (null result) (null expect))
	    (format t "(~S~{ ~S~}) => should be ~A not ~A~%"
		function arguments expect result
	    )
	)
    )
)

(defun bool-eval (expect form &aux result (error t) unused error-value)
    (multiple-value-setq
	(unused error-value)
	(ignore-errors
	    (setq result (eval form))
	    (setq error nil)
	)
    )
    (if error
	(format t "ERROR: ~S => ~S~%" form error-value)
	(or (eq (null result) (null expect))
	    (format t "~S => should be ~A not ~A~%"
		form expect result
	    )
	)
    )
)


;; format				- function

;; ~c
(format-test "A" "~C" #\A)
(format-test " " "~C" #\Space)
(format-test "A" "~:C" #\A)
(format-test "Space" "~:C" #\Space)
(format-test "#\\A" "~@C" #\A)
(format-test "#\\Space" "~@C" #\Space)
(format-test " " "~A" #\Space)
(let ((*print-escape* t)) (format-test " " "~A" #\Space))
(format-test "#\\Space" "~S" #\Space)
(let ((*print-escape* nil)) (format-test "#\\Space" "~S" #\Space))

;; ~%
(format-test "
" "~%")
(format-test "


" "~3%")

;; ~&
(format-test "" "~&")
(format-test "
" "~2&")

;; ~|
(format-test "" "~|")

;; ~~
(format-test "~~~" "~3~")

;; radix
(format-test "1101" "~,,' ,4:B" 13)
(format-test "1 0001" "~,,' ,4:B" 17)
(format-test "1101 0000 0101" "~14,,' ,4:B" 3333)
(format-test "1 22" "~3,,,' ,2:R" 17)
(format-test "6|55|35" "~,,'|,2:D" #xFFFF)
(format-test "1,000,000" "~,,,3:D" 1000000)
(format-test "one hundred and twenty-three thousand, four hundred and fifty-six"
	"~R" 123456)
(format-test "six hundred and fifty-four thousand, three hundred twenty-first"
	"~:R" 654321)
(format-test "MCCXXXIV" "~@R" 1234)
(format-test "MCCXXXXVIIII" "~@:R" 1249)
(format-test "3039" "~X" 12345)
(format-test "30071" "~O" 12345)
(format-test "9IX" "~36R" 12345)
(format-test "11000000111001" "~B" 12345)
(format-test "The answer is 5." "The answer is ~D." 5)
(format-test "The answer is   5." "The answer is ~3D." 5)
(format-test "The answer is 005." "The answer is ~3,'0D." 5)
(format-test "1111 1010 1100 1110" "~,,' ,4:B" #xFACE)
(format-test "1 1100 1110" "~,,' ,4:B" #x1CE)
(format-test "1111 1010 1100 1110" "~19,,' ,4:B" #xFACE)
(format-test "        1 1100 1110" "~19,,' ,4:B" #x1CE)

;; 6.37 and 6.38 are correct
#+xedit (format-test "6.38" "~4,2F" 6.375d0)
(format-test "10.0" "~,1F" 9.995d0)
;; 6.37E+2 and 6.38E+2 are correct
#+xedit (format-test " 6.38E+2" "~8,2E" 637.5)
(do*
    (
    (n '(3.14159 -3.14159 100.0 1234.0 0.006) (cdr n))
    (r '("  3.14| 31.42|  3.14|3.1416|3.14|3.14159"
	 " -3.14|-31.42| -3.14|-3.142|-3.14|-3.14159"
	 "100.00|******|100.00| 100.0|100.00|100.0"
	 "1234.00|******|??????|1234.0|1234.00|1234.0"
	 "  0.01|  0.06|  0.01| 0.006|0.01|0.006") (cdr r))
    (x (car n) (car n))
    )
    ((endp n))
    (format-test (car r)
	"~6,2F|~6,2,1,'*F|~6,2,,'?F|~6F|~,2F|~F" x x x x x x)
)
(do*
    (
    (n '(3.14159 -3.14159 1100.0 1.1e13 #+xedit 1.1e120) (cdr n))
    (r '("  3.14E+0| 31.42$-01|+.003E+03|  3.14E+0"
	 " -3.14E+0|-31.42$-01|-.003E+03| -3.14E+0"
	 "  1.10E+3| 11.00$+02|+.001E+06|  1.10E+3"
	 "*********| 11.00$+12|+.001E+16| 1.10E+13"
	 #+xedit
	 "*********|??????????|%%%%%%%%%|1.10E+120") (cdr r))
    (x (car n) (car n))
    )
    ((endp n))
    (format-test (car r)
	"~9,2,1,,'*E|~10,3,2,2,'?,,'$E|~9,3,2,-2,'%@E|~9,2E" x x x x)
)
(do
    (
    (k -5 (1+ k))
    (r '("Scale factor -5: | 0.000003E+06|"
	 "Scale factor -4: | 0.000031E+05|"
	 "Scale factor -3: | 0.000314E+04|"
	 "Scale factor -2: | 0.003142E+03|"
	 "Scale factor -1: | 0.031416E+02|"
	 "Scale factor  0: | 0.314159E+01|"
	 "Scale factor  1: | 3.141590E+00|"
	 "Scale factor  2: | 31.41590E-01|"
	 "Scale factor  3: | 314.1590E-02|"
	 "Scale factor  4: | 3141.590E-03|"
	 "Scale factor  5: | 31415.90E-04|"
	 "Scale factor  6: | 314159.0E-05|"
	 "Scale factor  7: | 3141590.E-06|") (cdr r))
    )
    ((endp r))
    (format-test (car r) "Scale factor ~2D: | ~12,6,2,VE|" k k 3.14159)
)
(do*
    (
    (n '(0.0314159 0.314159 3.14159 31.4159 314.159 3141.59 3.14E12
	 #+xedit 3.14d120) (cdr n))
    (r '("  3.14E-2|314.2$-04|0.314E-01|  3.14E-2"
         "  0.31   |0.314    |0.314    | 0.31    "
         "   3.1   | 3.14    | 3.14    |  3.1    "
         "   31.   | 31.4    | 31.4    |  31.    "
         "  3.14E+2| 314.    | 314.    |  3.14E+2"
         "  3.14E+3|314.2$+01|0.314E+04|  3.14E+3"
         "*********|314.0$+10|0.314E+13| 3.14E+12"
         #+xedit "*********|?????????|%%%%%%%%%|3.14E+120") (cdr r))
    (x (car n) (car n))
    )
    ((endp n))
    (format-test (car r) "~9,2,1,,'*G|~9,3,2,3,'?,,'$G|~9,3,2,0,'%G|~9,2G"
	x x x x)
)
(format-test "  1." "~4,0f" 0.5)
(format-test "  0." "~4,0f" 0.4)

;; ~p
(setq n 3)
(format-test "3 items found.""~D item~:P found." n)
(format-test "three dogs are here." "~R dog~:[s are~; is~] here." n (= n 1))
(format-test "three dogs are here." "~R dog~:*~[s are~; is~:;s are~] here." n)
(format-test "Here are three puppies.""Here ~[are~;is~:;are~] ~:*~R pupp~:@P." n)
(format-test "7 tries/1 win" "~D tr~:@P/~D win~:P" 7 1)
(format-test "1 try/0 wins" "~D tr~:@P/~D win~:P" 1 0)
(format-test "1 try/3 wins" "~D tr~:@P/~D win~:P" 1 3)

;; ~t
(format-test "        foo" "~8Tfoo")
#+xedit (format-test  "         foo" "~8,3Tfoo")
(format-test "         foo" "~8,3@Tfoo")
(format-test "   foo" "~1,3@Tfoo")

;; ~*
(format-test "2" "~*~D" 1 2 3 4)
(format-test "4" "~3*~D" 1 2 3 4)
(format-test "2" "~3*~2:*~D" 1 2 3 4)
(format-test "4 3 2 1 2 3 4" "~3@*~D ~2@*~D ~1@*~D ~0@*~D ~D ~D ~D" 1 2 3 4)

;; ~?
(format-test "<Foo 5> 7" "~? ~D" "<~A ~D>" '("Foo" 5) 7)
(format-test "<Foo 5> 7" "~? ~D" "<~A ~D>" '("Foo" 5 14) 7)
(format-test "<Foo 5> 7" "~@? ~D" "<~A ~D>" "Foo" 5 7)
(format-test "<Foo 5> 14" "~@? ~D" "<~A ~D>" "Foo" 5 14 7)


(format-error "~:[abc~:@(def~;ghi~:@(jkl~]mno~)" 1)
(format-error "~?ghi~)" "abc~@(def")


;; ~(...~)
(format-test "XIV xiv" "~@R ~(~@R~)" 14 14)
(format-test "Zero errors detected." "~@(~R~) error~:P detected." 0)
(format-test "One error detected." "~@(~R~) error~:P detected." 1)
(format-test "Twenty-three errors detected." "~@(~R~) error~:P detected." 23)

;; ~[...~]
(format-test "Persian Cat" "~[Siamese~;Manx~;Persian~] Cat" 2)
(format-test " Cat" "~[Siamese~;Manx~;Persian~] Cat" 3)
(format-test "Siamese Cat" "~[Siamese~;Manx~;Persian~] Cat" 0)
(setq *print-level* nil *print-length* 5)
(format-test " print length = 5"
  "~@[ print level = ~D~]~@[ print length = ~D~]" *print-level* *print-length*)
(setq foo "Items:~#[ none~; ~S~; ~S and ~S~:;~@{ ~#[~;and ~]~S~^,~}~].")
(format-test "Items: none." foo)
(format-test "Items: FOO." foo 'foo)
(format-test "Items: FOO and BAR." foo 'foo 'bar)
(format-test "Items: FOO, BAR, and BAZ." foo 'foo 'bar 'baz)
(format-test "Items: FOO, BAR, BAZ, and QUUX." foo 'foo 'bar 'baz 'quux)

;; ~{...~}
(format-test "The winners are: FRED HARRY JILL."
  "The winners are:~{ ~S~}." '(fred harry jill))
(format-test "Pairs: <A,1> <B,2> <C,3>." "Pairs:~{ <~S,~S>~}." '(a 1 b 2 c 3))
(format-test "Pairs: <A,1> <B,2> <C,3>."
  "Pairs:~:{ <~S,~S>~}." '((a 1) (b 2) (c 3)))
(format-test "Pairs: <A,1> <B,2> <C,3>."
  "Pairs:~:@{ <~S,~S>~}." '(a 1) '(b 2) '(c 3))

;; ~<...~>
(format-test "foo    bar" "~10<foo~;bar~>")
(format-test "  foo  bar" "~10:<foo~;bar~>")
(format-test "  foo bar " "~10:@<foo~;bar~>")
(format-test "    foobar" "~10<foobar~>")
(format-test "    foobar" "~10:<foobar~>")
(format-test "foobar    " "~10@<foobar~>")
(format-test "  foobar  " "~10:@<foobar~>")

;; ~^
(setq donestr "Done.~^  ~D warning~:P.~^  ~D error~:P.")
(format-test "Done." donestr)
(format-test "Done.  3 warnings." donestr 3)
(format-test "Done.  1 warning.  5 errors." donestr 1 5)
(format-test "/HOT .../HAMBURGER/ICE .../FRENCH ..."
  "~:{/~S~^ ...~}" '((hot dog) (hamburger) (ice cream) (french fries)))
(format-test "/HOT .../HAMBURGER .../ICE .../FRENCH"
  "~:{/~S~:^ ...~}" '((hot dog) (hamburger) (ice cream) (french fries)))
(format-test "/HOT .../HAMBURGER"
  "~:{/~S~:#^ ...~}" '((hot dog) (hamburger) (ice cream) (french fries)))
(setq tellstr "~@(~@[~R~]~^ ~A.~)")
(format-test "Twenty-three" tellstr 23)
(format-test " Losers." tellstr nil "losers")
(format-test "Twenty-three losers." tellstr 23 "losers")
(format-test "            FOO" "~15<~S~;~^~S~;~^~S~>" 'foo)
(format-test "FOO         BAR" "~15<~S~;~^~S~;~^~S~>" 'foo 'bar)
(format-test "FOO   BAR   BAZ" "~15<~S~;~^~S~;~^~S~>" 'foo 'bar 'baz)


;; make-pathname			- function
(equal-test #P"/public/games/chess.db"
    #'make-pathname :directory '(:absolute "public" "games")
		    :name "chess" :type "db")
(equal-test #P"/etc/passwd" #'list* #P"/etc/passwd")
(setq path (make-pathname :directory '(:absolute "public" "games")
			  :name "chess" :type "db"))
(eq-test path #'pathname path)
(eq-test nil #'pathname-host path)
(eq-test nil #'pathname-device path)
(equal-test '(:absolute "public" "games") #'pathname-directory path)
(equal-test "chess" #'pathname-name path)
(equal-test "db" #'pathname-type path)
(eq-test nil #'pathname-version path)
(equal-test #P"/tmp/foo.txt" #'make-pathname :defaults "/tmp/foo.txt")

#+xedit (equal-test #P"/tmp/foo.txt" #'pathname "///tmp///foo.txt")
;; XXX changed to remove extra separators
;; (equal-test #P"///tmp///foo.txt" #'pathname "///tmp///foo.txt")


;; merge-pathnames			- function
(equal-test #P"/tmp/foo.txt" #'merge-pathnames "/tmp/foo" "/tmp/foo.txt")
(equal-test #P"/tmp/foo.txt" #'merge-pathnames "foo" "/tmp/foo.txt")
(equal-test #P"/tmp/foo/bar.txt" #'merge-pathnames "foo/bar" "/tmp/foo.txt")

;; namestring				- function
(setq path (merge-pathnames "foo/bar" "/tmp/foo.txt"))
(equal-test "/tmp/foo/bar.txt" #'namestring path)
(equal-test "" #'host-namestring path)
(equal-test "/tmp/foo/" #'directory-namestring path)
(equal-test "bar.txt" #'file-namestring path)
(equal-test "/tmp/foo/bar.txt" #'enough-namestring path)
(equal-test "foo/bar.txt" #'enough-namestring path "/tmp/")
(equal-test "bar.txt" #'enough-namestring path "/tmp/foo/")
(equal-test "foo/bar.txt" #'enough-namestring path "/tmp/foo")

;; parse-namestring			- function
(equal-eval '(#P"foo" 3) '(multiple-value-list (parse-namestring "foo")))
(equal-eval '(#P"foo" 0) '(multiple-value-list (parse-namestring #P"foo")))



;; read					- function
(setq is (make-string-input-stream " foo "))
(eq-test t #'streamp is)
(eq-test t #'input-stream-p is)
(eq-test nil #'output-stream-p is)
(eq-test 'foo #'read is)
(eq-test t #'close is)
(setq is (make-string-input-stream "xfooy" 1 4))
(eq-test 'foo #'read is)
(eq-test t #'close is)
(setq is (make-string-input-stream ""))
(eq-test nil #'read is nil)
(eq-test 'end-of-string #'read is nil 'end-of-string)
(close is)
(error-test #'read is)
(error-test #'read is nil)
(error-test #'read is nil 'end-of-string)
(eq-test t #'streamp is)
(eq-test nil #'input-stream-p is)
(eq-test nil #'streamp "test")
(error-test #'input-stream-p "test")

;; read-char				- function
(setq is (make-string-input-stream "0123"))
(setq test nil)
(equal-eval '(#\0 #\1 #\2 #\3)
  '(do ((c (read-char is) (read-char is nil 'the-end)))
      ((not (characterp c)) test)
   (setq test (append test (list c)))))
(close is)
(setq is (make-string-input-stream "abc"))
(eql-test #\a #'read-char is)
(eql-test #\b #'read-char is)
(eql-test #\c #'read-char is)
(error-test #'read-char is)
(eq-test nil #'read-char is nil)
(eq-test :end-of-string #'read-char is nil :end-of-string)
(eq-test t #'close is)

;; read-char-no-hang			- function
(setq is (make-string-input-stream "0123"))
(setq test nil)
(equal-eval '(#\0 #\1 #\2 #\3)
  '(do ((c (read-char-no-hang is) (read-char-no-hang is nil 'the-end)))
      ((not (characterp c)) test)
   (setq test (append test (list c)))))
(close is)
(setq is (make-string-input-stream "abc"))
(eql-test #\a #'read-char-no-hang is)
(eql-test #\b #'read-char-no-hang is)
(eql-test #\c #'read-char-no-hang is)
(error-test #'read-char-no-hang is)
(eq-test nil #'read-char-no-hang is nil)
(eq-test :end-of-string #'read-char-no-hang is nil :end-of-string)
(eq-test t #'close is)
#+(and xedit unix)
(progn
    ;; wait one second for input pooling every 0.1 seconds
    (defun wait-for-cat ()
	(let ((time 0.0))
	    (loop
		(and (listen is) (return))
		(sleep 0.1)
		(when (>= (incf time 0.1) 1.0)
		    (format t "Cat is sleeping~%")
		    (return)))))
    (setq is (make-pipe "/bin/cat" :direction :io))
    (equal-test "dog" #'write-line "dog" is)
    (wait-for-cat)
    (eql-test #\d #'read-char-no-hang is)
    (eql-test #\o #'read-char-no-hang is)
    (eql-test #\g #'read-char-no-hang is)
    (eql-test #\Newline #'read-char-no-hang is)
    (eq-test nil #'read-char-no-hang is)
    (eq-test nil #'read-char-no-hang is)
    (equal-test "mouse" #'write-line "mouse" is)
    (wait-for-cat)
    (eql-test #\m #'read-char-no-hang is)
    (eql-test #\o #'read-char-no-hang is)
    (eql-test #\u #'read-char-no-hang is)
    (eql-test #\s #'read-char-no-hang is)
    (eql-test #\e #'read-char-no-hang is)
    (eql-test #\Newline #'read-char-no-hang is)
    (eq-test nil #'read-char-no-hang is)
    (eq-test t #'close is)
    (error-test #'read-char-no-hang is)
    (error-test #'read-char-no-hang is nil)
    (error-test #'read-char-no-hang is nil t)
)

;; read-from-string			- function
(equal-eval '(3 5)
  '(multiple-value-list (read-from-string " 1 3 5" t nil :start 2)))
(equal-eval '((a b c) 7)
  '(multiple-value-list (read-from-string "(a b c)")))
(error-test #'read-from-string "")
(eq-test nil #'read-from-string "" nil)
(eq-test 'end-of-file #'read-from-string "" nil 'end-of-file)

;; read-line				- function
(setq is (make-string-input-stream "line 1
line 2"))
(equal-eval '("line 1" nil) '(multiple-value-list (read-line is)))
(equal-eval '("line 2" t) '(multiple-value-list (read-line is)))
(error-test #'read-line is)
(equal-eval '(nil t) '(multiple-value-list (read-line is nil)))
(equal-eval '(end-of-string t)
  '(multiple-value-list (read-line is nil 'end-of-string)))


;; write				- function
;; XXX several write options still missing
(setq os (make-string-output-stream))
(equal-test '(1 2 3 4) #'write '(1 2 3 4) :stream os)
(equal-test "(1 2 3 4)" #'get-output-stream-string os)
(eq-test t #'streamp os)
(eq-test t #'output-stream-p os)
(eq-test nil #'input-stream-p os)
(equal-test '(:foo :bar) #'write '(:foo :bar) :case :downcase :stream os)
(equal-test "(:foo :bar)" #'get-output-stream-string os)
(equal-test '(:foo :bar) #'write '(:foo :bar) :case :capitalize :stream os)
(equal-test "(:Foo :Bar)" #'get-output-stream-string os)
(equal-test '(:foo :bar) #'write '(:foo :bar) :case :upcase :stream os)
(equal-test "(:FOO :BAR)" #'get-output-stream-string os)
(equal-test '(foo bar baz) #'write '(foo bar baz) :length 2 :stream os)
(equal-test "(FOO BAR ...)" #'get-output-stream-string os)
(equal-test '(foo (bar) baz) #'write '(foo (bar) baz) :level 1 :stream os)
(equal-test "(FOO # BAZ)" #'get-output-stream-string os)
(setq circle '#1=(1 #1#))
(eq-test circle #'write circle :circle t :stream os)
(equal-test "#1=(1 #1#)" #'get-output-stream-string os)
(eql-test #\Space #'write #\Space :stream os)
(equal-test "#\\Space" #'get-output-stream-string os)
(eql-test #\Space #'write #\Space :escape nil :stream os)
(equal-test " " #'get-output-stream-string os)
(eq-test t #'close os)
(eq-test nil #'output-stream-p os)
(error-test #'output-stream-p "test")
(error-test #'write 'foo :stream "bar")

;; fresh-line				- function
(setq os (make-string-output-stream))
(equal-test "some text" #'write-string "some text" os)
(eq-test t #'fresh-line os)
(eq-test nil #'fresh-line os)
(equal-test "more text" #'write-string "more text" os)
(equal-test "some text
more text" #'get-output-stream-string os)
(equal-test nil #'fresh-line os)
(equal-test nil #'fresh-line os)
(equal-test "" #'get-output-stream-string os)
(close os)
(error-test #'fresh-line 1)

;; prin1				- function
;;  (prin1 object stream) ==
;;	(write object :stream stream :escape t)
(setq p-os (make-string-output-stream) w-os (make-string-output-stream))
(dolist (object (list #\a 1 "string" 2.5d0 '(a . b) '(a b c) #P"foo"
		     *package* *standard-input* #c(1 2) #(1 2 3)
		     (make-hash-table)))
    (eq-test object #'prin1 object p-os)
    (eq-test object #'write object :stream w-os :escape t)
    (equal-test (get-output-stream-string p-os)
	#'get-output-stream-string w-os))
(close p-os)
(close w-os)
(error-test #'prin1 1 1)

;; princ				- function
;;  (princ object stream) ==
;;	(write object :stream stream :escape nil :readably nil)
;; XXX readably not yet implemented
(setq p-os (make-string-output-stream) w-os (make-string-output-stream))
(dolist (object (list #\a 1 "string" 2.5d0 '(a . b) '(a b c) #P"foo"
		     *package* *standard-input* #c(1 2) #(1 2 3)
		     (make-hash-table)))
    (eq-test object #'princ object p-os)
    (eq-test object #'write object :stream w-os :escape nil)
    (equal-test (get-output-stream-string p-os)
	#'get-output-stream-string w-os))
(close p-os)
(close w-os)
(error-test #'princ 1 1)

;; print				- function
;;  (print object stream) ==
;;	(progn
;;	    (terpri stream)
;;	    (write object :stream stream :escape t)
;;	    (write-char #\Space stream))
(setq p-os (make-string-output-stream) w-os (make-string-output-stream))
(dolist (object (list #\a 1 "string" 2.5d0 '(a . b) '(a b c) #P"foo"
		     *package* *standard-input* #c(1 2) #(1 2 3)
		     (make-hash-table)))
    (eq-test object #'print object p-os)
    (progn
	(eq-test nil #'terpri w-os)
	(eq-test object #'write object :stream w-os :escape t)
	(eql-test #\Space #'write-char #\Space w-os))
    (equal-test (get-output-stream-string p-os)
	#'get-output-stream-string w-os))
(close p-os)
(close w-os)
(error-test #'print 1 1)

;; terpri				- function
(setq os (make-string-output-stream))
(equal-test "some text" #'write-string "some text" os)
(eq-test nil #'terpri os)
(eq-test nil #'terpri os)
(equal-test "more text" #'write-string "more text" os)
(equal-test "some text

more text" #'get-output-stream-string os)
(equal-test nil #'terpri os)
(equal-test nil #'terpri os)
(equal-test "

" #'get-output-stream-string os)
(close os)
(error-test #'terpri 1)

;; write-char				- function
(equal-eval "a b"
    '(with-output-to-string (s) 
	(write-char #\a s)
	(write-char #\Space s)
	(write-char #\b s)))
(error-test #'write-char 1)

;; write-line				- function
(setq os (make-string-output-stream))
(equal-test "text" #'write-line "text" os)
(equal-test "text
" #'get-output-stream-string os)
(eql-test #\< #'write-char #\< os)
(equal-test "text" #'write-line "text" os :start 1 :end 3)
(eql-test #\> #'write-char #\> os)
(equal-test "<ex
>" #'get-output-stream-string os)
(error-test #'write-line 1)
(close os)

;; write-string				- function
(setq os (make-string-output-stream))
(equal-test "text" #'write-string "text" os)
(equal-test "text" #'get-output-stream-string os)
(eql-test #\< #'write-char #\< os)
(equal-test "text" #'write-string "text" os :start 1 :end 3)
(eql-test #\> #'write-char #\> os)
(equal-test "<ex>" #'get-output-stream-string os)
(error-test #'write-string #\a)
(close os)


;; open					- function
(setq name #P"delete-me.text")
(bool-eval t '(setq file (open name :direction :output)))
(equal-test "some text" #'write-line "some text" file)
(close file)
(equal-test "delete-me.text" #'file-namestring (truename name))
(setq file (open name :direction :output :if-exists :rename))
(equal-test "other text" #'write-line "other text" file)
(close file)
(equal-test "delete-me.text" #'file-namestring (truename name))
;; Clisp returns the pathname if the file exists
#+xedit (eq-test t #'delete-file name)
#+clisp (bool-test t #'delete-file name)
(setq backup
	#+xedit "delete-me.text~"
	#+clisp "delete-me.text%"
	#+cmu "delete-me.text.BAK")
(bool-test t #'delete-file backup)
(eq-test nil #'delete-file name)
(eq-test nil #'directory name)
(eq-test nil #'directory backup)
;; test append
(with-open-file (s name :direction :output :if-exists :error)
    (write-line "line 1" s))
(with-open-file (s name :direction :output :if-exists :append)
    (write-line "line 2" s))
(with-open-file (s name :direction :input)
    (equal-test "line 1" #'read-line s)
    (equal-test "line 2" #'read-line s)
    (eq-test 'eof #'read-line s nil 'eof)
)
(bool-test t #'delete-file name)
;; test overwrite
(with-open-file (s name :direction :output :if-exists :error)
    (write-line "overwrite-me" s))
(with-open-file (s name :direction :output :if-exists :overwrite)
    (write-line "some-text" s))
(with-open-file (s name :direction :input)
    (equal-test "some-text" #'read-line s)
    (eq-test 'eof #'read-line s nil 'eof))
;; test check for file existence
(eq-test nil #'open name :direction :output :if-exists nil)
(error-test #'open name :direction :output :if-exists :error)
(bool-test t #'delete-file name)
;; test check for no file existence
(eq-test nil #'open name :direction :output :if-does-not-exist nil)
(error-test #'open name :direction :output :if-does-not-exist :error)
#+xedit	;; test io -- not sure if this is the expected behaviour
(progn
    (with-open-file (s name :direction :io)
	(write-line "foo" s)
	(write-line "bar" s))
    (with-open-file (s name :direction :io :if-exists :append)
	(equal-test "foo" #'read-line s)
	(equal-test "bar" #'read-line s)
	(eq-test 'eof #'read-line s nil 'eof)
	(write-line "baz" s))
    (with-open-file (s name :direction :io :if-exists :append)
	(equal-test "foo" #'read-line s)
	(equal-test "bar" #'read-line s)
	(equal-test "baz" #'read-line s)
	(eq-test 'eof #'read-line s nil 'eof))
    (bool-test t #'delete-file name)
)

;; delete-file				- function
(eq-eval nil
    '(with-open-file (s "delete-me.text" :direction :output :if-exists :error)))
(eq-test t #'pathnamep (setq p (probe-file "delete-me.text")))
(bool-test t #'delete-file p)
(eq-test nil #'probe-file "delete-me.text")
(bool-eval t
    '(with-open-file (s "delete-me.text" :direction :output :if-exists :error)
       (delete-file s)))
(bool-test nil #'probe-file "delete-me.text")

;; rename-file				- function
(setq name "foo.bar")
(bool-eval t '(setq file (open name :direction :output :if-exists :error)))
(eq-test t #'close file)
(setq result (multiple-value-list (rename-file name "bar.foo")))
(eql-test 3 #'length result)
(eq-test t #'pathnamep (first result))
(eq-test t #'pathnamep (second result))
(eq-test t #'pathnamep (third result))
(equal-test (third result) #'truename "bar.foo")
(eq-test nil #'directory name)
(eq-test nil #'directory (second result))
(equal-test (list (third result)) #'directory (third result))
(error-test #'truename name)
(error-test #'truename (second result))
(eq-test nil #'probe-file name)
(bool-test t #'probe-file (first result))
(eq-test nil #'probe-file (second result))
(bool-test t #'probe-file (third result))
(bool-test t #'delete-file "bar.foo")
(eq-test nil #'delete-file (third result))
(eq-test nil #'delete-file (second result))
