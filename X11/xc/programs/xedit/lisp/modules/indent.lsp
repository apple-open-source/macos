;
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
;; $XFree86: xc/programs/xedit/lisp/modules/indent.lsp,v 1.7 2003/01/29 03:05:53 paulo Exp $
;;

(provide "indent")
(require "xedit")
(in-package "XEDIT")

(defconstant indent-spaces '(#\Tab #\Space))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; The final indentation function.
;; Parameters:
;;	indent
;;		Number of spaces to insert
;;	offset
;;		Offset to where indentation should be added
;;	no-tabs
;;		If set, tabs aren't inserted
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun indent-text (indent offset &optional no-tabs
		    &aux start line length index current tabs spaces string
			 barrier base result (point (point))
		   )

    ;; Initialize
    (setq
	start	(scan offset :eol :left)
	line	(read-text start (- offset start))
	length	(length line)
	index	(1- length)
	current	0
	base	0
    )

    (and (minusp indent) (setq indent 0))

    ;; Skip any spaces after offset, "paranoia check"
    (while (member (char-after offset) indent-spaces)
	(incf offset)
    )

    ;; Check if there are only spaces before `offset' and the line `start'
    (while (and (>= index 0) (member (char line index) indent-spaces))
	(decf index)
    )

    ;; `index' will be zero if there are only spaces in the `line'
    (setq barrier (+ start (incf index)))

    ;; Calculate `base' unmodifiable indentation, if any
    (dotimes (i index)
	(if (char= (char line i) #\Tab)
	    (incf base (- 8 (rem base 8)))
	    (incf base)
	)
    )

    ;; If any non blank character would need to be deleted
    (and (> base indent) (return-from indent-text nil))

    ;; Calculate `current' indentation
    (setq current base)
    (while (< index length)
	(if (char= (char line index) #\Tab)
	    (incf current (- 8 (rem current 8)))
	    (incf current)
	)
	(incf index)
    )

    ;; Maybe could also "optimize" the indentation even if it is already
    ;; correct, removing spaces "inside" tabs.
    (when (/= indent current)
	(if no-tabs
	    (setq
		length	(- indent base)
		result	(+ barrier length)
		string	(make-string length :initial-element #\Space)
	    )
	    (progn
		(multiple-value-setq (tabs spaces) (floor (- indent base) 8))
		(setq
		    length	(+ tabs spaces)
		    result	(+ barrier length)
		    string	(make-string length :initial-element #\Tab)
		)
		(fill string #\Space :start tabs)
	    )
	)

	(replace-text barrier offset string)
	(and (>= offset point) (>= point barrier) (goto-char result))
    )
)
(compile 'indent-text)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Helper function, returns indentation of a given offset
;; If `align' is set, stop once a non blank character is seen, that
;; is, use `offset' only as a line identifier
;; If `resolve' is set, it means that the offset is just a hint, it
;; maybe anywhere in the line
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun offset-indentation (offset &key resolve align
			   &aux
			   char
			   line
			   (start (scan offset :eol :left))
			   (indent 0))
    (if resolve
	(loop
	    (if (characterp (setq char (char-after start)))
		(if (char= char #\Tab)
		    (incf indent (- 8 (rem indent 8)))
		    ;; Not a tab, check if is a space
		    (if (char= char #\Space)
			(incf indent)
			;; Not a tab neither a space
			(return indent)
		    )
		)
		;; EOF found
		(return indent)
	    )
	    ;; Increment offset to check next character
	    (incf start)
	)
	(progn
	    (setq line (read-text start (- offset start)))
	    (dotimes (i (length line) indent)
		(if (char= (setq char (char line i)) #\Tab)
		    (incf indent (- 8 (rem indent 8)))
		    (progn
			(or align (member char indent-spaces)
			    (return indent)
			)
			(incf indent)
		    )
		)
	    )
	)
    )
)
(compile 'offset-indentation)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  A default/fallback indentation function, just copy indentation
;; of previous line.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun default-indent (syntax syntable)
    (let
	(
	(offset (scan (point) :eol :left))
	start
	left
	right
	)

	syntable	;; XXX hack to not generate warning about unused
			;; variable, should be temporary (until unused
			;; variables can be declared as such)

	(if
	    (or
		;; if indentation is disabled
		(and
		    (hash-table-p (syntax-options syntax))
		    (gethash :disable-indent (syntax-options syntax))
		)
		;; or if not at the start of a new line
		(> (scan offset :eol :right) offset)
	    )
	    (return-from default-indent)
	)

	(setq left offset)
	(loop
	    (setq
		start left
		left (scan start :eol :left :count 2)
		right (scan left :eol :right)
	    )
	    ;; if start of file reached
	    (and (>= left start) (return))
	    (when
		(setq
		    start
		    (position-if-not
			#'(lambda (char) (member char indent-spaces))
			(read-text left (- right left))
		    )
		)

		;; indent the current line
		(indent-text (offset-indentation (+ left start) :align t) offset)
		(return)
	    )
	)
    )
)
(compile 'default-indent)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Helper function
;;   Clear line before cursor if it is empty
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun indent-clear-empty-line (&aux left offset right line index)
    (setq
	offset	(scan (point) :eol :left)
	left	(scan offset :eol :left :count 2)
	right	(scan left :eol :right)
    )

    ;; If not at the first line in the file and line is not already empty
    (when (and (/= offset left) (/= left right))
	(setq
	    line	(read-text left (- right left))
	    index	(1- (length line))
	)
	(while (and (>= index 0) (member (char line index) indent-spaces))
	    (decf index)
	)
	;; If line was only spaces
	(and (minusp index) (replace-text left right ""))
    )
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Macro to be called whenever an indentation rule decides that
;; the parser is done.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro indent-macro-terminate (&optional result)
    `(return-from ind-terminate-block ,result)
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Like indent-terminate, but "rejects" the input for the current line
;; and terminates the loop.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro indent-macro-reject (&optional result)
   `(progn
	(setq ind-state ind-prev-state)
	(return-from ind-terminate-block ,result)
    )
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Like indent-reject, but "rejects" anything before the current token
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro indent-macro-reject-left (&optional result)
   `(progn
	(setq ind-state ind-matches)
	(return-from ind-terminate-block ,result)
    )
)


(defstruct indtoken
    regex			;; a string, character or regex
    token			;; the resulting token, nil or a keyword
    begin			;; begin a new table
    switch			;; switch to another table
    ;; begin and switch fields are used like the ones for the syntax highlight
    ;; syntoken structure.
    label			;; filed at compile time
    code			;; code to execute when it matches
)

(defstruct indtable
    label			;; a keyword, name of the table
    tokens			;; list of indtoken structures
    tables			;; list of indtable structures
    augments			;; augment list
)

(defstruct indaugment
    labels			;; list of keywords labeling tables
)

(defstruct indinit
    variables			;; list of variables and optional initialization
    ;; Format of variables must be suitable to LET*, example of call:
    ;;	(indinit
    ;;	    var1		;; initialized to NIL
    ;;	    (var2 (afun))	;; initialized to the value returned by AFUN
    ;;	)
)

(defstruct indreduce
    token			;; reduced token
    rules			;; list of rules
    label			;; unique label associated with rule, this
				;; field is automatically filled in the
				;; compilation process. this field exists
				;; to allow several indreduce definitions
				;; that result in the same token
    check			;; FORM evaluated, if T apply reduce rule
    code			;; PROGN to be called when a rule matches
)

;; NOTE, unlike "reduce" rules, "resolve" rules cannot be duplicated
(defstruct indresolve
    match			;; the matched token (or a list of tokens)
    code			;; PROGN to apply for this token
)

(defstruct indent
    reduces			;; list of indreduce structures
    tables			;; list of indtable structures
    inits			;; initialization list
    resolves			;; list of indresolve structures
    token-code			;; code to execute when a token matches
    check-code			;; code to execute before applying a reduce rule
    reduce-code			;; code to execute after reduce rule
    resolve-code		;; code to execute when matching a token
)

(defmacro defindent (variable label &rest lists)
   `(if (boundp ',variable)
	,variable
	(progn
	    (proclaim '(special ,variable))
	    (setq ,variable (compile-indent-table ,label ,@lists))
	)
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Create an indent token.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro indtoken (pattern token
		    &key icase nospec begin switch code (nosub t))
    (setq pattern (re-comp (eval pattern) :icase icase :nospec nospec :nosub nosub))
    (when (consp (re-exec pattern "" :notbol t :noteol t))
	(error "INDTOKEN: regex ~A matches empty string" pattern)
    )

    ;; result of macro, return token structure
    (make-indtoken
	:regex	pattern
	:token	token
	:begin	begin
	:switch	switch
	:code	code
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Create an indentation table. Basically a list of indentation tokens.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun indtable (label &rest definitions)
    ;; check for simple errors
    (unless (keywordp label)
	(error "INDTABLE: ~A is not a keyword" label)
    )
    (dolist (item definitions)
	(unless
	    (or
		(atom item)
		(indtoken-p item)
		(indtable-p item)
		(indaugment-p item)
	    )
	    (error "INDTABLE: invalid indent table argument ~A" item)
	)
    )

    ;; return indent table structure
    (make-indtable
	:label		label
	:tokens		(remove-if-not #'indtoken-p definitions)
	:tables		(remove-if-not #'indtable-p definitions)
	:augments	(remove-if-not #'indaugment-p definitions)
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Add identifier to list of augment tables.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun indaugment (&rest keywords)
    (dolist (keyword keywords)
	(unless (keywordp keyword)
	    (error "INDAUGMENT: bad indent table label ~A" keyword)
	)
    )

    ;; return augment list structure
    (make-indaugment :labels keywords)
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Add variables to initialization list
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro indinit (&rest variables)
    (make-indinit :variables variables)
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Create a "reduction rule"
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro indreduce (token check rules &rest code &aux nullp consp)
    ;; check for simple errors
    (unless (or (keywordp token) (null token))
	(error "INDREDUCE: ~A is not a keyword" token)
    )
    (dolist (rule rules)
	(or (listp rule) (error "INDREDUCE: invalid indent rule ~A" rule))
	;; XXX This test is not enough, maybe should add some sort of
	;; runtime check to avoid circularity.
	(and (eq token (car rule)) (null (cdr rule))
	    (error "INDREDUCE: ~A reduces to ~A" token)
	)
	(dolist (item rule)
	    (and (or nullp consp) (not (keywordp item))
		(error "INDREDUCE: a keyword must special pattern")
	    )
	    (if (consp item)
		(progn
		    (unless
			(or
			    (and
				(eq (car item) 'not)
				(keywordp (cadr item))
				(null (cddr item))
			    )
			    (and
				(eq (car item) 'or)
				(null (member-if-not #'keywordp (cdr item)))
			    )
			)
			(error "INDREDUCE: syntax error parsing ~A" item)
		    )
		    (setq consp t)
		)
		(progn
		    (setq nullp (null item) consp nil)
		    (unless (or (keywordp item) nullp (eq item t))
			(error "INDREDUCE: ~A is not a keyword" item)
		    )
		)
	    )
	)
;	(and consp
;	    (error "INDREDUCE: pattern must be followed by keyword")
;	)
    )

    ;; result of macro, return indent reduce structure
    (make-indreduce
	:token	token
	:check	check
	:rules	(remove-if #'null rules)
	:code	code
    )
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Create a "resolve rule"
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro indresolve (match &rest code)
    ;; check for simple errors
    (if (consp match)
	(dolist (token match)
	    (or (keywordp token) (error "INDRESOLVE: ~A is not a keyword" token))
	)
	(or (keywordp match) (error "INDRESOLVE: ~A is not a keyword" match))
    )

    ;; result of macro, return indent resolve structure
    (make-indresolve
	:match	match
	:code	code
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Helper function for compile-indent-table. Returns a list of all
;; tables and tokens for a given table, including tokens and tables
;; of children.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun list-indtable-elements (table &aux result sub-result)
    (setq result (cons (indtable-tokens table) (indtable-tables table)))
    (dolist (child (indtable-tables table))
	(setq sub-result (list-indtable-elements child))
	(rplaca result (append (car result) (car sub-result)))
	(rplacd result (append (cdr result) (cdr sub-result)))
    )
    ;; Return pair of all nested tokens and tables
    result
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; First pass adding augumented tokens to a table, done in two passes
;; to respect inheritance order.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun compile-indent-augment-list (table table-list &aux labels augment tokens)

    ;; Create a list of all augment tables.
    (dolist (augment (indtable-augments table))
	(setq labels (append labels (indaugment-labels augment)))
    )

    ;;  Remove duplicates and references to "itself", without warnings?
    (setq
	labels
	(remove (indtable-label table) (remove-duplicates labels :from-end t))
    )

    ;; Check if the specified indent tables exists!
    (dolist (label labels)
	(unless
	    (setq augment (car (member label table-list :key #'indtable-label)))
	    (error "COMPILE-INDENT-AUGMENT-LIST: Cannot augment ~A in ~A"
		label
		(indtable-label table)
	    )
	)

	;; Increase list of tokens.
	(setq tokens (append tokens (indtable-tokens augment)))
    )

    ;;  Store the tokens in the augment list. They will be added
    ;; to the indent table in the second pass.
    (setf (indtable-augments table) tokens)

    ;;  Recurse on every child table.
    (dolist (child (indtable-tables table))
	(compile-indent-augment-list child table-list)
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Last pass adding augmented tokens to a table.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun link-indent-augment-list (table)
    (setf
	(indtable-tokens table)
	(remove-duplicates
	    (nconc (indtable-tokens table) (indtable-augments table))
	    :key	#'indtoken-regex
	    :test	#'equal
	    :from-end	t
	)

	;;  Don't need to keep this list anymore.
	(indtable-augments table)
	()
    )

    (dolist (child (indtable-tables table))
	(link-indent-augment-list child)
    )
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Compile the indent reduction rules
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun compile-indent-reduces (reduces
			       &aux need label check rules reduce
				    check-code reduce-code)
    (dolist (item reduces)
	(setq
	    label	(indreduce-label item)
	    check	(indreduce-check item)
	    rules	(indreduce-rules item)
	    reduce	(indreduce-code  item)
	    need	(and
			    rules
			    (not label)
			    (or
				reduce
				(null check)
				(not (constantp check))
			    )
			)
	)
	(when need
	    (and (null label) (setq label (intern (string (gensym)) 'keyword)))

	    (setf (indreduce-label item) label)

	    (and
		(or (null check)
		    (not (constantp check))
		)
		(setq
		    check	(list (list 'eq '*ind-label* label) check)
		    check-code	(nconc check-code (list check))
		)
	    )

	    (and reduce
		(setq
		    reduce	(cons (list 'eq '*ind-label* label) reduce)
		    reduce-code	(nconc reduce-code (list reduce))
		)
	    )
	)
    )

    ;; XXX Instead of using COND, could/should use CASE
    ;; TODO Implement a smart CASE in the bytecode compiler, if
    ;;	    possible, should generate a hashtable, or a table
    ;;	    of indexes (for example when all elements in the cases
    ;;	    are characters) and then jump directly to the code.
    (if check-code
	(setq check-code (cons 'cond (nconc check-code '((t t)))))
	(setq check-code t)
    )
    (and reduce-code (setq reduce-code (cons 'cond reduce-code)))

    (values check-code reduce-code)
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Compile the indent resolve code
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun compile-indent-resolves (resolves &aux match resolve resolve-code)
    (and
	(/=
	    (length resolves)
	    (length (remove-duplicates resolves :key #'indresolve-match))
	)
	;; XXX Could do a more complete job and tell what is wrong...
	(error "COMPILE-INDENT-RESOLVES: duplicated labels")
    )

    (dolist (item resolves)
	(when (setq resolve (indresolve-code item))
	    (setq
		match
		(indresolve-match item)

		resolve
		(cons
		    (if (listp match)
			(list 'member '*ind-token* `',match :test `#'eq)
			(list 'eq '*ind-token* match)
		    )
		    resolve
		)

		resolve-code
		(nconc resolve-code (list resolve))
	    )
	)
    )

    (and resolve-code (cons 'cond resolve-code))
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Create an indentation table
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun compile-indent-table (name &rest lists
			     &aux main elements switches begins tables symbols
				  label code token-code check-code reduce-code
				  (inits (remove-if-not #'indinit-p lists))
				  (reduces (remove-if-not #'indreduce-p lists))
				  (resolves (remove-if-not #'indresolve-p lists))
			    )
    (setq
	lists	 (delete-if
		    #'(lambda (object)
			(or
			    (indinit-p object)
			    (indreduce-p object)
			    (indresolve-p object)
			)
		    )
		    lists)
	main	 (apply #'indtable name lists)
	elements (list-indtable-elements main)
	switches (remove-if #'null (car elements) :key #'indtoken-switch)
	begins   (remove-if #'null (car elements) :key #'indtoken-begin)
	tables	 (cons main (cdr elements))
    )

    ;; Check for typos in the keywords, or for not defined indent tables.
    (dolist (item (mapcar #'indtoken-switch switches))
	(unless
	    (or	(and (integerp item) (minusp item))
		(member item tables :key #'indtable-label)
	    )
	    (error "COMPILE-INDENT-TABLE: SWITCH ~A cannot be matched" item)
	)
    )
    (dolist (item (mapcar #'indtoken-begin begins))
	(unless (member item tables :key #'indtable-label)
	    (error "COMPILE-INDENT-TABLE: BEGIN ~A cannot be matched" item)
	)
    )

    ;; Build augment list.
    (compile-indent-augment-list main tables)
    (link-indent-augment-list main)

    ;; Change switch and begin fields to point to the indent table
    (dolist (item switches)
	(if (keywordp (indtoken-switch item))
	    (setf
		(indtoken-switch item)
		(car (member (indtoken-switch item) tables :key #'indtable-label))
	    )
	)
    )
    (dolist (item begins)
	(setf
	    (indtoken-begin item)
	    (car (member (indtoken-begin item) tables :key #'indtable-label))
	)
    )

    ;; Build initialization list
    (dolist (init inits)
	(setq symbols (nconc symbols (indinit-variables init)))
    )

    ;; Build token code
    (dolist (item (car elements))
	(when (setq code (indtoken-code item))
	    (setf
		label
		(intern (string (gensym)) 'keyword)

		(indtoken-label item)
		label

		code
		(list (list 'eq '*ind-label* label) code)

		token-code
		(nconc token-code (list code))
	    )
	)
    )

    (multiple-value-setq
	(check-code reduce-code)
	(compile-indent-reduces reduces)
    )

    (make-indent
	:tables		tables
	:inits		symbols
	:reduces	reduces
	:resolves	resolves
	:token-code	(and token-code (cons 'cond token-code))
	:check-code	check-code
	:reduce-code	reduce-code
	:resolve-code	(compile-indent-resolves resolves)
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Search rule-pattern in match-pattern
;; Returns offset of match, and it's length, if any
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun indent-search-rule (rule-pattern match-pattern
			   &aux start rule rulep matchp test offset length)
    (if (member-if-not #'keywordp rule-pattern)
	;; rule has wildcards
	(progn
	    (setq
		rulep	rule-pattern
		matchp	match-pattern
		start	match-pattern
	    )
	    (loop
		(setq rule (car rulep))
		(cond
		    ;; Special pattern
		    ((consp rule)
			(if (eq (car rule) 'not)
			    (progn
				(setq
				    test	(cadr rule)
				    rulep	(cdr rulep)
				    rule	(car rulep)
				)
				(while
				    (and
					;; something to match
					matchp
					;; NOT match is true
					(not (eq (car matchp) test))
					;; next match is not true
					(not (eq (car matchp) rule))
				    )
				    (setq matchp (cdr matchp))
				)
				(if (eq (car matchp) rule)
				    ;; rule matched
				    (setq
					matchp	(cdr matchp)
					rulep	(cdr rulep)
				    )
				    ;; failed
				    (setq
					rulep	rule-pattern
					matchp	(cdr start)
					start	matchp
				    )
				)
			    )
			    ;; (eq (car rule) 'or)
			    (progn
				(if (member (car matchp) (cdr rule) :test #'eq)
				    (setq rulep (cdr rulep) matchp (cdr matchp))
				    ;; failed
				    (progn
					;; end of match found!
					(and (null matchp) (return))
					;; reset search
					(setq
					    rulep	rule-pattern
					    matchp	(cdr start)
					    start	matchp
					)
				    )
				)
			    )
			)
		    )

		    ;; Skip until end of match-pattern or rule is found
		    ((null rule)
			(setq rulep (cdr rulep))
			;; If matches everything
			(if (null rulep)
			    (progn (setq matchp nil) (return))
			    ;; If next token cannot be matched
			    (unless
				(setq
				    matchp
				    (member (car rulep) matchp :test #'eq)
				)
				(setq rulep rule-pattern)
				(return)
			    )
			)
			(setq rulep (cdr rulep) matchp (cdr matchp))
		    )

		    ;; Matched
		    ((eq rule t)
			;; If there isn't a rule to skip
			(and (null matchp) (return))
			(setq rulep (cdr rulep) matchp (cdr matchp))
		    )

		    ;; Matched
		    ((eq rule (car matchp))
			(setq rulep (cdr rulep) matchp (cdr matchp))
		    )

		    ;; No match
		    (t
			;; end of match found!
			(and (null matchp) (return))
			;; reset search
			(setq
			    rulep	rule-pattern
			    matchp	(cdr start)
			    start	matchp
			)
		    )
		)

		;; if everything matched
		(or rulep (return))
	    )

	    ;; All rules matched
	    (unless rulep
		;; Calculate offset and length of match
		(setq offset 0 length 0)
		(until (eq match-pattern start)
		    (setq
			offset		(1+ offset)
			match-pattern	(cdr match-pattern)
		    )
		)
		(until (eq match-pattern matchp)
		    (setq
			length		(1+ length)
			match-pattern	(cdr match-pattern)
		    )
		)
	    )
	)
	;; no wildcards
	(and (setq offset (search rule-pattern match-pattern :test #'eq))
	     (setq length (length rule-pattern))
	)
    )

    (values offset length)
)
(compile 'indent-search-rule)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Indentation parser
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro indent-macro (ind-definition ind-offset &optional ind-no-tabs)
   `(prog*
	(
	;; Current indentation table
	(ind-table (car (indent-tables ,ind-definition)))

	;; The parser rules
	(ind-reduces (indent-reduces ,ind-definition))

	;; Token list for the table
	(ind-tokens (indtable-tokens ind-table))

	;; Stack of nested tables/states
	ind-stack

	;; indentation to be used
	(*indent* 0)

	;; offset to apply indentation
	*offset*

	;; Number of lines read
	(*ind-lines* 1)

	;; Matched token
	*ind-token*

	;; list of tokens after current match, should not be changed
	*ind-token-list*

	;; label associated with rule
	*ind-label*

	;; offset of match
	*ind-offset*

	;; length of match
	*ind-length*

	;; insert position
	(*ind-point* (point))

	(ind-from (scan ,ind-offset :eol :left))
	(ind-to ,ind-offset)
	(ind-line (read-text ind-from (- ind-to ind-from)))

	;; start of current line
	(*ind-start* ind-from)

	;; State information
	ind-state

	;; For use with (indent-macro-reject)
	ind-prev-state

	;; Matches for the current line
	ind-matches

	;; Matched tokens not yet used
	ind-cache

	;; Pattern being tested
	ind-token

	;; Used when searching for a regex
	ind-match

	;; Table to change
	ind-change

	;; Length of ind-line
	(ind-length (length ind-line))

	;; Don't parse after this offset
	(ind-end ind-length)

	;; Temporary variables used during loops
	ind-left
	ind-right
	ind-tleft
	ind-tright

	;; Set  when start of file is found
	ind-startp

	;; Flag for regex search
	(ind-noteol (< ind-to (scan ind-from :eol :right)))

	;; Initialization variables expanded here
	,@(indent-inits (eval ind-definition))
	)

	;; Initial input already read
	(go :ind-loop)

;------------------------------------------------------------------------
; Read a text line
:ind-read
	(setq
	    ind-to	ind-from
	    ind-from	(scan ind-from :eol :left :count 2)
	)
	;; If start of file reached
	(and (= ind-to ind-from) (setq ind-startp t) (go :ind-process))

	(setq
	    *ind-lines*		(1+ *ind-lines*)
	    ind-to		(scan ind-from :eol :right)
	    ind-line		(read-text ind-from (- ind-to ind-from))
	    ind-length		(length ind-line)
	    ind-end		ind-length
	    ind-noteol		nil
	    ind-cache		nil
	    ind-prev-state	ind-state
	)

;------------------------------------------------------------------------
; Loop parsing backwards
:ind-loop
	(setq ind-matches nil)
	(dolist (token ind-tokens)
	    ;; Prepare to loop
	    (setq
		ind-token	(indtoken-regex token)
		ind-left	0
	    )
	    ;; While the pattern matches
	    (loop
		(setq ind-right ind-left)
		(if
		    (consp
			(setq
			    ind-match
			    (re-exec
				ind-token
				ind-line
				:start	ind-left
				:end	ind-end
				:notbol (> ind-left 0)
				:noteol ind-noteol
			    )
			)
		    )

		    ;; Remember about match
		    (setq
			ind-match   (car ind-match)
			ind-left    (cdr ind-match)
			ind-matches (cons (cons token ind-match) ind-matches)
		    )

		    ;; No match
		    (return)
		)
		;; matched an empty string
		(and (= ind-left ind-right) (incf ind-left))

		;; matched a single eol or bol
		(and (>= ind-left ind-end) (return))
	    )
	)

	;; Add new matches to cache
	(when ind-matches
	    (setq
		ind-cache
		(stable-sort
		    (nconc (nreverse ind-matches) ind-cache) #'< :key #'cadr
		)
	    )
	)

	;; If nothing in the cache
	(or ind-cache (go :ind-process))

	(setq
	    ind-left	(cadar ind-cache)
	    ind-right	(cddar ind-cache)
	    ind-matches	(cdr ind-cache)
	)

	;; If only one element in the cache
	(or ind-matches	(go :ind-parse))

	(setq
	    ind-tleft	(cadar ind-matches)
	    ind-tright	(cddar ind-matches)
	)

	;; Remove overlaps
	(loop
	    (if (or (>= ind-tleft ind-right) (<= ind-tright ind-left))
		;; No overlap
		(progn
		    (setq
			ind-left    ind-tleft
			ind-right   ind-tright
			ind-matches (cdr ind-matches)
		    )
		    ;; If everything checked
		    (or ind-matches (return))
		)
		;; Overlap found
		(progn
		    (if (consp (cdr ind-matches))
			;; There are yet items to be checked
			(progn
			    (rplaca ind-matches (cadr ind-matches))
			    (rplacd ind-matches (cddr ind-matches))
			)
			;; Last item
			(progn
			    (rplacd (last ind-cache 2) nil)
			    (return)
			)
		    )
		)
	    )

	    ;; Prepare for next check
	    (setq
		ind-tleft   (cadar ind-matches)
		ind-tright  (cddar ind-matches)
	    )
	)

;------------------------------------------------------------------------
; Process the matched tokens
:ind-parse
	(setq ind-cache (nreverse ind-cache))

:ind-parse-loop
	(or (setq ind-match (car ind-cache)) (go :ind-process))

	(setq
	    ind-cache (cdr ind-cache)
	    ind-token (car ind-match)
	)

	(or (member ind-token ind-tokens :test #'eq)
	    (go :ind-parse-loop)
	)

	;; If a state should be added
	(when (setq ind-change (indtoken-token ind-token))
	    (setq
		ind-left    (cadr ind-match)
		ind-right   (cddr ind-match)

		*ind-offset*
		(+ ind-from ind-left)

		*ind-length*
		(- ind-right ind-left)

		ind-state
		(cons
		    (cons ind-change (cons *ind-offset* *ind-length*))
		    ind-state
		)

		*ind-label*
		(indtoken-label ind-token)
	    )

	    ;; Expand token code
	    ,(indent-token-code (eval ind-definition))
	)

	;; Check if needs to switch to another table
	(when (setq ind-change (indtoken-switch ind-token))
	    ;; Need to switch to a previous table
	    (if (integerp ind-change)
		;; Relative switch
		(while (and ind-stack (minusp ind-change))
		    (setq
			ind-table	(pop ind-stack)
			ind-change	(1+ ind-change)
		    )
		)
		;; Search table in the stack
		(until
		    (or
			(null ind-stack)
			(eq
			    (setq ind-table (pop ind-stack))
			    ind-change
			)
		    )
		)
	    )

	    ;; If no match or stack became empty
	    (and (null ind-table)
		(setq
		    ind-table
		    (car (indent-tables ,ind-definition))
		)
	    )
	)

	;; Check if needs to start a new table
	;; XXX use ind-tleft to reduce number of local variables
	(when (setq ind-tleft (indtoken-begin ind-token))
	    (setq
		ind-change  ind-tleft
		ind-stack   (cons ind-table ind-stack)
		ind-table   ind-change
	    )
	)

	;; If current "indent pattern table" changed
	(when ind-change
	    (setq
		ind-tokens  (indtable-tokens ind-table)
		ind-cache   (nreverse ind-cache)
		ind-end     (cadr ind-match)
		ind-noteol  (> ind-length ind-end)
	    )
	    (go :ind-loop)
	)

	(and ind-cache (go :ind-parse-loop))

;------------------------------------------------------------------------
; Everything checked, process result
:ind-process

	;; If stack is not empty, don't apply rules
	(and ind-stack (not ind-startp) (go :ind-read))

	(block ind-terminate-block
	    (setq ind-cache nil ind-tleft 0 ind-change (mapcar #'car ind-state))
	    (dolist (entry ind-reduces)
		(setq
		    *ind-token* (indreduce-token entry)
		    *ind-label* (indreduce-label entry)
		)
		(dolist (rule (indreduce-rules entry))
		    (loop
			;; Check if reduction can be applied
			(or
			    (multiple-value-setq
				(ind-match ind-length)
				(indent-search-rule rule ind-change)
			    )
			    (return)
			)

			(setq
			    ;; First element matched
			    ind-matches		(nthcdr ind-match ind-state)

			    ;; Offset of match
			    *ind-offset*	(cadar ind-matches)

			    *ind-token-list*	(nthcdr ind-match ind-change)

			    ;; Length of match, note that *ind-length*
			    ;; Will be transformed to zero bellow if
			    ;; the rule is deleting entries.
			    *ind-length*
			    (if (> ind-length 1)
				(progn
				    (setq
					;; XXX using ind-tright, to reduce
					;; number of local variables...
					ind-tright
					(nth (1- ind-length) ind-matches)

					ind-right
					(+  (cadr ind-tright)
					    (cddr ind-tright)
					)
				    )
				    (- ind-right *ind-offset*)
				)
				(cddar ind-matches)
			    )
			)

			;; XXX using ind-tleft as a counter, to reduce
			;; number of used variables...
			(and (>= (incf ind-tleft) 1000)
			    ;; Should never apply so many reduce rules on
			    ;; every iteration, if needs to, something is
			    ;; wrong in the indentation definition...
			    (error "~D INDREDUCE iterations, ~
				   now checking (~A ~A)"
				ind-tleft *ind-token* rule
			    )
			)

			;; Check if should apply the reduction
			(or
			    ;; Expand check code
			    ,(indent-check-code (eval ind-definition))
			    (return)
			)

			(if (null *ind-token*)
			    ;; Remove match
			    (progn
				(setq *ind-length* 0)
				(if (= ind-match 0)
				    ;; Matched the first entry
				    (setq
					ind-state
					(nthcdr ind-length ind-matches)
				    )
				    (progn
					(setq
					    ind-matches
					    (nthcdr (1- ind-match) ind-state)
					)
					(rplacd
					    ind-matches
					    (nthcdr (1+ ind-length) ind-matches)
					)
				    )
				)
			    )

			    ;; Substitute/simplify
			    (progn
				(rplaca (car ind-matches) *ind-token*)
				(when (> ind-length 1)
				    (rplacd (cdar ind-matches) *ind-length*)
				    (rplacd
					ind-matches
					(nthcdr ind-length ind-matches)
				    )
				)
			    )
			)
			(setq
			    ind-cache	    t
			    ind-change	    (mapcar #'car ind-state)
			)

			;; Expand reduce code
			,(indent-reduce-code (eval ind-definition))
		    )
		)
	    )

	    ;; ind-cache will be T if at least one change was done
	    (and ind-cache (go :ind-process))

	    ;; Start of file reached
	    (or ind-startp (go :ind-read))

	)    ;; end of ind-terminate-block


	(block ind-terminate-block
	    (setq *ind-token-list* (mapcar #'car ind-state))
	    (dolist (item ind-state)
		(setq
		    *ind-token*		(car item)
		    *ind-offset*	(cadr item)
		    *ind-length*	(cddr item)
		)
		;; Expand resolve code
		,(indent-resolve-code (eval ind-definition))
		(setq *ind-token-list* (cdr *ind-token-list*))
	    )
	)

	(and (integerp *indent*)
	     (integerp *offset*)
	    (indent-text *indent* *offset* ,ind-no-tabs)
	)
    )
)
