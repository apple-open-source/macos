Specification of the API a manpage/tcl has to conform to so that rule
files are able to process it properly.
======================================================================

The available commands are listed in the file 'manpage.api' too.  The
definitions in that file return errors. They should be loaded before
the actual definitions so that usage of an unimplemented command
causes a proper error message.

----------------------------------------------------------------------

The main commands are "manpage_begin", "manpage_end" and
"description". All three are required. The first two are the first and
last commands in a manpage. Neither text nor other commands may
precede "manpage_begin" nor follow "manpage_end". The command
"description" separates header and body of the manpage and may not be
omitted.

The only allowed text between "manpage_begin" and "description" is the
command "require". Other commands or normal text are not
permitted. "require" is used to list the packages the described
command(s) depend(s) on for its operation. This list can be empty.

After "description" text all other commands are allowed. The text can
be separated into highlevel blocks using named "section"s. Each block
can be further divided into paragraphs via "para".

The commands "see_also" and "keywords" define whole sections named
"SEE ALSO" and "KEYWORDS". They can occur everywhere in the manpage
but making them the last section is the usual thing to do. They can be
omitted.

There are four commands available to mark words, "arg", "cmd", "emph"
and "strong". The first two are used to mark words as command
arguments and as command names. The other two are visual markup to
emphasize words.

Another set of four commands is available to construct (nested)
lists. These are "list_begin", "list_end", "lst_item" and "call".  The
first two of these begin and end a list respectively. The argument to
the first command is either 'bullet' or 'enum' denoting the type of
the list (unordered vs. ordered). The third command starts list
items. Each item has some text directly associated with the bullet but
the major bulk of the item is the text following the item until the
next list command.

The last list command, "call" is special. It is used to describe the
syntax of a command and its arguments. It should not only cause the
appropriate markup of a list item at its place but also add the syntax
to the table of contents (synopsis) if supported by the output format
in question. nroff and HTML for example do. A logical format like TMML
doesn't.


I currently use the ?...? notation in my example to mark optional
arguments. This should better be done through a command. This command
is "opt". _Not_ "optarg" as it may span several arguments.

======================================================================

   [list_begin "bullet"] --
    	Starts a bulleted (unordered) list.
    [bullet] --
    	Starts a new item in a bulleted list.

    [list_begin "enum"] --
    	Starts a numbered list.
    [enum] --
    	Starts a new item in a numbered (ordered) list.  Successive list
	items are numbered [1], [2],  ... etc.

    [list_begin "definitions"] --
    	Starts a definition list.
    [lst_item _text_] --
    	Starts a new entry in a definition list.  _text_ is
	the list item header.

    [list_begin commands] --
    	Starts a command list, for describing the syntax of a
	command and its arguments.
    [call _cmd_ ? _arg_ _arg_ ... ? ] --
    	Starts an entry in a command list.  In addition, adds the
	command syntax summary "_cmd_ _args_..." to the SYNOPSIS section.
