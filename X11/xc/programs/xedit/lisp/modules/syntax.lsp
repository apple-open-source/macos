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
;; $XFree86: xc/programs/xedit/lisp/modules/syntax.lsp,v 1.11 2003/01/16 03:50:46 paulo Exp $
;;

(provide "syntax")
(require "xedit")
(in-package "XEDIT")

(defvar *syntax-symbols* '(
    syntax-highlight defsyntax defsynprop synprop-p syntax-p
    syntable syntoken synaugment
    *prop-default* *prop-keyword* *prop-number* *prop-string*
    *prop-constant* *prop-comment* *prop-preprocessor*
    *prop-punctuation* *prop-error* *prop-annotation*
))
(export *syntax-symbols*)
(in-package "USER")
(dolist (symbol xedit::*syntax-symbols*)
    (import symbol)
)
(in-package "XEDIT")
(makunbound '*syntax-symbols*)

#|
TODO:
o Add a command to match without increment the offset in the input, this
  may be useful for example in a case like:
	some-table
	    match "<"
		switch -1
	match "<"	<- the table already eated this, so it won't be matched.
  This must be carefully checked at compile time, such instruction should
  be in a token that returns or starts a new one, and even then, may need
  runtime check to make sure it won't enter an infinite loop.
o Allow combining properties, this is supported in Xaw, and could allow some
  very interesting effects for complex documents.
o Maybe have an separated function/loop for tables that don't have tokens
  that start/switch to another table, and/or have the contained attribute set.
  This could allow running considerably faster.
o Do a better handling of interactive edition for tokens that start and end
  with the same pattern, as an example strings, if the user types '"', it
  will parse up to the end of the file, "inverting" all strings.
o Allow generic code to be run once a match is found, such code could handle
  some defined variables and take decisions based on the parser state. This
  should be detected at compile time, to maybe run a different parser for
  such syntax tables, due to the extra time building the environment to
  call the code. This would be useful to "really" parse documents with
  complex syntax, for example, a man page source file.
o Add command to change current default property without initializing a new
  state.
o Fix problems matching EOL. Since EOL is an empty string match, if there
  is a rule to match only EOL, but some other rule matches up to the end
  of the input, the match to EOL will not be recognized. Currently the only
  way to handle this is to have a nested table that always returns once a
  match is found, so that it will restart the match loop code even if the
  input is at EOL.
  One possible solution would be to add the ending newline to the input,
  and then instead of matching "$", should match "\\n".
o XXX Usage of the variable newline-property must be reviewed in function
  syntax-highlight, if the text property has a background attribute,
  visual effect will look "strange", will paint a square with the
  background attribute at the end of every line in the matched text.
|#

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Some annotations to later write documentation for the module...
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
#|
    The current interface logic should be easy to understand for people
that have written lex scanners before. It has some extended semantics,
that could be translated to stacked BEGIN() statements in lex, but
currently does not have rules for matches in the format RE/TRAILING, as
well as code attached to rules (the biggest difference) and/or things
like REJECT and unput(). Also, at least currently, it is *really* quite
slower than lex.

	MATCHING RULES
	--------------
    When two tokens are matched at the same input offset, the longest
token is used, if the length is the same, the first definition is
used. For example:
	token1	=>	int
	token2	=>	[A-Za-z]+
	input	=>	integer
    Token1 matches "int" and token2 matches "integer", but since token2 is
longer, it is used. But in the case:
	token1	=>	int
	token2	=>	[A-Za-z]+
	input	=>	int
    Both, token1 and token2 match "int", since token1 is defined first, it
is used.
|#


;;  Initialize some default properties that may be shared in syntax
;; highlight definitions. Use of these default properties is encouraged,
;; so that "tokens" will be shown identically when editing program
;; sources in different programming languages.
(defsynprop *prop-default*
    "default"
    :font	"*courier-medium-r*12*"
    :foreground	"black")

(defsynprop *prop-keyword*
    "keyword"
    :font	"*courier-bold-r*12*"
    :foreground	"gray12")

(defsynprop *prop-number*
    "number"
    :font	"*courier-bold-r*12*"
    :foreground	"OrangeRed3")

(defsynprop *prop-string*
    "string"
    :font	"*lucidatypewriter-medium-r*12*"
    :foreground	"RoyalBlue2")

(defsynprop *prop-constant*
    "constant"
    :font	"*lucidatypewriter-medium-r*12*"
    :foreground	"VioletRed3")

(defsynprop *prop-comment*
    "comment"
    :font	"*courier-medium-o*12*"
    :foreground	"SlateBlue3")

(defsynprop *prop-preprocessor*
    "preprocessor"
    :font	"*courier-medium-r*12*"
    :foreground	"green4")

(defsynprop *prop-punctuation*
    "punctuation"
    :font	"*courier-bold-r*12*"
    :foreground	"gray12")

;; Control characters, not always errors...
(defsynprop *prop-control*
    "control"
    :font	"*courier-bold-r*12*"
    :foreground	"yellow2"
    :background	"red3")

(defsynprop *prop-error*
    "error"
    :font	"*new century schoolbook-bold*24*"
    :foreground	"yellow"
    :background	"red")

(defsynprop *prop-annotation*
    "annotation"
    :font	"*courier-medium-r*12*"
    :foreground	"black"
    :background	"PaleGreen")


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  The "main" definition of the syntax highlight coding interface.
;;  Creates a "special" variable with the given name, associating to
;; it an already compiled syntax table.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro defsyntax (variable label property indent options &rest lists)
    `(if (boundp ',variable)
	,variable
	(progn
	    (proclaim '(special ,variable))
	    (setq ,variable
		(compile-syntax-table
		    (string ',variable) ,options
		    (syntable ,label ,property ,indent ,@lists)
		)
	    )
	)
    )
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Just a wrapper to create a hash-table and bound it to a symbol.
;;  Example of call:
;;	(defsynoptions *my-syntax-options*
;;	    (:indent		.	8)
;;	    (:indent-option-1	.	1)
;;	    (:indent-option-2	.	2)
;;	)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defmacro defsynoptions (variable &rest options)
    `(if (boundp ',variable)
	,variable
	(progn
	    (proclaim '(special ,variable))
	    (setq ,variable (make-hash-table :initial-contents ',options))
	)
    )
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; These definitions should be "private".
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defstruct syntoken
    regex		;; A compiled regexp.
    property		;; NIL for default, or a synprop structure.
    contained		;; Only used when switch/begin is not NIL. Values:
			;;	NIL	  -> just switch to or begin new
			;;		     syntax table.
			;;	(not NIL) -> apply syntoken property
			;;		     (or default one) to matched
			;;		     text *after* switching to or
			;;		     beginning a new syntax table.
    switch		;; Values for switch are:
			;;	NIL	  -> do nothing
			;;	A keyword -> switch to the syntax table
			;;		     identified by the keyword.
			;;	A negative integer -> Pop the stack
			;;			      -<swich-value> times.
			;;			      A common value is -1,
			;;			     to switch to the previous
			;;			     state, but some times
			;;			     it is desired to return
			;;			     two or more times in
			;;			     in the stack.
			;;  NOTE: This is actually a jump, the stack is
			;; popped until the named syntax table is found,
			;; if the stack becomes empty, a new state is
			;; implicitly created.
    begin		;;  NIL or a keyword (like switch), but instead of
			;; popping the stack, it pushes the current syntax
			;; table to the stack and sets a new current one.
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Just a wrapper to make-syntoken.
;;	TODO: Add support for structure constructors.
;;	XXX: Note that the NOSUB only works with the xedit regex, it
;; will still return the match offsets, but will ignore subexpressions,
;; that is, parenthesis are used only for grouping.
;;	TODO: Create a new version of the re-exec call that returns
;; offsets in the format (<from> . <to>) and not
;; ((<from0> . <to0>) ... (<fromN> . <toN>)). Only the global result
;; is expected/used, so there is no reason to allocate more than one
;; cons cell per call.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun syntoken (pattern
		 &key icase nospec property contained switch begin (nosub t)
		 &aux
		 (regex
		    (re-comp pattern :icase icase :nospec nospec :nosub nosub)
		 )
		 check)

    ;;  Don't allow a regex that matches the null string enter the
    ;; syntax table list.
    (if (consp (setq check (re-exec regex "" :noteol t :notbol t)))
#+xedit	(error "SYNTOKEN: regex matches empty string ~S" regex)
#-xedit	()
    )

    (make-syntoken
	:regex		regex
	:property	property
	:contained	contained
	:switch		switch
	:begin		begin
    )
)


;;  This structure is defined only to do some type checking, it just
;; holds a list of keywords.
(defstruct synaugment
    labels		;; List of keywords labeling syntax tables.
)

(defstruct syntable
    label		;; A keyword naming this syntax table.
    property		;; NIL or a default synprop structure.
    indent		;; Indentation function for the syntax table.
    tokens		;; A list of syntoken structures.
    tables		;; A list of syntable structures.
    augments		;;  A list of synaugment structures, used only
			;; at "compile time", so that a table can be
			;; used before it's definition.
    bol			;;  One of the tokens match the empty string at
			;; the start of a line (loop optimization hint).
			;; Field filled at "link" time.
    eol			;;  Same comments as bol, but in this case, for
			;; the empty string at the end of a line.
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Just call make-syntable, but sorts the elements by type, allowing
;; a cleaner code when defining the syntax highlight rules.
;; XXX Same comments as for syntoken about the use of a constructor for
;; structures. TODO: when/if clos is implemented in the interpreter.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun syntable (label default-property indent &rest definitions)

    ;; Check for possible errors in the arguments.
    (unless (keywordp label)
	(error "SYNTABLE: ~A is not a keyword" label)
    )
    (unless
	(or
	    (null default-property)
	    (synprop-p default-property)
	)
	(error "SYNTABLE: ~A is an invalid text property"
	    default-property
	)
    )

    ;; Don't allow unknown data in the definition list.
    ;; XXX typecase should be added to the interpreter, and since
    ;;     the code is traversing the entire list, it could build
    ;;     now the arguments to make-syntable.
    (dolist (item definitions)
	(unless
	    (or

		;;  Allow NIL in the definition list, so that one
		;; can put conditionals in the syntax definition,
		;; and if the conditional is false, fill the slot
		;; with a NIL value.
		(atom item)
		(syntoken-p item)
		(syntable-p item)
		(synaugment-p item)
	    )
	    (error "SYNTABLE: invalid syntax table argument ~A" item)
	)
    )

    ;; Build the syntax table.
    (make-syntable
	:label		label
	:property	default-property
	:indent		indent
	:tokens		(remove-if-not #'syntoken-p definitions)
	:tables		(remove-if-not #'syntable-p definitions)
	:augments	(remove-if-not #'synaugment-p definitions)
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Just to do a "preliminary" error checking, every element must be a
;; a keyword, and also check for reserved names.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun synaugment (&rest keywords)
    (dolist (keyword keywords)
	(unless (keywordp keyword)
	    (error "SYNAUGMENT: bad syntax table label ~A" keyword)
	)
    )
    (make-synaugment :labels keywords)
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Recursive compile utility function.
;; Returns a cons in the format:
;;	car	=>	List of all syntoken structures
;;			(including child tables).
;;	cdr	=>	List of all child syntable structures.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun list-syntable-elements (table &aux result sub-result)
    (setq
	result
	(cons
	    (syntable-tokens table)
	    (syntable-tables table))
    )

    ;; For every child syntax table.
    (dolist (child (syntable-tables table))

	;; Recursively call list-syntable-elements.
	(setq sub-result (list-syntable-elements child))

	(rplaca result (append (car result) (car sub-result)))
	(rplacd result (append (cdr result) (cdr sub-result)))
    )

    ;; Return the pair of nested tokens and tables.
    result
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Append tokens of the augment list to the tokens of the specified
;; syntax table.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun compile-syntax-augment-list (table table-list
				    &aux labels augment tokens)

    ;; Create a list of all augment tables.
    (dolist (augment (syntable-augments table))
	(setq labels (append labels (synaugment-labels augment)))
    )

    ;;  Remove duplicates and references to "itself",
    ;; without warnings?
    (setq
	labels
	(remove
	    (syntable-label table)
	    (remove-duplicates labels :from-end t)
	)
    )

    ;; Check if the specified syntax tables exists!
    (dolist (label labels)
	(unless
	    (setq
		augment
		(car (member label table-list :key #'syntable-label))
	    )
	    (error "COMPILE-SYNTAX-AUGMENT-LIST: Cannot augment ~A in ~A"
		label
		(syntable-label table)
	    )
	)

	;; Increase list of tokens.
	(setq tokens (append tokens (syntable-tokens augment)))
    )

    ;;  Store the tokens in the augment list. They will be added
    ;; to the syntax table in the second pass.
    (setf (syntable-augments table) tokens)

    ;;  Recurse on every child table.
    (dolist (child (syntable-tables table))
	(compile-syntax-augment-list child table-list)
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Just add the augmented tokens to the token list, recursing on
;; every child syntax table.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun link-syntax-augment-table (table)
    (setf
	(syntable-tokens table)
	;;  When augmenting a table, duplicated tokens or different tokens
	;; that use the same regex pattern should be common.
	(remove-duplicates
	    (nconc (syntable-tokens table) (syntable-augments table))
	    :key	#'syntoken-regex
	    :test	#'equal
	    :from-end	t
	)

	;;  Don't need to keep this list anymore.
	(syntable-augments table)
	()
    )

    ;;  Check if one of the tokens match the empty string at the
    ;; start or end of a text line. XXX The fields bol and eol
    ;; are expected to be initialized to NIL.
    (dolist (token (syntable-tokens table))
	(when (consp (re-exec (syntoken-regex token) "" :noteol t))
	    (setf (syntable-bol table) t)
	    (return)
	)
    )
    (dolist (token (syntable-tokens table))
	(when (consp (re-exec (syntoken-regex token) "" :notbol t))
	    (setf (syntable-eol table) t)
	    (return)
	)
    )

    (dolist (child (syntable-tables table))
	(link-syntax-augment-table child)
    )
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; "Compile" the main structure of the syntax highlight code.
;; Variables "switches" and "begins" are used only for error checking.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun compile-syntax-table (name options main-table &aux syntax elements
			     switches begins tables properties)
    (unless (stringp name)
	(error "COMPILE-SYNTAX-TABLE: ~A is not a string" name)
    )

    (setq
	elements
	(list-syntable-elements main-table)

	switches
	(remove-if
	    #'null
	    (car elements)
	    :key #'syntoken-switch
	)

	begins
	(remove-if-not
	    #'keywordp
	    (car elements)
	    :key #'syntoken-begin
	)

	;;  The "main-table" isn't in the list, because
	;; list-syntable-elements includes only the child tables;
	;; this is done to avoid the need of removing duplicates here.
	tables
	(cons main-table (cdr elements))
    )

    ;; Check for typos in the keywords, or for not defined syntax tables.
    (dolist (item (mapcar #'syntoken-switch switches))
	(unless
	    (or
		(and
		    (integerp item)
		    (minusp item)
		)
		(member item tables :key #'syntable-label)
	    )
	    (error "COMPILE-SYNTAX-TABLE: SWITCH ~A cannot be matched"
		item
	    )
	)
    )
    (dolist (item (mapcar #'syntoken-begin begins))
	(unless (member item tables :key #'syntable-label)
	    (error "COMPILE-SYNTAX-TABLE: BEGIN ~A cannot be matched"
		item
	    )
	)
    )

    ;; Create a list of all properties used by the syntax.
    (setq
	properties
	(delete-duplicates

	    ;; Remove explicitly set to "default" properties.
	    (remove nil

		(append

		    ;; List all properties in the syntoken list.
		    (mapcar
			#'syntoken-property
			(car elements)
		    )

		    ;; List all properties in the syntable list.
		    (mapcar
			#'syntable-property
			tables
		    )
		)
	    )
	    :test #'string=
	    :key  #'synprop-name
	)
    )

    ;;  Provide a default property if none specified.
    (unless
	(member
	    "default"
	    properties
	    :test #'string=
	    :key #'synprop-name
	)
	(setq properties (append (list *prop-default*) properties))
    )


    ;;  Now that a list of all nested syntax tables is known, compile the
    ;; augment list. Note that even the main-table can be augmented to
    ;; include tokens of one of it's children.

    ;;  Adding the tokens of the augment tables must be done in
    ;; two passes, or it may cause surprises due to "inherited"
    ;; tokens, as the augment table was processed first, and
    ;; increased it's token list.
    (compile-syntax-augment-list main-table tables)

    ;;  Now just append the augmented tokens to the table's token list.
    (link-syntax-augment-table main-table)

    ;;  Change all syntoken switch and begin fields to point to the
    ;; syntable.
    (dolist (item switches)
	(if (keywordp (syntoken-switch item))
	    ;;  A switch may be relative, check if a keyword
	    ;; was specified.
	    (setf
		(syntoken-switch item)
		(car
		    (member
			(syntoken-switch item)
			tables
			:key #'syntable-label
		    )
		)
	    )
	)
    )
    (dolist (item begins)
	(setf
	    (syntoken-begin item)
	    (car
		(member
		    (syntoken-begin item)
		    tables
		    :key #'syntable-label
		)
	    )
	)
    )

    ;;  Don't need to add a entity for default properties
    (dolist (item (car elements))
	(and
	    (syntoken-property item)
	    (string= (synprop-name (syntoken-property item)) "default")
	    (setf (syntoken-property item) ())
	)
    )
    (dolist (item tables)
	(and
	    (syntable-property item)
	    (string= (synprop-name (syntable-property item)) "default")
	    (setf (syntable-property item) ())
	)
    )

    (setq syntax
	(make-syntax
	    :name	name
	    :options	options
	    :labels	tables
	    :quark
		(compile-syntax-property-list
		    name
		    properties
		)
	    :token-count
		(length (car elements))
	)
    )

    ;; Ready to run!
)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Loop applying the specifed syntax table to the text.
;;  XXX This function needs a review. Should compile the regex patterns
;; with newline sensitive match (and scan the entire file), and keep a
;; cache of matched tokens (that may be at a very longer offset), and,
;; when the match is removed from the cache, readd the token to the
;; token-list; if the token does not match, it will not be in the cache,
;; but should be removed from the token-list. If properly implemented, it
;; should be somewhat like 4 times faster, but I would not be surprised
;; if it becames even faster.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun syntax-highlight (*syntax*
			 &optional
			 (*from* (point-min))
			 (*to* (point-max))
			 interactive
			 &aux
#+debug			 (*line-number* 0)
			 stream
			 indent-table
			)

    ;;  Make sure the property list is in use.
    ;;  The interactive flag is only set after loading the file.
    (or interactive
	(property-list (syntax-quark *syntax*))
    )

#+debug
    (setq *from* 0 *to* 0)

#-debug
    (and (>= *from* *to*) (return-from syntax-highlight (values *from* nil)))

    ;;  Remove any existing properties from the text.
    (clear-entities *from* (1+ *to*))

    (setq stream
#-debug	(make-string-input-stream (read-text *from* (- *to* *from*)))
#+debug	*standard-input*
    )

    (prog*
	(
	;;  Used to check if end of file found but syntax stack did
	;; not finish.
	(point-max (point-max))

	;;  Used in interactive mode, to return the syntax table
	;; where the cursor is located.
	(point (point))

	;;  The current stack of states.
	stack

	;;  The current syntable.
	(syntax-table (car (syntax-labels *syntax*)))

	;;  The current syntable's default property.
	(default-property (syntable-property syntax-table))

	;;  Add this property to newlines as a hint to the interactive
	;; callback, so that it knows from where to restart parsing.
	newline-property

	;;  The tokens in the current syntax table that may match,
	;; i.e. the items in this list are not in nomatch.
	token-list

	;;  A pointer to the syntable token list, if token-list is
	;; eq to this value, cannot change it inplace.
	current-token-list

	;;  Help to avoid allocating too many new object cells, and
	;; optmizes a bit time in [n]?set-difference.
	;;  This optimizes only the processing of one line of text
	;; as nomatch must be rebuilt when reading a new line of text.
	token-list-stack

	;;  Matches for the current list of tokens.
	matches

	;;  Line of text.
	line

	;;  Length of the text line.
	length

	;;  A inverse cache, don't call re-exec when the regex is
	;; already known to not match.
	nomatch

	;;  Use cache as a list of matches to avoid repetitive
	;; unnecessary calls to re-exec.
	;;  cache is a list in which every element has the format:
	;;	(token . (start . end))
	;;  Line of text.
	cache

	;;  Used just to avoid a function call at every re-exec call.
	notbol

	match

	start
	left
	right
	result
	property

	;;  Beginig a new syntax table?
	begin

	;;  Switching to another syntax table?
	switch

	;;  Property flag when changing the current syntax table.
	contained

	;;  Flag to know if syntax table has changed.
	change

	;;  Variables used when removing invalid elements from the
	;; the cache.
	item
	from
	to
	)

;-----------------------------------------------------------------------
:read
#+debug-verbose
	(format t "** Entering :READ stack length is ~D~%" (length stack))
#+debug	(format t "~%[~D]> " (incf *line-number*))

	;;  If input has finished, return.
	(unless (setq line (read-line stream nil nil))
	    (when
		(and
		    ;; If a nested syntax table wasn't finished
		    (consp stack)
		    (<
			(setq *to* (scan *from* :eol :right))
			point-max
		    )
		)
		(setq line (read-text *from* (- *to* *from*)))
		(clear-entities *from* (1+ *to*))
		(go :again)
	    )
#-debug	    (close stream)
	    (return)
	)

;------------------------------------------------------------------------
:again
	(setq
	    start		0
	    length		(length line)
	    token-list		(syntable-tokens syntax-table)
	    current-token-list	token-list
	    token-list-stack	()
	    nomatch		()
	    cache		()
	)


	;;  If empty line, and current table does not have matches for
	;; the empty string at start or end of a text line.
	(when
	    (and
		(= length 0)
		(not (syntable-eol syntax-table))
		(not (syntable-bol syntax-table)))
#+debug-verbose
	    (format t "Empty line and table has no match to bol or eol~%")

	    (and newline-property
		(add-entity *from* 1 (synprop-quark newline-property)))
	    (go :update)
	)

;------------------------------------------------------------------------
:loop
#+debug-verbose
	(format t "** Entering :LOOP at offset ~D in table ~A, cache has ~D items~%"
	    start
	    (syntable-label syntax-table)
	    (length cache))

	(setq notbol (> start 0))

	;;  For every token that may match.
	(dolist
	    (token
		(setq
		    token-list
		    (if (eq token-list current-token-list)
			(set-difference token-list nomatch :test #'eq)
			(nset-difference token-list nomatch :test #'eq)
		    )
		)
	    )

	    ;;	Try to fetch match from cache.
	    (if (setq match (member token cache :test #'eq :key #'car))
		;;  Match is in the cache.

		(progn
		    ;;	Match must be moved to the beginning of the
		    ;; matches list, as a match from another syntax
		    ;; table may be also in the cache, but before
		    ;; the match for the current token.
#+debug-verbose	    (format t "cached: {~A:~S} ~A~%"
			(cdar match)
			(subseq line (cadar match) (cddar match))
			(syntoken-regex token))

		    ;;	Remove the match from the cache.
		    (if (eq match cache)

			;;  This could be changed to only set "matches"
			;; if it is not the first element of cache,
			;; but is unsafe, because other tokens may
			;; be added to "matches", and will end up
			;; before when joining "matches" and "cache".
			(progn
			    (setq cache (cdr cache))
			    (rplacd match matches)
			    (setq matches match))

			(progn
			    (if (= (length match) 1)
				(progn
				    (rplacd (last cache 2) nil)
				    (rplacd match matches)
				    (setq matches match))
				(progn
				    (setq matches (cons (car match) matches))
				    (rplaca match (cadr match))
				    (rplacd match (cddr match)))
			    )
			)
		    )

		    ;;	Exit loop if the all the remaining
		    ;; input was matched.
		    (when
			(and
			    (= start (cadar match))
			    (= length (cddar match))
			)
#+debug-verbose 	(format t "Rest of line match~%")
			(return)
		    )
		)

		;;  Not in the cache, call re-exec.
		(if
		    (consp
			(setq
			    match
			    (re-exec
				(syntoken-regex token)
				line
				:start	start
				:notbol	notbol)))

		    ;;	Match found.
		    (progn
#+debug-verbose		(format t "Adding to cache: {~A:~S} ~A~%"
			    (car match)
			    (subseq line (caar match) (cdar match))
			    (syntoken-regex token))

			;; Only the first pair is used.
			(setq match (car match))

			(cond
			    (
				(or
				    (null matches)
				    ;;	No overlap and after most
				    ;; recent match.
				    (>= (car match) (cddar matches))
				    ;; No overlap and before most
				    ;; recent match.
				    (<= (cdr match) (cadar matches))
				)
				(setq
				    matches
				    (cons (cons token match) matches)
				)
			    )
			    (
				(or
				    ;;	Overlap, but start before most
				    ;; recent match.
				    (< (car match) (cadar matches))
				    (and
					;;  Same offset as most recent
					;; match, but is longer.
					(= (car match) (cadar matches))
					(> (cdr match) (cddar matches))
				    )
				)
				(rplaca (car matches) token)
				(rplacd (car matches) match)
#+debug-verbose 		(format t "Replaced most recent match~%")
			    )
			    (t
#+debug-verbose 		(format t "Ignored~%")
				;; XXX The interpreter does not yet implement
				;; implicit tagbody in dolist, just comment
				;; the go call in that case. (Will just do
				;; an unecessary test...)
				(go :ignored)
			    )
			)

			;;  Exit loop if the all the remaining
			;; input was matched.
			(when
			    (and
				(= start (car match))
				(= length (cdr match)))
#+debug-verbose 	    (format t "Rest of line match~%")
			    (return))
		    )

		    ;;	Match not found.
		    (progn
#+debug-verbose 	(format t "Adding to nomatch: ~A~%"
			    (syntoken-regex token))
			(setq nomatch (cons token nomatch)))
		)
	    )
:ignored
	)

	;;  Add matches to the beginning of the cache list.
	(setq
	    ;;	Put matches with smaller offset first.
	    cache
	    (stable-sort (nconc (nreverse matches) cache) #'< :key #'cadr)

	    ;;	Make sure that when the match loop is reentered, this
	    ;; variable is NIL.
	    matches
	    ()
	)

	;;  While the first entry in the cache is not from the current table.
	(until (or (null cache) (member (caar cache) token-list :test #'eq))

#+debug-verbose
	    (format t "Not in the current table, removing {~A:~S} ~A~%"
		(cdar cache)
		(subseq line (cadar cache) (cddar cache))
		(syntoken-regex (caar cache)))

	    (setq cache (cdr cache))
	)


	;;  If nothing was matched in the entire/remaining line.
	(unless cache
	    (when default-property
		(if
		    (or
			(null result)
			(> start (cadar result))
			(not (eq (cddar result) default-property)))
		    (setq
			result
			(cons
			    (cons start (cons length default-property))
			    result
			)
		    )
		    (rplaca (cdar result) length)
		)
	    )

#+debug-verbose
	    (format t "No match until end of line~%")

	    ;;  Result already known, and there is no syntax table
	    ;; change, bypass :PARSE.
	    (and interactive
		(null indent-table)
		(<= 0 (- point *from*) length)
		(setq indent-table syntax-table))
	    (go :process)
	)

#+debug-verbose
	(format t "Removing first candidate from cache {~A:~S} ~A~%"
	    (cdar cache)
	    (subseq line (cadar cache) (cddar cache))
	    (syntoken-regex (caar cache))
	)

	;;  Prepare to choose best match.
	(setq
	    match	(car cache)
	    left	(cadr match)
	    right	(cddr match)
	    cache	(cdr cache)
	)

	;;  First element can be safely removed now.
	;;  If there is only one, skip loop below.
	(or cache (go :parse))

	;;  Remove elements of cache that must be discarded.
	(setq
	    item   (car cache)
	    from   (cadr item)
	    to     (cddr item)
	)

	(loop
	    (if
		(or

		    ;;	If everything removed from the cache.
		    (null item)

		    ;;	Or next item is at a longer offset than the
		    ;; end of current match.
		    (>= from right)
		)
		(return)
	    )

	    (and
		;;  If another match at the same offset.
		(= left from)

		;;  And if this match is longer than the current one.
		(> to right)

		(member (car item) token-list :test #'eq)

		(setq
		    match   item
		    right   to
		)
	    )

#+debug-verbose
	    (format t "Removing from cache {~A:~S} ~A~%"
		(cdar cache)
 		(subseq line from to)
		(syntoken-regex (caar cache)))

	    (setq
		cache	    (cdr cache)
		item	    (car cache)
		from	    (cadr item)
		to	    (cddr item)
	    )
	)


;-----------------------------------------------------------------------
:parse
#+debug-verbose
	(format t "** Entering :PARSE~%")

	(setq

	    ;;  Change match value to the syntoken.
	    match	(car match)

	    begin	(syntoken-begin match)
	    switch	(syntoken-switch match)
	    contained	(syntoken-contained match)
	    change	(or begin switch)
	)

	;;  Check for unmatched leading text.
	(when (and default-property (> left start))
#+debug-verbose (format t "No match in {(~D . ~D):~S}~%"
		start
		left
		(subseq line start left)
	    )
	    (if
		(or
		    (null result)
		    (> start (cadar result))
		    (not (eq (cddar result) default-property)))
		(setq
		    result
		    (cons
			(cons start (cons left default-property))
			result
		    )
		)
		(rplaca (cdar result) left)
	    )
	)

	;;  If the syntax table is not changed,
	;; or if the new table requires that the
	;; current default property be used.
	(unless (and change contained)

	    (and
		(> right left)
		(setq
		    property
		    (or
			;;  If token specifies the property.
			(syntoken-property match)
			default-property
		    )
		)

		;;  Add matched text.
		(if
		    (or
			(null result)
			(> left (cadar result))
			(not (eq (cddar result) property))
		    )
		    (setq
			result
			(cons
			    (cons left (cons right property))
			    result
			)
		    )
		    (rplaca (cdar result) right)
		)
	    )

#+debug-verbose
	    (format t "(0)Match found for {(~D . ~D):~S}~%"
		left
		right
		(subseq line left right)
	    )
	)


	;;  Update start offset in the input now!
	(and interactive
	    (null indent-table)
	    (<= start (- point *from*) right)
	    (setq indent-table syntax-table))
	(setq start right)


	;;  When changing the current syntax table.
	(when change
	    (when switch
		(if (numberp switch)

		    ;;	If returning to a previous state.
		    ;;	Don't generate an error if the stack
		    ;; becomes empty?
		    (while
			(< switch 0)

			(setq
			    syntax-table	(pop stack)
			    token-list		(pop token-list-stack)
			    switch		(1+ switch)
			)
		    )

		    ;;	Else, not to a previous state, but
		    ;; returning to a named syntax table,
		    ;; search for it in the stack.
		    (while
			(and

			    (setq
				token-list	(pop token-list-stack)
				syntax-table	(pop stack)
			    )

			    (not (eq switch syntax-table))
			)
			;;  Empty loop.
		    )
		)

		;;  If no match found while popping
		;; the stack.
		(unless syntax-table

		    ;;	Return to the topmost syntax table.
		    (setq
			syntax-table
			(car (syntax-labels *syntax*))
		    )
		)

#+debug-verbose	(format t "switching to ~A offset: ~D~%"
		    (syntable-label syntax-table)
		    start
		)

		(if (null token-list)
		    (setq token-list (syntable-tokens syntax-table))
		)
	    )

	    (when begin
		;;  Save state for a possible
		;; :SWITCH later.
		(setq
		    stack	     (cons syntax-table stack)
		    token-list-stack (cons token-list token-list-stack)
		    token-list	     (syntable-tokens begin)
		    syntax-table     begin
		)
#+debug-verbose	(format t "begining ~A offset: ~D~%"
		    (syntable-label syntax-table)
		    start
		)
	    )

	    ;;	Change current syntax table.
	    (setq
		default-property    (syntable-property syntax-table)
		current-token-list  (syntable-tokens syntax-table)
	    )

	    ;;  Set newline property, to help interactive callback
	    ;;  Only need to have a defined value, for now don't care
	    ;; about wich value is being used, neither if there is
	    ;; a value to be set.
	    (if (null stack)
		(setq newline-property nil)
		(or newline-property
		    (setq newline-property default-property)
		    (setq newline-property (syntoken-property match))
		)
	    )

	    ;; If processing of text was deferred.
	    (when contained

		(and
		    (> right left)
		    (setq
			property
			(or
			    (syntoken-property match)
			    default-property
			)
		    )
		    ;;	Add matched text with the updated property.
		    (if
			(or
			    (null result)
			    (> left (cadar result))
			    (not (eq (cddar result) property))
			)
			(setq
			    result
			    (cons
				(cons left (cons right property))
				result
			    )
			)
			(rplaca (cdar result) right)
		    )
		)

#+debug-verbose (format t "(1)Match found for {(~D . ~D):~S}~%"
		    left
		    right
		    (subseq line left right)
		)
	    )

	    (go :loop)
	)


;-----------------------------------------------------------------------
	;;  Wait for the end of the line to process, so that
	;; it is possible to join sequential matches with the
	;; same text property.
	(and (or cache (< start length)) (go :loop))
:process

#+debug-verbose
	(format t "** Entering :PROCESS~%")

	(if result
	    (progn
		;;  If the last property was at the end of the line,
		;; there are nested syntax tables, and there is a
		;; default property, include the newline in the property,
		;; as a hint to the interactive callback.
		(and
		    newline-property
		    (if
			(and
			    (eq (cddar result) newline-property)
			    (= length (cadar result))
			)
			(rplaca (cdar result) (1+ length))
			(setq
			    result
			    (cons
				(cons length (cons (1+ length) newline-property))
				result
			    )
			)
		    )
		)

		;;  Result was created in reversed order.
		(nreverse result)
		(dolist (item result)
		    (setq
			left		(car item)
			right		(cadr item)
			property	(cddr item))

		    ;; Use the information.
		    (add-entity
			(+ *from* left)
			(- right left)
			(synprop-quark property))
		)
	    )

	    (and newline-property
		(add-entity
		    (+ *from* length)
		    1
		    (synprop-quark newline-property))
	    )
	)

;------------------------------------------------------------------------
:update
	;; Prepare for new matches.
	(setq
	    result	nil

	    ;;	Update offset to read text.
	    ;;	Add 1 for the skipped newline.
	    *from*	(+ *from* length 1)
	)

	(go :read)
    )

#+debug (terpri)
    (values *to* indent-table)
)

(compile 'syntax-highlight)
