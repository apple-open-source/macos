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
;; $XFree86: xc/programs/xedit/lisp/test/regex.lsp,v 1.1 2002/12/10 03:59:04 paulo Exp $
;;

;; Basic regex tests. This file is only for xedit lisp and for it's regex
;; library. Note that the regex library used by xedit lisp is not mean't
;; to be fully compatible with most regexes, but to be as fast as possible.
;; This means that some patterns that looks basic may never be matched,
;; but it is expected that almost any pattern can be rewritten to be
;; matched, or in the worst case, it may be required to search in the
;; regions matched by a previous regex.

(defun re-test (expect &rest arguments &aux result (error t) unused error-value)
    (multiple-value-setq
	(unused error-value)
	(ignore-errors
	    (setq result (apply #'re-exec arguments))
	    (setq error nil)))
    (if error
	(format t "ERROR: (re-exec~{ ~S~}) => ~S~%" arguments error-value)
	(or (equal result expect)
	    (format t "(re-exec~{ ~S~}) => should be ~S not ~S~%"
		arguments expect result))))

;; errors only generated for regex compilation (or incorrect arguments)
(defun re-error (&rest arguments &aux result (error t) unused error-value)
    (multiple-value-setq
	(unused error-value)
	(ignore-errors
	    (setq result (apply #'re-comp arguments))
	    (setq error nil)))
    (or error
	(format t "ERROR: no error for (re-comp~{ ~S~})" arguments)))

(re-error "")
(re-error "a**")
(re-error "[a")
(re-error "a{")
(re-error "a(")
(re-error "a||b")
(re-error "|b|c")
(re-error "a|b|")

(setq re (re-comp "abc"))
(re-test '((0 . 3)) re "abc")
(re-test '((0 . 3)) re "abc" :notbol t)
(re-test '((0 . 3)) re "abc" :noteol t)
(re-test '((0 . 3)) re "abc" :notbol t :noteol t)
(re-test '((14 . 17)) re "aaaaaaaaaaaaaaabc")
(re-test '((14 . 17)) re "aaaaaaaaaaaaaaabc" :start 12 :end 17)
(re-test '((30 . 33)) re "xxxxxxxxxxxxxxaaaaaaaaaaaaaaaaabcxx")
(re-test '((30 . 33)) re "xxxxxxxxxxxxxxaaaaaaaaaaaaaaaaabcxx" :start 28 :end 34)

(setq re (re-comp "^abc"))
(re-test '((0 . 3)) re "abc")
(re-test :nomatch re "xabc")
(re-test '((1 . 4)) re "xabc" :start 1)
(re-test :nomatch re "xabc" :start 1 :notbol t)

(setq re (re-comp "abc$"))
(re-test '((0 . 3)) re "abc")
(re-test :nomatch re "xabcx")
(re-test '((1 . 4)) re "xabcx" :end 4)
(re-test :nomatch re "xabc" :end 4 :noteol t)

(setq re (re-comp "^abc$"))
(re-test '((0 . 3)) re "abc")
(re-test :nomatch re "xabcx")
(re-test '((1 . 4)) re "xabcx" :start 1 :end 4)
(re-test :nomatch re "xabcx" :start 1 :end 4 :notbol t)
(re-test :nomatch re "xabcx" :start 1 :end 4 :noteol t)
(re-test :nomatch re "xabcx" :start 1 :end 4 :notbol t :noteol t)
(re-test nil re "abc" :count 0)

(setq re (re-comp "abc|bcd|cde"))
(re-test '((0 . 3)) re "abc")
(re-test '((1 . 4)) re "aabc")
(re-test '((3 . 6)) re "xxxbcdef")
(re-test '((8 . 11)) re "abdzzzcdabcde")
(re-test '((13 . 16)) re "xxxxabdecdabdcde")

(setq re (re-comp "^abc|bcd$|cde"))
(re-test '((0 . 3)) re "abcde")
(re-test '((3 . 6)) re "xabcde")
(re-test '((1 . 4)) re "xabcde" :start 1)
(re-test '((3 . 6)) re "xabcde" :start 1 :notbol t)
(re-test '((2 . 5)) re "xabcd")
(re-test :nomatch re "xabcd" :noteol t)
(re-test nil re "xabcd" :count 0)
(re-test :nomatch re "abcdx" :notbol t)

(setq re (re-comp "a?bc|ab?c|abc?"))
(re-test '((0 . 3)) re "abc")
(re-test :nomatch re "xxxb")
(re-test '((3 . 5)) re "xxxbc")
(re-test '((5 . 7)) re "sssssab")
(re-test '((0 . 3)) re "abcd")
(re-test '((1 . 4)) re "aabcdef")
(re-test '((1 . 3)) re "aabbccdef")	;; ab matches abc?

(setq re (re-comp "a?bc"))
(re-test '((2 . 4)) re "acbcd")
(re-test '((2 . 5)) re "acabcd")

(setq re (re-comp "ab?c"))
(re-test '((1 . 3)) re "xacc")
(re-test '((2 . 5)) re "xxabcc")

(setq re (re-comp "abc?"))
(re-test '((1 . 3)) re "xababc")
(re-test '((2 . 5)) re "xxabccabc")

(setq re (re-comp "a*bc|ab*c|abc*"))
(re-test '((0 . 9)) re "aaaaaaabc")
(re-test '((1 . 10)) re "xaaaaaaabc")
(re-test '((3 . 12)) re "xyzaaaaaaabc")
(re-test '((0 . 4)) re "abbc")
(re-test '((2 . 9)) re "xxabbbbbc")
(re-test '((0 . 12)) re "abcccccccccc")
(re-test '((0 . 12)) re "abccccccccccd")
(re-test '((16 . 29)) re "xxxxxxxaaaaaaaaaabbbbbbbbbbbccccccccccc")
(re-test '((11 . 13)) re "xxxbbbbbbbbbc")
(re-test '((8 . 10)) re "aaaaazbxacd")

(setq re (re-comp "a*bc"))
(re-test '((2 . 4)) re "acbcd")
(re-test '((2 . 5)) re "acabcd")
(re-test '((2 . 8)) re "acaaaabcd")

(setq re (re-comp "ab*c"))
(re-test '((1 . 3)) re "xacc")
(re-test '((2 . 5)) re "xxabcc")
(re-test '((3 . 8)) re "xxaabbbcc")

(setq re (re-comp "abc*"))
(re-test '((1 . 3)) re "xababc")
(re-test '((2 . 5)) re "xxabcbabccc")
(re-test '((3 . 7)) re "axxabccabc")

(setq re (re-comp "a+bc|ab+c|abc+"))
(re-test :nomatch re "xxxbc")
(re-test '((1 . 6)) re "xaaabc")
(re-test '((8 . 12)) re "zzzzaaaaabbc")
(re-test '((7 . 15)) re "zzzzaaaabbbbbbcccc")

(setq re (re-comp "a.c"))
(re-test '((0 . 3)) re "abc")
(re-test '((1 . 4)) re "aaac")
(re-test :nomatch re "xac")
(re-test '((3 . 6)) re "xaxaac")
(re-test '((2 . 5)) re "xxabc")
(re-test '((3 . 6)) re "acxaxc")

(setq re (re-comp "a*c"))
(re-test '((0 . 1)) re "c")
(re-test '((5 . 6)) re "xxxxxc")
(re-test '((8 . 9)) re "xxxxxxxxc")
(re-test '((7 . 8)) re "xxxxxxxcc")
(re-test '((0 . 2)) re "ac")
(re-test '((0 . 5)) re "aaaac")
(re-test '((1 . 3)) re "xac")
(re-test '((3 . 6)) re "xxxaac")
(re-test '((2 . 4)) re "xxac")
(re-test '((4 . 6)) re "xxxxac")

(setq re (re-comp "a+c"))
(re-test '((2 . 5)) re "xxaac")
(re-test '((3 . 8)) re "xxxaaaac")
(re-test '((6 . 8)) re "xaaaabac")
(re-test :nomatch re "xxxc")
(re-test '((4 . 9)) re "xxxxaaaaccc")

(setq re (re-comp "a{4}b"))
(re-test '((19 . 24)) re "xabxxaabxxxaaabxxxxaaaab")
(re-test '((4 . 9)) re "aaabaaaab")

(setq re (re-comp "a{4,}b"))
(re-test '((3 . 8)) re "xxxaaaab")
(re-test '((8 . 25)) re "zaaabzzzaaaaaaaaaaaaaaaab")

(setq re (re-comp "a{,4}b"))
(re-test '((0 . 1)) re "b")
(re-test '((8 . 9)) re "xxxxxxxxb")
(re-test '((6 . 11)) re "xaaaaaaaaab")
(re-test '((3 . 5)) re "xxxab")
(re-test '((6 . 10)) re "aaaaaxaaab")

(setq re (re-comp "a{2,4}b"))
(re-test :nomatch re "xab")
(re-test '((1 . 4)) re "xaab")
(re-test '((1 . 5)) re "xaaab")
(re-test '((2 . 7)) re "xxaaaab")
(re-test '((4 . 9)) re "xxxaaaaab")

(setq re (re-comp "foo(bar|baz)fee"))
(re-test '((9 . 18)) re "feebarbazfoobarfee")
(re-test '((9 . 18) (12 . 15)) re "feebarbazfoobarfee" :count 2)
(re-test '((13 . 22)) re "foofooobazfeefoobazfee")
(re-test '((13 . 22) (16 . 19)) re "foofooobazfeefoobazfee" :count 3)

(setq re (re-comp "foo(bar|baz)fee" :nosub t))
(re-test '((9 . 18)) re "feebarbazfoobarfee")
(re-test '((9 . 18)) re "feebarbazfoobarfee" :count 2)
(re-test '((13 . 22)) re "foofooobazfeefoobazfee")
(re-test '((13 . 22)) re "foofooobazfeefoobazfee" :count 3)

(setq re (re-comp "f(oo|ee)ba[rz]"))
(re-test :nomatch re "barfoebaz")
(re-test '((3 . 9) (4 . 6)) re "bazfoobar" :count 2)
(re-test '((3 . 9) (4 . 6)) re "barfeebaz" :count 2)

(setq re (re-comp "f(oo|ee)ba[rz]" :nosub t))
(re-test :nomatch re "barfoebaz")
(re-test '((3 . 9)) re "bazfoobar" :count 2)
(re-test '((3 . 9)) re "barfeebaz" :count 2)

(setq re (re-comp "\\<(int|char)\\>"))
(re-test '((15 . 18)) re "aint character int foo")
(re-test '((15 . 18) (15 . 18)) re "aint character int foo" :count 2)

(setq re (re-comp "\\<(int|char)\\>" :nosub t))
(re-test '((15 . 18)) re "aint character int foo" :count 2)

(setq re (re-comp "foo.*bar"))
(re-test '((11 . 17)) re "barfoblaboofoobarfoobarfoobar")

(setq re (re-comp "foo.+bar"))
(re-test :nomatch re "foobar")
(re-test '((6 . 13)) re "fobbarfooxbarfooybar")

(setq re (re-comp "foo.?bar"))
(re-test '((1 . 7)) re "xfoobar")
(re-test :nomatch re "xxfooxxbar")
(re-test '((3 . 10)) re "yyyfootbar")

(setq re (re-comp "a.*b.*c"))
(re-test '((0 . 3)) re "abc")
(re-test '((9 . 18)) re "xxxxxxxxxabbbbbbbccaaaaabbbc")

(setq re (re-comp "a.+b.*c"))
(re-test :nomatch re "xxxabc")
(re-test '((2 . 7)) re "xxaxbbc")

(setq re (re-comp "a.+b.?c"))
(re-test '((1 . 5)) re "xaabc")
(re-test '((2 . 7)) re "xxaabbc")

(setq re (re-comp "(foo.*|bar)fee"))
(re-test '((3 . 9) (3 . 6)) re "barfoofee" :count 2)
(re-test '((0 . 9) (0 . 6)) re "foobarfee" :count 2)
(re-test '((4 . 10) (4 . 7)) re "xxfobarfee" :count 2)
(re-test '((3 . 17) (3 . 14)) re "barfooooooobarfee" :count 2)
(re-test '((4 . 10) (4 . 7)) re "xxfobarfeefoobar" :count 2)

(setq re (re-comp "(foo.+|bar)fee"))
(re-test :nomatch re "barfoofee" :count 2)
(re-test '((3 . 10) (3 . 7)) re "barfooxfee" :count 2)

(setq re (re-comp "(foo.?|bar)fee"))
(re-test :nomatch re "foobar" :count 2)
(re-test '((2 . 8) (2 . 5)) re "bafoofee" :count 2)
(re-test '((2 . 9) (2 . 6)) re "bafooofeebarfee" :count 4)
(re-test '((2 . 8) (2 . 5)) re "bafoofeebarfee" :count 2)
(re-test nil re "bafoofeebarfee" :count 0)
(re-test '((2 . 8)) re "bafoofeebarfee" :count 1)

(setq re (re-comp "(a|b|c)\\1"))
(re-test '((0 . 2) (0 . 1)) re "aa" :count 2)

(setq re (re-comp "(a|b|c)(a|b|c)\\1\\2"))
(re-test '((0 . 4) (0 . 1) (1 . 2)) re "acac" :count 5)
(re-test '((4 . 8) (4 . 5) (5 . 6)) re "xxxxacac" :count 4)
(re-test '((24 . 28) (24 . 25) (25 . 26)) re "xxacabacbcacbbacbcaaccabcaca" :count 3)
(re-test '((4 . 8) (4 . 5) (5 . 6)) re "xyabcccc" :count 3)
(re-test '((4 . 8) (4 . 5)) re "xyabcccc" :count 2)
(re-test '((4 . 8)) re "xyabcccc" :count 1)
(re-test nil re "xyabcccc" :count 0)

(setq re (re-comp "(a*b)\\1"))
(re-test '((3 . 15) (3 . 9)) re "xxxaaaaabaaaaab" :count 2)
(re-test '((7 . 9) (7 . 8)) re "abaabaxbb" :count 2)

(setq re (re-comp "(ab+c)\\1"))
(re-test '((3 . 13) (3 . 8)) re "xaaabbbcabbbc" :count 3)

(setq re (re-comp "(ab?c)\\1"))
(re-test :nomatch re "abcac" :count 2)
(re-test '((4 . 8) (4 . 6)) re "acabacac" :count 2)
(re-test '((5 . 11) (5 . 8)) re "abcacabcabc" :count 2)
(re-test '((3 . 7) (3 . 5)) re "abcacac" :count 2)

(setq re (re-comp "a(.*)b\\1"))
(re-test '((3 . 5) (4 . 4)) re "xxxab" :count 2)
(re-test '((4 . 12) (5 . 8)) re "xxxxazzzbzzz" :count 2)

(setq re (re-comp "abc" :icase t))
(re-test '((0 . 3)) re "AbC")

(setq re (re-comp "[0-9][a-z]+" :icase t))
(re-test '((3 . 10)) re "xxx0aaZxYT9")

(setq re (re-comp "a.b" :icase t))
(re-test '((10 . 13)) re "aaaaaaaaaaaxB")

(setq re (re-comp "a.*z" :icase t))
(re-test '((3 . 9)) re "xxxAaaaaZ")
(re-test '((2 . 6)) re "xxaaaZaaa")

(setq re (re-comp "\\<(lambda|defun|defmacro)\\>" :icase t))
(re-test '((5 . 11)) re "    (lambda")
(re-test '((5 . 11) (5 . 11)) re "    (lambda" :count 2)
(re-test :nomatch re "lamda defunn deffmacro")

(setq re (re-comp "\\<(nil|t)\\>" :icase t))
(re-test '((3 . 6)) re "it Nil")
(re-test '((3 . 6) (3 . 6)) re "it Nil" :count 6)
(re-test :nomatch re "nilo")

(setq re (re-comp "\\<(begin|end)\\>" :icase t))
(re-test '((21 . 24) (21 . 24)) re "beginning the ending EnD" :count 7)

(setq re (re-comp "a.*" :newline t))
(re-test '((0 . 1)) re "a
aaa")
(re-test '((3 . 4)) re "xyza
aa")

(setq re (re-comp "a.+" :newline t))
(re-test '((2 . 5)) re "a
aaa")
(re-test '((5 . 7)) re "xyza
aa")

(setq re (re-comp "a.?" :newline t))
(re-test '((0 . 1)) re "a
aaa")
(re-test '((3 . 4)) re "xyza
aa")

(setq re (re-comp "a.*b.*c" :newline t))
(re-test '((11 . 14)) re "xxaa
zyacb
abc")
(re-test '((6 . 9)) re "xxxab
abc
c")

(setq re (re-comp "a.+b.*c" :newline t))
(re-test '((6 . 10)) re "ab
bc
abbc")

(setq re (re-comp "a.?b.*c" :newline t))
(re-test '((4 . 8)) re "ab
cabbc
cc")

(setq re (re-comp "^foo$" :newline t))
(re-test '((11 . 14)) re "bar
foobar
foo")
(re-test '((0 . 3)) re "foo
bar
foo
bar")
(re-test '((8 . 11)) re "foo
bar
foo
bar" :notbol t)
(re-test '((8 . 11)) re "foo
bar
foo" :notbol t)
(re-test :nomatch re "foo
bar
foo" :notbol t :noteol t)

(setq re (re-comp "^\\s*#\\s*(define|include)\\s+.+" :newline t))
(re-test '((8 . 18)) re "#define
#include x")
(re-test '((8 . 18) (9 . 16)) re "#define
#include x" :count 2)

(setq re (re-comp "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"))
(re-test '((3 . 259)) re "zzzxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxzzz")

(setq re (re-comp "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890~ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890~ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890~ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890~ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890~"))
(re-test '((13 . 333)) re "String here: ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890~ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890~ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890~ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890~ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890~/")

(setq re (re-comp "(.*)\\D(\\d+)"))
(re-test '((0 . 6) (0 . 3) (4 . 6)) re "abcW12" :count 3)
(re-test '((0 . 6) (0 . 3)) re "abcW12" :count 2)
(re-test '((0 . 6)) re "abcW12" :count 1)
(re-test nil re "abcW12" :count 0)
(re-test '((0 . 6) (0 . 3) (4 . 6)) re "abcW12abcW12" :count 3)
(re-test '((0 . 6) (0 . 3) (4 . 6)) re "abcW12abcW12a" :count 3)

(setq re (re-comp ".*\\d"))
(re-test '((0 . 2)) re "a1a1a1aaaaaaa")			; minimal match only

(setq re (re-comp "(.*)\\d"))
(re-test '((0 . 2) (0 . 1)) re "a1a1a1aaaaaaa" :count 2); minimal match only

(setq re (re-comp ".*(\\d)"))
(re-test '((0 . 2) (1 . 2)) re "a1a1a1aaaaaaa" :count 2); minimal match only

;; XXX this very simple pattern was entering an infinite loop
;; actually, this pattern is not supported, just test if is not
;; crashing (not supported because it is not cheap to match variations
;; of the pattern)
(setq re (re-comp "(.*a)?"))
(re-test '((0 . 1)) re "aaaa")		; expected, minimal match
(re-test '((0 . 1) (0 . 1)) re "aaaa" :count 2)
