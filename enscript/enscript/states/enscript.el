;;
;; Emacs help commands for enscript.
;; Copyright (c) 1997 Markku Rossi.
;; Author: Markku Rossi <mtr@iki.fi>
;;

;;
;; This file is part of GNU enscript.
;;
;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program; see the file COPYING.  If not, write to
;; the Free Software Foundation, 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.
;;

;/* Keywords:
;   (build-re '(auto break case char const continue default do double else
;               enum extern float for goto if int long register return
;		short signed sizeof static struct switch typedef union
;		unsigned void volatile while))
;*/

(defun fetch-first-chars (lst)
  "Fetch the initial character of list LST of strings."
  (let ((result '())
	(str ""))
    (mapcar
     (lambda (str)
       (let ((ch (string-to-char str)))
	 (if (not (member ch result))
	     (setq result (cons ch result)))))
     lst)
    (sort result (function <))))

(defun fetch-with-prefix (prefix lst)
  "Fetch the list items from list LST with start with PREFIX.  The fetched
items are modified so that the prefix is removed from strings."
  (let ((result '())
	(prefix-len (length prefix)))
    (mapcar
     (lambda (str)
       (if (and (>= (length str) prefix-len)
		(string= prefix (substring str 0 prefix-len)))
	   (setq result (cons (substring str prefix-len) result))))
     lst)
    result))

(defun build-tree (lst)
  "Build a regular expressions tree from list LST of words to match."
  (mapcar
   (lambda (prefix)
     (if (= prefix 0)
	 ""
       (setq prefix (char-to-string prefix))
       (let ((result (fetch-with-prefix prefix lst)))
	 (if (= (length result) 1)
	     (concat prefix (car result))
	   (let ((rest (build-tree result)))
	     (if (and (= (length rest) 1) (listp (car rest)))
		 (cons (concat prefix (car (car rest))) (cdr (car rest)))
	       (cons prefix rest)))))))
   (fetch-first-chars lst)))

(defun join (list glue result)
  (if (stringp list)
      list
    (if (= (length list) 1)
	(concat result (car list))
      (join (cdr list) glue (concat result (car list) glue)))))

(defun join-column (list glue result column pos)
  (if (and (> (+ pos (length (car list)) (length glue)) column) (> pos 0))
      (let ((len (length result))
	    (gluelen (length glue)))
	(join-column list glue
		     (concat (substring result 0 (- len gluelen)) "\\\n" glue)
		     column 0))
    (if (= (length list) 1)
	(concat result (car list))
      (join-column (cdr list) glue (concat result (car list) glue) column
		   (+ pos (length (car list)) (length glue))))))

(defun join-tree (tree case-insensitive)
  "Join regular expression tree TREE to a string.  Argument CASE-INSENSITIVE
specifies whatever the generated expression matches its words case
insensitively or not."
  (join-column
   (mapcar
    (lambda (item)
      (if (stringp item)
	  (if case-insensitive
	      (make-case-insensitive-regexp item)
	    item)
	(concat (if case-insensitive
		    (make-case-insensitive-regexp (car item))
		  (car item))
		"("
		(join (join-tree (cdr item) case-insensitive) "|" "") ")")))
    tree)
   "|" "" 70 0))

(defun make-case-insensitive-regexp (string)
  (let ((result ""))
    (while (not (string= string ""))
      (let* ((ch (string-to-char string))
	     (uch (upcase ch)))
	(if (= ch uch)
	    (progn
	      (setq string (substring string 1))
	      (setq result (concat result (char-to-string ch))))
	  (setq string (substring string 1))
	  (setq result (concat result "[" (char-to-string ch)
			       (char-to-string uch) "]")))))
    result))

(defun build-re (words &optional case-insensitive)
  "Build an optimized regular expression from list WORDS which can contain
symbols and strings.  Optional second argument CASE-INSENSITIVE specifies
whatever the created regular expression should match its keywords case
insensitively or not.  The default is case sensitive matching.  If the
function is enclosed in C-comments, it inserts the generated regular expression
after the closing \"*/\" sequence, otherwise it returns regular expression
as a string."
  (save-excursion
    (let ((re (concat "/\\b("
		      (join-tree (build-tree (mapcar (lambda (item)
						       (if (stringp item)
							   item
							 (symbol-name item)))
						     words))
				 case-insensitive)
		      ")\\b/ {")))
      (if (search-forward "*/" nil t)
	  (progn
	    (open-line 2)
	    (next-line 1)
	    (insert "  " re))
	re))))
