" Vim syntax file
"
" Language:	Logtalk
" Maintainer:	Paulo Moura <pmoura@noe.ubi.pt>
" Last Change:	2003 Apr 25


" Quit when a syntax file was already loaded:

if version < 600
	syntax clear
elseif exists("b:current_syntax")
	finish
endif


" Logtalk is case sensitive:

syn case match


" Logtalk clause functor

syn match	logtalkOperator		":-"


" Logtalk quoted atoms and strings

syn region	logtalkString		start=+"+	skip=+\\"+	end=+"+
syn region	logtalkAtom		start=+'+	skip=+\\'+	end=+'+


" Logtalk message sending operators

syn match	logtalkOperator		"::"
syn match	logtalkOperator		"\^\^"


" Logtalk external call

syn region	logtalkExtCall		matchgroup=logtalkExtCallTag		start="{"		matchgroup=logtalkExtCallTag		end="}"		contains=ALL


" Logtalk opening entity directives

syn region	logtalkOpenEntityDir	matchgroup=logtalkOpenEntityDirTag	start=":- object("	matchgroup=logtalkOpenEntityDirTag	end=")\."	contains=ALL
syn region	logtalkOpenEntityDir	matchgroup=logtalkOpenEntityDirTag	start=":- protocol("	matchgroup=logtalkOpenEntityDirTag	end=")\."	contains=ALL
syn region	logtalkOpenEntityDir	matchgroup=logtalkOpenEntityDirTag	start=":- category("	matchgroup=logtalkOpenEntityDirTag	end=")\."	contains=ALL


" Logtalk closing entity directives

syn match	logtalkCloseEntityDir	":- end_object\."
syn match	logtalkCloseEntityDir	":- end_protocol\."
syn match	logtalkCloseEntityDir	":- end_category\."


" Logtalk entity relations

syn region	logtalkEntityRel	matchgroup=logtalkEntityRelTag	start="instantiates("	matchgroup=logtalkEntityRelTag	end=")"		contains=logtalkEntity		contained
syn region	logtalkEntityRel	matchgroup=logtalkEntityRelTag	start="specializes("	matchgroup=logtalkEntityRelTag	end=")"		contains=logtalkEntity		contained
syn region	logtalkEntityRel	matchgroup=logtalkEntityRelTag	start="extends("	matchgroup=logtalkEntityRelTag	end=")"		contains=logtalkEntity		contained
syn region	logtalkEntityRel	matchgroup=logtalkEntityRelTag	start="imports("		matchgroup=logtalkEntityRelTag	end=")"		contains=logtalkEntity		contained
syn region	logtalkEntityRel	matchgroup=logtalkEntityRelTag	start="implements("		matchgroup=logtalkEntityRelTag	end=")"		contains=logtalkEntity		contained


" Logtalk directives

syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- initialization("	matchgroup=logtalkDirTag	end=")\."	contains=ALL
syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- info("		matchgroup=logtalkDirTag	end=")\."	contains=ALL
syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- mode("		matchgroup=logtalkDirTag	end=")\."	contains=logtalkOperator,logtalkAtom
syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- dynamic("		matchgroup=logtalkDirTag	end=")\."	contains=ALL
syn match	logtalkDirTag		":- dynamic\."
syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- discontiguous("	matchgroup=logtalkDirTag	end=")\."	contains=ALL
syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- public("		matchgroup=logtalkDirTag	end=")\."	contains=ALL
syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- protected("		matchgroup=logtalkDirTag	end=")\."	contains=ALL
syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- private("		matchgroup=logtalkDirTag	end=")\."	contains=ALL
syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- metapredicate("	matchgroup=logtalkDirTag	end=")\."	contains=ALL
syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- op("			matchgroup=logtalkDirTag	end=")\."	contains=ALL
syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- calls("		matchgroup=logtalkDirTag	end=")\."	contains=ALL
syn region	logtalkDir		matchgroup=logtalkDirTag	start=":- uses("		matchgroup=logtalkDirTag	end=")\."	contains=ALL


" Logtalk built-in predicates

syn keyword	logtalkBuiltIn		current_object
syn keyword	logtalkBuiltIn		current_protocol
syn keyword	logtalkBuiltIn		current_category

syn keyword	logtalkBuiltIn		create_object
syn keyword	logtalkBuiltIn		create_protocol
syn keyword	logtalkBuiltIn		create_category

syn keyword	logtalkBuiltIn		object_property
syn keyword	logtalkBuiltIn		protocol_property
syn keyword	logtalkBuiltIn		category_property

syn keyword	logtalkBuiltIn		abolish_object
syn keyword	logtalkBuiltIn		abolish_protocol
syn keyword	logtalkBuiltIn		abolish_category

syn keyword	logtalkBuiltIn		extends_object
syn keyword	logtalkBuiltIn		extends_protocol
syn keyword	logtalkBuiltIn		implements_protocol
syn keyword	logtalkBuiltIn		instantiates_object
syn keyword	logtalkBuiltIn		specializes_object
syn keyword	logtalkBuiltIn		imports_category

syn keyword	logtalkBuiltIn		abolish_events
syn keyword	logtalkBuiltIn		current_event
syn keyword	logtalkBuiltIn		define_events

syn keyword	logtalkBuiltIn		current_logtalk_flag
syn keyword	logtalkBuiltIn		set_logtalk_flag

syn keyword	logtalkBuiltIn		logtalk_compile
syn keyword	logtalkBuiltIn		logtalk_load

syn keyword	logtalkBuiltIn		forall
syn keyword	logtalkBuiltIn		logtalk_version
syn keyword	logtalkBuiltIn		retractall


" Logtalk built-in methods

syn keyword	logtalkBuiltInMethod	parameter
syn keyword	logtalkBuiltInMethod	self
syn keyword	logtalkBuiltInMethod	sender
syn keyword	logtalkBuiltInMethod	this

syn keyword	logtalkBuiltInMethod	current_predicate
syn keyword	logtalkBuiltInMethod	predicate_property

syn keyword	logtalkBuiltInMethod	abolish
syn keyword	logtalkBuiltInMethod	asserta
syn keyword	logtalkBuiltInMethod	assertz
syn keyword	logtalkBuiltInMethod	clause
syn keyword	logtalkBuiltInMethod	retract
syn keyword	logtalkBuiltInMethod	retractall

syn keyword	logtalkBuiltInMethod	bagof
syn keyword	logtalkBuiltInMethod	findall
syn keyword	logtalkBuiltInMethod	forall
syn keyword	logtalkBuiltInMethod	setof

syn keyword	logtalkBuiltInMethod	before
syn keyword	logtalkBuiltInMethod	after


" Mode operators

syn match	logtalkOperator		"?"
syn match	logtalkOperator		"@"


" Control constructs

syn keyword	logtalkKeyword		true
syn keyword	logtalkKeyword		fail
syn keyword	logtalkKeyword		call
syn match	logtalkOperator		"!"
syn match	logtalkOperator		","
syn match	logtalkOperator		";"
syn match	logtalkOperator		"->"
syn keyword	logtalkKeyword		catch
syn keyword	logtalkKeyword		throw


" Term unification

syn match	logtalkOperator		"="
syn keyword	logtalkKeyword		unify_with_occurs_check
syn match	logtalkOperator		"\\="


" Term testing

syn keyword	logtalkKeyword		var
syn keyword	logtalkKeyword		atom
syn keyword	logtalkKeyword		integer
syn keyword	logtalkKeyword		float
syn keyword	logtalkKeyword		atomic
syn keyword	logtalkKeyword		compound
syn keyword	logtalkKeyword		nonvar
syn keyword	logtalkKeyword		number


" Term comparison

syn match	logtalkOperator		"@=<"
syn match	logtalkOperator		"=="
syn match	logtalkOperator		"\\=="
syn match	logtalkOperator		"@<"
syn match	logtalkOperator		"@>"
syn match	logtalkOperator		"@>="


" Term creation and decomposition

syn keyword	logtalkKeyword		functor
syn keyword	logtalkKeywor		arg
syn match	logtalkOperator		"=\.\."
syn keyword	logtalkKeyword		copy_term


" Arithemtic evaluation

syn keyword	logtalkOperator		is


" Arithemtic comparison

syn match	logtalkOperator		"=:="
syn match	logtalkOperator		"=\\="
syn match	logtalkOperator		"<"
syn match	logtalkOperator		"=<"
syn match	logtalkOperator		">"
syn match	logtalkOperator		">="


" Stream selection and control

syn keyword	logtalkKeyword		current_input
syn keyword	logtalkKeyword		current_output
syn keyword	logtalkKeyword		set_input
syn keyword	logtalkKeyword		set_output
syn keyword	logtalkKeyword		open
syn keyword	logtalkKeyword		close
syn keyword	logtalkKeyword		flush_output
syn keyword	logtalkKeyword		stream_property
syn keyword	logtalkKeyword		at_end_of_stream
syn keyword	logtalkKeyword		set_stream_position


" Character input/output

syn keyword	logtalkKeyword		get_char
syn keyword	logtalkKeyword		get_code
syn keyword	logtalkKeyword		peek_char
syn keyword	logtalkKeyword		peek_code
syn keyword	logtalkKeyword		put_char
syn keyword	logtalkKeyword		put_code
syn keyword	logtalkKeyword		nl


" Byte input/output

syn keyword	logtalkKeyword		get_byte
syn keyword	logtalkKeyword		peek_byte
syn keyword	logtalkKeyword		put_byte


" Term input/output

syn keyword	logtalkKeyword		read_term
syn keyword	logtalkKeyword		read
syn keyword	logtalkKeyword		write_term
syn keyword	logtalkKeyword		write
syn keyword	logtalkKeyword		writeq
syn keyword	logtalkKeyword		write_canonical
syn keyword	logtalkKeyword		op
syn keyword	logtalkKeyword		current_op
syn keyword	logtalkKeyword		char_conversion
syn keyword	logtalkKeyword		current_char_conversion


" Logic and control

syn match	logtalkOperator		"\\+"
syn keyword	logtalkKeyword		once
syn keyword	logtalkKeyword		repeat


" Atomic term processing

syn keyword	logtalkKeyword		atom_length
syn keyword	logtalkKeyword		atom_concat
syn keyword	logtalkKeyword		sub_atom
syn keyword	logtalkKeyword		atom_chars
syn keyword	logtalkKeyword		atom_codes
syn keyword	logtalkKeyword		char_code
syn keyword	logtalkKeyword		number_chars
syn keyword	logtalkKeyword		number_codes


" Implementation defined hooks functions

syn keyword	logtalkKeyword		set_prolog_flag
syn keyword	logtalkKeyword		current_prolog_flag
syn keyword	logtalkKeyword		halt


" Evaluable functors

syn match	logtalkOperator		"+"
syn match	logtalkOperator		"-"
syn match	logtalkOperator		"\*"
syn match	logtalkOperator		"//"
syn match	logtalkOperator		"/"
syn keyword	logtalkKeyword		rem
syn keyword	logtalkKeyword		mod
syn keyword	logtalkKeyword		abs
syn keyword	logtalkKeyword		sign
syn keyword	logtalkKeyword		float_integer_part
syn keyword	logtalkKeyword		float_fractional_part
syn keyword	logtalkKeyword		float
syn keyword	logtalkKeyword		floor
syn keyword	logtalkKeyword		truncate
syn keyword	logtalkKeyword		round
syn keyword	logtalkKeyword		ceiling


" Other arithemtic functors

syn match	logtalkOperator		"\*\*"
syn keyword	logtalkKeyword		sin
syn keyword	logtalkKeyword		cos
syn keyword	logtalkKeyword		atan
syn keyword	logtalkKeyword		exp
syn keyword	logtalkKeyword		log
syn keyword	logtalkKeyword		sqrt


" Bitwise functors

syn match	logtalkOperator		">>"
syn match	logtalkOperator		"<<"
syn match	logtalkOperator		"/\\"
syn match	logtalkOperator		"\\/"
syn match	logtalkOperator		"\\"


" Logtalk end-of-clause

syn match	logtalkOperator		"\."


" Logtalk list operator

syn match	logtalkOperator		"|"


" Logtalk comments

syn region	logtalkBlockComment	start="/\*"	end="\*/"
syn match	logtalkLineComment	"%.*"


syn sync ccomment maxlines=50


" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet

if version >= 508 || !exists("did_logtalk_syn_inits")
	if version < 508
		let did_logtalk_syn_inits = 1
		command -nargs=+ HiLink hi link <args>
	else
		command -nargs=+ HiLink hi def link <args>
	endif

	HiLink	logtalkBlockComment	Comment
	HiLink	logtalkLineComment	Comment

	HiLink	logtalkOpenEntityDir	Normal
	HiLink	logtalkOpenEntityDirTag	Statement

	HiLink	logtalkEntity		Normal

	HiLink	logtalkEntityRel	Normal
	HiLink	logtalkEntityRelTag	Statement

	HiLink	logtalkCloseEntityDir	Statement

	HiLink	logtalkDir		Normal
	HiLink	logtalkDirTag		Statement

	HiLink	logtalkAtom		String
	HiLink	logtalkString		String

	HiLink	logtalkKeyword		Keyword

	HiLink	logtalkBuiltIn		Keyword
	HiLink	logtalkBuiltInMethod	Keyword

	HiLink	logtalkOperator		Operator

	HiLink	logtalkExtCall		Normal
	HiLink	logtalkExtCallTag	Operator

	delcommand HiLink

endif


let b:current_syntax = "logtalk"

set ts=8
