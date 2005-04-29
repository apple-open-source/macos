.\" $XConsortium: progguide.mm,v 1.5 94/04/18 21:04:07 rws Exp $
.\" Copyright (c) 1992  X Consortium
.\" 
.\" Permission is hereby granted, free of charge, to any person obtaining a
.\" copy of this software and associated documentation files (the "Software"), 
.\" to deal in the Software without restriction, including without limitation 
.\" the rights to use, copy, modify, merge, publish, distribute, sublicense, 
.\" and/or sell copies of the Software, and to permit persons to whom the 
.\" Software furnished to do so, subject to the following conditions:
.\" 
.\" The above copyright notice and this permission notice shall be included in
.\" all copies or substantial portions of the Software.
.\" 
.\" THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
.\" IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
.\" FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL 
.\" THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
.\" WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
.\" OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
.\" SOFTWARE.
.\" 
.\" Except as contained in this notice, the name of the X Consortium shall not 
.\" be used in advertising or otherwise to promote the sale, use or other 
.\" dealing in this Software without prior written authorization from the 
.\" X Consortium.
'
.ds dD Programmers Guide for the X Test Suite
.so 00.header
'\"
'\" Start and end of a user-typed display
'\"
.de eS
.in +2
.nf
.ft C
.ps -2
..
.de eE
.ps +2
.ft R
.fi
.in -2
..
.de cS
.DS I
.ft C
.ps -2
..
.de cE
.ps +2
.ft R
.DE
..
'\" # Courier
.de C
\fC\\$1\fP\\$2\fC\\$3\fP\\$4\fC\\$5\fP\\$6
..
'\"	###
.H 1 "Introduction"
This document is a Programmers Guide to the \*(xT.
.P
Instructions for installing and running the
\*(xT are contained in the document
"User Guide for the \*(xT".
It is not
necessary to read the Programmers Guide in order to install and run the test
suite.
.H 1 "Purpose of this guide"
The information in this section is designed to be used by a programmer
intending to review the source code in the
revised \*(xT. It is also intended to be used by an experienced
programmer, familiar 
with the \*(xW, to modify or extend the \*(xT to add additional tests.
.P
Before reading this document, it is necessary to read
the document "User Guide for the \*(xT".
This is because the nomenclature used in this
document is explained in the "User Guide". Appendix F of that document is 
a glossary, which explains the meaning of 
some terms which may not be in common usage.
.P
The directory structure used within the \*(xT is described further in 
"Appendix A - Contents of \*(xR" in
the "User Guide". You should be familiar with that 
appendix before reading further.
.H 1 "Contents of this guide"
The test set source files in the 
revised \*(xT have been developed in the format of a 
simple language, specially produced as part of the
test suite development project. 
Files in the test suite which use this 
language have the suffix ".m" and are known as dot-m files.
.P
The syntax of this language is described in the next section of this 
document entitled "Source file syntax".
.P
During the stage one review of the development project, it was determined
that there were some advantages in methods of automatically generating 
source code for the tests from templates. The dot-m file may be seen as 
a template for the tests. At the same time, it was important that 
any utilities associated with the code generation should be commonly
available or provided within the test suite.
.P
For this reason, a utility 
.C mc
has been provided to convert dot-m files
into C source code and produce Makefiles automatically
for the test set.
The file formats which may be produced
are described in the section entitled
"Source file formats".
Summaries of usage of the utilies are given in appendices D-F.
.P
As part of the test suite development, a number of conventions were 
established to define how the syntax of this language should be used in 
writing test sets.
It is useful to understand these conventions in order to understand 
the structure of the existing tests.
You are recommended to use the existing tests as a model,
and follow the same structure when modifying or extending the \*(xT.
This is described in the section entitled
"Source file structure". 
.P
The test set structure has deliberately 
been kept as simple as possible, and common 
functions have been developed in libraries. The contents of these
libraries is described in the section entitled "Source file libraries".
.H 2 "Typographical conventions used in this document"
The following conventions have been used in this document:
.AL
.LI
Items appearing in angle brackets <> should be substituted with a suitable 
value.
.LI
Items appearing in square brackets [] are optional.
.LE
.SK
.H 1 "Source file syntax"
It is a design requirement that 
the test code for each test purpose has an associated description of what 
is being tested
(an
.I assertion )
and a description of the procedure used to test it
(a
.I "test strategy" ).
All of these items are contained in dot-m files,
and programs are used to extract the relevant parts.
The utilies developed for the purpose are
outlined in appendices D-F, and the format of the files they output
is described in the section entitled
"Source file formats.
.P
The format of a dot-m file consists of a number of sections
introduced by keywords. The sections of a dot-m file are as follows:
.AL
.LI
A copyright notice.
.LI
A section introduced by the >>TITLE keyword. This section 
defines the name of the function being tested and its arguments.
.P
The >>TITLE keyword, and the declaration and arguments 
which follows it, are together known as the "title section".
.LI
Optionally, there may be a section introduced by the >>MAKE keyword.
This section defines additional rules for 
.C make
beyond the default rules.
.P
The >>MAKE keyword, and the text which follows it, are together
known as a "make section".
.LI
Optionally, there may be a section introduced by the >>CFILES keyword.
This section defines additional C source files
source files that should be compiled and linked 
(together with the
C source files produced by
.C mc )
when building a test set.
.LI
Optionally, there may be a section introduced by the >>EXTERN keyword.
This section defines C source code which will be in scope for 
all test purposes in the test set.
.P
The >>EXTERN keyword, and the source code which follows it, are together
known as an "extern section".
.LI
For each test purpose in the test set, there will be 
a section introduced by an >>ASSERTION keyword. These sections 
each contain the text of an assertion for the function being tested.
.P
The >>ASSERTION keyword, and the text which follows it, are together
known as an "assertion section".
.LI
An assertion section is normally followed by a section introduced 
by a >>STRATEGY keyword, which is followed by the strategy. This is 
a description of how the assertion is tested.
.P
The >>STRATEGY keyword, and the strategy which follows it, are together
known as a "strategy section".
.LI
The strategy is always followed by a section introduced
by a >>CODE keyword, which is followed by a section of C source 
code which will test 
whether the assertion is true or false on the system being tested.
.P
The >>CODE keyword, and the C source code which follows it, are together
known as a "code section".
.LI
Optionally, the >>INCLUDE keyword includes, from a file, one or more 
of the above sections, at that point in the dot-m file. 
It terminates the previous section.
.LE
.P
The keywords introducing the sections are defined in detail below.
.P
There are also optional keywords which may appear anywhere in a dot-m file:
.P
.AL
.LI
The >>SET keyword sets options which apply in the dot-m file.
.LI
The >># keyword introduces a comment in a dot-m file.
.LE
.P
The optional keywords are also defined in detail below.
.P
.H 2 "Title section - >>TITLE"
.H 3 "Description"
.cS
\&>>TITLE <function> <section>
.cE
.P
This keyword must be used at the start of a test purpose immediately after
the copyright message.
.P
It may be followed by the declaration of the data type returned and 
the function arguments.
These lines may be omitted if XCALL is never used in any code section 
in the test set.
.P
.AL
.LI
The line after >>TITLE should specify the data type returned.
.LI
The next line should specify the function call with any arguments.
This is no longer mandatory, since the information is not used in the current 
version of 
.C mc .
So the second line may be left completely blank.
.LI
The following lines should specify the arguments and may set them 
to any expression (not necessarily a constant expression). 
Usually arguments are set to the return value of a function call, 
a global variable inserted by the 
.C mc
utility,
or a global variable declared in an >>EXTERN section.
.P
Static variables will be created to match these arguments, and
will be reset as specified automatically before the start 
of each test purpose, by the function 
.C setargs() 
(described in the section entitled
"Source file formats").
.LE
.H 3 "Arguments"
.VL 12 2
.LI function
This is the name of the function in the \*(xW to be tested. 
In the \*(xP
it should be the name of an X\ Protocol request or 
an X\ event.
In the \*(xL
it should be the name of an Xlib function or
an X\ event.
.P
The XCALL macro will invoke the named function or macro (which should be in 
Xlib) with the arguments specified on the lines following >>TITLE.
.LI section
This is the section of the \*(xT in which this particular test set is stored. 
It should be the name of a directory in 
.C $TET_ROOT/tset .
The section name is used in formatting the assertions in the test set,
and for output to the journal file for reporting purposes.
The section name does not affect the building and execution of the tests.
.LE
.H 2 "Make section - >>MAKE"
.H 3 "Description"
.cS
\&>>MAKE
.cE
.P
This keyword may be used anywhere in the dot-m file after the end of 
a section.
.P
It should be followed by lines which will be copied into the Makefile
by the 
.C mc
utility, when the Makefile is remade using the 
.C -m
option.
.P
The lines specified will be joined together and placed in order 
before the first 
rule in the Makefile. In this way, the lines copied may contain both 
initialisation of Make variables, and additional rules. 
.P
This keyword should be used sparingly, since the additional Makefile lines 
must be consistent with the rest of the
Makefile (see later section entitled "Makefile").
.P
These lines are designed to allow auxiliary programs to be made by 
.C make
(in addition to the default target which is 
.C Test ).
For example, if test purposes in the test set 
execute programs 
.C Test1 
and 
.C Test2 ,
which must also be built, 
the >>MAKE keyword can be 
followed by lines which specify rules for building 
these executable programs. This can be done as follows:
.P
.cS
>>MAKE
AUXFILES=Test1 Test2
AUXCLEAN=Test1.o Test1 Test2.o Test2

all: Test

Test1 : Test1.o $(LIBS) $(TCMCHILD)
	$(CC) $(LDFLAGS) -o $@ Test1.o $(TCMCHILD) $(LIBLOCAL) $(LIBS) $(SYSLIBS)
 
Test2 : Test2.o $(LIBS) $(TCMCHILD)
	$(CC) $(LDFLAGS) -o $@ Test2.o $(TCMCHILD) $(LIBLOCAL) $(LIBS) $(SYSLIBS)
.cE
Notice that the default target 
.C Test
is still made by the new default Make rule.
Also, notice that \fCTest1\fP and \fCTest2\fP are not dependencies for the 
\fCall\fP target. They are dependencies for \fCTest\fP.
(See the later section entitled "Makefile".)
.H 2 "Additional source files - >>CFILES"
.H 3 "Description"
.cS
\&>>CFILES <filename> ...
.cE
This keyword may be used anywhere in the dot-m file after the end
of a section.
.P
The list of files following the keyword are taken as the names of C
source files that should be compiled and linked (together with the 
C source files produced by 
.C mc )
when building a test set.
This allows code to be split among several files.
.P
The only effect is to alter the Makefile that is produced by
.C mmkf .
.H 2 "Extern section - >>EXTERN"
.H 3 "Description"
.cS
\&>>EXTERN
.cE
.P
This keyword may be used anywhere in the dot-m file after the end of 
a section.
.P
It should be followed by lines of C source code which will be copied 
into the C source file
by the 
.C mc
utility, when the C source code is remade.
.P
These lines will be copied unaltered into the C source file 
before the source code for the first test purpose.
.P
This section is useful for including three types of source code:
.AL
.LI
static variables declarations which are used by a number of 
test purposes in the test set.
.LI
static functions which are used by a number of 
test purposes in the test set.
.LI
header file inclusions which are needed in addition to the default header file
inclusions in the C source code.
.LE
.H 2 "Assertion section - >>ASSERTION"
.H 3 "Description"
.cS
\&>>ASSERTION <test-type> [ <category> [<reason>] ]
.cE
.P
This keyword is used at the start of each test purpose.
.P
It should be followed by the text of the assertion
which is a description of what is tested by this particular test purpose.
.P
The text should not contain troff font change commands (or any other 
nroff/troff commands). This is because the majority of nroff/troff 
commands will not be understood by the
.C ma 
utility.
However, various macros can be
used to enable mapping of the format of special text 
onto similar fonts to those used in the \*(xW documentation.
These are described in appendix B.
.P
The keyword xname in the assertion text will be replaced by 
.C mc 
with the name of the function
under test obtained from the >>TITLE keyword.
.P
Unless the 
.C category
argument specifies an extended assertion, or the 
.C test-type
is 
.C gc
or
.C def ,
the assertion text must be followed by the >>STRATEGY keyword, strategy 
section, >>CODE keyword and code section.
Refer to the description of the 
.C category 
argument.
.P
If the >>CODE keyword is missing, following the text of an assertion 
which is not an extended assertion, the 
.C mc
utility will insert code to produce a result code UNREPORTED for this 
test purpose when the test set is executed.
.H 3 "Arguments"
.VL 12 2
.LI test-type
.VL 6 2
.LI Good
.P
This is a "good" test.
The function under test is expected to give a successful result.
.P
By convention these assertions appear in the dot-m file
before all assertions with test-type Bad.
.LI Bad
.P
This is a "bad" test.
The function under test is expected to give an unsuccessful result
under the conditions that the test imposes.
.P
By convention these assertions appear in the dot-m file 
after all assertions with test-type Good.
.P
The assertion text for many of these assertions is included via the .ER
keyword, described below.
.LI gc
.P
The assertion text states which gc components affect the function under test.
In this case the remaining arguments are unused, and 
.C mc
inserts into the C source file a series of assertions, strategies and
test code corresponding to the gc components listed in the assertion text 
via the macro
.cS
\&.M gc-comp ,
.cE
or
.cS
\&.M gc-comp .
.cE
.P
The assertions, strategies and test
code are included, from files in the directory
\fC$TET_ROOT/xtest/lib/gc\fP.
.P
The \.M macro is used, since the gc components correspond to structure members 
in a gc structure.
.LI def
.P
The assertion text is tested in the test for another assertion.
These assertions are often definitions of terms, which cannot be tested in 
isolation, hence the abbreviation "def".
The remaining arguments are unused, and
.C mc
inserts code into the C file to issue the result code NOTINUSE, and 
issues a message stating that the assertion is tested elsewhere.
.LE
.ne 5
.LI category
.P
.P
This is the assertion category, modelled on the corresponding
codes in the document\*F
.FS
Obtainable from Publication Sales, IEEE\ Service\ Center, P.O.\ Box\ 1331,
445\ Hoes\ Lane, Piscataway, NJ\ 08854-1331, (201)\ 981-0060
.FE
POSIX.3 entitled "Test Methods for Measuring Conformance to POSIX".
It is either A, B, C or D.
.P
If the assertion tests a conditional feature, it is categorised as type 
C or D, otherwise it is categorised as type A or B.
.P
If the assertion is classified as an "extended assertion" 
it is categorised as type
B or D. 
Otherwise it is categorised
as type A or C and is known as a "base assertion".
.TS
center box;
c | c | c.
	Base Assertion	Extended Assertion
_
Required Feature	A	B
_
Conditional Feature	C	D
.TE
.P
Tests are always required for base assertions. Tests are not required 
for extended assertions, but should be provided if possible. 
Extended assertions are used to describe features that may be difficult 
to test.
.P
In some 
cases partial testing may be performed for extended assertions.
An example is that it may be possible to test that some specific common
faults are not present.
In this the result code would be FAIL if an error is detected, or 
UNTESTED if no failure is detected, but the assertion is still 
not fully tested.
.P
For this reason, the strategy and code sections are optional for 
extended assertions.
If they are not supplied, 
.C mc
will automatically generate source code to
put out a result code UNTESTED, with a message which describes 
the reason.
If they are supplied, they will override the automatically generated sections.
.P
Since there is not yet an equivalent document to POSIX.3 for
the \*(xW then these codes are subject to change. For example, an 
assertion classified as an "extended assertion" (type B) might 
become a "base assertion" (type A) if a test method is later identified.
.P
The following table lists the allowed test result codes for each category.
.P
.TS
center box;
c | l.
Category	Allowed Result Codes
_
A	PASS, FAIL, UNRESOLVED
B	PASS, FAIL, UNTESTED, UNRESOLVED
C	PASS, FAIL, UNSUPPORTED, UNRESOLVED
D	PASS, FAIL, UNSUPPORTED, UNTESTED, UNRESOLVED
.TE
.LI reason
.P
In the case of extended assertions (category B or D)
a reason code must be supplied. These are the same as in POSIX.3.
A list of the reason codes, and the corresponding text of the reason, are 
shown in appendix A.
.LE
.H 2 "Strategy section - >>STRATEGY"
.H 3 "Description"
.cS
\&>>STRATEGY
.cE
.P
If the category of an assertion is A or C, 
this keyword must be used immediately after the assertion section.
.P
If the category of an assertion is B or D, 
this keyword may be optionally be used immediately after the assertion section.
.P
It should be followed by the strategy, which is a description of how the 
assertion is to be tested.
.P
The text of the strategy is in free format sentences.
It may contain the xname keyword, which is an abbreviation for the name 
of the function under test.
.P
The use of the XCALL keyword in the strategy section as an abbreviation 
for "Call xname" is discontinued, 
although in this release, some occurrences remain.
.H 2 "Code section - >>CODE"
.H 3 "Description"
.cS
\&>>CODE [<BadThing>]
.cE
.P
This keyword must be used immediately after the strategy section.
.P
It should be followed by the C source code which will test the assertion.
.P
The C source code will be converted by 
.C mc
into a format suitable for the use by the 
.SM "TET API" .
The way in which this is done is described 
in the section entitled
"Source file formats".
.P
A blank line must separate the declarations of automatic variables 
in the test function
from the first executable statement. There must be no other blank lines 
within these declarations.
.P
The utility 
.C mc
also expands the XCALL macro to call the function under test, and 
to call library functions, before and after the function under test, 
to install and deinstall error handlers and flush pending requests to the 
X server. The way in which this is done is described
in the section entitled
"Source file formats".
.H 3 "Arguments"
.VL 12 2
.LI BadThing
.P
This is an optional argument to the >>CODE macro.
.P
If it is omitted,
the XCALL macro will cause code to be 
inserted to ensure that the function 
under test produces no X\ Protocol error, and issues a result code FAIL if an 
error is detected. 
.P
If it is set to the symbolic value of an \*(xW
error code, the XCALL macro will cause code to be 
inserted to ensure that the 
function under test produces that X\ Protocol error, 
and issues a result code FAIL 
if the wrong error, or no error, is detected.
.LE
.H 2 "Included section - >>INCLUDE"
.H 3 "Description"
.cS
\&>>INCLUDE <filename>
.cE
This keyword includes the contents of
.C filename ,
which must be a file containing 
one or more sections in the dot-m file format, optionally containing a 
copyright header.
The sections in the included file should
produce a valid dot-m file, if they were included directly at the point
of inclusion. The >>INCLUDE keyword terminates the preceding section - 
it cannot be used in the middle of a section.
.P
The included sections are processed at the point of inclusion when 
the C source code is generated by the
.C mc
utility.
.P
The >>INCLUDE keyword is usually used when including test purposes
which are common to more than one test set.
The >>INCLUDE mechanism allows an entire test purpose (including 
assertion, strategy and code sections) to be included.
.P
The >>INCLUDE keyword should not be used for merely including
common functions called by a number of test purposes. 
If the functions are common to 
one dot-m file, they should be placed in an extern section. If the functions
are common to many test sets, they should be placed in one of the 
\*(xT libraries.
.SK
.H 2 "Included errors - .ER"
.H 3 "Description"
.cS
\&.ER [Bad]Access grab
\&.ER [Bad]Access colormap-free
\&.ER [Bad]Access colormap-store
\&.ER [Bad]Access acl
\&.ER [Bad]Access select
\&.ER [Bad]Alloc
\&.ER [Bad]Atom [val1] [val2] ... \(dg
\&.ER [Bad]Color
\&.ER [Bad]Cursor [val1] [val2] ... \(dg
\&.ER [Bad]Drawable [val1] [val2] ... \(dg
\&.ER [Bad]Font bad-font
\&.ER [Bad]Font bad-fontable
\&.ER [Bad]GC
\&.ER [Bad]Match inputonly
\&.ER [Bad]Match gc-drawable-depth
\&.ER [Bad]Match gc-drawable-screen
\&.ER [Bad]Match wininputonly
\&.ER [Bad]Name font
\&.ER [Bad]Name colour
\&.ER [Bad]Pixmap [val1] [val2] ... \(dg
\&.ER [Bad]Value <arg> [mask] <val1> [val2] ... \(dd
\&.ER [Bad]Window [val1] [val2] ... \(dg
.cE
.S -1
\(dg - these arguments are optional.
.P
\(dd - the <arg> and at least <val1> argument must be supplied. The 
mask argument, and additional arguments, are optional.
.P
Note - the \fCBad\fP prefix is in each case optional.
.S 
.P
This keyword 
causes
.C mc
to insert into the C source file the text of an assertion, 
and in some cases default strategy and
default
test code to test for the generation of a particular X\ Protocol error by
an Xlib function.
.P
In some cases there is no strategy and code in the included file,
because only the assertion is common - the strategy and code 
sections are specific to each test purpose, and must be provided immediately
after the .ER keyword.
.P
The default strategy and code sections (if included) may be overridden by
sections in the dot-m file immediately after the .ER keyword.
.P
Note that this keyword does not insert the >>ASSERTION keyword - this must 
appear on the line before .ER is invoked. Thus the keyword does not
include the entire assertion section.
.P
The assertion text, strategy and test
code are included from files in the directory
\fC$TET_ROOT/xtest/lib/error\fP.
.P
The names of these files, and the 
assertion text in each file, is shown in appendix C.
.H 2 "Set options - >>SET"
.H 3 "Description"
.cS
\&>>SET startup <func_startup>
\&>>SET cleanup <func_cleanup>
\&>>SET tpstartup <func_tpstartup>
\&>>SET tpcleanup <func_tpcleanup>
\&>>SET need-gc-flush
\&>>SET fail-return
\&>>SET fail-no-return
\&>>SET return-value <return_value>
\&>>SET no-error-status-check
\&>>SET macro [ <macroname> ]
\&>>SET begin-function
\&>>SET end-function
.cE
.P
These options control how the
.C mc
utility converts the dot-m file into a C source file.
.P
Except where specifically stated, 
they may appear anywhere in the dot-m file and apply from that point on, unless
reset by a further >>SET keyword with the same first argument.
.P
.H 3 "Arguments"
.VL 12 2
.LI startup 
.br
The name of the function called before all test 
purposes is to be set to func_startup (rather than the default, startup()).
.LI cleanup 
.br
The name of the function called after all test 
purposes is to be set to func_cleanup (rather than the default, cleanup()).
.LI tpstartup 
.br
The name of the function called before each test 
purpose is to be set to func_tpstartup (rather than the default, tpstartup()).
.LI tpcleanup 
.br
The name of the function called after each test 
purpose is to be set to func_tpcleanup (rather than the default, tpcleanup()).
.LI need-gc-flush
.br
When the XCALL macro is expanded, code to call the \*(xT library 
function gcflush(display, gc) will be inserted
after the code to call the function under test.
.LI fail-return
.br
When the XCALL macro is expanded, code to end the test purpose 
will be inserted where an error is reported
(the default is to continue after an error is reported).
.LI fail-no-return
.br
When the XCALL macro is expanded, no code to end the test purpose 
will be inserted where an error is reported (this reverses the effect earlier
using >>SET fail-return).
.LI return-value
.br
When the XCALL macro is expanded, and the Xlib function call has return type
.C Status  ,
and the return value of XCALL is not saved for testing in the calling code,
code will be inserted to report an error if the function under test does not 
return <return_value>. 
(By default, 
when the Xlib function call has return type
.C Status  ,
an error is reported for assertions with test-type
Good if the return value is zero, and for assertions with test-type Bad if
the return value is non-zero).
.LI no-error-status-check
.br
When the XCALL macro is expanded, the default code to check for X Protocol 
errors will not be inserted. The test purpose can perform alternative checking
after invoking XCALL.
This setting only applies up to the end of the current section.
.LI macro 
.br
There is a macro in an \*(xW header file for which a
test set source file will be produced which uses identical test purposes
to the function under test. This is used to automatically
generate test purposes for 
the Display and Screen information macros, which are identical to those 
for the corresponding Xlib functions.
.P
The macro name is set to 
.C <macroname>
- the default is the
.C function
argument in the >>TITLE keyword,
with the leading letter `X' removed.
.P
This option must be specified 
.B before 
the title section of the dot-m file.
.P
Note - this option may not be used for macros which have no arguments.
.LI begin-function
.br
The name of an additional function called before each test 
purpose, after tpstartup() and global function arguments are initialised.
.LI end-function
.br
The name of an additional function called after each test 
purpose, before tpcleanup().
.LE
.H 2 "Comment lines - >>#"
.H 3 "Description"
.cS
\&>># <Comment text>
.cE
This keyword specifies a one line comment. These are not intended to replace 
code comments in the code section - in fact the 
.C mc
utility does not copy dot-m format comments to the C source file.
Comments in the dot-m file are used for higher level comments rather
than detailed comments. For example, comments are used to record the 
history of the development of the dot-m file, to preserve previous versions 
of assertions where necessary,
and to draw attention to unresolved problem areas.
.SK
.H 1 "Source file formats"
This section describes the output from
various utilities which may be used to format 
the contents of a dot-m file.
.P
The code-maker utility
.C mc
builds C source files from a dot-m file. 
Appendix D gives a usage summary.
.P
The Makefile utility
.C mmkf
builds Makefiles from a dot-m file. 
Appendix E gives a usage summary.
.P
The assertion utility
.C ma
produces a list of assertions from a dot-m file. 
Appendix F gives a usage summary.
.P
Instructions for building and installing the 
.C mc
utility are given in the "User Guide". When the utility 
.C mc
is built and installed, the utilities
.C mmkf
and
.C ma ,
which are links to the same program 
.C mc 
differing only in name, are automatically installed as well.
.H 2 "C files for standalone executable - \fCTest.c\fP"
The command 
.cS
mc -o Test.c stclpmsk.m
.cE
produces a C file named 
.C Test.c .
This may be compiled to produce a standalone executable file named
.C Test .
Instructions on building and executing the tests are given in the "User Guide".
The
.C Test.c
files are not provided as part of this release, but are built 
automatically when building the \*(xT.
.P
The C file contains all of the interface code required
to invoke the test purposes from the 
.SM TET 
and a description of the assertion being tested is placed as a comment
above the code for each test 
purpose to make it easy to understand what is being tested.
.P
The remaining parts of this section describe the format of the 
.C Test.c 
files in more detail. The descriptions are in the order in which the text
is inserted into the 
.C Test.c 
file.
.P
Some parts of the 
.C Test.c 
file are constructed by copying in template files specifically written 
to work with 
.C mc .
These files are all located in the directory 
\fC$TET_ROOT/xtest/lib/mc\fP.
.H 3 "Copyright header"
A copyright header is inserted as a C source comment block. This will
contain lines showing the SCCS versions of the dot-m file and any
included files.
.H 3 "SYNOPSIS section"
A synopsis defining the arguments of the function being tested is inserted
as a C source comment block.
This is constructed from the lines following the >>TITLE keyword in the 
dot-m file. 
It includes the data type returned and any arguments
to the function.
.P
The synopsis section is omitted if there are no lines 
following the the >>TITLE keyword.
.P
For example:
.cS
/*
 * SYNOPSIS:
 *   void
 *   XSetClipMask(display, gc, pixmap)
 *   Display *display;
 *   GC gc;
 *   Pixmap pixmap;
 */
.cE
.H 3 "Include files"
For the \*(xL, when the
.C section 
argument to the >>TITLE keyword is 
other than 
.C XPROTO ,
the contents of the file 
.C mcinclude.mc
are then included.
.P
In this release the contents of this file are 
as follows:
.cS
#include	<stdlib.h>
#include	"xtest.h"
#include	"Xlib.h"
#include	"Xutil.h"
#include	"Xresource.h"
#include	"tet_api.h"
#include	"xtestlib.h"
#include	"pixval.h"
.cE
.P
For the \*(xP, when the
.C section 
argument to the >>TITLE keyword is 
.C XPROTO
the contents of the file 
.C mcxpinc.mc
are then included.
.P
In this release the contents of this file are 
as follows:
.cS
#include	<stdlib.h>
#include	"xtest.h"
#include	"tet_api.h"
.cE
.H 3 "External variables"
For the \*(xL, when the
.C section 
argument to the >>TITLE keyword is 
other than 
.C XPROTO ,
the contents of the file 
.C mcextern.mc
are then included.
.P
In this release the contents of this file are 
as follows:
.cS
extern	Display  *Dsp;
extern	Window   Win;

extern	Window   ErrdefWindow;
extern	Drawable ErrdefDrawable;
extern	GC       ErrdefGC;
extern	Colormap ErrdefColormap;
extern	Pixmap   ErrdefPixmap;
extern	Atom     ErrdefAtom;
extern	Cursor   ErrdefCursor;
extern	Font     ErrdefFont;
.cE
.P
The external variables are defined in the file 
.C startup.c
or (when linking a program executed via \fCtet_exec()\fP)
in the file
.C ex_startup.c .
.P
For the \*(xP, when the
.C section 
argument to the >>TITLE keyword is 
.C XPROTO
the contents of the file 
.C mcxpext.mc
are then included.
.P
In this release this file is empty.
.H 3 "Test set symbol and name"
A symbol is defined indicating the function under test. 
.P
For example:
.P
.cS
#define T_XSetClipMask	1
.cE
.P
You may use this in a
.C #ifdef
control line,
to distinguish special cases for particular functions in code sections
of a dot-m file included via the >>INCLUDE keyword.
.P
The global variable 
.C TestName 
is initialised to the name of the function under test, which is the
.C function
argument given to the >>TITLE keyword.
.P
For example:
.P
.cS
char    *TestName = "XSetClipMask";
.cE
.P
You can use this within a code section 
of a dot-m file to obtain the name of the function under test.
.H 3 "Definitions for arguments"
Symbols are defined to correspond with any arguments to the 
.C function 
specified in the >>TITLE keyword.
.P
These correspond to the lines following the >>TITLE keyword in the 
dot-m file. 
.P
For example:
.P
.cS
/*
 * Defines for different argument types
 */
#define A_DISPLAY display
#define A_GC gc
#define A_PIXMAP pixmap
#define A_DRAWABLE pixmap
.cE
.P
These are used by the code in various included files, to substitute a symbol
representing a particular argument type with the
actual variable used as the argument by the function under test.
.H 3 "Static variables"
Static variables are defined to correspond with any arguments to the 
.C function 
specified in the >>TITLE keyword.
.P
These correspond to the lines following the >>TITLE keyword in the 
dot-m file. 
.P
For example:
.P
.cS
/*
 * Arguments to the XSetClipMask function
 */
static Display *display;
static GC gc;
static Pixmap pixmap;
.cE
.P
These are the arguments that will be passed to the function under test 
when the XCALL macro is expanded.
.P
You can initialise these in a code section of a 
dot-m file as required prior to invoking the macro XCALL.
.P
These variables will be initialised at the start of each test purpose
using the function 
.C setargs() 
described below in the section entitled "Initialising arguments".
.H 3 "Test purpose number"
You can use this within a code section 
of a dot-m file to obtain the number of the current test purpose.
.cS
int     tet_thistest;
.cE
.H 3 "Initialising arguments"
A function 
.C setargs()
is defined to initialise the arguments to the function under
test.
.P
Code to call this function is inserted at the start of each 
test purpose before the code you put in the code section.
.P
The arguments are initialised to have the value of the expression
you specified in the title section. This does not have to be a constant 
expression - for example, it my be a return value of a function in 
a library or extern section.
By default arguments are initialised to zero values.
.P
For example:
.cS
/*
 * Called at the beginning of each test purpose to reset the
 * arguments to their initial values
 */
static void
setargs()
{
	display = Dsp;
	gc = 0;
	pixmap = 0;
}
.cE
.H 3 "Initialising arguments when test-type is Bad"
A function 
.C seterrdef()
is defined to initialise some of the arguments to the function under
test to values which are suitable for conducting the included 
error tests.
.P
This is called in some of the included error tests to initialise 
the arguments to known good values.
.P
For example:
.cS
/*
 * Set arguments to default values for error tests
 */
static void
seterrdef()
{
	gc = ErrdefGC;
	pixmap = ErrdefPixmap;
}
.cE
.H 3 "Code sections"
The code sections in the dot-m file are converted into a sequence of
functions named t\fCnnn\fP(), where \fCnnn\fP is a three digit number which 
is filled with leading zeros if necessary, the first code section 
being named t001(). This is known as a "test function".
.P
Each of the test functions is preceded by the corresponding assertion 
for the test purpose, in a C source code comment, labelled with the 
test purpose number.
.P
Code to call the library function 
.C tpstartup()
is inserted at the start of the test function, immediately 
after any automatic variables declared in your code section.
This function performs initialisation required for each test purpose,
including setting error handlers to trap unexpected errors. 
.P
Code to call the automatically generated test-set specific function 
.C setargs()
is then inserted. This function 
is described further in the previous section entitled "Initialising arguments".
.P
The contents of your code section are then inserted. 
.P
The macro XCALL in a dot-m file is expanded to call the function 
under test with arguments
corresponding to the lines following the >>TITLE keyword.
.P
For example:
.cS
	XCALL;
.cE
is by default expanded to 
.cS
	startcall(display);
	if (isdeleted())
		return;
	XSetClipMask(display, gc, pixmap);
	endcall(display);
	if (geterr() != Success) {
		report("Got %s, Expecting Success", errorname(geterr()));
		FAIL;
	}
.cE
.P
The stages in this expansion are as follows:
.AL
.LI
The library function 
.C startcall()
is called to check for any outstanding unexpected X protocol errors, 
which might have been generated, for example, during the setup 
part of the test. 
A call to 
.C XSync()
is made to achieve this.
.LI
The library function 
.C startcall()
installs a test error handler in place of the unexpected X protocol 
error handler.
.LI
The library function 
.C isdeleted()
returns 
.C True
if the test purpose has already issued a result code UNRESOLVED due to 
an earlier call to 
.C delete() .
(This must be done after calling 
.C startcall()
in case 
.C XSync()
flushed an unexpected X protocol error.)
.LI
The function under test is then called with the arguments listed 
in the title section.
.LI
The library function 
.C endcall()
is called to check for any X protocol errors caused by the 
function under test.
A call to 
.C XSync()
is made to achieve this.
.LI
The library function 
.C endcall()
installs the unexpected X protocol 
error handler.
.LI
The test error handler saves the number of the most recent X protocol error. 
It is accessed by calling the function 
.C geterr()
and the value is checked. The value is expected to be 
.C Success 
by default, or 
.C BadThing
if the code section was introduced in the dot-m file by 
.cS
>>CODE BadThing
.cE
.P
If it is desirable to skip checking the error status at this point, the 
option 
.cS
>>SET no-error-status-check
.cE
may be inserted in the dot-m file in the current code section before the 
XCALL. 
This setting only applies up to the end of the current section.
.LE
.H 3 "TET initialisation code"
The 
.C mc
utility 
adds a reference to the function into an array of functions which can 
be invoked via the 
.SM "TET API" .
.P
For example:
.P
.cS
struct tet_testlist tet_testlist[] = {
	t001, 1,
	t002, 2,
	t003, 3,
	NULL, 0
};
.cE
Code to calculate the number of test purposes in the test set is 
inserted as follows:
.cS
int 	ntests = sizeof(tet_testlist)/sizeof(struct tet_testlist)-1;
.cE
.P
Finally, the names of the startup and cleanup functions are used to 
initialise variables used by the 
.SM "TET API" .
These functions are
called before the first test purpose and after the last test purpose.
The functions called can be overridden using the options
>>SET startup and >>SET cleanup.
.P
The default library functions perform initialisation including 
reading the TET configuration variables and opening a default 
client.
.P
Should you override the default startup and cleanup functions, you 
are recommended to call 
.C startup()
as the first line of your startup function and
.C cleanup()
as the last line of your cleanup function.
.cS
void	(*tet_startup)() = startup;
void	(*tet_cleanup)() = cleanup;
.cE
.H 2 "C files for standalone executable in macro tests - \fCMTest.c\fP"
When the dot-m file contains the line 
.cS
>>SET macro
.cE
the command
.cS
mc -m -o MTest.c scrncnt.m
.cE
produces a C file named 
.C MTest.c .
This may be compiled to produce a standalone executable file named
.C MTest .
Instructions on building and executing the tests are given in the "User Guide".
The
.C MTest.c
files are not provided as part of this release, but are built 
automatically when building the \*(xT.
.P
The file
.C MTest.c
is identical to the file
.C Test.c
except that a macro (which is expected to be made visible by including 
the file Xlib.h)
is tested instead of the Xlib function named in the 
title section of the dot-m file.
.P
The macro name is set to the 
.C <macroname>
argument of the >>SET macro option - if there is no >>SET macro
option in the file, or no argument specified, the default is the
.C function
argument in the >>TITLE keyword,
with the leading letter `X' removed.
.H 2 "C files for linked executable - \fClink.c\fP"
The command 
.cS
mc -l -o link.c stclpmsk.m
.cE
produces a C file named 
.C link.c .
This is identical to the 
.C Test.c
file with the exception of the initialisation code
which 
enables the source code to be
compiled and linked into a space-saving executable file.
This is an executable file
which may invoke any of the test purposes in the various link.c files,
thereby reducing the number of executable files required, and 
saving space.
The
.C link.c
files are not provided as part of this release, but are built 
automatically when building the \*(xT.
.P
The remaining parts of this section describe the differences in
format of the 
.C link.c 
and
.C Test.c
files. 
.P
The differences are associated with the TET initialisation code 
being in a separate source file named 
.C linktbl.c ,
rather than in the test set source file.
.P
A 
.C linktbl.c 
file is provided for each section of the \*(xT in each subdirectory of
.C $TET_ROOT/xtest/tset .
This file contains a pointer to an array of 
.C linkinfo
structures, one for each test set in the section.
Each 
.C linkinfo 
structure contains the following items:
.VL 20 0
.LI \fCname\fP
a unique name for that test set (the name of the test set directory).
.LI \fCtestname\fP
the actual Xlib function tested by the test set.
.LI \fCntests\fP
the number of test purposes in the test set.
.LI \fCtestlist\fP
a pointer to the array of test functions constructed for that test set
from the contents of the link.c file. 
.LI \fClocalstartup\fP
a pointer to the startup function specific to that test set.
.LI \fClocalcleanup\fP
a pointer to the cleanup function specific to that test set.
.LE
.P
Later in this section there are example values of these structure members.
.P
When the space-saving executable is executed,
the TET 
initialisation code in the library function 
.C linkstart.c
determines which test set is required. This is done by matching \fCargv[0]\fP
with a \fCname\fP element in the array of 
.C linkinfo
structures. The test functions specified by the corresponding 
\fCtestlist\fP
element of the
.C linkinfo
structure are then executed, preceded and followed by the corresponding
startup and cleanup function respectively.
.H 3 "Test set symbol and name"
The global variable 
.C TestName
is made static.
.cS
static char    *TestName = "XSetClipMask";
.cE
.H 3 "Test purpose number"
This is defined in
.C linktbl.c , 
and is made available via the following code:
.cS
extern int    tet_thistest;
.cE
.H 3 "TET initialisation code"
The global variable 
.C tet_testlist
is made static.
.P
For example:
.P
.cS
static struct tet_testlist tet_testlist[] = {
	t001, 1,
	t002, 2,
	t003, 3,
	NULL, 0
};
.cE
The global variable 
.C ntests
is made static.
.cS
static int    ntests = sizeof(tet_testlist)/sizeof(struct tet_testlist)-1;
.cE
The 
.C linkinfo 
structure specific to this test set is defined.
.P
For example:
.P
.cS
struct linkinfo EXStClpMsk = {
      "stclpmsk",
      "XSetClipMask",
      &ntests,
      tet_testlist,
      0,
      0,
};
.cE
.P
The TET variables for the startup and cleanup functions are defined in
.C linktbl.c , 
and are made available via the following code:
.cS
extern void   (*tet_startup)();
extern void   (*tet_cleanup)();
.cE
.H 2 "C files for linked executable in macro tests - \fCmlink.c\fP"
When the dot-m file contains the line 
.cS
>>SET macro
.cE
the command
.cS
mc -m -l -o mlink.c scrncnt.m
.cE
produces a C file named 
.C mlink.c .
The
.C mlink.c
files are not provided as part of this release, but are built 
automatically when building the \*(xT.
.P
The file
.C mlink.c
is identical to the file
.C link.c ,
except that a macro (which is expected to be made visible by including 
the file Xlib.h)
is tested instead of the Xlib function named in the 
title section of the dot-m file.
.P
The macro name is set to the 
.C <macroname>
argument of the >>SET macro option - if there is no >>SET macro
option in the file, or no argument specified, the default is the
.C function
argument in the >>TITLE keyword,
with the leading letter `X' removed.
.H 2 "Makefile"
The command 
.cS
mmkf -o Makefile scrncnt.m
.cE
produces a Makefile which can be used to build
all the C source files described in the previous sections
and to build the test executables from the C files.
.P
Further instructions appear in the "User Guide" in the section 
entitled 
"Building, executing and reporting tests without using the TET".
.P
The Makefiles produced by 
.C mc
are portable in that they use symbolic names to describe commands 
and parameters which may vary from system to system.
The values of these symbolic names are all obtained by a utility 
.C pmake 
from the 
build configuration file, which is described in the 
"User Guide" in the section entitled
"Configuring the \*(xT".
.P
The targets in the Makefile which can be invoked by 
.C pmake
are as follows:
.VL 12 0
.LI "\fCpmake Test\fP"
.br
Builds standalone executable version of the test set.
.LI "\fCpmake Test.c\fP"
.br
Builds 
.C Test.c
using 
.C mc
with the format described in the earlier section entitled
"C files for standalone executable - \fCTest.c\fP".
.LI "\fCpmake MTest\fP"
.br
Builds standalone executable version of the test set to test the macro 
version of the function.
.LI "\fCpmake MTest.c\fP"
.br
Builds 
.C MTest.c
using 
.C mc
with the format described in the earlier section entitled
"C files for standalone executable - \fCMTest.c\fP".
.LI "\fCpmake linkexec\fP"
.br
Builds the object files and links
which can be used to produce a linked executable file. These 
targets are used when building space-saving executables as 
described in the "User Guide".
.LI "\fCpmake link.c\fP"
.br
Builds 
.C link.c
using 
.C mc
with the format described in the earlier section entitled
"C files for linked executable - \fClink.c\fP".
.LI "\fCpmake mlink.c\fP"
.br
Builds 
.C mlink.c
using 
.C mc
with the format described in the earlier section entitled
"C files for linked executable - \fCmlink.c\fP".
.LI "\fCpmake clean\fP"
.br
This
removes object files and temporary files from the test set directory.
.LI "\fCpmake clobber\fP"
.br
This
removes object files and temporary files from the test set directory
and additionally removes all the source files which 
.C mc
can remake.
.LE
.P
The remaining parts of this section describe the format of the 
Makefiles in more detail.
.P
Refer to the section entitled "Make section - >>MAKE" for examples of how
the variables
.SM AUXFILES 
and
.SM AUXCLEAN 
may be set.
.H 3 "Copyright header"
A copyright header is inserted as a comment block. This will contain
lines showing the SCCS versions of the dot-m file and any included files.
.H 3 "Make variables"
A series of 
.C make
variables are initialised
to represent the names of 
the source, object and executable files.
.cS
SOURCES=scrncnt.m 
CFILES=Test.c 
OFILES=Test.o 
MOFILES=MTest.o 
LOFILES=link.o mlink.o 
LINKOBJ=scrncnt.o
LINKEXEC=scrncnt
.cE
.H 3 "Targets for \*(xP"
For the \*(xP, when the
.C section 
argument to the >>TITLE keyword is 
.C XPROTO
the contents of the file 
.C mmxpinit.mc
are then included.
.P
This file initialises various 
.C make
variables to specific values for the \*(xP.
.P
In this release the contents of this file are as follows:
.cS
#
# X Protocol tests.
#

# CFLAGS - Compilation flags specific to the X Protocol tests.
#
CFLAGS=$(XP_CFLAGS)
SYSLIBS=$(XP_SYSLIBS)
LIBS=$(XP_LIBS)

# LINTFLAGS - Flags for lint specific to the X Protocol tests.
#
LINTFLAGS=$(XP_LINTFLAGS)
LINTLIBS=$(XP_LINTLIBS)

.cE
.H 3 "Targets for standalone executable - \fCTest\fP"
The contents of the file 
.C mmsa.mc 
are included. These are the targets to create the standalone executable file
.C Test .
.cS
#
# Build a standalone version of the test case.
#
Test: $(OFILES) $(LIBS) $(TCM) $(AUXFILES)
	$(CC) $(LDFLAGS) -o $@ $(OFILES) $(TCM) $(LIBLOCAL) $(LIBS) $(SYSLIBS)

Test.c: $(SOURCES)
	$(CODEMAKER) -o Test.c $(SOURCES)

.cE
.H 3 "Targets for standalone executable - \fCMTest\fP"
If the dot-m file contains the >>SET macro option, 
the contents of the file 
.C mmmsa.mc 
are included. These are the targets to create the standalone executable file
.C MTest
for the macro version of the specified Xlib function.
.cS
#
# Build a standalone version of the test case using the macro version
# of the function.
#
MTest: $(MOFILES) $(LIBS) $(TCM) $(AUXFILES)
	$(CC) $(LDFLAGS) -o $@ $(MOFILES) $(TCM) $(LIBLOCAL) $(LIBS) $(SYSLIBS)

MTest.c: $(SOURCES)
	$(CODEMAKER) -m -o MTest.c $(SOURCES)

.cE
.H 3 "Targets for linked executable"
The contents of the file 
.C mmlink.mc 
are included. 
These are the targets to create object files and links
which can be used to produce a linked executable file. 
These targets are used when building space-saving executables as described in
the "User Guide".
.cS
#
# A version of the test that can be combined together with
# all the other tests to make one executable.  This will save a
# fair amount of disk space especially if the system does not
# have shared libraries.  Different names are used so that
# there is no possibility of confusion.
#
link.c: $(SOURCES)
	$(CODEMAKER) -l -o link.c $(SOURCES)

# Link the objects into one large object.
#
$(LINKOBJ): $(LOFILES)
	$(LD) $(LINKOBJOPTS) $(LOFILES) -o $(LINKOBJ)

# Link the object file into the parent directory.
#
\^../$(LINKOBJ): $(LINKOBJ)
	$(RM) ../$(LINKOBJ)
	$(LN) $(LINKOBJ) ..

# Make a link to the combined executable.
#
$(LINKEXEC): ../Tests
	$(RM) $(LINKEXEC)
	$(LN) ../Tests $(LINKEXEC)

\^../Tests: ../$(LINKOBJ)

linkexec:: $(LINKEXEC) $(AUXFILES) ;

.cE
.H 3 "Targets for linked executable - macro version"
If the dot-m file contains the >>SET macro option, 
the contents of the file 
.C mmmlink.mc 
are included. 
These are the targets to create object files and links
for the macro version of the specified Xlib function
which can be used to produce a linked executable file. 
These targets are used when building space-saving executables as described in
the "User Guide".
.cS
# A version of the test that can be combined with all the other tests for
# the macro version of the function.
#
mlink.c: $(SOURCES)
	$(CODEMAKER) -m -l -o mlink.c $(SOURCES)

linkexec:: m$(LINKEXEC) $(AUXFILES) ;

m$(LINKEXEC): ../Tests
	$(RM) m$(LINKEXEC)
	$(LN) ../Tests m$(LINKEXEC)
.cE
.H 3 "Targets for libraries"
For the \*(xL, when the
.C section 
argument to the >>TITLE keyword is 
other than
.C XPROTO ,
the contents of the file 
.C mmlib.mc
are then included.
.P
In this release the contents of this file are 
as follows:
.cS
# 
# This part of the makefile checks for the existance of the libraries
# and creates them if necessary.
#

# The xtestlib is made if it doesn't exist
#
$(XTESTLIB):
	cd $(XTESTROOT)/src/lib; $(TET_BUILD_TOOL) install

# The fontlib is made if it doesn't exist
#
$(XTESTFONTLIB):
	cd $(XTESTROOT)/fonts; $(TET_BUILD_TOOL) install

.cE
For the \*(xP, when the
.C section 
argument to the >>TITLE keyword is 
.C XPROTO
the contents of the file 
.C mmxplib.mc
are then included. This file is identical to 
.C mmlib.mc
except for the following additional lines:
.cS
# The X Protocol test library is made if it doesn't exist
#
$(XSTLIB):
	cd $(XTESTROOT)/src/libproto; $(TET_BUILD_TOOL) install
.cE
.H 3 "Targets for cleaning and linting"
The contents of the file 
.C mmmisc.mc
are then included. 
.P
This file includes a 
.C clean
target to remove object files and temporary files, and a 
.C clobber
target which additionally removes all the source files which 
.C mc
can remake.
.P
There is also a
.C LINT
target which enables the C source files to be checked against lint 
libraries specified in the build configuration file.
.cS
#
# Miscellaneous housekeeping functions.
#

# clean up object and junk files.
#
clean:
	$(RM) Test $(OFILES) $(LOFILES) $(LINKOBJ) $(LINKEXEC) core\e
		MTest m$(LINKEXEC) $(MOFILES) CONFIG Makefile.bak $(AUXCLEAN)

# clobber - clean up and remove remakable sources.
#
clobber: clean
	$(RM) MTest.c Test.c mlink.c link.c Makefile

# Lint makerules
#
lint: $(CFILES)
	$(LINT) $(LINTFLAGS) $(CFILES) $(LINTTCM) $(LINTLIBS)

LINT:lint

.cE
.H 3 "Targets for building known good image files"
The contents of the file 
.C mmpgen.mc
are then included. 
.P
These include targets which enable the test set to be built so that 
it generates known good image files.
.P
These are not intended to be used outside the development 
environment at UniSoft.
.cS
#
# Pixel generation makerules for generating the reference
# known good image files.
#

PVOFILES=pvtest.o

pvgen: $(PVOFILES) $(PVLIBS) $(TCM)
	$(CC) $(LDFLAGS) -o $@ $(PVOFILES) $(TCM) \e
	$(PVLIBS) $(SYSLIBS) $(SYSMATHLIB)

pvtest.o: pvtest.c
	cc -c -DGENERATE_PIXMAPS $(CFLAGS) pvtest.c

pvtest.c: Test.c
	$(RM) pvtest.c; \e
	$(LN) Test.c pvtest.c

.cE
.H 3 "Targets for included files"
Rules are included to specify the dependency of the C source files
on any included files.
.P
For example:
.P
.cS
Test.c link.c: $(XTESTLIBDIR)/error/EAll.mc
Test.c link.c: $(XTESTLIBDIR)/error/EGC.mc
Test.c link.c: $(XTESTLIBDIR)/error/EPix.mc
.cE
.H 2 "Formatting assertions"
The command 
.cS
ma -o stclpmsk.a -h -m stclpmsk.m
.cE
produces in the file 
.C stclpmsk.a 
a list of the assertions from the 
assertion sections of the specified dot-m file.
The assertions are output in nroff format. All macros
used in the assertion text can be obtained using the -h and -s 
options as described below.
.P
The remaining parts of this section describe the output format 
in more detail.
.H 3 "Copyright header"
A copyright header is output as an nroff comment block.
This will contain lines showing the SCCS versions of the dot-m file
and any included files.
.H 3 "Macro definitions"
If the -h option was specified, macros that are later used in the 
assertion text will be output from the file 
.C maheader.mc .
.H 3 "Title"
The line
.cS
\&.TH <function> <section> 
.cE
is output, where 
.C <function> 
and 
.C <section>
are obtained from the title section of the dot-m file.
.P
The default macro definition for .TH in 
.C maheader.mc
causes the section and function name to be printed at the top 
of each page.
.H 3 "Assertions"
For each assertion section, the line
.cS
\&.TI <category> \e" <function>-n
.cE
is output, where 
.C <category> 
is obtained from the second argument of the >>ASSERTION keyword and 
.C <function>
is obtained from the title section of the dot-m file,
and 
.C n
is the number of the assertion in the test set.
.P
This is 
followed by the assertion text in which 
.C xname
is converted to 
.C <function> .
For example:
.cS
\&.TI A \e" XSetClipMask-1
A call to 
\&.F XSetClipMask  
sets the
\&.M clip_mask
component of the specified GC to the value of the
\&.A pixmap
argument.
.cE
.P
The other macros used in the assertion text to control font changes
are described in appendix B.
.P
The default macro definition for .TH in 
.C maheader.mc
causes the example assertion to be printed as follows:
.P
.cS
       \fBAssertion XSetClipMask-1(A).\fR
       \fRA call to \fCXSetClipMask\fP sets the \fIclip_mask\fP component of the
       specified GC to the value of the \f(COpixmap\fP argument.
.cE
.SK
.H 1 "Source file structure"
This section describes the C source coding style and conventions which 
which have been used in the development of the revised \*(xT. These 
conventions 
apply to the structure of the code sections of the dot-m files, whose 
overall structure is defined in previous sections of the Programmers
Guide. In some 
cases (particularly in the structure of the \*(xP) 
the style and conventions have been deleloped from the earlier T7 
\*(xT.
.P
You are advised to study the contents of this section before attempting to 
modify or extend the \*(xT. The contents of this section will give you 
guidelines on how to structure the test code so that it is easy 
to follow, gives correct and reliable information when the tests are 
executed, and is written as compactly as possible.
.P
Libraries of common functions have been used and further 
developed in the revised \*(xT 
in order to keep the source code in the test sets as compact as possible.
The rest of this section describes recommendations on how particular library
functions should be used. It does not describe the contents of the
libraries in detail. 
A complete list of library contents is provided in the section entitled
"Source file libraries".
.P
During the development of the \*(xL, a library of support 
functions has been developed. This library includes functions for 
performing common operations required when testing the \*(xW, as well as 
performing common reporting operations. This library includes a small
number of functions developed for the Xlib tests within the T7 \*(xT.
This library is known as the 
"X test suite library" in this document, and the source of the library is in 
the directory 
.C $TET_ROOT/xtest/src/lib .
.P
Calls to any function in this library may be made by any test set in the 
\*(xT. 
.H 2 "Structure of the \*(xL"
This section describes the structure of the code sections of the \*(xL.
.P
The \*(xL are the tests for sections 2 to 10 of the X11R4 Xlib specifications.
They are stored in subdirectories of the directories \fCCH02\fP to \fCCH10\fP
(which are to be found in the directory \fC$TET_ROOT/xtest/tset\fP).
There is a subdirectory for each Xlib function containing a dot-m file
which includes all the test purposes provided for that Xlib function.
The naming scheme which is used for these directories is described in 
appendix B of the "User Guide".
.H 3 "Result code macros"
It is good practice where possible to structure the test so that only 
one test result code 
is assigned before the code section returns or ends.
.P
The significance of the various test result codes that may
be assigned are described more fully in appendix D of the 
"User Guide".
.P
The following macros
may be used to assign the test result code. 
These macros call the function 
\fCtet_result()\fP
which is part of the TET API.
.VL 12 0
.LI PASS
.br
This assigns test result code PASS.
.LI FAIL
.br
This assigns test result code FAIL.
.LI UNRESOLVED
.br
This assigns test result code UNRESOLVED.
.LI UNSUPPORTED
.br
This assigns test result code UNSUPPORTED.
.LI UNTESTED
.br
This assigns test result code UNTESTED.
.LI NOTINUSE
.br
This assigns test result code NOTINUSE.
.LI WARNING
.br
This assigns test result code WARNING.
.LI FIP
.br
This assigns test result code FIP.
.LE
.P
Note that there are two other test result codes which may not be assigned
directly within a test purpose. 
.P
The result code UNINITIATED will be assigned to a test purpose from within the 
TET when the function \fCtet_delete()\fP
has been called in an earlier test purpose
or startup function. This is useful to prevent initiation of later test 
purposes when it is not possible to continue executing test purposes in 
the test set.
.P
The result code NORESULT will be assigned to a test purpose from within the 
TET if the test purpose is initiated but no result code has been
output by the time control returns from the test purpose to the TET.
.P
The FAIL macro also increments a failure counter which is used to prevent 
a result code being assigned in a later call to CHECKPASS (see below).
.H 3 "Result code functions"
There are a series of convenience functions which output a particular 
test result code preceded by a test information message of type REPORT.
(See "Outputting test information messages", below).
.P
In each case the arguments are
exactly like those for \fIprintf(3)\fP.
.P
These are as follows:
.P
.VL 12 0
.LI untested()
.br
This function may be used for an extended assertion 
to output the test result code UNTESTED, preceded by a message.
.LI unsupported()
.br
This function may be used for a conditional assertion 
to output the test result code UNSUPPORTED, preceded by a message.
.LI notinuse()
.br
This function may be used to output the test result code NOTINUSE,
preceded by a message.
.LI delete()
.br
This function may be used to output the test result code UNRESOLVED,
preceded by a message.
.LE
.H 3 "Assigning result codes"
The code should be structured such that a PASS result code is only 
assigned if there is no doubt that the assertion being tested has been 
determined to be positively true on the system being tested. Absence of
failure should not be taken as proof of success. For this reason, there
should if possible be just one place where a PASS result may be assigned, 
whilst there may be many code paths which report other result codes.
.P
The result code FAIL should not be called until the function under
test has been called.
.P
During execution of the test purpose, it may not be possible to setup the 
conditions for the assertion to be conclusively tested.  In this case 
the result code UNRESOLVED should be assigned rather than FAIL.
.P
For example:
.cS
.ft C
>>CODE

	if (setup()) {
		delete("setup() failed; the test could not be completed");
		return;
	}

	ret = XCALL;

	if (ret == 0)
		PASS;
	else
		fAIL;
.ft
.cE
.H 3 "Assigning result codes for extended assertions"
Extended assertions are described in more detail as part of the 
>>ASSERTION keyword.
.P
In some cases partial testing may be done for extended
assertions. In this case, the result code would be FAIL if an error 
is detected, or UNTESTED if no failure is detected but the assertion is still
not fully tested.
.P
For example:
.cS
.ft C
>>CODE

	ret = XCALL;

	if (ret == 0)
		PASS;
	else
		untested("The assertion could not be completely tested");
.ft
.cE
.H 3 "Assigning result codes for conditional assertions"
Conditional assertions are described in more detail as part of the 
>>ASSERTION keyword.
.P
It is usual to determine at the beginning of the test purpose
whether the conditional feature described in the assertion is supported.
.P
For example:
.cS
.ft C
>>CODE

	if (!feature_supported) {
		unsupported("The required feature is not supported");
		return;
	}

	ret = XCALL;

	if (ret == 0)
		PASS;
	else
		FAIL;
.ft
.cE
.H 3 "Assigning result codes for multi-part tests"
It is often the case that the test strategy for an assertion requires 
a number of separate checks to be performed, all of which must pass before
the test purpose can be assigned a PASS result code. 
.P
In order to ensure that all relevant checks have been performed, 
a macro CHECK is provided which increments a pass counter. At the end of the
test, a further macro CHECKPASS checks that the counter has reached the
required value before assigning a PASS result. (The expected value 
of the pass counter is normally a constant, but may be a function of
a loop counter if the test involves calling CHECK in a loop.)
.P
The macro CHECK uses \fCtrace()\fP to print the pass counter and line number
in the TET journal file.
The format of the TET journal file is described further in appendix C
of the 
"User Guide".
.P
CHECKPASS also ensures that the pass counter is not zero, and that the fail 
counter is zero.
.P
For example:
.cS
.ft C
>>CODE

	n_ret = -1;
	ret = XCALL;

	if (ret == 0)
		CHECK;
	else
		FAIL;

	if (n_ret == expected_number)
		CHECK;
	else
		FAIL;

	CHECKPASS(2);
.ft
.cE
.P
In the case of extended assertions, the macro CHECKUNTESTED may be called, 
which is identical to CHECKPASS, except that the final result code 
assigned will be UNTESTED.
.H 3 "Outputting test information messages"
Test information messages are normally output to describe the reason
for any test result codes which are other than PASS, and for other
purposes, as described in this section.
.P
Appendix D of the "User Guide" describes the 
four different categories of test information messages which may appear 
in the TET journal file. This section describes how these messages are
output from the test purpose.
.P
The functions described in this section call the function 
\fCtet_infoline()\fP
which is part of the TET API.
.P
.VL 12 0
.LI REPORT
.br
A test information message with type REPORT is used to report the reason
for any test result code which is other than PASS. A warning message is 
printed by the report writer \fCrpt\fP if a test information message of 
type REPORT is given in a test purpose which produced a test result code PASS.
.P
This is output using the function \fCreport()\fP, which takes arguments 
exactly like those for \fIprintf(3)\fP.
.LI CHECK
.br
A test information message with type CHECK should not be output directly - 
this should only be done via the CHECK macro.
.LI TRACE
.br
A test information message with type TRACE is used to
describe the state of the test being executed.
.P
This is not output to the TET journal file if the execution 
configuration parameter XT_OPTION_NO_TRACE is set to Yes.
.P
This is output using the function \fCtrace()\fP, which takes arguments 
exactly like those for \fIprintf(3)\fP.
.LI DEBUG
.br
A test information message with type DEBUG is a debug message inserted 
during the development of the test.
.P
This is only output to the TET journal file if the value of the 
execution configuration parameter XT_DEBUG is greater than or equal to 
the level of the debug message.
.P
This is output using the function \fCdebug()\fP, which takes arguments 
exactly like those for \fIprintf(3)\fP, except that the \fIprintf(3)\fP
arguments are preceded by a single argument which is the debug level.
The debug level should be between 1 and 3. 
.LE
.P
For example:
.cS
.ft C
>>CODE

	debug(1, "about to call %s", TestName);
	ret = XCALL;

	if (ret == 0)
		trace("%s returned %d", TestName, ret);
		PASS;
	} else {
		report("%s returned %d instead of 0", TestName, ret);
		FAIL;
	}
.ft
.cE
.H 3 "Creating new test purposes"
You can create new test purposes within an existing dot-m file using the 
guidelines in this section.
.P
It is expected that in doing this you will be primarily aiming to produce
new test purposes for a particular Xlib function. You should add the 
new test purpose to the dot-m file containing the test purposes for that 
Xlib function.
.H 4 "Creating new sections in the dot-m file"
You are advised to create an assertion section and strategy section 
at the end of the file, using as a template one of the existing 
sections in the dot-m file.
.P
You should then create a code section commencing with the >>CODE 
keyword. Since there are many different styles of Xlib functions 
which may be tested, there are few additional guidelines that can be given 
beyond those contained in earlier parts of this guide.
.H 4 "Creating test purposes which use pixmap verification"
If you have not done so yet, refer to the section entitled 
"Examining image files" in the "User Guide".
This explains some background to the pixmap verification scheme, and 
in particular how to view image files produced when running the \*(xT.
.P
A number of test purposes supplied in the \*(xT use a scheme known as 
pixmap verification, to compare the image produced by the X server with a 
known good image which is stored in a known good image file. 
.P
All the required known good image files for the test programs in the 
\*(xT (as supplied) have been created in advance.
The known good image files for each test program 
are supplied in the \*(xT in the test set directory in which the dot-m 
file is supplied. They are named a\fCnnn\fP.dat, where \fCnnn\fP is the 
number of the test purpose for which the known good image file was generated.
.P
The known good image files are generated as follows. The \*(xT is compiled
with the additional compilation flag -DGENERATE_PIXMAPS, and linked with 
a replacement Xlib which determines analytically the expected X server display
contents at any point. At the points where pixmap verification is going 
to be performed, the expected image is instead written to a data file, which 
is the known good image file.
.P
It is not possible to generate further known good image files in this way, 
because the replacement Xlib is not part of the \*(xT.
.P
However, it is possible 
to write a server-specific image file 
containing the 
contents of the X server display at points where pixmap verification is 
going to be performed. 
This may be useful for the purposes of validation and regression 
testing against a known server.
This may be done by working through the following stages:
.P
.AL
.LI
Create the test purpose with a call to the macro PIXCHECK at the 
point where you want to validate the image displayed by the X server.
Note that the macro PIXCHECK calls the macros CHECK or FAIL depending on 
whether the image displayed by the X server matches that in the image file.
The code invoked by the macro PIXCHECK(display, drawable) is as follows:
.P
.cS
	if (verifyimage(display, drawable, (struct area *)0))
		CHECK;
	else
		FAIL;
.cE
.P
The function 
.C verifyimage()
is described in more detail in the section entitled 
"Source file libraries".
.P
.LI
Build and execute the test without using the
.SM TCC ,
(refer to the "User Guide")
and check that the newly created 
test purpose gives result code UNRESOLVED due to 
the absence of a known good image file as follows:
.cS
pmake 
pt
prp
.cE
.LI
Rerun the test,
saving the image produced by the X server
as follows:
.cS
pt -v XT_SAVE_SERVER_IMAGE=Yes
.cE
.LI
This should create a file named a\fCnnn\fP.sav, where \fCnnn\fP is the name 
of the newly created test purpose. This is a server-specific image file.
Rename this file to the name used 
for known good image files as follows:
.cS
mv a\fCnnn\fP.sav a\fCnnn\fP.dat
.cE
.LI
Check that the process has worked by executing the test without using the 
.SM TCC ,
and enabling pixmap verification against the server-specific image file
as follows:
.cS
pt
prp
.cE
.P
The newly created test purpose should give a result code of PASS.
.LE
.P
It is particularly important that new test purposes are added at the end 
of the file if an earlier test purpose calls the macro \fCPIXCHECK\fP. 
This is because inserting a test purpose before another test purpose 
will cause the later test purpose to be renumbered. As well as causing 
unnecessary confusion in other ways, this will cause the later test purpose
to now look for the wrong known good image file.
.H 2 "Structure of the \*(xP"
This section describes the structure of the code sections of the \*(xP.
.P
The \*(xP are the touch tests for the X Protocol (version 11).
They are stored in subdirectories of the directory \fCXPROTO\fP
(which is to be found in the directory \fC$TET_ROOT/xtest/tset\fP).
There is a subdirectory for each X Protocol request containing a dot-m file
which includes all the test purposes provided for that X Protocol request.
The naming scheme which is used for these directories is described in 
appendix B of the "User Guide".
.P
During the development of the \*(xP, extensive use has also been made of 
a library of support functions developed in the earlier T7 \*(xT. This 
library is known as the "X\ Protocol library" in this document, 
and the source of the library is in
the directory
.C $TET_ROOT/xtest/src/libproto .
.P
Calls to any function in this library may be made by any of the \*(xP in the 
\*(xT. 
.H 3 "Structure of the code sections"
In the T7 release of the \*(xT, each of the \*(xP consisted of a 
\fCmain()\fP function which called library functions to 
send an X Protocol request to the X server, and 
checked for the correct response (reply, error or nothing).
.P
In the revised \*(xT, the test code originally in the 
\fCmain()\fP function has been moved to a function called \fCtester()\fP
which is located in an >>EXTERN section in each dot-m file,
so that it can be called from each test purpose as described below. 
The test function \fCtester()\fP is 
in turn called from a library function \fCtestfunc()\fP.
.P
For example:
.P
.cS
.ft C
>>CODE

	test_type = GOOD;

	/* Call a library function to exercise the test code */
	testfunc(tester);
.ft
.cE
.P
By default, 
the library function \fCtestfunc()\fP calls \fCtester()\fP for each byte 
orientation. The test function \fCtester()\fP is called in a sub-process 
via the TET API function \fCtet_fork()\fP, and returns the exit status of 
the test process to \fCtestfunc()\fP. 
.P
If required, the execution configuration parameter XT_DEBUG_BYTE_SEX may be
set to NATIVE, REVERSE, MSB or LSB to call \fCtester()\fP just once
with the required byte orientation.
.P
Each client has a test type, which is initialised when the client is created.
The test type determines whether X Protocol requests sent by the client 
are to be good requests or invalid requests (expecting an X Protocol error 
to be returned). The test type may be modified during the lifetime of 
the client by invoking the macro \fCSet_Test_Type()\fP.
In many tests, this is done by setting the test type to one of the following
values before calling \fCSend_Req()\fP, then setting it to GOOD immediately
afterwards for subsequent library calls:
.P
.VL 12 0
.LI GOOD
.br
The request sent will be a known good X Protocol request (unless
otherwise modified in \fCtester()\fP before calling \fCSend_Req()\fP).
.LI BAD_LENGTH
.br
The request sent will have length field less than the minimum needed 
to contain the request.
.LI JUST_TOO_LONG
.br
The request sent will have length field greater than the minimum needed 
to contain the request (and, for requests where the length is used to
determine the number of fields in the request, the length is also 
not the minimum length plus a multiple of the field size).
.LI TOO_LONG
.br
The request sent will have a length field which is greater than that 
accepted by the X server under test.
.LI BAD_IDCHOICE1
.br
The request sent will have a resource ID that is already in use 
(it is the responsibility of the function \fCtester()\fP to ensure the 
resource ID is in use before calling \fCSend_Req()\fP).
.LI BAD_IDCHOICE2
.br
The request sent will have a resource ID that is out of range 
(it is the responsibility of the function \fCtester()\fP to ensure the 
resource ID is out of range before calling \fCSend_Req()\fP).
.LI BAD_VALUE
.br
The request sent will have an invalid mask bit set
(it is the responsibility of the function \fCtester()\fP to ensure the 
mask field contains an invalid bit before calling \fCSend_Req()\fP).
.LI OPEN_DISPLAY
.br
A special value used only in the test for OpenDisplay 
for testing the connection setup protocol.
.LI SETUP
.br
The initial test type of a client, which will cause errors during 
test setup to produce result code
.SM UNRESOLVED 
rather than 
.SM FAIL .
.LE
.H 3 "Outputting test information and result code"
Errors may be detected and reported both within the test function 
\fCtester()\fP and within library functions. When an error is detected,
the function \fCLog_Err()\fP should be 
called. This increments an error count and uses \fCreport()\fP
to output a test information message of type REPORT to the TET journal file.
.P
If no error is detected, the function Log_Trace() may be called to 
record that the expected response was received. This uses \fCtrace()\fP
to output a test information message of type TRACE to the TET journal file.
.P
You can also use the function Log_Debug() to 
output more detailed test information such as the contents of request, 
reply and event structures. This uses \fCdebug()\fP
to output a test information message of type DEBUG at level one
to the TET journal file.
.P
The function \fCExit()\fP should be called at any point after an error has 
occurred, which will assign a test result code FAIL and print the error 
count (or UNRESOLVED if the error counter is zero). The exit status will be
EXIT_FAIL in this case.
.P
If \fCtester()\fP performs all checks and the results are correct, 
the function \fCExit_OK()\fP should be called. The exit status will be 
EXIT_SUCCESS in this case.
.P
A result code of PASS is only assigned to a test purpose in the library
function \fCtestfunc()\fP if all calls to 
\fCtester()\fP give exit status EXIT_SUCCESS. It should not be assigned 
anywhere else.
.H 3 "Creating new test purposes"
You can create new test purposes within an existing dot-m file using the 
guidelines in this section.
.P
It is expected that in doing this you will be primarily aiming to produce
new test purposes for a particular X Protocol request. You should add the 
new test purpose to the dot-m file containing the test purposes for that 
X Protocol request. 
.H 4 "Creating new sections in the dot-m file"
You are advised to create an assertion section and strategy section 
at the end of the file, using as a template one of the existing 
sections in the dot-m file.
.P
You should then create a code section which passes a function \fCmy_test()\fP
you are about to create to the library function \fCtestfunc()\fP.
.P
For example:
.P
.cS
.ft C
>>CODE

	test_type = GOOD;

	/* Call a library function to exercise the test code */
	testfunc(my_test);
.ft
.cE
.H 4 "Creating a new test function"
You should create a function \fCmy_test()\fP in an >>EXTERN section 
in the dot-m file using the guidelines in this section.
.P
A client is a connection to the X server under test.
Each X Protocol request is sent from a particular client.
You can create a client numbered \fCclient\fP
using \fCCreate_Client(client)\fP. 
Normally a single client is created, but it is 
possible to create more than one client. This will be necessary when 
testing the effect on the server of multiple clients.
.P
The client data structure \fCXst_clients\fP is used to store the 
information about each client you have created. This includes resource ID's
and
a display structure which is filled in when the client is created.
The client data structure is documented in more detail in the header file in 
which it is defined (\fC$TET_ROOT/xtest/include/Xstlib.h\fP).
.P
Next you will need to create a request structure. The function 
\fCMake_Req(client, req_type)\fP should
be called to create a request of a specified type \fCreq_type\fP for 
a specified client \fCclient\fP
and return a pointer to the request structure. The request structure will
be filled in with defaults which may be suitable for the test purpose you are 
creating. The file 
.C MakeReq.c
in the X Protocol library fills in the default values.
.P
Should you want to change the defaults you can do this at any point between 
creating the request structure and sending it to the X server. It may
be modified by accessing the structure members. The format of the request 
structures is exactly as defined in your X Protocol header file (normally
\fC/usr/include/X11/Xproto.h\fP). You can alter value list items using the 
following functions:
.P
.VL 12 0
.LI \fCAdd_Masked_Value()\fP
.br
.LI \fCDel_Masked_Value()\fP
.br
.LI \fCClear_Masked_Value()\fP
.br
.LI \fCAdd_Counted_Value()\fP
.br
.LI \fCClear_Counted_Value()\fP
.br
.LI \fCAdd_Counted_Bytes()\fP
.br
.LE
.P
When you have the request structure you wish to pass to the X server, 
call the function \fCSend_Req(client)\fP. 
This sends the request \fCreq\fP from client \fCclient\fP to the X server, 
and handles byte swapping and request packing as necessary.
If you wish the X Protocol library to further modify the request 
structure to send an invalid protocol request, set the test type of the 
client before calling \fCSend_Req(client)\fP using the macro 
\fCSet_Test_Type(client)\fP. The possible test types are listed in the 
earlier section entitled "Structure of the code sections".
.P
To check that the X server has reacted correctly to the request sent, you 
will need to call the function \fCExpect()\fP. For convenience, a number 
of macros and functions have been created to call \fCExpect()\fP 
depending on the outcome you are expecting. These are as follows:
.P
.VL 12 0
.LI "\fCExpect_Event(client, event_type)\fP"
.br
This expects an event of type \fCevent_type\fP to be sent back from the 
X server to client \fCclient\fP.
.LI "\fCExpect_Reply(client, req_type)\fP"
.br
This expects a reply to the X Protocol request of type \fCreq_type\fP 
to be sent back from the X server to client \fCclient\fP.
.LI "\fCExpect_Error(client, error_type)\fP"
.br
This expects an error of type \fCerror_type\fP 
to be sent back from the X server to client \fCclient\fP.
.LI "\fCExpect_BadLength(client)\fP"
.br
This expects a BadLength error
to be sent back from the X server to client \fCclient\fP.
.LI "\fCExpect_BadIDChoice(client)\fP"
.br
This expects a BadIDChoice error
to be sent back from the X server to client \fCclient\fP.
.LI "\fCExpect_Nothing(client)\fP"
.br
This expects neither an error, event or reply
to be sent back from the X server to client \fCclient\fP.
.LE
.P
The \fCExpect()\fP function will check that the response from the X server
is of the correct type and has the correct length. It will be byte swapped 
and unpacked as necessary into an event or reply structure, to which a 
pointer will be returned.
.P
It is recommended that one of these functions be called immediately after 
sending an X Protocol request to the X server.
This causes any pending response from the X server to be flushed out, 
and checked. This makes it easier to locate wrong responses from the X server.
This is effectively designing the test to run synchronously.
.P
Once an error, event or reply has been returned, it can be examined directly.
.P
Since the structures allocated for 
requests, replies and events are allocated dynamically, it is wise to free
the structure after use. this may be done using the functions
\fCFree_Req()\fP, 
\fCFree_Reply()\fP and 
\fCFree_Event()\fP.
.P
When the outcome of sending the X Protocol request has been assessed, 
you will want to either report an error or output a trace 
message indicating that the expected response was received.
Refer to the earlier section entitled
"Outputting test information and result code".
.P
You should end the test purpose if every part of the test purpose has 
succeeded by calling \fCExit_OK()\fP. This should only be done once, 
because it is the means of passing back to the library function 
\fCtestfunc()\fP the fact that the test purpose passed. If at 
an earlier part of the test purpose an error occurs and it is desired to 
exit, call \fCExit()\fP.
.H 4 "Creating test purposes to test X Protocol extensions"
The nature of the extension mechanism in X makes it difficult to
just add support in the switch statements throughout the X Protocol 
library to support protocol extensions. 
.P
The reason for this is that you do not know the value of the event 
types and reply types until you have queried the X server.
.P
For this reason, you are recommended to review the scope of the work 
that would be required in modifying the supplied X Protocol library
before attempting to test X Protocol extensions.
You can use the supplied X Protocol library as a framework, 
and develop new versions of routines which handle events and replies. 
.SK
.H 1 "Source file libraries"
This sections lists the contents of the principal libraries of 
source files used by many tests in the \*(xT.
.H 2 "The \*(xT library"
A library of common subroutines for the \*(xT
has source in \fC$TET_ROOT/xtest/src/lib\fP.
This is built automatically when building tests in the \*(xT.
Should it be required to build it separately for any reason
run the command.
.cS
cd $TET_ROOT/xtest/src/lib
pmake install
.cE
The list of source files in this library,
with a brief description of the contents of each file, is as follows:
.VL 12 0
.LI XTestExt.c
.br
If 
.SM XTESTEXTENSION
is defined,
this file contains routines to access the 
.SM XTEST
extension in order to simulate device events and obtain information on the 
cursor attributes of windows.
.P
If
.SM XTESTEXTENSION
is not defined, dummy routines are used instead. 
.P
If
.SM XTESTEXTENSION
is not defined, client-side functions
previously in file XTestLib.c (now available in the 
.SM XTEST
extension library)
are still included.
These are XTestDiscard() (to discard current request in request 
buffer) and XTestSetGContextOfGC() and XTestSetVisualIDOfVisual()
(to set values in opaque Xlib data structures). These functions require
access to data structures now in the internal Xlib header file
.C Xlibint.h .
.LI badcmap.c
.br
Create an invalid colourmap ID by creating a readonly colourmap
of the default visual type.
.LI badfont.c
.br
Return a bad font ID by loading a font and then unloading it.
.LI badgc.c
.br
Return a bad GC id on display disp by creating a GC and
invalidating it using the 
.SM XTEST
extension library function
XTestSetGContextOfGC.
.LI badpixm.c
.br
Return a bad pixmap id on display disp by creating
a pixmap and freeing it.
.LI badvis.c
.br
Make a visual bad by using the 
.SM XTEST
extension library function
XTestSetVisualIDOfVisual.
.LI badwin.c
.br
Return a bad window id on display disp by creating
a window and destroying it.
.LI bitcount.c
.br
Handle bits in words.
.LI block.c
.br
Check whether process blocks when testing event handling functions.
.LI buildtree.c
.br
Build a tree of windows specified by a list which determines position,
size and parentage of each window.
.LI checkarea.c
.br
Check pixels inside and/or outside an area of a drawable are set to given values.
.LI checkevent.c
.br
Check two arbitrary events to see if they match, report
an error if they don't.
.LI checkfont.c
.br
Check returned font characteristics, properties, text extents and widths 
against those expected in the supplied test fonts.
.LI checkgc.c
.br
Check GC components against expected values.
.LI checkimg.c
.br
Check pixels inside and/or outside an area of an image are set to given values.
.LI checkpixel.c
.br
Check specified pixels of a drawable are set to given values.
.LI checktile.c
.br
Check that an area of a drawable is filled with a specified tile.
.LI config.c
.br
Initialise the config structure by getting all the execution
parameters.
.LI crechild.c
.br
Create a mapped child window for a parent window, and wait for the 
child window to become viewable.
.LI cursor.c
.br
Routines for accessing cursor information. This includes convenience functions 
for checking the cursor defined for a given window.
These routines call those in XTestExt.c to use the 
.SM XTEST 
extension to access the cursor information.
.LI delete.c
.br
Set the test result code for the current test purpose to 
.SM UNRESOLVED .
.LI devcntl.c
.br
Routines for input device control. This includes convenience functions 
for pressing keys and buttons and remembering those pressed. 
These routines call those in XTestExt.c to use the 
.SM XTEST 
extension to simulate the required device events.
.LI dset.c
.br
Set every pixel in a drawable to a specified value.
.LI dumpimage.c
.br
Dump the contents of an image to a file.
.LI environ.c
.br
Contains a test suite specific version of putenv() (which may not be available
on POSIX.1 systems). This is required to set up the environment 
before some calls to tet_exec().
.LI err.c
.br
Test error handler (installed when calling the function under test).
Unexpected error handler (installed at all other times).
I/O error handler (installed at all times).
Obtain the error code and resource ID saved by the test error handler.
.LI events.c
.br
Handle the serial fields of incoming requests.
.LI ex_startup.c
.br
Generic startup routines required before executing the first test purpose
and after executing the last test purpose.
The routines 
.C exec_startup()
and
.C exec_cleanup()
in this file should be called at the start and end of the 
.C main()
function of each program executed via the TET function
.C tet_exec() .
.LI exposechk.c
.br
Check that either enough expose events were received to
restore the window, or that the window has been restored from backing
store.
.LI extenavail.c
.br
If
.SM XTESTEXTENSION
is defined, the function IsExtTestAvailable() returns True if the 
server extension
.SM XTEST
is available, otherwise it returns False.
.P
If
.SM XTESTEXTENSION
is not defined, the function IsExtTestAvailable() always returns False.
.LI gcflush.c
.br
Flush the GC cache.
.LI gcinclude.c
.br
Functions which are called from the code included to test the 
correctness of use of GC components by the drawing functions.
.P
The only function included at present is 
.C setfuncpixel() ,
which finds the first pixel set in a drawable (this will vary depending on the 
drawing function).
.LI getevent.c
.br
Check if there are events on the queue and if so return the
first one.
.LI getsize.c
.br
Get the size of a drawable.  Just uses XGetGeometry but avoids all
the other information that you get with that.
.LI gettime.c
.br
Get the current server time. Use a property attached to the root window
of the display called 
.SM XT_TIMESTAMP 
and replace it with 42 (32-bits).
The PropertyNotify event that is generated supplies the time stamp returned.
.LI iponlywin.c
.br
Create an input only window.
.LI issuppvis.c
.br
The function 
.C issuppvis() 
takes a visual class as argument and returns true
if such a class is supported by the server under test.
This function uses the XGetVisualInfo() function rather than
the user-supplied 
.SM XT_VISUAL_CLASSES 
parameter.
.P
The function
.C visualsupported()
takes a mask indicating a set of visuals, and returns
a mask indicating the subset that is supported.
If the mask is 0L then the mask shows all supported
visuals.
.P
The function 
.C resetsupvis()
takes a mask indicating a set of visuals. Subsequent calls to 
.C nextsupvis()
will return the next supported visual specified in the mask and increment a 
counter.
The function 
.C nsupvis()
returns this counter.
.LI linkstart.c
.br
Define global variables used by the TET which are required when 
linking test programs to produce a space-saving executable.
.P
When the space-saving executable is executed,
the TET 
initialisation code in the library function 
.C linkstart.c
determines which test set is required. This is done by matching \fCargv[0]\fP
with the \fCname\fP elements in the array of 
.C linkinfo
structures. The corresponding test functions specified by the 
\fCtestlist\fP
element of the
.C linkinfo
structure are then executed.
.LI lookupname.c
.br
Convert symbolic values from \*(xW header files to appropriate names.
.LI makecolmap.c
.br
Make a colourmap for the screen associated with the default
root window.
.LI makecur.c
.br
Create a cursor that can be used within the test suite.
The cursor is created using XCreateFontCursor.  The shape
chosen can be controlled through the configuration variable
.SM XT_FONTCURSOR_GOOD .
.LI makegc.c
.br
Make a GC suitable for use with the given drawable.
.LI makeimg.c
.br
Creates a general purpose image that can be used within the
test suite. The image is cleared to W_BG.
.LI makepixm.c
.br
Creates a general purpose pixmap that can be used within the
test suite. The pixmap is cleared to W_BG.
.LI makeregion.c
.br
Creates a general purpose region that can be used within the
test suite.
.LI makewin.c
.br
Creates a general purpose windows that can be used within the
test suite.
.LI makewin2.c
.br
Creates windows corresponding to a particular area.
.LI maxsize.c
.br
Obtain the number of cells in a colourmap.
.LI nextvclass.c
.br
Functions to cycle through all the visual classes that are supposed to
supported by the display/screen that is being tested.
Note that these functions are only used in the tests for 
XMatchVisualInfo and XGetVisualInfo.
.P
The function
.C initvclass()
initialises the visual class table.  The visual classes that are
supported are supplied by the test suite user in the
variable 
.SM XT_VISUAL_CLASSES , 
together with the depths at which they are supported.
.P
The function
.C resetvclass()
resets the visual class table. Subsequent calls to 
.C nextvclass()
obtain the next visual class and depth.
The function 
.C nvclass()
returns the size of the visual class table.
.LI nextvinf.c
.br
Functions to cycle through all the visuals that are supported
on the screen under test.
These functions use the XGetVisualInfo() function rather than
the user-supplied 
.SM XT_VISUAL_CLASSES 
parameter. If the parameter 
.SM XT_DEBUG_VISUAL_IDS 
is set to a non-empty string, 
only the visuals ID's in the string are used.
.P
The function
.C resetvinf()
obtains a list of all visuals supported for a particular screen.
Subsequent calls to 
.C nextvinf()
obtain the next visual.
The function 
.C nvinf()
returns the number of visuals.
.LI nondpth1pix.c
.br
Obtain a pixmap of depth other than 1 if such a pixmap is supported.
.LI notmember.c
.br
Returns a list of numbers that are not members of given list.
(This is used to test assertions of the form "When an argument 
is other than X or Y, then a BadValue error occurs".
.LI opendisp.c
.br
Open a connection to the server under test.
.LI openfonts.c
.br
Open the xtest fonts, and place their ID's in the fonts array.
.LI pattern.c
.br
Draw a pattern consisting of vertical bands on the specified drawable.
.LI pfcount.c
.br
Functions which may take arguments which are set to the
pass and fail counters in test set code created by 
.C mc .
Calls to the 
.C pfcount
functions are inserted in order to use the counters at least once, 
and so prevent 
.C lint
reporting unwanted errors.
.LI pointer.c
.br
Routines to move the pointer, and determine if the pointer has been moved.
.LI regid.c
.br
Routines are provided to register resources created during a test purpose.
Wherever possible, library functions register resources, and test purposes
may do so directly if desired.
Registered resources are then destroyed at the end of the test purpose.
.LI report.c
.br
Reporting functions, which output test information messages to the TET 
journal file. 
These all use the TET reporting function
.C tet_infoline() .
.LI rpt.c
.br
Reporting functions, which output test information messages to the TET 
journal file, and additionally assign a test result code.
These all use the TET reporting function
.C tet_infoline() .
.LI savimage.c
.br
The function 
.C savimage()
returns a pointer to a saved image on a drawable using 
.C XGetImage .
.P
The function 
.C compsavimage()
checks that the image currently on the drawable matches a saved image.
.P
The function
.C diffsavimage()
checks that the image currently on the drawable differs from a saved image.
.P
These functions are used where the precise pixels drawn cannot be 
determined in advance, but the test result may still be infered by 
image comparisons.
.LI setline.c
.br
Convenience functions to set line width, cap style, 
line style and join style in a GC, using 
.C XChangeGC() .
.LI settimeout.c
.br
The function
.C settimeout()
sets a timeout which causes the process to exit after a timeout. This 
should be done only in a child process of a test purpose created by 
.C tet_fork() .
.P
The function
.C cleartimeout()
clears a previously set timeout.
.LI stackorder.c
.br
The function 
.C stackorder()
uses 
.C XQueryTree()
to determine the position of a window in the stacking order.
.LI startcall.c
.br
The function 
.C startcall()
checks for any outstanding unexpected X protocol errors, 
which might have been generated, for example, during the setup 
part of the test. 
A call to 
.C XSync()
is made to achieve this.
.P
The function 
.C startcall()
installs a test error handler in place of the unexpected X protocol 
error handler.
.P
The function 
.C endcall()
checks for any X protocol errors caused by the 
function under test.
A call to 
.C XSync()
is made to achieve this.
.P
The function 
.C endcall()
installs the unexpected X protocol 
error handler.
.LI startup.c
.br
Generic startup routines called by TET before executing the first test purpose
and after executing the last test purpose.
.LI tpstartup.c
.br
Generic startup routines called by TET before executing each test purpose
and after executing each test purpose.
.LI verimage.c
.br
The function 
.C verifyimage()
uses 
.C XGetImage()
to obtain the contents of the specified drawable.
This is then compared with the contents of a "known good image file".
If there is a discrepancy,
the image produced by the server is dumped to a file using 
.C dumpimage()
together with the known good image. 
The image produced by the server and the known good image may be 
examined as described in the section in the 
"User Guide"
entitled
"Examining image files".
.P
If the execution configuration parameter 
.SM XT_DEBUG_NO_PIXCHECK
is set to 
.C Yes ,
the image checking is skipped in 
.C verifyimage() .
.P
If the execution configuration parameter
.SM XT_SAVE_SERVER_IMAGE
is set to 
.C Yes ,
the image produced by the server is dumped to a file using 
.C dumpimage()
(regardless of whether it matches the "known good image file").
.P
For more background on pixmap verification see the earlier section
entitled "Creating test purposes which use pixmap verification".
.LI winh.c
.br
Build a tree of windows to test event generation, propogation and delivery.
.LI xthost.c
.br
Specifies operating system dependent data used by the access control list
functions.
This includes arrays of
.C XHostAddress 
structures.
These should be checked and if necessary edited refering to the 
section in the 
"User Guide"
entitled
"System dependent source files".
.LI xtestlib.h
.br
This file contains definitions which are common to many of the 
source files in the \*(xT library, and it is included in those 
source files.
.LI xtlibproto.h
.br
This file contains declarations and (if required by an
.SM ANSI 
Standard-C compiler) 
function prototypes for all the functions in the source files 
in the \*(xT library.
.LE
.H 2 "The X Protocol library"
A library of common subroutines for the \*(xP in the \*(xT
has source in \fC$TET_ROOT/xtest/src/libproto\fP.
This is built automatically when building tests in the \*(xT.
Should it be required to build it separately for any reason
run the command.
.cS
cd $TET_ROOT/xtest/src/libproto
pmake install
.cE
The list of source files in this library,
with a brief description of the contents of each file, is as follows:
.VL 12 0
.LI ClientMng.c
.br
Having established a client connection to the X server using the functions
in ConnectMng.c, allocate a client data
structure and fill in its display structure.
.LI ConnectMng.c
.br
Establish a client connection to the X server.
.LI DataMove.c
.br
Convert individual fields into format for sending to the X server.
.LI DfltVals.c
.br
Obtain reasonable default values for contents of request structures.
.LI Expect.c
.br
Check for the expected response (error, event, reply, or nothing)
from the X server.
.LI JustALink.c
.br
This file is a link to one of the files XlibXtst.c, XlibOpaque.c, 
or XlibNoXtst.c. The link is created when the X Protocol library 
is built, and the file used depends on the configuration parameter
.SM XP_OPEN_DIS .
.LI Log.c
.br
Log test results.
.LI MakeReq.c
.br
Construct a request structure using the functions in DfltVals.c,
which has reasonable default values so that it may be immediately sent 
to the X server using the functions in SendReq.c.
.LI RcvErr.c
.br
.LI RcvEvt.c
.br
.LI RcvRep.c
.br
Unpack the response from the server into a structure (RcvErr.c for errors,
RcvEvt.c for events, RcvRep.c for replies; these all use DataMove.c to do 
the unpacking).
.LI ResMng.c
.br
Create a resource (e.g. atom, window) and store its resource ID
in the client data structure.
.LI SendEvt.c
.br
Pack an event structure into a request structure (only used by SendEvent
protocol request).
.LI SendReq.c
.br
Pack a request structure in correct format using the functions in DataMove.c
and send to the X server.
.LI SendSup.c
.br
Support routines for packing request structure.
.LI ShowErr.c
.br
.LI ShowEvt.c
.br
.LI ShowRep.c
.br
.LI ShowReq.c
.br
.LI ShowSup.c
.br
Display contents of structures in nice human-readable form (ShowErr.c for
errors, ShowEvt.c for events, ShowRep.c for replies, and ShowReq.c for
requests, all of which call ShowSup.c support routines).
.LI TestMng.c
.br
Manage the setup and closedown of the tests. This file includes definitions and 
initialisation of global variables (including TET configuration 
variables) and assigning test result codes.
.LI TestSup.c
.br
Support routines for handling mapping state and event masks of windows.
.LI Timer.c
.br
Set up a timer that will execute a certain routine on completion.
.LI Utils.c
.br
Utilities for isolating operating system dependencies.
.LI Validate.c
.br
Routines to check whether the server under test supports the feature 
being tested (eg. writable colour cells).
.LI ValListMng.c
.br
Modify the value lists at the ends of request structures.
.LI XlibNoXtst.c
.br
This file contains functions which emulate the post R5 enhanced connection 
setup scheme. A connection can be established in client native or byte-swapped
orientations, and (when testing XOpenDisplay) both valid and invalid 
byte orderings may be sent to the X server.
The connection is made using operating system specific
procedures which were developed in 4.2BSD environment, and may need 
modifications to work on other systems.
.LI XlibOpaque.c
.br
This file contains portable functions to handle connection setup where 
the Xlib implementation does not support the post R5 enhanced connection 
setup scheme.
The Xlib functions XOpenDisplay and ConnectionNumber are called here to 
obtain a connection using the client native byte orientation,
and subsequent X Protocol requests are made using this connection.
.LI XlibXtst.c
.br
This file contains portable functions to handle connection setup where 
the Xlib implementation supports the post R5 enhanced connection setup scheme.
The enhancement involves using additional parameters to the 
Xlib function _XConnectDisplay() which allow a byte swapped connection to 
be established. Details of operating system specific connection setup 
procedures including networking are thus not needed in the X Protocol library.
.LI XstIO.c
.br
Routines to handle protocol packet transmission and reception
including fatal I/O errors.
.LI delete.c
.br
Set the test result code for the current test purpose to UNRESOLVED.
.LI linkstart.c
.br
Define global variables used by the TET which are required when 
linking test programs to produce a space-saving executable.
.P
When the space-saving executable is executed,
the TET 
initialisation code in the library function 
.C linkstart.c
determines which test set is required. This is done by matching \fCargv[0]\fP
with the \fCname\fP elements in the array of 
.C linkinfo
structures. The corresponding test functions specified by the 
\fCtestlist\fP
element of the
.C linkinfo
structure are then executed.
.LI startup.c
.br
Generic startup routines called by TET before executing the first test purpose.
.LI tpstartup.c
.br
Generic startup routines called by TET before executing each test purpose.
.LI DataMove.h
.br
This file contains the macros for byte swapping and word alignment.
.LI XstlibInt.h
.br
This file contains definitions which are common to many of the 
source files in the X Protocol library, and it is included in those 
source files.
.LI XstosInt.h
.br
This file contains definitions related to operating system functions
which are common to many of the 
source files in the X Protocol library, and it is included in those 
source files.
.LE
.H 2 "The X test fonts library"
A library of common subroutines defining the 
characteristics of the test fonts for the \*(xT
has source in \fC$TET_ROOT/xtest/fonts\fP.
This is built automatically when building tests in the \*(xT.
Should it be required to build it separately for any reason
run the command.
.cS
cd $TET_ROOT/xtest/fonts
pmake install
.cE
The source files 
.C xtfont0.c
to 
.C xtfont6.c
contain definitions of
.C XFontStruct
structures named
.C xtfont0
to
.C xtfont6
which define the characteristics of the test fonts used by many of the 
text drawing functions.
.SK
.H 1 "Appendix A - reason codes for extended assertions"
The reason code
is a number between 1 and 6 (currently) and is used if
and only if the category is B or D. This number corresponds to a reason
from the following table which is coded into 
.C mc .
.P
The text of the 
reason will be printed with a result code UNTESTED if there 
is no >>CODE.
.P
1 - "There is no known portable test method for this assertion",
.br
2 - "The statement in the X11 specification is not specific enough to write a test",
.br
3 - "There is no known reliable test method for this assertion",
.br
4 - "Testing the assertion would require setup procedures that involve an unreasonable amount of effort by the user of the test suite.",
.br
5 - "Testing the assertion would require an unreasonable amount of time or resources on most systems",
.br
6 - "Creating a test would require an unreasonable amount of test development time."
.SK
.H 1 "Appendix B - commands for fonts and symbols in assertions"
In the text of assertions there should be no in-line nroff font changes.
This is because the font names may need to be changed on some systems.
.P
As an alternative, a number of macros have been defined which are understood
by the utilities developed during stage two of the project. The definition
of these macros uses appropriate fonts to correspond closely with those
used by the \*(xW documentation.
.AL
.LI
Arguments to a function should be written:
.P
\&.A window
.LI
Function names should be written:
.P
\&.F XAllocColorCells
.P
(When the special symbol xname is used it can be left as it is, so
the .F form only needs using when refering to some other function. We
have avoided cross references to other functions where possible).
.LI
Structure members should be written:
.P
\&.M override_redirect 
.P
.LI
Symbols should be written:
.P
\&.S InputOutput
.P
This is used for everything that is in the courier font in the \*(xW
documentation and
which is not a function name or structure member.
This includes the #define constants in the headers and typedef'ed names.
.P
.cS
Eg.
	BadColor
	IsViewable
	DirectColor
	Visual
	Display
	MotionNotifyEvent
.cE
.LE
.P
Punctuation separated by white space from the argument will be 
in the original font, as in mm.
.P
\&.A InputOutput ,
.br
\&.A InputOnly .
.P
.DL
.LI
There is a .SM macro, as in mm.  Any word that
is uppercase only should use it to obtain a reduced point size.
.P
\&.SM DEBUG
.br
\&.SM MIT
.LE
.SK
.H 1 "Appendix C - Included error assertions"
The .ER keyword is described in the section entitled 
"Included errors - .ER".
.P
This appendix gives the names the files which are included when this 
keyword is used with the supported arguments,
and shows the text of the assertions in those files.
.P
All the files from which included tests are stored are located 
in the directory \fC$TET_ROOT/xtest/lib/error\fP.
.P
The names of the files which are included, and the text of the assertion
contained in the file, are specified in the following list:
.VL 8 0
.LI "Access grab"
.br
File included: EAcc1.mc
.br
Assertion text:
.eS
When an attempt to grab  a  key/button  combination  already
grabbed  by  another  client is made, then a BadAccess error
occurs.
.eE
.LI "Access colormap-free"
.br
File included: EAcc2.mc
.br
Assertion text:
.eS
When an attempt to free a colormap entry  not  allocated  by
the client is made, then a BadAccess error occurs.
.eE
.LI "Access colormap-store"
.br
File included: EAcc3.mc
.br
Assertion text:
.eS
When an attempt to store into a read-only or an  unallocated
colormap entry is made, then a BadAccess error occurs.
.eE
.LI "Access acl"
.br
File included: EAcc4.mc
.br
Assertion text:
.eS
When an attempt is made to modify the  access  control  list
from  a  client that is not authorised in a server-dependent
way to do so, then a BadAccess error occurs.
.eE
.LI "Access select"
.br
File included: EAcc5.mc
.br
Assertion text:
.eS
When an attempt to select an event type is  made,  which  at
most  one  client can select, and another client has already
selected it then a BadAccess error occurs.
.eE
.LI "Alloc"
.br
File included: EAll.mc
.br
Assertion text:
.eS
When the server fails to allocate a required resource,  then
a BadAlloc error occurs.
.eE
.LI "Atom [ARG1] [ARG2] ..."
.br
File included: EAto.mc
.br
Assertion text:
.eS
When an atom argument does not name a valid  Atom [, ARG1]  [or
ARG2], then a BadAtom error occurs.
.eE
.LI "Color"
.br
File included: ECol.mc
.br
Assertion text:
.eS
When a colourmap argument does not name a  valid  colourmap,
then a BadColor error occurs.
.eE
.LI "Cursor [ARG1] [ARG2] ..."
.br
File included: ECur.mc
.br
Assertion text:
.eS
When a cursor argument does not name a valid Cursor [, ARG1] [or
ARG2], then a BadCursor error occurs.
.eE
.LI "Drawable [ARG1] [ARG2] ..."
.br
File included: EDra.mc
.br
Assertion text:
.eS
When a drawable argument does not  name  a  valid  Drawable,
[ARG1] [or ARG2], then a BadDrawable error occurs.
.eE
.LI "Font bad-font"
.br
File included: EFon1.mc
.br
Assertion text:
.eS
When a font argument does not name  a  valid  font,  then  a
BadFont error occurs.
.eE
.LI "Font bad-fontable"
.br
File included: EFon2.mc
.br
Assertion text:
.eS
When the font argument does not name  a  valid  GContext  or
font resource, then a BadFont error occurs.
.eE
.LI "GC"
.br
File included: EGC.mc
.br
Assertion text:
.eS
When the GC argument does not name  a  defined  GC,  then  a
BadGC error occurs.
.eE
.LI "Match inputonly"
.br
File included: EMat1.mc
.br
Assertion text:
.eS
When a drawable argument  is  an  InputOnly  window  then  a
BadMatch error occurs.
.eE
.LI "Match gc-drawable-depth"
.br
File included: EMat2.mc
.br
Assertion text:
.eS
When the graphics context and the drawable do not  have  the
same depth, then a BadMatch error occurs.
.eE
.LI "Match gc-drawable-screen"
.br
File included: EMat3.mc
.br
Assertion text:
.eS
When the graphics context and the drawable were not  created
for the same root, then a BadMatch error occurs.
.eE
.LI "Match wininputonly"
.br
File included: EMat4.mc
.br
Assertion text:
.eS
When the window argument  is  an  InputOnly  window  then  a
BadMatch error occurs.
.eE
.LI "Name font"
.br
File included: ENam1.mc
.br
Assertion text:
.eS
When the specified font does not exist, then a BadName error
occurs.
.eE
.LI "Name colour"
.br
File included: ENam2.mc
.br
Assertion text:
.eS
When the specified colour does not  exist,  then  a  BadName
error occurs.
.eE
.LI "Pixmap [ARG1] [ARG2] ..."
.br
File included: EPix.mc
.br
Assertion text:
.eS
When a pixmap argument does not name a valid Pixmap [, ARG1] [or
ARG2], then a BadPixmap error occurs.
.eE
.LI "Value ARG1 VAL1 [VAL2] ..."
.br
File included: EVal.mc \(dg
.br
Assertion text:
.eS
When the value of ARG1 is other than VAL1 [or  VAL2],  then  a
BadValue error occurs.
.eE
.P
.S -1
\(dg - the assertion text is not in the included file, but is 
inserted directly by 
.C mc .
.S
.LI "Value ARG1 mask VAL1 [VAL2] ..."
.br
File included: EVal.mc \(dg
.br
Assertion text:
.eS
When the value of ARG1 is not a bitwise combination of  VAL1
[or VAL2], then a BadValue error occurs.
.eE
.P
.S -1
\(dg - the assertion text is not in the included file, but is 
inserted directly by 
.C mc .
.S
.LI "Window [ARG1] [ARG2] ..."
.br
File included: EWin.mc
.br
Assertion text:
.eS
When a window argument does not name a valid Window [, ARG1] [or
ARG2], then a BadWindow error occurs.
.eE
.LE
.SK
.H 1 "Appendix D - \fCmc\fP utility"
.B Usage
.cS
mc [-a a_list] [-o <output-file>] [-l] [-m] [-s] [-p] [<input-file>]
.cE
The 
.C mc
utility
outputs a C source file containing tests specified in the input 
file 
.C <input-file> ,
which must be a dot-m file which has the format specified
in the section entitled "Source file syntax". 
.P
If no 
.C <input-file>
is specified, the input is taken from standard input.
Multiple input files
can be processed by the utility, but the overall syntax must still conform 
to that defined in the section entitled "Source file syntax". 
A consequence of this is that you cannot specify another title section 
for a different function
and expect to output tests for more than one function simultaneously.
Limited diagnostics are
given if the file does not have the required syntax. By default, the 
C source file is written to the standard output stream.
.P
More details of the formats of the C source files produced by 
.C mc
are given in the section entitled "Source file formats".
.P
.B Options
.VL 5 0
.LI "\fC-a a_list\fP"
.br
This permits the specification of a list of assertions of the form 
.C n1-m1,n2-m2,...
to be output. Test code will only be output corresponding to the tests 
in the specified ranges.
.LI "\fC-o output-file\fP"
.br
This sends the output to the file 
.C <output-file>
instead of the standard output stream.
.LI \fC-l\fP
.br
This option outputs a C source file containing tests with modified 
startup code which allows the source code to be compiled and linked into
a space-saving executable file. The format of these files
is described in the section entitled 
"C files for linked executable - \fClink.c\fP".
.LI \fC-m\fP
.br
This option outputs a C source file containing tests for the 
macro version of the function specified in the title section of the 
dot-m file. The format of these files 
is described in the section entitled 
"C files for standalone executable in macro tests - \fCMTest.c\fP".
.P
The macro name is set to the 
.C <macroname>
argument of the >>SET macro option - if there is no >>SET macro
option in the file, or no argument specified, the default is the
.C function
argument in the >>TITLE keyword,
with the leading letter `X' removed.
.LI \fC-s\fP
.br
This option outputs a test strategy from the dot-m file
as a C source code comment block between the assertion and the 
code. The test strategy is derived from the corresponding 
strategy section in the dot-m file.
.LI \fC-p\fP
.br
This causes additional output including indicators of line number
in the original dot-m file (where possible). 
This means that any diagnostics produced by
.I cc(1)
or
.I lint(1)
will refer to the line numbers in the original dot-m file rather 
than the C source file.
.LE
.SK
.H 1 "Appendix E - \fCmmkf\fP utility"
.B Usage
.cS
mmkf [-o <output-file>] [-s sections] [<input_file>]
.cE
The 
.C mmkf
utility
outputs a Makefile corresponding to the specified input file
.C <input-file> ,
which must be a dot-m file which has the format specified
in the section entitled "Source file syntax". 
The Makefile can build all the C source files that can be 
output by 
.C mc
from the input file
.C <input-file> .
.P
If no 
.C <input-file>
is specified, the input is taken from standard input.
Multiple input files
can be processed by the utility, but the overall syntax must still conform 
to that defined in the section entitled "Source file syntax". 
A consequence of this is that you cannot specify another title section 
for a different function
and expect to output Makefiles for more than one function simultaneously.
Limited diagnostics are
given if the file does not have the required syntax. By default, the 
Makefile is written to the standard output stream.
.P
More details of the formats of the Makefiles produced by 
.C mmkf
are given in the sub-section entitled "Makefile" in the section entitled
"Source file formats".
.P
.B Options
.VL 5 0
.LI "\fC-o output-file\fP"
.br
This sends the output to the file 
.C <output-file>
instead of the standard output stream.
.LI "\fC-s sections\fP"
.br
This option enables output of certain optional sections of the 
Makefile. By default, output of all these sections is enabled. There
is no reason why you should need to use this option with the 
current version of the \*(xT.
.P
The
.C sections 
argument is a character string which may contain the key letters 
.C l ,
.C L ,
.C m
and 
.C p .
If these characters are included, the specified sections of the Makefile 
are then output.
.TS
box center;
l | l.
Key	Optional 
letter	section
_
l	Targets for linked executable
L	Targets for libraries
m	Targets for linting and cleaning
p	Targets for building known good image files
.TE
.LE
.SK
.H 1 "Appendix F - \fCma\fP utility"
.B Usage
.cS
ma [-a a_list] [-o <output-file>] [-h] [-s] [-p] [-m] [<input-file>]
.cE
The 
.C ma
utility
outputs a file containing a list of assertions in
.I nroff(1) 
format (requiring no macros other than those supplied in file
\fCmaheader.mc\fP). 
The assertions are specified in the input file 
.C <input-file> ,
which must be a dot-m file which has the format specified
in the section entitled "Source file syntax". 
.P
If no 
.C <input-file>
is specified, the input is taken from standard input.
Multiple input files
can be processed by the utility, but the overall syntax must still conform 
to that defined in the section entitled "Source file syntax". 
A consequence of this is that you cannot specify another title section 
for a different function
and expect to output assertions for more than one function simultaneously.
Limited diagnostics are
given if the file does not have the required syntax. By default, the 
assertion list is written to the standard output stream.
.P
More details of the format of the assertion list produced by
.C ma
are given in the sub-section entitled "Formatting assertions" 
in the section entitled
"Source file formats".
.P
.B Options
.VL 5 0
.LI "\fC-a a_list\fP"
.br
This permits the specification of a list of assertions of the form 
.C n1-m1,n2-m2,...
to be output. Assertions will only be output corresponding to the tests 
in the specified ranges.
.LI "\fC-o output-file\fP"
.br
This sends the output to the file 
.C <output-file>
instead of the standard output stream.
.LI \fC-h\fP
.br
The macros required for formatting the assertions are included 
at the start of the output stream. These are copied from the file 
\fCmaheader.mc\fP.
.P
By default, the macros are not copied to the output stream.
.LI \fC-s\fP
.br
If this option is specified, and the 
.C -h
option is specified, the line 
.cS
\&.so head.t
.cE
will be output at the start of the output stream.
.P
This option is not intended for general use - it was used when 
distributing assertions in compact form for external review.
.LI \fC-p\fP
.br
The macros \fC.NS\fP and \fC.NE\fP
will be output before and after each line in the dot-m file which is a 
comment (commencing with >>#). By default, dot-m file comments are not output.
The macros \fC.NS\fP and \fC.NE\fP are defined in \fCmaheader.mc\fP;
they cause the dot-m file comment lines to be printed in italic font by
.I nroff(1) .
.P
This option is not intended for general use - it was used when 
reviewing assertions before delivery.
.LI \fC-m\fP
.br
This option outputs assertions for the macro version of the function 
specified in the title section of the dot-m file.
.P
The macro name is set to the 
.C <macroname>
argument of the >>SET macro option - if there is no >>SET macro
option in the file, or no argument specified, the default is the
.C function
argument in the >>TITLE keyword,
with the leading letter `X' removed.
.LE
.TC
