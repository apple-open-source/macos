#! /usr/bin/perl -w
#
# Module name: BlockParse
# Synopsis: Block parser code
#
# Author: David Gatwood (dgatwood@apple.com)
# Last Updated: $Date: 2005/01/15 01:41:55 $
# 
# Copyright (c) 1999-2004 Apple Computer, Inc.  All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
#
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
#
# @APPLE_LICENSE_HEADER_END@
#
######################################################################
package HeaderDoc::BlockParse;

BEGIN {
	foreach (qw(Mac::Files)) {
	    $MOD_AVAIL{$_} = eval "use $_; 1";
    }
}

use Carp qw(cluck);
use Exporter;
foreach (qw(Mac::Files Mac::MoreFiles)) {
    eval "use $_";
}

@ISA = qw(Exporter);
@EXPORT = qw(blockParse nspaces blockParseOutside getAndClearCPPHash);

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash quote parseTokens isKeyword warnHDComment classTypeFromFieldAndBPinfo casecmp get_super);

use strict;
use vars qw($VERSION @ISA);
use File::Basename qw(basename);
$VERSION = '$Revision: 1.1.2.93 $';

################ Portability ###################################
my $isMacOS;
my $pathSeparator;
if ($^O =~ /MacOS/io) {
        $pathSeparator = ":";
        $isMacOS = 1;
} else {
        $pathSeparator = "/";
        $isMacOS = 0;
}


my %CPP_HASH = ();
my %CPP_ARG_HASH = ();
my $cppDebugDefault = 0;
my $cppDebug = 0;
my $nestedcommentwarn = 0;
my $warnAllMultipleDefinitions = 1;


################ Code ###################################


# /*!
#    This is a trivial function that returns a look at the top of a stack.
#    This seems like it should be part of the language.  If there is an
#    equivalent, this should be dropped.
# */
sub peek
{
	my $ref = shift;
	my @stack = @{$ref};
	my $tos = pop(@stack);
	push(@stack, $tos);

	return $tos;
}

# /*! getLangAndSublangFromClassType takes a class token (class, \@class,
#     \@interface, etc.) and returns a lang and sublang.  Pretty trivial,
#     but critical....
#  */
sub getLangAndSublangFromClassType
{
    my $classtype = shift;
    my $lang = "C";
    my $sublang = "C";

    if ($HeaderDoc::lang ne "C") {
	# This should never change for java/javascript/php.  :-)
	return ($HeaderDoc::lang, $HeaderDoc::sublang);
    }

    if ($classtype =~ /\@/) {
	$sublang = "occ";
    } elsif ($classtype =~ /class/) {
	$sublang = "cpp";
    }

    return ($lang, $sublang);
}

# /*!
#    This is a variant of peek that returns the right token to match
#    the left token at the top of a brace stack.
#  */
sub peekmatch
{
	my $ref = shift;
	my $filename = shift;
	my $linenum = shift;
	my @stack = @{$ref};
	my $tos = pop(@stack);
	push(@stack, $tos);

	SWITCH: {
	    ($tos eq "{") && do {
			return "}";
		};
	    ($tos eq "#") && do {
			return "#";
		};
	    ($tos eq "(") && do {
			return ")";
		};
	    ($tos eq "/") && do {
			return "/";
		};
	    ($tos eq "'") && do {
			return "'";
		};
	    ($tos eq "\"") && do {
			return "\"";
		};
	    ($tos eq "`") && do {
			return "`";
		};
	    ($tos eq "<") && do {
			return ">";
		};
	    ($tos eq "[") && do {
			return "]";
		};
	    ($tos eq "\@interface") && do {
			return "\@end";
		};
	    ($tos eq "\@protocol") && do {
			return "\@end";
		};
	    {
		# default case
		warn "$filename:$linenum:Unknown block delimiter \"$tos\".  Please file a bug.\n";
		return $tos;
	    };
	}
}

# /*! The blockParse function is the core of HeaderDoc's parse engine.
#     @param filename the filename being parser.
#     @param fileoffset the line number where the current block begins.  The line number printed is (fileoffset + inputCounter).
#     @param inputLinesRef a reference to an array of code lines.
#     @param inputCounter the offset within the array.  This is added to fileoffset when printing the line number.
#     @param argparse disable warnings when parsing arguments to avoid seeing them twice.
#     @param ignoreref a reference to a hash of tokens to ignore on all headers.
#     @param perheaderignoreref a reference to a hash of tokens, generated from \@ignore headerdoc comments.
#     @param perheaderignorefuncmacrosref a reference to a hash of tokens, generated from \@ignorefunmacro headerdoc comments.
#     @param keywordhashref a reference to a hash of keywords.
#     @param case_sensitive boolean: controls whether keywords should be processed in a case-sensitive fashion.
#     @result Returns ($inputCounter, $declaration, $typelist, $namelist, $posstypes, $value, \@pplStack, $returntype, $privateDeclaration, $treeTop, $simpleTDcontents, $availability).
# */
sub blockParse
{
    my $filename = shift;
    my $fileoffset = shift;
    my $inputLinesRef = shift;
    my $inputCounter = shift;
    my $argparse = shift;
    my $ignoreref = shift;
    my $perheaderignoreref = shift;
    my $perheaderignorefuncmacrosref = shift;
    my $keywordhashref = shift;
    my $case_sensitive = shift;

    # Initialize stuff
    my @inputLines = @{$inputLinesRef};
    my $declaration = "";
    my $publicDeclaration = "";

    # Debugging switches
    my $retDebug         = 0;
    my $localDebug       = 0 || $HeaderDoc::fileDebug;
    my $listDebug        = 0;
    my $parseDebug       = 0 || $HeaderDoc::fileDebug;
    my $sodDebug         = 0 || $HeaderDoc::fileDebug;
    my $valueDebug       = 0;
    my $parmDebug        = 0;
    my $cbnDebug         = 0;
    my $macroDebug       = 0;
    my $apDebug          = 0;
    my $tsDebug          = 0;
    my $treeDebug        = 0;
    my $ilcDebug         = 0;
    my $regexpDebug      = 0;
    my $parserStackDebug = 0 || $HeaderDoc::fileDebug;
    my $hangDebug        = 0;
    my $offsetDebug      = 0;

    $cppDebug = $cppDebugDefault || $HeaderDoc::fileDebug;

    # State variables (part 1 of 3)
    # my $typestring = "";
    my $continue = 1; # set low when we're done.
    my $parsedParamParse = 0; # set high when current token is part of param.
    # my @parsedParamList = (); # currently active parsed parameter list.
    # my @pplStack = (); # stack of parsed parameter lists.  Used to handle
                       # fields and parameters in nested callbacks/structs.
    # my @freezeStack = (); # copy of pplStack when frozen.
    # my $frozensodname = "";
    # my $stackFrozen = 0; # set to prevent fake parsed params with inline funcs
    my $lang = $HeaderDoc::lang;
    my $perl_or_shell = 0;
    my $sublang = $HeaderDoc::sublang;
    my $callback_typedef_and_name_on_one_line = 1; # deprecated
    # my $returntype = "";
    # my $freezereturn = 0;       # set to prevent fake return types with inline funcs
    my $treeNest = 0;           # 1: nest future content under this node.
                                # 2: used if you want to nest, but have already
                                # inserted the contents of the node.
    my $sethollow = 0;
    my $setNoInsert = 0;
    my $treepart = "";          # There are some cases where you want to drop a token
                                # for formatting, but keep it in the parse tree.
                                # In that case, treepart contains the original token,
                                # while part generally contains a space.
    # my $availability = "";      # holds availability string if we find an av macro.
    # my $seenTilde = 0;          # set to 1 for C++ destructor.

    if ($argparse && $tsDebug) { $tsDebug = 0; }

    # Configure the parse tree output.
    my $treeTop = HeaderDoc::ParseTree->new(); # top of parse tree.
    my $treeCur = $treeTop;   # current position in parse tree
    my $treeSkip = 0;         # set to 1 if "part" should be dropped in tree.
    my $treePopTwo = 0;       # set to 1 for tokens that nest, but have no
                              # explicit ending token ([+-:]).
    my $treePopOnNewLine = 0; # set to 1 for single-line comments, macros.
    my @treeStack = ();       # stack of parse trees.  Used for popping
                              # our way up the tree to simplify tree structure.

    # Leak a node here so that every real node has a parent.
    $treeCur = $treeCur->addChild("");
    $treeTop = $treeCur;

    my $lastACS = "";

    # The argparse switch is a trigger....
    if ($argparse && $apDebug) { 
	$localDebug   = 1;
	$listDebug    = 1;
	$parseDebug   = 1;
	$sodDebug     = 1;
	$valueDebug   = 1;
	$parmDebug    = 1;
	$cbnDebug     = 1;
	$macroDebug   = 1;
	# $apDebug      = 1;
	$tsDebug      = 1;
	$treeDebug    = 1;
	$ilcDebug     = 1;
	$regexpDebug  = 1;
    }
    if ($argparse && ($localDebug || $apDebug)) {
	print "ARGPARSE MODE!\n";
	print "IPC: $inputCounter\nNLINES: ".$#inputLines."\n";
    }

    print "INBP\n" if ($localDebug);

# warn("in BlockParse\n");

    # State variables (part 2 of 3)
    my $parserState = HeaderDoc::ParserState->new();
    $parserState->{hollow} = $treeTop;
    $parserState->{lang} = $lang;
    $parserState->{inputCounter} = $inputCounter;
    $parserState->{initbsCount} = 0; # included for consistency....
    my @parserStack = ();

    # print "TEST: ";
    # if (defined($parserState->{parsedParamList})) {
	# print "defined\n"
    # } else { print "undefined.\n"; }
    # print "\n";

    # my $inComment = 0;
    # my $inInlineComment = 0;
    # my $inString = 0;
    # my $inChar = 0;
    # my $inTemplate = 0;
    my @braceStack = ();
    # my $inOperator = 0;
    my $inPrivateParamTypes = 0;  # after a colon in a C++ function declaration.
    # my $onlyComments = 1;         # set to 0 to avoid switching to macro parse.
                                  # mode after we have seen a code token.
    # my $inMacro = 0;
    # my $inMacroLine = 0;          # for handling macros in middle of data types.
    # my $seenMacroPart = 0;        # used to control dropping of macro body.
    # my $macroNoTrunc = 1;         # used to avoid truncating body of macros
                                  # that don't begin with parenthesis or brace.
    # my $inBrackets = 0;           # square brackets ([]).
    my $inPType = 0;              # in pascal types.
    my $inRegexp = 0;             # in perl regexp.
    my $regexpNoInterpolate = 0;  # Don't interpolate (e.g. tr)
    my $inRegexpTrailer = 0;      # in the cruft at the end of a regexp.
    my $hollowskip = 0;
    my $ppSkipOneToken = 0;       # Comments are always dropped from parsed
                                  # parameter lists.  However, inComment goes
                                  # to 0 on the end-of-comment character.
                                  # This prevents the end-of-comment character
                                  # itself from being added....

    my $regexppattern = "";       # optional characters at start of regexp
    my $singleregexppattern = ""; # members of regexppattern that take only
                                  # one argument instead of two.
    my $regexpcharpattern = "";   # legal chars to start a regexp.
    my @regexpStack = ();         # stack of RE tokens (since some can nest).

    # Get the parse tokens from Utilities.pm.
    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
	$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$typedefname, $varname, $constname, $structisbrace, $macronameref,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp)
		= parseTokens($lang, $sublang);

    if ($parseDebug) {
	print "SOT: $sotemplate EOF: $eotemplate OP: $operator SOC: $soc EOC: $eoc ILC: $ilc\n";
	print "SOFUNC: $sofunction SOPROC: $soprocedure SOPREPROC: $sopreproc LBRACE: $lbrace RBRACE:  $rbrace\n";
 	print "UNION: $unionname STRUCT: $structname TYPEDEF: $typedefname VAR: $varname CONST: $constname\n";
 	print "STRUCTISBRACE: $structisbrace MACRONAMEREF: $macronameref CLASSRE: $classregexp\n";
	print "CLASSBRACERE: $classbraceregexp CLASSCLOSEBRACERE: $classclosebraceregexp ACCESSRE: $accessregexp\n";
    }
    

    # Set up regexp patterns for perl, variable for perl or shell.
    if ($lang eq "perl" || $lang eq "shell") {
	$perl_or_shell = 1;
	if ($lang eq "perl") {
		$regexpcharpattern = '\\{|\\#\\(|\\/|\\\'|\\"|\\<|\\[|\\`';
		$regexppattern = "qq|qr|qx|qw|q|m|s|tr|y";
		$singleregexppattern = "qq|qr|qx|qw|q|m";
	}
    }

    my $pascal = 0;
    if ($lang eq "pascal") { $pascal = 1; }

    # State variables (part 3 of 3)
    # my $lastsymbol = "";          # Name of the last token, wiped by braces,
                                  # parens, etc.  This is not what you are
                                  # looking for.  It is used mostly for
                                  # handling names of typedefs.

    # my $name = "";                # Name of a basic data type.
    # my $callbackNamePending = 0;  # 1 if callback name could be here.  This is
                                  # only used for typedef'ed callbacks.  All
                                  # other callbacks get handled by the parameter
                                  # parsing code.  (If we get a second set of
                                  # parsed parameters for a function, the first
                                  # one becomes the callback name.)
    # my $callbackName = "";        # Name of this callback.
    # my $callbackIsTypedef = 0;    # 1 if the callback is wrapped in a typedef---
                                  # sets priority order of type matching (up
                                  # one level in headerdoc2HTML.pl).

    # my $namePending = 0;          # 1 if name of func/variable is coming up.
    # my $basetype = "";            # The main name for this data type.
    # my $posstypes = "";           # List of type names for this data type.
    # my $posstypesPending = 1;     # If this token could be one of the
                                  # type names of a typedef/struct/union/*
                                  # declaration, this should be 1.
    # my $sodtype = "";             # 'start of declaration' type.
    # my $sodname = "";             # 'start of declaration' name.
    # my $sodclass = "";            # 'start of declaration' "class".  These
                                  # bits allow us keep track of functions and
                                  # callbacks, mostly, but not the name of a
                                  # callback.

    # my $simpleTypedef = 0;        # High if it's a typedef w/o braces.
    # my $simpleTDcontents = "";    # Guts of a one-line typedef.  Don't ask.
    # my $seenBraces = 0;           # Goes high after initial brace for inline
                                  # functions and macros -only-.  We
                                  # essentially stop parsing at this point.
    # my $kr_c_function = 0;        # Goes high if we see a K&R C declaration.
    # my $kr_c_name = "";           # The name of a K&R function (which would
                                  # otherwise get lost).

    my $lastchar = "";            # Ends with the last token, but may be longer.
    my $lastnspart = "";          # The last non-whitespace token.
    my $lasttoken = "";           # The last token seen (though [\n\r] may be
                                  # replaced by a space in some cases.
    # my $startOfDec = 1;           # Are we at the start of a declaration?
    my $prespace = 0;             # Used for indentation (deprecated).
    my $prespaceadjust = 0;       # Indentation is now handled by the parse
                                  # tree (colorizer) code.
    my $scratch = "";             # Scratch space.
    my $curline = "";             # The current line.  This is pushed onto
                                  # the declaration at a newline and when we
                                  # enter/leave certain constructs.  This is
                                  # deprecated in favor of the parse tree.
    my $curstring = "";           # The string we're currently processing.
    my $continuation = 0;         # An obscure spacing workaround.  Deprecated.
    my $forcenobreak = 0;         # An obscure spacing workaround.  Deprecated.
    # my $occmethod = 0;            # 1 if we're in an ObjC method.
    my $occspace = 0;             # An obscure spacing workaround.  Deprecated.
    # my $occmethodname = "";       # The name of an objective C method (which
                                  # gets augmented to be this:that:theother).
    # my $preTemplateSymbol = "";   # The last symbol prior to the start of a
                                  # C++ template.  Used to determine whether
                                  # the type returned should be a function or
                                  # a function template.
    # my $preEqualsSymbol = "";     # Used to get the name of a variable that
                                  # is followed by an equals sign.
    # my $valuepending = 0;         # True if a value is pending, used to
                                  # return the right value.
    # my $value = "";               # The current value.
    my $parsedParam = "";         # The current parameter being parsed.
    my $postPossNL = 0;           # Used to force certain newlines to be added
                                  # to the parse tree (to end macros, etc.)
    # my $categoryClass = "";
    # my $classtype = "";
    # my $inClass = 0;

    my $pushParserStateAfterToken = 0;
    my $pushParserStateAfterWordToken = 0;
    my $pushParserStateAtBrace = 0;
    my $occPushParserStateOnWordTokenAfterNext = 0;

    $HeaderDoc::hidetokens = 0;

    # Loop unti the end of file or until we've found a declaration,
    # processing one line at a time.
    my $nlines = $#inputLines;
    my $incrementoffsetatnewline = 0;
    while ($continue && ($inputCounter <= $nlines)) {
	$HeaderDoc::CurLine = $inputCounter + $fileoffset;
	my $line = $inputLines[$inputCounter++];
	my @parts = ();

	$line =~ s/^\s*//go;
	$line =~ s/\s*$//go;
	# $scratch = nspaces($prespace);
	# $line = "$scratch$line\n";
	# $curline .= $scratch;
	$line .= "\n";

	print "LINE[$inputCounter] : $line\n" if ($offsetDebug);

	# The tokenizer
	if ($lang eq "perl" || $lang eq "shell") {
	    @parts = split(/("|'|\#|\{|\}|\(|\)|\s|;|\\|\W)/, $line);
	} else {
	    @parts = split(/("|'|\/\/|\/\*|\*\/|::|==|<=|>=|!=|\<\<|\>\>|\{|\}|\(|\)|\s|;|\\|\W)/, $line);
	}

	# See note about similar block below.  This block is for fixing the
	# "missing newline" problem, which otherwise would cause line numbers
	# to sometimes be wrong.
	push(@parts, "BOGUSBOGUSBOGUSBOGUSBOGUS");
	my $xpart = "";
	foreach my $nextxpart (@parts) {
	    if (!length($nextxpart)) { next; }
	    if (!length($xpart)) { $xpart = $nextxpart; next; }
	    if ($xpart eq "\n" && $nextxpart ne "BOGUSBOGUSBOGUSBOGUSBOGUS") {
		print "FOUND EXTRA NEWLINE\n" if ($offsetDebug);
		# $fileoffset++;
		$incrementoffsetatnewline++;
	    }
	    $xpart = $nextxpart;
	}
	pop(@parts);

	$parserState->{inInlineComment} = 0;
	print "inInlineComment -> 0\n" if ($ilcDebug);

        # warn("line $inputCounter\n");

if ($localDebug || $cppDebug) {foreach my $partlist (@parts) {print "PARTLIST: $partlist\n"; }}

	# This block of code needs a bit of explanation, I think.
	# We need to be able to see the token that follows the one we
	# are currently processing.  To do this, we actually keep track
	# of the current token, and the previous token, but name then
	# $nextpart and $part.  We do processing on $part, which gets
	# assigned the value from $nextpart at the end of the loop.
	#
	# To avoid losing the last part of the declaration (or needing
	# to unroll an extra copy of the entire loop code) we push a
	# bogus entry onto the end of the stack, which never gets
	# used (other than as a bogus "next part") because we only
	# process the value in $part.
	#
	# To avoid problems, make sure that you don't ever have a regexp
	# that would match against this bogus token.
	#
	my $part = "";
	my $argparse = 0;
	if (1 || $HeaderDoc::enable_cpp) {
		my $newrawline = "";
		my $incppargs = 0;
		my $cppstring = "";
		my $cppname = "";
		my $lastcpppart = "";
		my @cppargs = ();
		my $inChar = 0; my $inString = 0; my $inComment = $parserState->{inComment}; my $inSLC = $parserState->{inInlineComment};
		my $inParen = 0;
		my $inMacro = $parserState->{inMacro};
		my $inMacroTail = 0;
		if ($parserState->{sodname} && ($parserState->{sodname} ne "")) {
			$inMacroTail = 1;
		}
		print "INMACROTAIL: $inMacroTail\n" if ($cppDebug);

		my @cpptrees;
		my $cpptreecur = HeaderDoc::ParseTree->new();
		my $cpptreetop = $cpptreecur;

		# print "CHECK LINE $line\n";
		if ($line =~ /^\s*#include (.*)$/) {
			my $rest = $1;
			$rest =~ s/^\s*//s;
			$rest =~ s/\s*$//s;
			if ($rest !~ s/^\<(.*)\>$/$1/s) {
				$rest =~ s/^\"(.*)\"$/$1/s;
			}
			my $filename = basename($rest);
			if ($HeaderDoc::HeaderFileCPPHashHash{$filename}) {
				my $includehash = HeaderDoc::IncludeHash->new();
				$includehash->{FILENAME} = $filename;
				$includehash->{LINENUM} = $inputCounter + $fileoffset;
				$includehash->{HASHREF} = $HeaderDoc::HeaderFileCPPHashHash{$filename};
				push(@HeaderDoc::cppHashList, $includehash);
# print "PUSH HASH\n";
				push(@HeaderDoc::cppArgHashList, $HeaderDoc::HeaderFileCPPArgHashHash{$filename});
			}
		} elsif ($line =~ /^\s*#define\s+/) {
			# print "inMacro -> 1\n";
			$inMacro = 1;
		}
		my $cppleaddebug = 0;
		do {
		    my $pos = 0;
		    my $dropargs = 0;
		    while ($pos < scalar(@parts)) {
			my $part = @parts[$pos];
			if (length($part)) {
			    print "CPPLEADPART: $part\n"if ($cppleaddebug);
			    if (!$inString && !$inChar) {
				if ($inComment && $part eq $eoc) {
					print "EOC\n"if ($cppleaddebug);
					$inComment = 0;
				} elsif ($inSLC && $part =~ /[\r\n]/) {
					print "EOSLC\n"if ($cppleaddebug);
					$inSLC = 0;
				} elsif (!$inSLC && $part eq $soc) {
					print "SOC\n"if ($cppleaddebug);
					$inComment = 1;
				} elsif (!$inComment && $part eq $ilc) {
					print "INSLC\n"if ($cppleaddebug);
					$inSLC = 1;
				}
			    }
			    my $skip = 0;
			    if (!$incppargs) {
				my $newpart = $part;
				my $hasargs = 0;
				if (!$inComment && !$inSLC) {
					($newpart, $hasargs) = cpp_preprocess($part, $HeaderDoc::CurLine);
					# Don't drop tokens in macros.
					if ($hasargs == 2 && $inMacro) {
						$newpart = $part;
						$hasargs = 0;
					}
					# Don't change the macro name.  (If a
					# macro gets redefined, ignore it.)
					if ($inMacro && !$inMacroTail) {
						$newpart = $part;
						$hasargs = 0;
					}
				}
				if ($hasargs) {
					$incppargs = 1;
					$cppname = $part;
					if ($hasargs == 2) {
						$dropargs = 1;
						print "Dropping arguments for ignored macro \"$part\"\n" if ($cppDebug);
					}
				} else {
					my $newpartnl = $newpart;
					my $newpartnlcount = ($newpartnl =~ tr/\n//);
					my $partnl = $part;
					my $partnlcount = ($partnl =~ tr/\n//);
					my $nlchange = ($newpartnlcount - $partnlcount);
					print "NLCHANGE: $nlchange (FILEOFFSET = $fileoffset)\n" if ($offsetDebug);
					$fileoffset -= $nlchange;
					if ($inMacro) {
						if ($newpart ne $part) {
							print "CHANGING NEWPART FROM \"$newpart\" TO " if ($cppDebug);
							$newpart =~ s/^\s*/ /s;
							$newpart =~ s/\s*$//s;
							$newpart =~ s/(.)\n/$1 \\\n/sg;
							$newpart =~ s/\\$/ /s;
							print "$newpart\n" if ($cppDebug);
						}
					}
					$newrawline .= $newpart;
				}
			    } elsif ($incppargs == 1) {
				if ($part eq "(") {
					# Don't do anything until leading parenthesis.
					$incppargs = 3;
					$inParen++;
				}
			    } elsif ($incppargs == 3) {
				if ($part eq '\\') {
					if (!$inMacro && ($lastcpppart eq '\\')) { $lastcpppart = ""; }
					# else {
						# $lastcpppart = $part; 
						# if ($inMacro) {
# print "IMTEST\n" if ($cppDebug > 1);
							# my $npos = $pos + 1;
							# while ($npos < scalar(@parts)) {
							    # my $npart = $parts[$npos];
							    # if (length($npart)) {
# print "NEXTPART: \"".$parts[$npos]."\"\n" if ($cppDebug > 1);
								# if ($npart =~ /\s/) {
									# if ($npart =~ /[\n\r]/) {
# print "SKIP1\n" if ($cppDebug > 1);
										# $skip = 1; last;
									# } else {
# print "SPC\n" if ($cppDebug > 1);
									# }
								# } else {
# print "LAST\n" if ($cppDebug > 1);
									# last;
								# }
							    # }
							    # $npos++;
							# }
						# }
					# }
				} elsif ($part eq '"') {
					if ($lastcpppart ne '\\') {
						if (!$inChar && !$inComment && !$inSLC) {
							$inString = !$inString;
						}
					}
					$lastcpppart = $part;
				} elsif ($part eq "'") {
					if ($lastcpppart ne '\\') {
						if (!$inString && !$inComment && !$inSLC) {
							$inChar = !$inChar;
						}
					}
					$lastcpppart = $part;
				} elsif (!$inChar && !$inString && !$inComment && !$inSLC) {
					if ($part eq "(") {
						$inParen++;
						push(@cpptrees, $cpptreecur);
						$cpptreecur = $cpptreecur->firstchild(HeaderDoc::ParseTree->new());
					} elsif ($part eq ")") {
						$inParen--;
						if (scalar(@cpptrees)) {
							$cpptreecur = pop(@cpptrees);
							while ($cpptreecur && $cpptreecur->next()) {
								$cpptreecur = $cpptreecur->next();
							}
						}
						if (!$inParen) {
							push(@cppargs, $cpptreetop);
							$cppstring = "";
							$cpptreetop = HeaderDoc::ParseTree->new();
							$cpptreecur = $cpptreetop;
							$skip = 1;
							$incppargs = 0;
							if (!$dropargs) {
								my $addon = cpp_argparse($cppname, $HeaderDoc::CurLine, \@cppargs);
								if ($inMacro) {
									print "CHANGING ADDON FROM \"$addon\" TO " if ($cppDebug);
									$addon =~ s/^\s*/ /s;
									$addon =~ s/\s*$//s;
									$addon =~ s/(.)\n/$1 \\\n/sg;
									$addon =~ s/\\$/ /s;
									print "$addon\n" if ($cppDebug);
								}
								$newrawline .= $addon;
							}
							$dropargs = 0;
						}
					} elsif (($inParen == 1) && (!$inChar && !$inString && !$inComment && !$inSLC) && ($part eq ",")) {
						push(@cppargs, $cpptreetop);
						$cpptreetop = HeaderDoc::ParseTree->new();
						$cpptreecur = $cpptreetop;
						$cppstring = "";
						$skip = 1;
					} elsif (($part =~ /\s/) && (!$inParen)) {
						$incppargs = 0;
						if (!$dropargs) {
							my $addon = cpp_argparse($cppname, $HeaderDoc::CurLine, \@cppargs);
							if ($inMacro) {
									print "CHANGING ADDON FROM \"$addon\" TO " if ($cppDebug);
									$addon =~ s/^\s*/ /s;
									$addon =~ s/\s*$//s;
									$addon =~ s/(.)\n/$1 \\\n/sg;
									$addon =~ s/\\$/ /s;
									print "$addon\n" if ($cppDebug);
							}
							$newrawline .= $addon;
						}
						$dropargs = 0;
					}
					$lastcpppart = $part;
				}
				if ($skip) { $skip = 0; }
				else {
					my $xpart = $part;

					if ($part =~ /[\r\n]/) { $xpart = " "; }
					$cpptreecur = $cpptreecur->next(HeaderDoc::ParseTree->new());
					$cpptreecur->token($xpart);
				}
				$cppstring .= $part;
			    }
			    if ($inMacro && $part ne "define" &&
				$part =~ /\w/ && !$inParen) {
					$inMacroTail = 1;
			    }
			}
			$pos++;
		    }
		    if ($incppargs) {
			# print "YO\n";
			if ($parserState->{inMacro} || $inMacro) {
			# print "YOYO\n";
				if ($cppstring !~ s/\\\s*$//s) {
print "CPPS: \"$cppstring\"\n";
					warn "Non-terminated macro.\n";
					$incppargs = 0;
				}
			}
		    }
		    if ($incppargs || $inComment) {
			print "Fetching new line ($incppargs, $inComment)\n" if ($cppleaddebug);
			$HeaderDoc::CurLine = $inputCounter + $fileoffset;
			$line = $inputLines[$inputCounter++];
			# @parts = split(/(\W)/, $line);
			if ($lang eq "perl" || $lang eq "shell") {
			    @parts = split(/("|'|\#|\{|\}|\(|\)|\s|;|\\|\W)/, $line);
			} else {
			    @parts = split(/("|'|\/\/|\/\*|\*\/|::|==|<=|>=|!=|\<\<|\>\>|\{|\}|\(|\)|\s|;|\\|\W)/, $line);
			}
		    }
		} until (!$incppargs && !$inComment);
		# The tokenizer
		if ($lang eq "perl" || $lang eq "shell") {
			@parts = split(/("|'|\#|\{|\}|\(|\)|\s|;|\\|\W)/, $newrawline);
		} else {
			@parts = split(/("|'|\/\/|\/\*|\*\/|::|==|<=|>=|!=|\<\<|\>\>|\{|\}|\(|\)|\s|;|\\|\W)/, $newrawline);
		}
		while (scalar(@cpptrees)) {
			my $temptree = pop(@cpptrees);
			if ($temptree != $cpptreetop) {
				$temptree->dispose();
			}
		}
		$cpptreetop->dispose();
	}
	my @stripparts = @parts;
	@parts = ();
	foreach my $strippart (@stripparts) {
		if (length($strippart)) {
			push(@parts, $strippart);
		}
	}
	push(@parts, "BOGUSBOGUSBOGUSBOGUSBOGUS");

if ($localDebug || $cppDebug) {foreach my $partlist (@parts) {print "POSTCPPPARTLIST: \"$partlist\"\n"; }}

	foreach my $nextpart (@parts) {
	    $treeSkip = 0;
	    $treePopTwo = 0;
	    # $treePopOnNewLine = 0;

	    # The current token is now in "part", and the literal next
	    # token in "nextpart".  We can't just work with this as-is,
	    # though, because you can have multiple spaces, null
	    # tokens when two of the tokens in the split list occur
	    # consecutively, etc.

	    print "MYPART: $part\n" if ($localDebug);

	    $forcenobreak = 0;
	    if ($nextpart eq "\r") { $nextpart = "\n"; }
	    if ($localDebug && $nextpart eq "\n") { print "NEXTPART IS NEWLINE!\n"; }
	    if ($localDebug && $part eq "\n") { print "PART IS NEWLINE!\n"; }
	    if ($nextpart ne "\n" && $nextpart =~ /\s/o) {
		# Replace tabs with spaces.
		$nextpart = " ";
	    }
	    if ($part ne "\n" && $part =~ /\s/o && $nextpart ne "\n" &&
		$nextpart =~ /\s/o) {
			# we're a space followed by a space.  Drop ourselves.
			next;
	    }
	    print "PART IS \"$part\"\n" if ($localDebug || $parserStackDebug);
	    print "CURLINE IS \"$curline\"\n" if ($localDebug || $hangDebug);

	    if (!length($nextpart)) {
		print "SKIP NP\n" if ($localDebug);
		next;
	    }
	    if (!length($part)) {
		print "SKIP PART\n" if ($localDebug);
		$part = $nextpart;
		next;
	    }

	    if ($occPushParserStateOnWordTokenAfterNext > 1) {
		if ($part =~ /\w/) {
			$occPushParserStateOnWordTokenAfterNext--;
			print "occPushParserStateOnWordTokenAfterNext -> $occPushParserStateOnWordTokenAfterNext (--)\n" if ($localDebug || $parseDebug);
		}
	    } elsif ($occPushParserStateOnWordTokenAfterNext) {
		if ($part =~ /\w/) {
			$parserState->{lastTreeNode} = $treeCur;
			print "parserState pushed onto stack[occPushParserStateOnWordTokenAfterNext]\n" if ($parserStackDebug);
			$curline = "";
			push(@parserStack, $parserState);
			$parserState = HeaderDoc::ParserState->new();
			$parserState->{skiptoken} = 0;
			$parserState->{lang} = $lang;
			$parserState->{inputCounter} = $inputCounter;
			$parserState->{initbsCount} = scalar(@braceStack);
			$parserState->{noInsert} = $setNoInsert;
			$setNoInsert = 0;
			$pushParserStateAtBrace = 0;
			$occPushParserStateOnWordTokenAfterNext = 0;
		}
	    }

	    # If we get here, we aren't skipping a null or whitespace token.
	    # Let's print a bunch of noise if debugging is enabled.

	    # if ($part eq "\n" && $nextpart ne "BOGUSBOGUSBOGUSBOGUSBOGUS") {
		# $fileoffset++;
	    # }
	    if ($part eq "\n" && $incrementoffsetatnewline) {
		$incrementoffsetatnewline--;
		$fileoffset++;
	    }

	    if ($parseDebug) {
		print "PART: $part, type: $parserState->{typestring}, inComment: $parserState->{inComment}, inInlineComment: $parserState->{inInlineComment}, inChar: $parserState->{inChar}.\n" if ($localDebug);
		print "PART: onlyComments: $parserState->{onlyComments}, inClass: $parserState->{inClass}\n";
		print "PART: classIsObjC: $parserState->{classIsObjC}, PPSAT: $pushParserStateAfterToken, PPSAWordT: $pushParserStateAfterWordToken, PPSABrace: $pushParserStateAtBrace, occPPSOnWordTokenAfterNext: $occPushParserStateOnWordTokenAfterNext\n";
		print "PART: bracecount: " . scalar(@braceStack) . " (init was $parserState->{initbsCount}).\n";
		print "PART: inString: $parserState->{inString}, callbackNamePending: $parserState->{callbackNamePending}, namePending: $parserState->{namePending}, lastsymbol: $parserState->{lastsymbol}, lasttoken: $lasttoken, lastchar: $lastchar, SOL: $parserState->{startOfDec}\n" if ($localDebug);
		print "PART: sodclass: $parserState->{sodclass} sodname: $parserState->{sodname}\n";
		print "PART: sodtype: $parserState->{sodtype}\n";
		print "PART: simpleTypedef: $parserState->{simpleTypedef}\n";
		print "PART: posstypes: $parserState->{posstypes}\n";
		print "PART: seenBraces: $parserState->{seenBraces} inRegexp: $inRegexp\n";
		print "PART: regexpNoInterpolate: $regexpNoInterpolate\n";
		print "PART: seenTilde: $parserState->{seenTilde}\n";
		print "PART: CBN: $parserState->{callbackName}\n";
		print "PART: regexpStack is:";
		foreach my $token (@regexpStack) { print " $token"; }
		print "\n";
		print "PART: npplStack: ".scalar(@{$parserState->{pplStack}})." nparsedParamList: ".scalar(@{$parserState->{parsedParamList}})." nfreezeStack: ".scalar(@{$parserState->{freezeStack}})." frozen: $parserState->{stackFrozen}\n";
		print "PART: inMacro: $parserState->{inMacro} treePopOnNewLine: $treePopOnNewLine\n";
		print "PART: occmethod: $parserState->{occmethod} occmethodname: $parserState->{occmethodname}\n";
		print "PART: returntype is $parserState->{returntype}\n";
		print "length(declaration) = " . length($declaration) ."; length(curline) = " . length($curline) . "\n";
	    } elsif ($tsDebug || $treeDebug) {
		print "BPPART: $part\n";
	    }
	    if ($parserStackDebug) {
		print "parserState: STACK CONTAINS ".scalar(@parserStack)." STATES\n";
		print "parserState is $parserState\n";
	    }

	    # The ignore function returns either null, an empty string,
	    # or a string that gives the text equivalent of an availability
            # macro.  If the token is non-null and the length is non-zero,
	    # it's an availability macro, so blow it in as if the comment
	    # contained an @availability tag.
	    # 
	    my $tempavail = ignore($part, $ignoreref, $perheaderignoreref);
	    # printf("PART: $part TEMPAVAIL: $tempavail\n");
	    if ($tempavail && ($tempavail ne "1")) {
		$parserState->{availability} = $tempavail;
	    }

	    # Here be the parser.  Abandon all hope, ye who enter here.
	    $treepart = "";
	    if ($parserState->{inClass} == 3) {
		print "INCLASS3\n" if ($parseDebug);
		if ($part eq ")") {
			$parserState->{inClass} = 1;
			$parserState->{categoryClass} .= $part;
			print "parserState will be pushed onto stack[cparen3]\n" if ($parserStackDebug);
			# $parserState->{lastTreeNode} = $treeCur;
			# push(@parserStack, $parserState);
			# $parserState = HeaderDoc::ParserState->new();
			# $parserState->{lang} = $lang;
			# $parserState->{inputCounter} = $inputCounter;
			# $parserState->{initbsCount} = scalar(@braceStack);
			$pushParserStateAfterToken = 1;
		} elsif ($part eq ":") {
			$parserState->{inClass} = 1;
			if ($parserState->{classIsObjC}) {
				print "occPushParserStateOnWordTokenAfterNext -> 2\n" if ($localDebug || $parseDebug);
				$occPushParserStateOnWordTokenAfterNext = 2;
			} else {
				$pushParserStateAfterWordToken = 1;
			}
			# if ($sublang eq "occ") {
				# $pushParserStateAtBrace = 2;
			# }
		} elsif ($part =~ /</ && $parserState->{classIsObjC}) {
			print "pushParserStateAfterWordToken -> 0 (Conforming)\n" if ($localDebug || $parseDebug);
			print "inClassConformingToProtocol -> 1\n" if ($localDebug || $parseDebug);
			$pushParserStateAfterWordToken = 0;
			$parserState->{inClassConformingToProtocol} = 1;
			$occPushParserStateOnWordTokenAfterNext = 0;
		} elsif ($part =~ />/ && $parserState->{classIsObjC} && $parserState->{inClassConformingToProtocol}) {
			print "inClassConformingToProtocol -> 0\n" if ($localDebug || $parseDebug);
			$pushParserStateAfterToken = 1;
			print "pushParserStateAfterWordToken -> 1 (Conforming)\n" if ($localDebug || $parseDebug);
			$parserState->{inClassConformingToProtocol} = 0;
		} else {
			$parserState->{categoryClass} .= $part;
		}
	    } elsif ($parserState->{inClass} == 2) {
		print "INCLASS2\n" if ($parseDebug);
		if ($part eq ")") {
			$parserState->{inClass} = 1;
			$parserState->{lastTreeNode} = $treeCur;
			print "parserState pushed onto stack[cparen2]\n" if ($parserStackDebug);
			$curline = "";
			push(@parserStack, $parserState);
			$parserState = HeaderDoc::ParserState->new();
			$parserState->{skiptoken} = 1;
			$parserState->{lang} = $lang;
			$parserState->{inputCounter} = $inputCounter;
			$parserState->{initbsCount} = scalar(@braceStack);
		} elsif ($part eq ":") {
			$parserState->{inClass} = 1;
			if ($parserState->{classIsObjC}) {
				print "occPushParserStateOnWordTokenAfterNext -> 2\n" if ($localDebug || $parseDebug);
				$occPushParserStateOnWordTokenAfterNext = 2;
			} else {
				$pushParserStateAfterWordToken = 2;
			}
		} elsif ($part =~ /\w/) {
			# skip the class name itself.
			$parserState->{inClass} = 3;
		}
	    } elsif ($parserState->{inClass} == 1) {
		print "INCLASS1\n" if ($parseDebug);
		# print "inclass Part is $part\n";
		if ($part eq ":") {
			print "INCLASS COLON\n" if ($parseDebug);
			$parserState->{forceClassName} = $parserState->{sodname};
			$parserState->{forceClassSuper} = "";
			# print "XSUPER: $parserState->{forceClassSuper}\n";
		} elsif ($part eq "{" || $part eq ";") {
			print "INCLASS BRCSEMI\n" if ($parseDebug);
			$parserState->{forceClassDone} = 1;
			if ($parserState->{classIsObjC} && $part eq "{") {
				$parserState->{lastTreeNode} = $treeCur;
				print "parserState pushed onto stack[OCC-BRCSEMI]\n" if ($parserStackDebug);
				$curline = "";
				push(@parserStack, $parserState);
				$parserState = HeaderDoc::ParserState->new();
				$parserState->{skiptoken} = 0;
				$parserState->{lang} = $lang;
				$parserState->{inputCounter} = $inputCounter;
				$parserState->{initbsCount} = scalar(@braceStack) + 1; # NOTE: add one here because it will change in the SWITCH to follow.
				$parserState->{noInsert} = $setNoInsert;
				$setNoInsert = 0;
				$pushParserStateAtBrace = 0;
				$occPushParserStateOnWordTokenAfterNext = 0;
				$pushParserStateAfterToken = 1;
			} elsif ($part eq ";") {
				$pushParserStateAtBrace = 0;
				$occPushParserStateOnWordTokenAfterNext = 0;
				$pushParserStateAfterToken = 0;
			}
		} elsif ($parserState->{forceClassName} && !$parserState->{forceClassDone}) {
			print "INCLASS ADD\n" if ($parseDebug);
			if ($part =~ /[\n\r]/) {
				$parserState->{forceClassSuper} .= " ";
			} else {
				$parserState->{forceClassSuper} .= $part;
			}
			# print "SUPER IS $parserState->{forceClassSuper}\n";
		} elsif ($part =~ /</ && $parserState->{classIsObjC} && $occPushParserStateOnWordTokenAfterNext) {
			print "INCLASS <\n" if ($parseDebug);
			print "pushParserStateAfterWordToken -> 0 (Conforming)\n" if ($localDebug || $parseDebug);
			print "inClassConformingToProtocol -> 1\n" if ($localDebug || $parseDebug);
			$pushParserStateAfterWordToken = 0;
			$parserState->{inClassConformingToProtocol} = 1;
			$occPushParserStateOnWordTokenAfterNext = 0;
		} elsif ($part =~ />/ && $parserState->{classIsObjC} && $parserState->{inClassConformingToProtocol}) {
			print "INCLASS >\n" if ($parseDebug);
			print "inClassConformingToProtocol -> 0\n" if ($localDebug || $parseDebug);
			$pushParserStateAfterToken = 1;
			print "pushParserStateAfterWordToken -> 1 (Conforming)\n" if ($localDebug || $parseDebug);
			$parserState->{inClassConformingToProtocol} = 0;
		} elsif ($occPushParserStateOnWordTokenAfterNext && $part =~ /\w/) {
			print "INCLASS OCCSUPER\n" if ($parseDebug);
			$parserState->{occSuper} = $part;
		} elsif (!$parserState->{classIsObjC}) {
			print "INCLASS NOTOBJC (OTHER)\n" if ($parseDebug);
			if ($part =~ /[\*\(]/) {
				print "INCLASS DROP\n" if ($parseDebug);
				$parserState->{inClass} = 0; # We're an instance.  Either a variable or a function.
				$parserState->{sodtype} = $parserState->{preclasssodtype} . $parserState->{sodtype};
			}
		# } else {
			# print "BUG\n";
		}
	    };
	    SWITCH: {

		# Macro handlers

		(($parserState->{inMacro} == 1) && ($part eq "define")) && do{
			# define may be a multi-line macro
			print "INMACRO AND DEFINE\n" if ($parseDebug || $localDebug);
			$parserState->{inMacro} = 3;
			$parserState->{sodname} = "";
			my $pound = $treeCur->token();
			if ($pound eq "$sopreproc") {
				$treeNest = 2;
				$treePopOnNewLine = 2;
				$pound .= $part;
				$treeCur->token($pound);
			}
			last SWITCH;
		};
		(($parserState->{inMacro} == 1) && ($part =~ /(if|ifdef|ifndef|endif|else|pragma|import|include)/o)) && do {
			print "INMACRO AND IF/IFDEF/IFNDEF/ENDIF/ELSE/PRAGMA/IMPORT/INCLUDE\n"  if ($parseDebug || $localDebug);
			# these are all single-line macros
			$parserState->{inMacro} = 4;
			$parserState->{sodname} = "";
			my $pound = $treeCur->token();
			if ($pound eq "$sopreproc") {
				$treeNest = 2;
				$treePopOnNewLine = 1;
				$pound .= $part;
				$treeCur->token($pound);
				if ($part eq "endif") {
					# the rest of the line is not part of the macro
					$treeNest = 0;
					$treePopOnNewLine = 0;
					$treeSkip = 1;
				}
			}
			last SWITCH;
		};
		(($parserState->{inMacroLine} == 1) && ($part =~ /(if|ifdef|ifndef|endif|else|pragma|import|include|define)/o)) && do {
			print "INMACROLINE AND IF/IFDEF/IFNDEF/ENDIF/ELSE/PRAGMA/IMPORT/INCLUDE\n" if ($parseDebug || $localDebug);
			my $pound = $treeCur->token();
			if ($pound eq "$sopreproc") {
				$pound .= $part;
				$treeCur->token($pound);
				if ($part =~ /define/o) {
					$treeNest = 2;
					$treePopOnNewLine = 2;
				} elsif ($part eq "endif") {
					# the rest of the line is not part of the macro
					$treeNest = 0;
					$treePopOnNewLine = 0;
					$treeSkip = 1;
				} else {
					$treeNest = 2;
					$treePopOnNewLine = 1;
				}
			}
			last SWITCH;
		};
		($parserState->{inMacro} == 1 && ($part ne $soc) && ($part ne $eoc)) && do {
			print "INMACRO IS 1, CHANGING TO 2 (NO PROCESSING)\n" if ($parseDebug || $localDebug);
			# error case.
			$parserState->{inMacro} = 2;
			last SWITCH;
		};
		($parserState->{inMacro} > 1 && $part ne "//" && $part !~ /[\n\r]/ && ($part ne $soc) && ($part ne $eoc)) && do {
			print "INMACRO > 1, PART NE //" if ($parseDebug || $localDebug);
			print "PART: $part\n" if ($macroDebug);
			if ($parserState->{seenMacroPart} && $HeaderDoc::truncate_inline) {
				if (!(scalar(@braceStack) - $parserState->{initbsCount})) {
					if ($part =~ /\s/o && $parserState->{macroNoTrunc} == 1) {
						$parserState->{macroNoTrunc} = 0;
					} elsif ($part =~ /[\{\(]/o) {
						if (!$parserState->{macroNoTrunc}) {
							# $parserState->{seenBraces} = 1;
							$HeaderDoc::hidetokens = 3;
						}
					} else {
						$parserState->{macroNoTrunc} = 2;
					}
				}
			}
			if ($part =~ /[\{\(]/o) {
				push(@braceStack, $part);
				print "PUSH\n" if ($macroDebug);
			} elsif ($part =~ /[\}\)]/o) {
				if ($part ne peekmatch(\@braceStack, $filename, $inputCounter)) {
					warn("$filename:$inputCounter:Initial braces in macro name do not match.\nWe may have a problem.\n");
				}
				pop(@braceStack);
				print "POP\n" if ($macroDebug);
			}

			if ($part =~ /\S/o) {
				$parserState->{seenMacroPart} = 1;
				$parserState->{lastsymbol} = $part;
				if (($parserState->{sodname} eq "") && ($parserState->{inMacro} == 3)) {
					print "DEFINE NAME IS $part\n" if ($macroDebug);
					$parserState->{sodname} = $part;
				}
			}
			$lastchar = $part;
			last SWITCH;
		};

		# Regular expression handlers

		# print "IRE: $inRegexp IRT: $inRegexpTrailer IS: $parserState->{inString} ICo $parserState->{inComment} ILC: $parserState->{inInlineComment} ICh $parserState->{inChar}\n";
		(length($regexppattern) && $part =~ /^($regexppattern)$/ && !($inRegexp || $inRegexpTrailer || $parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) && do {
			my $match = $1;
			print "REGEXP WITH PREFIX\n" if ($regexpDebug);
			$regexpNoInterpolate = 0;
			if ($match =~ /^($singleregexppattern)$/) {
				# e.g. perl PATTERN?
				$inRegexp = 2;
			} else {
				$inRegexp = 4;
				# print "REGEXP PART IS \"$part\"\n";
				if ($part eq "tr") { $regexpNoInterpolate = 1; }
				# if ($part =~ /tr/) { $regexpNoInterpolate = 1; }
			}
			last SWITCH;
		}; # end regexppattern
		(($inRegexp || $parserState->{lastsymbol} eq "~") && (length($regexpcharpattern) && $part =~ /^($regexpcharpattern)$/ && (!scalar(@regexpStack) || $part eq peekmatch(\@regexpStack, $filename, $inputCounter)))) && do {
			print "REGEXP?\n" if ($regexpDebug);
			if (!$inRegexp) {
				$inRegexp = 2;
			}

			if ($lasttoken eq "\\") {
				# jump to next match.
				$lasttoken = $part;
				$parserState->{lastsymbol} = $part;
				next SWITCH;
			}
print "REGEXP POINT A\n" if ($regexpDebug);
			$lasttoken = $part;
			$parserState->{lastsymbol} = $part;

			if ($part eq "#" &&
			    ((scalar(@regexpStack) != 1) || 
			     (peekmatch(\@regexpStack, $filename, $inputCounter) ne "#"))) {
				if ($nextpart =~ /^\s/o) {
					# it's a comment.  jump to next match.
					next SWITCH;
				}
			}
print "REGEXP POINT B\n" if ($regexpDebug);

			if (!scalar(@regexpStack)) {
				push(@regexpStack, $part);
				$inRegexp--;
			} else {
				my $match = peekmatch(\@regexpStack, $filename, $inputCounter);
				my $tos = pop(@regexpStack);
				if (!scalar(@regexpStack) && ($match eq $part)) {
					$inRegexp--;
					if ($inRegexp == 2 && $tos eq "/") {
						# we don't double the slash in the
						# middle of a s/foo/bar/g style
						# expression.
						$inRegexp--;
					}
					if ($inRegexp) {
						push(@regexpStack, $tos);
					}
				} elsif (scalar(@regexpStack) == 1) {
					push(@regexpStack, $tos);
					if ($tos =~ /['"`]/o || $regexpNoInterpolate) {
						# these don't interpolate.
						next SWITCH;
					}
				} else {
					push(@regexpStack, $tos);
					if ($tos =~ /['"`]/o || $regexpNoInterpolate) {
						# these don't interpolate.
						next SWITCH;
					}
					push(@regexpStack, $part);
				}
			}
print "REGEXP POINT C\n" if ($regexpDebug);
			if (!$inRegexp) {
				$inRegexpTrailer = 2;
			}
			last SWITCH;
		}; # end regexpcharpattern

		# Start of preprocessor macros

		($part eq "$sopreproc") && do {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				if ($parserState->{onlyComments}) {
					print "inMacro -> 1\n" if ($macroDebug);
					$parserState->{inMacro} = 1;
					# $continue = 0;
		    			# print "continue -> 0 [1]\n" if ($localDebug || $macroDebug);
				} elsif ($curline =~ /^\s*$/o) {
					$parserState->{inMacroLine} = 1;
					print "IML\n" if ($localDebug);
				} elsif ($postPossNL) {
					print "PRE-IML \"$curline\"\n" if ($localDebug || $macroDebug);
					$treeCur = $treeCur->addSibling("\n", 0);
					bless($treeCur, "HeaderDoc::ParseTree");
					$parserState->{inMacroLine} = 1;
					$postPossNL = 0;
				}
			    }
			};

		# Start of token-delimited functions and procedures (e.g.
		# Pascal and PHP)

		($part eq "$sofunction" || $part eq "$soprocedure") && do {
				$parserState->{sodclass} = "function";
				print "K&R C FUNCTION FOUND [1].\n" if ($localDebug);
				$parserState->{kr_c_function} = 1;
				$parserState->{typestring} = "function";
				$parserState->{startOfDec} = 2;
				$parserState->{namePending} = 1;
				# if (!$parserState->{seenBraces}) { # TREEDONE
					# $treeNest = 1;
					# $treePopTwo++;
					# push(@treeStack, $treeCur);
					# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
					# bless($treeCur, "HeaderDoc::ParseTree");
				# }
				print "namePending -> 1 [1]\n" if ($parseDebug);
				last SWITCH;
			};

		# C++ destructor handler.

		($part =~ /\~/o && $lang eq "C" && $sublang eq "cpp" && !!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) && do {
				print "TILDE\n" if ($localDebug);
				$parserState->{seenTilde} = 2;
				$lastchar = $part;
				$parserState->{onlyComments} = 0;
				# $name .= '~';
				last SWITCH;
			};

		# Objective-C method handler.

		($part =~ /[-+]/o && $parserState->{onlyComments}) && do {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				print "OCCMETHOD\n" if ($localDebug);
				# Objective C Method.
				$parserState->{occmethod} = 1;
				$parserState->{occmethodtype} = $part;
				$lastchar = $part;
				$parserState->{onlyComments} = 0;
				print "[a]onlyComments -> 0\n" if ($macroDebug);
				if (!$parserState->{seenBraces}) { # TREEDONE
				    if (!$parserState->{hollow}) {
					print "SETHOLLOW -> 1\n" if ($parserStackDebug);
					$sethollow = 1;
				    }
				    $treeNest = 1;
				    $treePopTwo = 1;
				}
			    }
			    last SWITCH;
			};

		# Newline handler.

		($part =~ /[\n\r]/o) && do {
				$treepart = $part;
				if ($inRegexp) {
					warn "$filename:$inputCounter:multi-line regular expression\n";
				}
				print "NLCR\n" if ($tsDebug || $treeDebug || $localDebug);
				if ($lastchar !~ /[\,\;\{\(\)\}]/o && $nextpart !~ /[\{\}\(\)]/o) {
					if ($lastchar ne "*/" && $nextpart ne "/*") {
						if (!$parserState->{inMacro} && !$parserState->{inMacroLine} && !$treePopOnNewLine) {
							print "NL->SPC\n" if ($localDebug);
							$part = " ";
							print "LC: $lastchar\n" if ($localDebug);
							print "NP: $nextpart\n" if ($localDebug);
							$postPossNL = 2;
						} else {
							$parserState->{inMacroLine} = 0;
						}
					}
				}
				if ($treePopOnNewLine < 0) {
					# pop once for //, possibly again for macro
					$treePopOnNewLine = 0 - $treePopOnNewLine;
					$treeCur = $treeCur->addSibling($treepart, 0);
					bless($treeCur, "HeaderDoc::ParseTree");
					# push(@treeStack, $treeCur);
					$treeSkip = 1;
					$treeCur = pop(@treeStack);
					if (!$treeCur) {
						$treeCur = $treeTop;
						warn "$filename:$inputCounter:Attempted to pop off top of tree\n";
					}
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print "TSPOP [1]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
				}
				if ($treePopOnNewLine == 1 || ($treePopOnNewLine && $parserState->{lastsymbol} ne "\\")) {
					$treeCur = $treeCur->addSibling($treepart, 0);
					bless($treeCur, "HeaderDoc::ParseTree");
					# push(@treeStack, $treeCur);
					$treeSkip = 1;
					$treeCur = pop(@treeStack);
					if (!$treeCur) {
						$treeCur = $treeTop;
						warn "$filename:$inputCounter:Attempted to pop off top of tree\n";
					}
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print "TSPOP [1a]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
					$treePopOnNewLine = 0;
				}
				next SWITCH;
			};

		# C++ template handlers

		($part eq $sotemplate && !$parserState->{seenBraces}) && do {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				print "inTemplate -> 1\n" if ($localDebug);
	print "SBS: " . scalar(@braceStack) . ".\n" if ($localDebug);
				$parserState->{inTemplate} = 1;
				if (!(scalar(@braceStack) - $parserState->{initbsCount})) {
					$parserState->{preTemplateSymbol} = $parserState->{lastsymbol};
				}
				$parserState->{lastsymbol} = "";
				$lastchar = $part;
				$parserState->{onlyComments} = 0;
				push(@braceStack, $part); pbs(@braceStack);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeNest = 1;
					# push(@treeStack, $treeCur);
					# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
					# bless($treeCur, "HeaderDoc::ParseTree");
				}
				print "[b]onlyComments -> 0\n" if ($macroDebug);
			    }
			    last SWITCH;
			};
		($part eq $eotemplate && !$parserState->{seenBraces}) && do {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && (!(scalar(@braceStack)-$parserState->{initbsCount}) || $parserState->{inTemplate})) {
				if ($parserState->{inTemplate})  {
					print "parserState->{inTemplate} -> 0\n" if ($localDebug);
					$parserState->{inTemplate} = 0;
					$parserState->{lastsymbol} = "";
					$lastchar = $part;
					$curline .= " ";
					$parserState->{onlyComments} = 0;
					print "[c]onlyComments -> 0\n" if ($macroDebug);
				}
				my $top = pop(@braceStack);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeCur->addSibling($part, 0); $treeSkip = 1;
					$treeCur = pop(@treeStack) || $treeTop;
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print "TSPOP [2]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
				}
				if ($top ne "$sotemplate") {
					warn("$filename:$inputCounter:Template (angle) brackets do not match.\nWe may have a problem.\n");
				}
			    }
			    last SWITCH;
			};

		#
		# Handles C++ access control state, e.g. "public:"
		#

		($part eq ":") && do {
			# fall through to next colon handling case if we fail.
			if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				if (length($accessregexp) && ($lastnspart =~ /$accessregexp/)) {
					# We're special.
					print "PERMANENT ACS CHANGE from $HeaderDoc::AccessControlState to $1\n" if ($localDebug);
					$parserState->{sodname} = "";
					$parserState->{typestring} = "";
					$parserState->{hollow} = undef;
					$parserState->{onlyComments} = 1;
					$hollowskip = 1;
					$HeaderDoc::AccessControlState = $1;
					$lastACS = $1;
					last SWITCH;
				}
			}
		    };

		(length($accessregexp) && ($part =~ /$accessregexp/)) && do {
			if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				# We're special.
				print "TEMPORARY ACS CHANGE from $HeaderDoc::AccessControlState to $1\n" if ($localDebug);
				$parserState->{sodname} = "";
				$lastACS = $HeaderDoc::AccessControlState;
				$HeaderDoc::AccessControlState = $1;
			} else {
				next SWITCH;
			}
		};

		#
		# C++ copy constructor handler.  For example:
		# 
		# char *class(void *a, void *b) :
		#       class(pri_type, pri_type);
		#
		($part eq ":") && do {
			if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				if ($parserState->{occmethod}) {
				    $parserState->{name} = $parserState->{lastsymbol};
				    $parserState->{occmethodname} .= "$parserState->{lastsymbol}:";
				    # Start doing line splitting here.
				    # Also, capture the method's name.
				    if ($parserState->{occmethod} == 1) {
					$parserState->{occmethod} = 2;
					if (!$prespace) { $prespaceadjust = 4; }
					$parserState->{onlyComments} = 0;
					print "[d]onlyComments -> 0\n" if ($macroDebug);
				    }
				} else {
				    if ($lang eq "C" && $sublang eq "cpp") {
					if (!(scalar(@braceStack)-$parserState->{initbsCount}) && $parserState->{sodclass} eq "function") {
					    $inPrivateParamTypes = 1;
					    $declaration .= "$curline";
					    $publicDeclaration = $declaration;
					    $declaration = "";
					} else {
					    next SWITCH;
					}
					if (!$parserState->{stackFrozen}) {
						if (scalar(@{$parserState->{parsedParamList}})) {
						    foreach my $node (@{$parserState->{parsedParamList}}) {
							$node =~ s/^\s*//so;
							$node =~ s/\s*$//so;
							if (length($node)) {
								push(@{$parserState->{pplStack}}, $node)
							}
						    }
						    @{$parserState->{parsedParamList}} = ();
						    print "parsedParamList pushed\n" if ($parmDebug);
						}
						# print "SEOPPLS\n";
						# for my $item (@{$parserState->{pplStack}}) {
							# print "PPLS: $item\n";
						# }
						# print "OEOPPLS\n";
						@{$parserState->{freezeStack}} = @{$parserState->{pplStack}};
						$parserState->{frozensodname} = $parserState->{sodname};
						$parserState->{stackFrozen} = 1;
					}
				    } else {
					next SWITCH;
				    }
				}
			    if (!$parserState->{seenBraces} && !$parserState->{occmethod}) { # TREEDONE
				    # $treeCur->addSibling($part, 0); $treeSkip = 1;
				    $treeNest = 1;
				    $treePopTwo = 1;
				    # $treeCur = pop(@treeStack) || $treeTop;
				    # bless($treeCur, "HeaderDoc::ParseTree");
			    }
			    last SWITCH;
			    } else {
				next SWITCH;
			    }
			};

		# Non-newline, non-carriage-return whitespace handler.

		($part =~ /\s/o) && do {
				# just add white space silently.
				# if ($part eq "\n") { $parserState->{lastsymbol} = ""; };
				$lastchar = $part;
				last SWITCH;
		};

		# backslash handler (largely useful for macros, strings).

		($part =~ /\\/o) && do { $parserState->{lastsymbol} = $part; $lastchar = $part; };

		# quote and bracket handlers.

		($part eq "\"") && do {
				print "dquo\n" if ($localDebug);

				# print "QUOTEDEBUG: CURSTRING IS '$curstring'\n";
				# print "QUOTEDEBUG: CURLINE IS '$curline'\n";
				if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
					$parserState->{onlyComments} = 0;
					print "[e]onlyComments -> 0\n" if ($macroDebug);
					print "LASTTOKEN: $lasttoken\nCS: $curstring\n" if ($localDebug);
					if (($lasttoken !~ /\\$/o) && ($curstring !~ /\\$/o)) {
						if (!$parserState->{inString}) {
						    if (!$parserState->{seenBraces}) { # TREEDONE
							$treeNest = 1;
							# push(@treeStack, $treeCur);
							# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
							# bless($treeCur, "HeaderDoc::ParseTree");
						    }
						} else {
						    if (!$parserState->{seenBraces}) { # TREEDONE
							$treeCur->addSibling($part, 0); $treeSkip = 1;
							$treeCur = pop(@treeStack) || $treeTop;
							$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
							$treeCur = $treeCur->lastSibling();
							$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
							print "TSPOP [3]\n" if ($tsDebug || $treeDebug);
							bless($treeCur, "HeaderDoc::ParseTree");
						    }
						}
						$parserState->{inString} = (1-$parserState->{inString});
					}
				}
				$lastchar = $part;
				$parserState->{lastsymbol} = "";

				last SWITCH;
			};
		($part eq "[") && do {
				print "lbracket\n" if ($localDebug);

				if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inString})) {
					$parserState->{onlyComments} = 0;
					print "[f]onlyComments -> 0\n" if ($macroDebug);
				}
				push(@braceStack, $part); pbs(@braceStack);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeNest = 1;
					# push(@treeStack, $treeCur);
					# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
					# bless($treeCur, "HeaderDoc::ParseTree");
				}
				$curline = spacefix($curline, $part, $lastchar);
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				last SWITCH;
			};
		($part eq "]") && do {
				print "rbracket\n" if ($localDebug);

				if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inString})) {
					$parserState->{onlyComments} = 0;
					print "[g]onlyComments -> 0\n" if ($macroDebug);
				}
				my $top = pop(@braceStack);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeCur->addSibling($part, 0); $treeSkip = 1;
					$treeCur = pop(@treeStack) || $treeTop;
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print "TSPOP [4]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
				}
				if ($top ne "[") {
					warn("$filename:$inputCounter:Square brackets do not match.\nWe may have a problem.\n");
					warn("Declaration to date: $declaration$curline\n");
				}
				pbs(@braceStack);
				$curline = spacefix($curline, $part, $lastchar);
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				last SWITCH;
			};
		($part eq "'") && do {
				print "squo\n" if ($localDebug);

				if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inString})) {
					if ($lastchar ne "\\") {
						$parserState->{onlyComments} = 0;
						print "[h]onlyComments -> 0\n" if ($macroDebug);
						if (!$parserState->{inChar}) {
						    if (!$parserState->{seenBraces}) { # TREEDONE
							$treeNest = 1;
							# push(@treeStack, $treeCur);
							# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
							# bless($treeCur, "HeaderDoc::ParseTree");
						    }
						} else {
						    if (!$parserState->{seenBraces}) { # TREEDONE
							$treeCur->addSibling($part, 0); $treeSkip = 1;
							$treeCur = pop(@treeStack) || $treeTop;
							$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
							$treeCur = $treeCur->lastSibling();
							$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
							print "TSPOP [5]\n" if ($tsDebug || $treeDebug);
							bless($treeCur, "HeaderDoc::ParseTree");
						    }
						}
						$parserState->{inChar} = !$parserState->{inChar};
					}
					if ($lastchar =~ /\=$/o) {
						$curline .= " ";
					}
				}
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				last SWITCH;
			};

		# Inline comment (two slashes in c++, hash in perl/shell)
		# handler.

		($part eq $ilc && ($lang ne "perl" || $lasttoken ne "\$")) && do {
				print "ILC\n" if ($localDebug || $ilcDebug);

				if (!($parserState->{inComment} || $parserState->{inChar} || $parserState->{inString} || $inRegexp)) {
					$parserState->{inInlineComment} = 4;
					print "inInlineComment -> 1\n" if ($ilcDebug);
					$curline = spacefix($curline, $part, $lastchar, $soc, $eoc, $ilc);
					if (!$parserState->{seenBraces}) { # TREEDONE
						$treeNest = 1;

						if (!$treePopOnNewLine) {
							$treePopOnNewLine = 1;
						} else {
							$treePopOnNewLine = 0 - $treePopOnNewLine;
						}
						print "treePopOnNewLine -> $treePopOnNewLine\n" if ($ilcDebug);

						# $treeCur->addSibling($part, 0); $treeSkip = 1;
						# $treePopOnNewLine = 1;
						# $treeCur = pop(@treeStack) || $treeTop;
						# bless($treeCur, "HeaderDoc::ParseTree");
					}
				} elsif ($parserState->{inComment}) {
					my $linenum = $inputCounter + $fileoffset;
					if (!$argparse) {
						# We've already seen these.
						if ($nestedcommentwarn) {
							warn("$filename:$linenum:Nested comment found [1].  Ignoring.\n");
						}
						# This isn't really a problem.
						# Don't warn to avoid bogus
						# warnings for apple_ref and
						# URL markup in comments.
					}
					# warn("XX $argparse XX $inputCounter XX $fileoffset XX\n");
				}
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				last SWITCH;
			};

		# Standard comment handlers: soc = start of comment,
		# eoc = end of comment.

		($part eq $soc) && do {
				print "SOC\n" if ($localDebug);

				if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} || $parserState->{inString})) {
					$parserState->{inComment} = 4; 
					$curline = spacefix($curline, $part, $lastchar);
					if (!$parserState->{seenBraces}) {
						$treeNest = 1;
						# print "TSPUSH\n" if ($tsDebug || $treeDebug);
						# push(@treeStack, $treeCur);
						# $treeCur = $treeCur->addChild("", 0);
						# bless($treeCur, "HeaderDoc::ParseTree");
					}
				} elsif ($parserState->{inComment}) {
					my $linenum = $inputCounter + $fileoffset;
					# Modern compilers shouldn't have trouble with this.  It occurs |
					# frequently in apple_ref markup (e.g. //apple_ref/C/instm/    \|/
					# IOFireWireDeviceInterface/AddIsochCallbackDispatcherToRunLoop/*Add
					# IsochCallbackDispatcherToRunLoopIOFireWireLibDeviceRefCFRunLoopRef)
					if ($nestedcommentwarn) {
						warn("$filename:$linenum:Nested comment found [2].  Ignoring.\n");
					}
				}
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				last SWITCH;
			};
		($part eq $eoc) && do {
				print "EOC\n" if ($localDebug);

				if ($parserState->{inComment} && !($parserState->{inInlineComment} || $parserState->{inChar} || $parserState->{inString})) {
					$parserState->{inComment} = 0;
					$curline = spacefix($curline, $part, $lastchar);
					$ppSkipOneToken = 1;
					if (!$parserState->{seenBraces}) {
                                        	$treeCur->addSibling($part, 0); $treeSkip = 1;
                                        	$treeCur = pop(@treeStack) || $treeTop;
						$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
						$treeCur = $treeCur->lastSibling();
						$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
                                        	print "TSPOP [6]\n" if ($tsDebug || $treeDebug);
                                        	bless($treeCur, "HeaderDoc::ParseTree");
					}
}
				elsif (!$parserState->{inComment}) {
					my $linenum = $inputCounter + $fileoffset;
					warn("$filename:$linenum:Unmatched close comment tag found.  Ignoring.\n");
				} elsif ($parserState->{inInlineComment}) {
					my $linenum = $inputCounter + $fileoffset;
					# We'll leave this one on for now.
					if (1 || $nestedcommentwarn) {
						warn("$filename:$linenum:Nested comment found [3].  Ignoring.\n");
					}
				}
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				last SWITCH;
			};

		# Parenthesis and brace handlers.

		($part eq "(") && do {
			    my @tempppl = undef;
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && !($inRegexp && $regexpNoInterpolate)) {
			        if (!(scalar(@braceStack)-$parserState->{initbsCount})) {
				    # start parameter parsing after this token
				    print "parsedParamParse -> 2\n" if ($parmDebug);
				    $parsedParamParse = 2;
				    print "parsedParamList wiped\n" if ($parmDebug);
				    @tempppl = @{$parserState->{parsedParamList}};
				    @{$parserState->{parsedParamList}} = ();
				    $parsedParam = "";
			        }
				$parserState->{onlyComments} = 0;
				print "[i]onlyComments -> 0\n" if ($macroDebug);
				if ($parserState->{simpleTypedef} && !(scalar(@braceStack)- $parserState->{initbsCount})) {
					$parserState->{simpleTypedef} = 0;
					$parserState->{simpleTDcontents} = "";
					$parserState->{sodname} = $parserState->{lastsymbol};
					$parserState->{sodclass} = "function";

					# @@@ DAG CHECKME.  I think this needed
					# to be changed to respect freezereturn
					# and hollow, but in the unlikely event
					# that we should start seeing any weird
					# "missing return type info" bugs,
					# this next line might need to be
					# put back in rather than the lines
					# that follow it.

					# $parserState->{returntype} = "$declaration$curline";

					if (!$parserState->{freezereturn} && $parserState->{hollow}) {
						$parserState->{returntype} = "$declaration$curline";
 	    				} elsif (!$parserState->{freezereturn} && !$parserState->{hollow}) {
						$parserState->{returntype} = "$curline";
						$declaration = "";
					}
				}
				$parserState->{posstypesPending} = 0;
				if ($parserState->{callbackNamePending} == 2) {
					$parserState->{callbackNamePending} = 3;
					print "callbackNamePending -> 3\n" if ($localDebug || $cbnDebug);
				}
				print "lparen\n" if ($localDebug);

				push(@braceStack, $part); pbs(@braceStack);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeNest = 1;
					# push(@treeStack, $treeCur);
					# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
					# bless($treeCur, "HeaderDoc::ParseTree");
				}
				$curline = spacefix($curline, $part, $lastchar);

				print "LASTCHARCHECK: \"$lastchar\" \"$lastnspart\" \"$curline\".\n" if ($localDebug);
				if ($lastnspart eq ")") {  # || $curline =~ /\)\s*$/so
print "HERE: DEC IS $declaration\nENDDEC\nCURLINE IS $curline\nENDCURLINE\n" if ($localDebug);
				    # print "CALLBACKMAYBE: $parserState->{callbackNamePending} $parserState->{sodclass} ".scalar(@braceStack)."\n";
				    print "SBS: ".scalar(@braceStack)."\n" if ($localDebug);
				    if (!$parserState->{callbackNamePending} && ($parserState->{sodclass} eq "function") && ((scalar(@braceStack)-$parserState->{initbsCount}) == 1)) { #  && $argparse
					# Guess it must be a callback anyway.
					my $temp = pop(@tempppl);
					$parserState->{callbackName} = $temp;
					$parserState->{name} = "";
					$parserState->{sodclass} = "";
					$parserState->{sodname} = "";
					print "CALLBACKHERE ($temp)!\n" if ($cbnDebug);
				    }
				    if ($declaration =~ /.*\n(.*?)\n$/so) {
					my $lastline = $1;
print "LL: $lastline\nLLDEC: $declaration" if ($localDebug);
					$declaration =~ s/(.*)\n(.*?)\n$/$1\n/so;
					$curline = "$lastline $curline";
					$curline =~ s/^\s*//so;
					$prespace -= 4;
					$prespaceadjust += 4;
					
					$forcenobreak = 1;
print "NEWDEC: $declaration\nNEWCURLINE: $curline\n" if ($localDebug);
				    } elsif (length($declaration) && $callback_typedef_and_name_on_one_line) {
print "SCARYCASE\n" if ($localDebug);
					$declaration =~ s/\n$//so;
					$curline = "$declaration $curline";
					$declaration = "";
					$prespace -= 4;
					$prespaceadjust += 4;
					
					$forcenobreak = 1;
				    }
				} else { print "OPARENLC: \"$lastchar\"\nCURLINE IS: \"$curline\"\n" if ($localDebug);}

				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				if ($parserState->{startOfDec} == 2) {
					$parserState->{sodclass} = "function";
					$parserState->{freezereturn} = 1;
					$parserState->{returntype} =~ s/^\s*//so;
					$parserState->{returntype} =~ s/\s*$//so;
				}
				$parserState->{startOfDec} = 0;
				if ($curline !~ /\S/o) {
					# This is the first symbol on the line.
					# adjust immediately
					$prespace += 4;
					print "PS: $prespace immediate\n" if ($localDebug);
				} else {
					$prespaceadjust += 4;
					print "PSA: $prespaceadjust\n" if ($localDebug);
				}
			    }
			    print "OUTGOING CURLINE: \"$curline\"\n" if ($localDebug);
			    last SWITCH;
			};
		($part eq ")") && do {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && !($inRegexp && $regexpNoInterpolate && (peek(\@regexpStack) ne "("))) {
			        if ((scalar(@braceStack)-$parserState->{initbsCount}) == 1) {
				    # stop parameter parsing
				    $parsedParamParse = 0;
				    print "parsedParamParse -> 0\n" if ($parmDebug);
				    $parsedParam =~ s/^\s*//so; # trim leading space
				    $parsedParam =~ s/\s*$//so; # trim trailing space

				    if ($parsedParam ne "void") {
					# ignore foo(void)
					push(@{$parserState->{parsedParamList}}, $parsedParam);
					print "pushed $parsedParam into parsedParamList [1]\n" if ($parmDebug);
				    }
				    $parsedParam = "";
			        }
				$parserState->{onlyComments} = 0;
				print "[j]onlyComments -> 0\n" if ($macroDebug);
				print "rparen\n" if ($localDebug);


				my $test = pop(@braceStack); pbs(@braceStack);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeCur->addSibling($part, 0); $treeSkip = 1;
					$treeCur = pop(@treeStack) || $treeTop;
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print "TSPOP [6a]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
				}
				if (!($test eq "(")) {		# ) brace hack for vi
					warn("$filename:$inputCounter:Parentheses do not match.\nWe may have a problem.\n");
					warn("Declaration to date: $declaration$curline\n");
				}
				$curline = spacefix($curline, $part, $lastchar);
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				$parserState->{startOfDec} = 0;
				if ($curline !~ /\S/o) {
					# This is the first symbol on the line.
					# adjust immediately
					$prespace -= 4;
					print "PS: $prespace immediate\n" if ($localDebug);
				} else {
					$prespaceadjust -= 4;
					print "PSA: $prespaceadjust\n" if ($localDebug);
				}
			    }
			    last SWITCH;
			};
		(casecmp($part, $lbrace, $case_sensitive)) && do {
			    if ($parserState->{onlyComments} && !$parserState->{inComment} && !$parserState->{inInlineComment} && !$parserState->{inChar} && !($inRegexp && $regexpNoInterpolate) && scalar(@parserStack)) {
				# Somebody put in a brace in the middle of
				# a class or else we're seeing ObjC private
				# class bits.  Either way, throw away the
				# curly brace.

				print "NOINSERT\n" if ($parserStackDebug);

				$pushParserStateAtBrace = 1;
				# $setNoInsert = 1;
				$parserState->{noInsert} = 1;
			    }
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && !($inRegexp && $regexpNoInterpolate)) {
				$parserState->{onlyComments} = 0;
				print "[k]onlyComments -> 0\n" if ($macroDebug);
				if (scalar(@{$parserState->{parsedParamList}})) {
					foreach my $node (@{$parserState->{parsedParamList}}) {
						$node =~ s/^\s*//so;
						$node =~ s/\s*$//so;
						if (length($node)) {
							push(@{$parserState->{pplStack}}, $node)
						}
					}
					@{$parserState->{parsedParamList}} = ();
					print "parsedParamList pushed\n" if ($parmDebug);
				}

				# start parameter parsing after this token
				print "parsedParamParse -> 2\n" if ($parmDebug);
				$parsedParamParse = 2;

				if (!$parserState->{inClass} && ($parserState->{sodclass} eq "function" || $parserState->{inOperator})) {
					$parserState->{seenBraces} = 1;
					if (!$parserState->{stackFrozen}) {
						@{$parserState->{freezeStack}} = @{$parserState->{pplStack}};
						$parserState->{frozensodname} = $parserState->{sodname};
						$parserState->{stackFrozen} = 1;
					}
					@{$parserState->{pplStack}} = ();
				}
				$parserState->{posstypesPending} = 0;
				$parserState->{namePending} = 0;
				$parserState->{callbackNamePending} = -1;
				$parserState->{simpleTypedef} = 0;
				$parserState->{simpleTDcontents} = "";
				print "callbackNamePending -> -1\n" if ($localDebug || $cbnDebug);
				print "lbrace\n" if ($localDebug);

				push(@braceStack, $part); pbs(@braceStack);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeNest = 1;
					print "TN -> 1\n" if ($localDebug);
					# push(@treeStack, $treeCur);
					# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
					# bless($treeCur, "HeaderDoc::ParseTree");
				}
				$curline = spacefix($curline, $part, $lastchar);
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				$parserState->{startOfDec} = 0;
				if ($curline !~ /\S/o) {
					# This is the first symbol on the line.
					# adjust immediately
					$prespace += 4;
					print "PS: $prespace immediate\n" if ($localDebug);
				} else {
					$prespaceadjust += 4;
					print "PSA: $prespaceadjust\n" if ($localDebug);
				}
			    }
			    last SWITCH;
			};
		(casecmp($part, $rbrace, $case_sensitive)) && do {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && !($inRegexp && $regexpNoInterpolate && (peek(\@regexpStack) ne "$lbrace"))) {

				my $oldOC = $parserState->{onlyComments};
				print "rbrace???\n" if ($localDebug);
				# $parserState->{onlyComments} = 0;	# If this is all we've seen, there's either a bug or we're
									# unrolling a class or similar anyway.
				print "[l]onlyComments -> 0\n" if ($macroDebug);

				my $bsCount = scalar(@braceStack);
				if (scalar(@parserStack) && !($bsCount - $parserState->{initbsCount})) {
print "parserState: ENDOFSTATE\n" if ($parserStackDebug);
					if ($parserState->{noInsert} || $oldOC) {
						print "parserState insertion skipped[RBRACE]\n" if ($parserStackDebug);
					} elsif ($parserState->{hollow}) {
						my $treeRef = $parserState->{hollow};

						$parserState->{lastTreeNode} = $treeCur;
						$treeRef->addRawParsedParams(\@{$parserState->{parsedParamList}});
						$treeRef->parserState($parserState);
					} else {
						warn "Couldn't insert info into parse tree[1].\n";
					}

					print "parserState popped from parserStack[rbrace]\n" if ($parserStackDebug);
					$parserState = pop(@parserStack) || $parserState;
					if ($lang eq "php") {
							# print "PHP OUT OF BRACES?: ".scalar(@braceStack)."\n";
						if (scalar(@braceStack) == 1) {
							# PHP classes end at
							# the brace.
							$continue = 0;
						}
					}
					if ($parserState->{noInsert}) {
						# This is to handle the end of
						# the private vars in an
						# Objective C class.
						print "parserState: Hit me.\n" if ($localDebug);
						$parserState = HeaderDoc::ParserState->new();
						$parserState->{skiptoken} = 1;
						$parserState->{lang} = $lang;
						$parserState->{inputCounter} = $inputCounter;
						# It's about to go down by 1.
						$parserState->{initbsCount} = scalar(@braceStack) - 1;
					}
					# $parserState->{onlyComments} = 1;
				} else {
					print "NO CHANGE IN PARSER STATE STACK (nPARSERSTACK = ".scalar(@parserStack).", $bsCount != $parserState->{initbsCount})\n" if ($parseDebug || $parserStackDebug);
				}

				if ((scalar(@braceStack)-$parserState->{initbsCount}) == 1) {
					# stop parameter parsing
					$parsedParamParse = 0;
					print "parsedParamParse -> 0\n" if ($parmDebug);
					$parsedParam =~ s/^\s*//so; # trim leading space
					$parsedParam =~ s/\s*$//so; # trim trailing space

					if (length($parsedParam)) {
						# ignore foo(void)
						push(@{$parserState->{parsedParamList}}, $parsedParam);
						print "pushed $parsedParam into parsedParamList [1b]\n" if ($parmDebug);
					}
					$parsedParam = "";
				} else {
					# start parameter parsing after this token
					print "parsedParamParse -> 2\n" if ($parmDebug);
					$parsedParamParse = 2;
				}

				if (scalar(@{$parserState->{parsedParamList}})) {
					foreach my $node (@{$parserState->{parsedParamList}}) {
						$node =~ s/^\s*//so;
						$node =~ s/\s*$//so;
						if (length($node)) {
							push(@{$parserState->{pplStack}}, $node)
						}
					}
					@{$parserState->{parsedParamList}} = ();
					print "parsedParamList pushed\n" if ($parmDebug);
				}

				print "rbrace\n" if ($localDebug);

				my $test = pop(@braceStack); pbs(@braceStack);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeCur->addSibling($part, 0); $treeSkip = 1;
					$treeCur = pop(@treeStack) || $treeTop;
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print "TSPOP [7]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
				}
				if (!($test eq "$lbrace") && (!length($structname) || (!($test eq $structname) && $structisbrace))) {		# } brace hack for vi.
					warn("$filename:$inputCounter:Braces do not match.\nWe may have a problem.\n");
					warn("Declaration to date: $declaration$curline\n");
				}
				$curline = spacefix($curline, $part, $lastchar);
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				$parserState->{startOfDec} = 0;
				if ($curline !~ /\S/o) {
					# This is the first symbol on the line.
					# adjust immediately
					$prespace -= 4;
					print "PS: $prespace immediate\n" if ($localDebug);
				} else {
					$prespaceadjust -= 4;
					print "PSA: $prespaceadjust\n" if ($localDebug);
				}
			    }
			    last SWITCH;
			};

		# Typedef, struct, enum, and union handlers.

		(length($classregexp) && $part =~ /^\@$/ && !$parserState->{inComment} && !$parserState->{inChar} && !$parserState->{inString} && !$parserState->{inInlineComment}) && do {
				my $temp = "\@".$nextpart;
				if ($temp =~ /$classregexp/) {
					$nextpart = "\@".$nextpart;
					$parserState->{classIsObjC} = 1;
				} elsif ($temp =~ /$classclosebraceregexp/) {
					$nextpart = "\@".$nextpart;
				}
				next SWITCH;
			};
		(length($classclosebraceregexp) && ($part =~ /$classclosebraceregexp/) && !$parserState->{inComment} && !$parserState->{inChar} && !$parserState->{inString} && !$parserState->{inInlineComment}) && do {
				if ($part ne peekmatch(\@braceStack, $filename, $inputCounter)) {
					warn("$filename:inputCounter:Class braces do not match.\nWe may have a problem.\n");
				}
				$parserState->{seenBraces} = 1;
				pop(@braceStack);
				$treeCur->addSibling($part, 0); $treeSkip = 1;
				$treeCur = pop(@treeStack) || $treeTop;
				$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
				$treeCur = $treeCur->lastSibling();
				$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
				print "TSPOP [6]\n" if ($tsDebug || $treeDebug);
				bless($treeCur, "HeaderDoc::ParseTree");
				$part =~ s/^\@//s;
				if ( 1 || $nextpart ne ";") {
					# Objective C protocol/interface declarations end at the close curly brace.
					# No ';' necessary (though we'll eat it if it's there.
					# No, we won't.  Deal with it.
					if (scalar(@parserStack) == 1) {
						# Throw away current parser state, since
						# it will always be empty anyway.
						$parserState = pop(@parserStack) || $parserState;

						$continue = 0;
						print "continue -> 0 [occend]\n" if ($localDebug);
					} else {
					    if (!$parserState->{onlyComments}) {
						# @@@ Process entry here
						if ($parserState->{noInsert}) {
							print "parserState insertion skipped[\@end]\n" if ($parserStackDebug);
						} elsif ($parserState->{hollow}) {
							my $treeRef = $parserState->{hollow};
							$parserState->{lastTreeNode} = $treeCur;

							$treeRef->addRawParsedParams(\@{$parserState->{parsedParamList}});
							$treeRef->parserState($parserState);
						} else {
							warn "Couldn't insert info into parse tree[2].\n";
						}

						print "parserState: Created parser state[1].\n" if ($parserStackDebug);
						print "CURLINE CLEAR[PRS2]\n" if ($localDebug);
						$curline = "";
						$parserState = HeaderDoc::ParserState->new();
						$parserState->{skiptoken} = 1;
						$parserState->{lang} = $lang;
						$parserState->{inputCounter} = $inputCounter;
						$parserState->{initbsCount} = scalar(@braceStack);
					    }
					    print "parserState popped from parserStack[\@end]\n" if ($parserStackDebug);
					    $parserState = pop(@parserStack) || $parserState;
					}
				}
			};
		(!$parserState->{inTemplate} && length($classregexp) && $part =~ /$classregexp/) && do {
			my $localclasstype = $1;
			if ($part =~ /^\@/) { $part =~ s/^\@//s; }
			if (!(scalar(@braceStack)-$parserState->{initbsCount})) {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				print "ITISACLASS\n" if ($localDebug);
				if (!length($parserState->{sodclass})) {
					print "GOOD.\n" if ($localDebug);
					$parserState->{inClass} = 1;
					$pushParserStateAtBrace = 1;
					if ($localclasstype =~ /\@interface/) {
						$parserState->{inClass} = 2;
						$pushParserStateAtBrace = 0;
					} elsif ($localclasstype =~ /\@protocol/) {
						$pushParserStateAtBrace = 0;
						$pushParserStateAfterWordToken = 2;
					}
			    		$parserState->{sodclass} = "class";
					$parserState->{classtype} = $localclasstype;
					$parserState->{preclasssodtype} = $parserState->{sodtype} . $part;
					$parserState->{sodtype} = "";
			    		$parserState->{startOfDec} = 1;

					$parserState->{onlyComments} = 0;
					print "[m]onlyComments -> 0\n" if ($macroDebug);
					$continuation = 1;
					# Get the parse tokens from Utilities.pm.

					if (length($classbraceregexp) && ($localclasstype =~ /$classbraceregexp/)) {
						print "CLASS ($localclasstype) IS A BRACE.\n" if ($localDebug);
						push(@braceStack, $localclasstype); pbs(@braceStack);
						$treeNest = 1;
					# } else {
						# print "CBRE: \"$classbraceregexp\"\n";
					}


					($lang, $sublang) = getLangAndSublangFromClassType($localclasstype);
					$HeaderDoc::lang = $lang;
					$HeaderDoc::sublang = $sublang;

					($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
						$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
						$typedefname, $varname, $constname, $structisbrace, $macronameref,
						$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp)
							= parseTokens($lang, $sublang);
					print "ARP: $accessregexp\n" if ($localDebug);


			    		last SWITCH;
				}
			    }
			}
		};

		($part eq $structname || $part =~ /^enum$/o || $part =~ /^union$/o) && do {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				if ($structisbrace) {
                                	if ($parserState->{sodclass} eq "function") {
                                        	$parserState->{seenBraces} = 1;
						if (!$parserState->{stackFrozen}) {
							@{$parserState->{freezeStack}} = @{$parserState->{pplStack}};
							$parserState->{frozensodname} = $parserState->{sodname};
							$parserState->{stackFrozen} = 1;
						}
						@{$parserState->{pplStack}} = ();
                                	}
                                	$parserState->{posstypesPending} = 0;
                                	$parserState->{callbackNamePending} = -1;
                                	$parserState->{simpleTypedef} = 0;
					$parserState->{simpleTDcontents} = "";
                                	print "callbackNamePending -> -1\n" if ($localDebug || $cbnDebug);
                                	print "lbrace\n" if ($localDebug);

                                	push(@braceStack, $part); pbs(@braceStack);
					if (!$parserState->{seenBraces}) { # TREEDONE
						$treeNest = 1;
						# push(@treeStack, $treeCur);
						# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
						# bless($treeCur, "HeaderDoc::ParseTree");
					}
                                	$curline = spacefix($curline, $part, $lastchar);
                                	$parserState->{lastsymbol} = "";
                                	$lastchar = $part;

                                	$parserState->{startOfDec} = 0;
                                	if ($curline !~ /\S/o) {
                                        	# This is the first symbol on the line.
                                        	# adjust immediately
                                        	$prespace += 4;
                                        	print "PS: $prespace immediate\n" if ($localDebug);
                                	} else {
                                        	$prespaceadjust += 4;
                                        	print "PSA: $prespaceadjust\n" if ($localDebug);
                                	}
				} else {
					if (!$parserState->{simpleTypedef}) {
						$parserState->{simpleTypedef} = 2;
					}
					# if (!$parserState->{seenBraces}) { # TREEDONE
						# $treePopTwo++;
						# $treeNest = 1;
						# push(@treeStack, $treeCur);
						# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
						# bless($treeCur, "HeaderDoc::ParseTree");
					# }
				}
				$parserState->{onlyComments} = 0;
				print "[n]onlyComments -> 0\n" if ($macroDebug);
				$continuation = 1;
				# $parserState->{simpleTypedef} = 0;
				if ($parserState->{basetype} eq "") { $parserState->{basetype} = $part; }
				# fall through to default case when we're done.
				if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inString} || $parserState->{inChar})) {
					$parserState->{namePending} = 2;
					print "namePending -> 2 [2]\n" if ($parseDebug);
					if ($parserState->{posstypesPending}) { $parserState->{posstypes} .=" $part"; }
				}
				if ($parserState->{sodclass} eq "") {
					$parserState->{startOfDec} = 0; $parserState->{sodname} = "";
print "sodname cleared (seu)\n" if ($sodDebug);
				}
				$lastchar = $part;
			    }; # end if
			}; # end do
		($part =~ /^$typedefname$/) && do {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				if (!(scalar(@braceStack)-$parserState->{initbsCount})) { $parserState->{callbackIsTypedef} = 1; $parserState->{inTypedef} = 1; }
				$parserState->{onlyComments} = 0;
				print "[o]onlyComments -> 0\n" if ($macroDebug);
				$continuation = 1;
				$parserState->{simpleTypedef} = 1;
				# previous case falls through, so be explicit.
				if ($part =~ /^$typedefname$/) {
				    if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inString} || $parserState->{inChar})) {
					if ($pascal) {
					    $parserState->{namePending} = 2;
					    $inPType = 1;
					    print "namePending -> 2 [3]\n" if ($parseDebug);
					}
					if ($parserState->{posstypesPending}) { $parserState->{posstypes} .=" $part"; }
					if (!($parserState->{callbackNamePending})) {
						print "callbackNamePending -> 1\n" if ($localDebug || $cbnDebug);
						$parserState->{callbackNamePending} = 1;
					}
				    }
				}
				if ($parserState->{sodclass} eq "") {
					$parserState->{startOfDec} = 0; $parserState->{sodname} = "";
print "sodname cleared ($typedefname)\n" if ($sodDebug);
				}
				$lastchar = $part;
			    }; # end if
			}; # end do

		# C++ operator keyword handler

		($part eq "$operator") && do {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				$parserState->{inOperator} = 1;
				$parserState->{sodname} = "";
			    }
			    $parserState->{lastsymbol} = $part;
			    $lastchar = $part;
			    last SWITCH;
			    # next;
			};

		# Punctuation handlers

		($part =~ /;/o) && do {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				if ($parsedParamParse) {
					$parsedParam =~ s/^\s*//so; # trim leading space
					$parsedParam =~ s/\s*$//so; # trim trailing space
					if (length($parsedParam)) { push(@{$parserState->{parsedParamList}}, $parsedParam); }
					print "pushed $parsedParam into parsedParamList [2semi]\n" if ($parmDebug);
					$parsedParam = "";
				}
				# skip this token
				$parsedParamParse = 2;
				$parserState->{freezereturn} = 1;
				# $parserState->{onlyComments} = 0;	# If this is all we've seen, there's either a bug or we're
									# unrolling a class or similar anyway.
				$parserState->{temponlyComments} = $parserState->{onlyComments};
				print "[p]onlyComments -> 0\n" if ($macroDebug);
				print "valuepending -> 0\n" if ($valueDebug);
				$parserState->{valuepending} = 0;
				$continuation = 1;
				if ($parserState->{occmethod}) {
					$prespaceadjust = -$prespace;
				}
				# previous case falls through, so be explicit.
				if ($part =~ /;/o && !$parserState->{inMacroLine} && !$parserState->{inMacro}) {
				    my $bsCount = scalar(@braceStack)-$parserState->{initbsCount};
				    if (!$bsCount && !$parserState->{kr_c_function}) {
					if ($parserState->{startOfDec} == 2) {
						$parserState->{sodclass} = "constant";
						$parserState->{startOfDec} = 1;

					} elsif (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} || $parserState->{inString})) {
						$parserState->{startOfDec} = 1;

					}
					# $parserState->{lastsymbol} .= $part;
				    }
				    if (!$bsCount) {
					$treeCur = pop(@treeStack) || $treeTop;
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print "TSPOP [8]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
					while ($treePopTwo--) {
						$treeCur = pop(@treeStack) || $treeTop;
						$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
						$treeCur = $treeCur->lastSibling();
						$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
						print "TSPOP [9]\n" if ($tsDebug || $treeDebug);
						bless($treeCur, "HeaderDoc::ParseTree");
					}
					$treePopTwo = 0;
				    }
				}
				$lastchar = $part;
			    }; # end if
			}; # end do
		($part eq "=" && ($parserState->{lastsymbol} ne "operator") && (!($parserState->{inOperator} && $parserState->{lastsymbol} =~ /\W/ && $parserState->{lastsymbol} =~ /\S/)) && !($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) && do {
				$parserState->{onlyComments} = 0;
				print "[q]onlyComments -> 0\n" if ($macroDebug);
				if ($part =~ /=/o && !(scalar(@braceStack)-$parserState->{initbsCount}) &&
				    $nextpart !~ /=/o && $lastchar !~ /=/o &&
				    $parserState->{sodclass} ne "function" && !$inPType) {
					print "valuepending -> 1\n" if ($valueDebug);
					$parserState->{valuepending} = 1;
					$parserState->{preEqualsSymbol} = $parserState->{lastsymbol};
					$parserState->{sodclass} = "constant";
					$parserState->{startOfDec} = 0;
				}; # end if
			}; # end do
		($part =~ /,/o) && do {
				if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
					$parserState->{onlyComments} = 0;
					print "[r]onlyComments -> 0\n" if ($macroDebug);
				}
				if ($part =~ /,/o && $parsedParamParse && !($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && ((scalar(@braceStack)-$parserState->{initbsCount}) == 1)) {
					print "$part is a comma\n" if ($localDebug || $parseDebug);
					$parsedParam =~ s/^\s*//so; # trim leading space
					$parsedParam =~ s/\s*$//so; # trim trailing space
					if (length($parsedParam)) { push(@{$parserState->{parsedParamList}}, $parsedParam); }
					print "pushed $parsedParam into parsedParamList [2]\n" if ($parmDebug);
					$parsedParam = "";
					# skip this token
					$parsedParamParse = 2;
					print "parsedParamParse -> 2\n" if ($parmDebug);
				}; # end if
			}; # end do
		{ # SWITCH default case

		    # Handler for all other text (data types, string contents,
		    # comment contents, character contents, etc.)

	# print "TEST CURLINE IS \"$curline\".\n";
		    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
		      if (!ignore($part, $ignoreref, $perheaderignoreref)) {
			if ($part =~ /\S/o) {
				$parserState->{onlyComments} = 0;
				print "[s]onlyComments -> 0\n" if ($macroDebug);
			}
			if (!$continuation && !$occspace) {
				$curline = spacefix($curline, $part, $lastchar);
			} else {
				$continuation = 0;
				$occspace = 0;
			}
	# print "BAD CURLINE IS \"$curline\".\n";
			if (length($part) && !($parserState->{inComment} || $parserState->{inInlineComment})) {
				if ($localDebug && $lastchar eq ")") {print "LC: $lastchar\nPART: $part\n";}
	# print "XXX LC: $lastchar SC: $parserState->{sodclass} LG: $lang\n";
				if ($lastchar eq ")" && $parserState->{sodclass} eq "function" && ($lang eq "C" || $lang eq "Csource") && !(scalar(@braceStack)-$parserState->{initbsCount})) {
					if ($part !~ /^\s*;/o) {
						# warn "K&R C FUNCTION FOUND.\n";
						# warn "NAME: $parserState->{sodname}\n";
						if (!isKeyword($part, $keywordhashref, $case_sensitive)) {
							print "K&R C FUNCTION FOUND [2].\n" if ($localDebug);
							$parserState->{kr_c_function} = 1;
							$parserState->{kr_c_name} = $parserState->{sodname};
						}
					}
				}
				$lastchar = $part;
				if ($part =~ /\w/o || $part eq "::") {
				    if ($parserState->{callbackNamePending} == 1) {
					if (!($part =~ /^struct$/o || $part =~ /^enum$/o || $part =~ /^union$/o || $part =~ /^$typedefname$/)) {
						# we've seen the initial type.  The name of
						# the callback is after the next open
						# parenthesis.
						print "callbackNamePending -> 2\n" if ($localDebug || $cbnDebug);
						$parserState->{callbackNamePending} = 2;
					}
				    } elsif ($parserState->{callbackNamePending} == 3) {
					print "callbackNamePending -> 4\n" if ($localDebug || $cbnDebug);
					$parserState->{callbackNamePending} = 4;
					$parserState->{callbackName} = $part;
					$parserState->{name} = "";
					$parserState->{sodclass} = "";
					$parserState->{sodname} = "";
				    } elsif ($parserState->{callbackNamePending} == 4) {
					if ($part eq "::") {
						print "callbackNamePending -> 5\n" if ($localDebug || $cbnDebug);
						$parserState->{callbackNamePending} = 5;
						$parserState->{callbackName} .= $part;
					} elsif ($part !~ /\s/o) {
						print "callbackNamePending -> 0\n" if ($localDebug || $cbnDebug);
						$parserState->{callbackNamePending} = 0;
					}
				    } elsif ($parserState->{callbackNamePending} == 5) {
					if ($part !~ /\s/o) {
						print "callbackNamePending -> 4\n" if ($localDebug || $cbnDebug);
						if ($part !~ /\*/o) {
							$parserState->{callbackNamePending} = 4;
						}
						$parserState->{callbackName} .= $part;
					}
				    }
				    if ($parserState->{namePending} == 2) {
					$parserState->{namePending} = 1;
					print "namePending -> 1 [4]\n" if ($parseDebug);
				    } elsif ($parserState->{namePending}) {
					if ($parserState->{name} eq "") { $parserState->{name} = $part; }
					$parserState->{namePending} = 0;
					print "namePending -> 0 [5]\n" if ($parseDebug);
				    }
				} # end if ($part =~ /\w/o)
				if ($part !~ /[;\[\]]/o && !$parserState->{inBrackets})  {
					my $opttilde = "";
					if ($parserState->{seenTilde}) { $opttilde = "~"; }
					if ($parserState->{startOfDec} == 1) {
						print "Setting sodname (maybe type) to \"$part\"\n" if ($sodDebug);
						$parserState->{sodname} = $opttilde.$part;
						if ($part =~ /\w/o) {
							$parserState->{startOfDec}++;
						}
					} elsif ($parserState->{startOfDec} == 2) {
						if ($part =~ /\w/o && !$parserState->{inTemplate}) {
							$parserState->{preTemplateSymbol} = "";
						}
						if ($parserState->{inOperator}) {
						    $parserState->{sodname} .= $part;
						} else {
						    if (length($parserState->{sodname})) {
							$parserState->{sodtype} .= " $parserState->{sodname}";
						    }
						    $parserState->{sodname} = $opttilde.$part;
						}
print "sodname set to $part\n" if ($sodDebug);
					} else {
						$parserState->{startOfDec} = 0;
					}
				} elsif ($part eq "[") { # if ($part !~ /[;\[\]]/o)
					$parserState->{inBrackets} += 1;
					print "inBrackets -> $parserState->{inBrackets}\n" if ($sodDebug);
				} elsif ($part eq "]") {
					$parserState->{inBrackets} -= 1;
					print "inBrackets -> $parserState->{inBrackets}\n" if ($sodDebug);
				} # end if ($part !~ /[;\[\]]/o)
				if (!($part eq $eoc)) {
					print "SETTING LS ($part)\n" if ($parseDebug);
					if ($parserState->{typestring} eq "") { $parserState->{typestring} = $part; }
					if ($parserState->{lastsymbol} =~ /\,\s*$/o) {
						$parserState->{lastsymbol} .= $part;
					} elsif ($parserState->{inTypedef} && !(scalar(@braceStack)-$parserState->{initbsCount}) && $part =~ /,/) {
						$parserState->{lastsymbol} .= $part;
					} elsif ($part =~ /^\s*\;\s*$/o) {
						$parserState->{lastsymbol} .= $part;
					} elsif (length($part)) {
						# warn("replacing lastsymbol with \"$part\"\n");
						$parserState->{lastsymbol} = $part;
					}
				} # end if (!($part eq $eoc))
			} # end if (length($part) && !($parserState->{inComment} || $parserState->{inInlineComment}))
		      }
		    } # end if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}))
		} # end SWITCH default case
	    } # end SWITCH
	    if (length($part)) { $lasttoken = $part; }
	    if (length($part) && $inRegexpTrailer) { --$inRegexpTrailer; }
	    if ($postPossNL) { --$postPossNL; }
	    if (($parserState->{simpleTypedef} == 1) && ($part ne $typedefname) &&
		   !($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} ||
		     $inRegexp)) {
		# print "NP: $parserState->{namePending} PTP: $parserState->{posstypesPending} PART: $part\n";
		$parserState->{simpleTDcontents} .= $part;
	    }

	    my $ignoretoken = ignore($part, $ignoreref, $perheaderignoreref);
	    my $hide = ($ignoretoken && !($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}));
	    print "TPONL: $treePopOnNewLine TPTWO: $treePopTwo\n" if ($tsDebug);
	    print "TN: $treeNest TS: $treeSkip nTS: ".scalar(@treeStack)."\n" if ($tsDebug);
	    if (!$treeSkip) {
		if (!$parserState->{seenBraces}) { # TREEDONE
			if ($treeNest != 2) {
				# If we really want to skip and nest, set treeNest to 2.
				if (length($treepart)) {
					if ((($parserState->{inComment} == 1) || ($parserState->{inInlineComment} == 1)) && $treepart !~ /[\r\n!]/) {
						$treeCur->token($treeCur->token() . $treepart);
						# print "SHORT\n";
					} else {
						$treeCur = $treeCur->addSibling($treepart, $hide);
					}
					$treepart = "";
				} else {
					if ((($parserState->{inComment} == 1) || ($parserState->{inInlineComment} == 1)) && $treepart !~ /[\r\n!]/) {
						$treeCur->token($treeCur->token() . $part);
						# print "SHORT\n";
					} else {
						$treeCur = $treeCur->addSibling($part, $hide);
					}
				}
				bless($treeCur, "HeaderDoc::ParseTree");
			}
			# print "TC IS $treeCur\n";
			# $treeCur = %{$treeCur};
			if ($treeNest) {
				if ($sethollow) {
					print "WILL INSERT (SETHOLLOW) at ".$treeCur->token()."\n" if ($parserStackDebug);
					$parserState->{hollow} = $treeCur;
					$sethollow = 0;
				}
				print "TSPUSH\n" if ($tsDebug || $treeDebug);
				push(@treeStack, $treeCur);
				$treeCur = $treeCur->addChild("", 0);
				bless($treeCur, "HeaderDoc::ParseTree");
			}
		}
	    }
	    if ($parserState->{inComment} > 1) { $parserState->{inComment}--; }
	    if ($parserState->{inInlineComment} > 1) { $parserState->{inInlineComment}--; }
	    if (($parserState->{inComment} == 1) && $treepart eq "!") {
		$parserState->{inComment} = 3;
	    }
	    if (($parserState->{inInlineComment} == 1) && $treepart eq "!") {
		$parserState->{inInlineComment} = 3;
	    }
	    $treeNest = 0;

	    if (!$parserState->{freezereturn} && $parserState->{hollow}) {
		$parserState->{returntype} = "$declaration$curline";
 	    } elsif (!$parserState->{freezereturn} && !$parserState->{hollow}) {
		$parserState->{returntype} = "$curline";
		$declaration = "";
	    }

	    # From here down is... magic.  This is where we figure out how
	    # to handle parsed parameters, K&R C types, and in general,
	    # determine whether we've received a complete declaration or not.
	    #
	    # About 90% of this is legacy code to handle proper spacing.
	    # Those bits got effectively replaced by the parseTree class.
	    # The only way you ever see this output is if you don't have
	    # any styles defined in your config file.

	    if (($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) ||
		!$ignoretoken) {
		if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) &&
                    !$ppSkipOneToken) {
	            if ($parsedParamParse == 1) {
		        $parsedParam .= $part;
	            } elsif ($parsedParamParse == 2) {
		        $parsedParamParse = 1;
		        print "parsedParamParse -> 1\n" if ($parmDebug);
	            }
		}
		if ($ppSkipOneToken) {
			$hollowskip = $ppSkipOneToken
		}
		$ppSkipOneToken = 0;
		print "MIDPOINT CL: $curline\nDEC:$declaration\nSCR: \"$scratch\"\n" if ($localDebug);
	        if (!$parserState->{seenBraces}) {
		    # Add to current line (but don't put inline function/macro
		    # declarations in.

		    if ($parserState->{inString}) {
			$curstring .= $part;
		    } else {
			if (length($curstring)) {
				if (length($curline) + length($curstring) >
				    $HeaderDoc::maxDecLen) {
					$scratch = nspaces($prespace);
					# @@@ WAS != /\n/ which is clearly
					# wrong.  Suspect the next line
					# if we start losing leading spaces
					# where we shouldn't (or don't where
					# we should).  Also was just /g.
					if ($curline !~ /^\s*\n/so) { $curline =~ s/^\s*//sgo; }
					
					# NEWLINE INSERT
					print "CURLINE CLEAR [1]\n" if ($localDebug);
					$declaration .= "$scratch$curline\n";
					$curline = "";
					$prespace += $prespaceadjust;
					$prespaceadjust = 0;
					$prespaceadjust -= 4;
					$prespace += 4;
				} else {
					# no wrap, so maybe add a space.
					if ($lastchar =~ /\=$/o) {
						$curline .= " ";
					}
				}
				$curline .= $curstring;
				$curstring = "";
			}
			if ((length($curline) + length($part) > $HeaderDoc::maxDecLen)) {
				$scratch = nspaces($prespace);
				# @@@ WAS != /\n/ which is clearly
				# wrong.  Suspect the next line
				# if we start losing leading spaces
				# where we shouldn't (or don't where
				# we should).  Also was /g instead of /sg.
				if ($curline !~ /^\s*\n/so) { $curline =~ s/^\s*//sgo; }
				# NEWLINE INSERT
				$declaration .= "$scratch$curline\n";
				print "CURLINE CLEAR [2]\n" if ($localDebug);
				$curline = "";
				$prespace += $prespaceadjust;
				$prespaceadjust = 0;
				$prespaceadjust -= 4;
				$prespace += 4;
			}
			if (length($curline) || $part ne " ") {
				# Add it to curline unless it's a space that
				# has inadvertently been wrapped to the
				# start of a line.
				$curline .= $part;
			}
		    }
		    if (peek(\@braceStack) ne "<") {
		      if ($part =~ /\n/o || ($part =~ /[\(;,]/o && $nextpart !~ /\n/o &&
		                      !$parserState->{occmethod}) ||
                                     ($part =~ /[:;.]/o && $nextpart !~ /\n/o &&
                                      $parserState->{occmethod})) {
			if ($curline !~ /\n/o && !($parserState->{inMacro} || ($pascal && (scalar(@braceStack)-$parserState->{initbsCount})) || $parserState->{inInlineComment} || $parserState->{inComment} || $parserState->{inString})) {
					# NEWLINE INSERT
					$curline .= "\n";
			}
			# Add the current line to the declaration.

			$scratch = nspaces($prespace);
			if ($curline !~ /\n/o) { $curline =~ s/^\s*//go; }
			if ($declaration !~ /\n\s*$/o) {
				$scratch = " ";
				if ($localDebug) {
					my $zDec = $declaration;
					$zDec = s/ /z/sg;
					$zDec = s/\t/Z/sg;
					print "ZEROSCRATCH\n";
					print "zDec: \"$zDec\"\n";
				}
			}
			$declaration .= "$scratch$curline";
				print "CURLINE CLEAR [3]\n" if ($localDebug);
			$curline = "";
			# $curline = nspaces($prespace);
			print "PS: $prespace -> " . $prespace + $prespaceadjust . "\n" if ($localDebug);
			$prespace += $prespaceadjust;
			$prespaceadjust = 0;
		      } elsif ($part =~ /[\(;,]/o && $nextpart !~ /\n/o &&
                                      ($parserState->{occmethod} == 1)) {
			print "SPC\n" if ($localDebug);
			$curline .= " "; $occspace = 1;
		      } else {
			print "NOSPC: $part:$nextpart:$parserState->{occmethod}\n" if ($localDebug);
		      }
		    }
		}

		if ($parserState->{temponlyComments}) {
			# print "GOT TOC: ".$parserState->{temponlyComments}."\n";
			$parserState->{onlyComments} = $parserState->{temponlyComments};
			$parserState->{temponlyComments} = undef;
		}

	        print "CURLINE IS \"$curline\".\n" if ($localDebug);
	        my $bsCount = scalar(@braceStack);
		print "ENDTEST: $bsCount \"$parserState->{lastsymbol}\"\n" if ($localDebug);
		print "KRC: $parserState->{kr_c_function} SB: $parserState->{seenBraces}\n" if ($localDebug);
	        if (!($bsCount - $parserState->{initbsCount}) && $parserState->{lastsymbol} =~ /;\s*$/o) {
		    # print "DPA\n";
		    if ((!$parserState->{kr_c_function} || $parserState->{seenBraces}) && !$parserState->{inMacro}) {
		        # print "DPB\n";
			if (!scalar(@parserStack)) {
			    $continue = 0;
			    print "continue -> 0 [3]\n" if ($localDebug);
			} elsif (!$parserState->{onlyComments}) {
				# @@@ Process entry here
				if ($parserState->{noInsert}) {
					print "parserState insertion skipped[SEMI-1]\n" if ($parserStackDebug);
				} elsif ($parserState->{hollow}) {
					my $treeRef = $parserState->{hollow};

					$parserState->{lastTreeNode} = $treeCur;
					$treeRef->addRawParsedParams(\@{$parserState->{parsedParamList}});
					$treeRef->parserState($parserState);
				} elsif ($parserState->{classtype} && length($parserState->{classtype})) {
					warn "Couldn't insert info into parse tree[3class].\n" if ($localDebug);
				} else {
					warn "Couldn't insert info into parse tree[3].\n";
					print "Printing tree.\n";
					$parserState->print();
					$treeTop->dbprint();
				}

				print "parserState: Created parser state[2].\n" if ($parserStackDebug);
				$parserState = HeaderDoc::ParserState->new();
				$parserState->{skiptoken} = 1;
				$parserState->{lang} = $lang;
				$parserState->{inputCounter} = $inputCounter;
				$parserState->{initbsCount} = scalar(@braceStack);
				print "NEWRETURNTYPE: $parserState->{returntype}\n" if ($localDebug);
				print "CURLINE CLEAR[PRS2]\n" if ($localDebug);
				$curline = "";
			}
		    }
	        } else {
		    print("bsCount: $bsCount - $parserState->{initbsCount}, ls: $parserState->{lastsymbol}\n") if ($localDebug);
		    pbs(@braceStack);
	        }
	        if (!($bsCount - $parserState->{initbsCount}) && $parserState->{seenBraces} && ($parserState->{sodclass} eq "function" || $parserState->{inOperator}) && 
		    ($nextpart ne ";")) {
			# Function declarations end at the close curly brace.
			# No ';' necessary (though we'll eat it if it's there.
			if (!scalar(@parserStack)) {
				$continue = 0;
				print "continue -> 0 [4]\n" if ($localDebug);
			} elsif (!$parserState->{onlyComments}) {
				# @@@ Process entry here
				if ($parserState->{noInsert}) {
					print "parserState insertion skipped[SEMI-2]\n" if ($parserStackDebug);
				} elsif ($parserState->{hollow}) {
					my $treeRef = $parserState->{hollow};

					$parserState->{lastTreeNode} = $treeCur;
					$treeRef->addRawParsedParams(\@{$parserState->{parsedParamList}});
					$treeRef->parserState($parserState);
				} else {
					warn "Couldn't insert info into parse tree[4].\n";
				}

				print "parserState: Created parser state[3].\n" if ($parserStackDebug);
				$parserState = HeaderDoc::ParserState->new();
				$parserState->{skiptoken} = 1;
				$parserState->{lang} = $lang;
				$parserState->{inputCounter} = $inputCounter;
				$parserState->{initbsCount} = scalar(@braceStack);
				print "CURLINE CLEAR[PRS3]\n" if ($localDebug);
				$curline = "";
			}
	        }
	        if (($parserState->{inMacro} == 3 && $parserState->{lastsymbol} ne "\\") || $parserState->{inMacro} == 4) {
		    if ($part =~ /[\n\r]/o && !$parserState->{inComment}) {
			print "MLS: $parserState->{lastsymbol}\n" if ($macroDebug);
			print "PARSER STACK CONTAINS ".scalar(@parserStack)." FRAMES\n" if ($cppDebug || $parserStackDebug);
			if (!scalar(@parserStack)) {
				$continue = 0;
				print "continue -> 0 [5]\n" if ($localDebug);
			} elsif (!$parserState->{onlyComments}) {
				# @@@ Process entry here
				print "DONE WITH MACRO.  HANDLING.\n" if ($localDebug || $parseDebug);

				if ($parserState->{inMacro} == 3) { cpp_add($parserState->{hollow}); }

				if ($parserState->{noInsert}) {
					print "parserState insertion skipped\n" if ($parserStackDebug);
				} elsif ($parserState->{hollow}) {
					my $treeRef = $parserState->{hollow};

					$parserState->{lastTreeNode} = $treeCur;
					$treeRef->addRawParsedParams(\@{$parserState->{parsedParamList}});
					$treeRef->parserState($parserState);
				} else {
					warn "Couldn't insert info into parse tree[5].\n";
				}

				print "parserState: Created parser state[4].\n" if ($parserStackDebug);
				$parserState = HeaderDoc::ParserState->new();
				$parserState->{skiptoken} = 1;
				$parserState->{lang} = $lang;
				$parserState->{inputCounter} = $inputCounter;
				$parserState->{initbsCount} = scalar(@braceStack);
				print "CURLINE CLEAR[PRS4]\n" if ($localDebug);
				$curline = "";
			}
		    }
	        } elsif ($parserState->{inMacro} == 2) {
		    my $linenum = $inputCounter + $fileoffset;
		    warn "$filename:$linenum:Declaration starts with # but is not preprocessor macro\n";
	        } elsif ($parserState->{inMacro} == 3 && $parserState->{lastsymbol} eq "\\") {
			print "TAIL BACKSLASH ($continue)\n" if ($localDebug || $macroDebug);
		}
	        if ($parserState->{valuepending} == 2) {
		    # skip the "=" part;
		    $parserState->{value} .= $part;
	        } elsif ($parserState->{valuepending}) {
		    $parserState->{valuepending} = 2;
		    print "valuepending -> 2\n" if ($valueDebug);
	        }
	    } # end if "we're not ignoring this token"


	    print "OOGABOOGA\n" if ($parserStackDebug);
	    if ($pushParserStateAfterToken == 1) {
			print "parserState pushed onto stack[token]\n" if ($parserStackDebug);
			$parserState->{lastTreeNode} = $treeCur;
			$curline = "";
			push(@parserStack, $parserState);
			$parserState = HeaderDoc::ParserState->new();
			$parserState->{skiptoken} = 1;
			$parserState->{lang} = $lang;
			$parserState->{inputCounter} = $inputCounter;
			$parserState->{initbsCount} = scalar(@braceStack);
			$pushParserStateAfterToken = 0;
			$pushParserStateAtBrace = 0;
	    } elsif ($pushParserStateAfterWordToken == 1) {
		if ($part =~ /\w/) {
			print "parserState pushed onto stack[word]\n" if ($parserStackDebug);
			$parserState->{lastTreeNode} = $treeCur;
			$curline = "";
			push(@parserStack, $parserState);
			$parserState = HeaderDoc::ParserState->new();
			$parserState->{skiptoken} = 1;
			$parserState->{lang} = $lang;
			$parserState->{inputCounter} = $inputCounter;
			$parserState->{initbsCount} = scalar(@braceStack);
			$pushParserStateAfterWordToken = 0;
		}
	    } elsif ($pushParserStateAfterWordToken) {
		print "PPSAFTERWT CHANGED $pushParserStateAfterWordToken -> " if ($parserStackDebug);
		$pushParserStateAfterWordToken--;
		print "$pushParserStateAfterWordToken\n" if ($parserStackDebug);
	    } elsif ($pushParserStateAtBrace) {
		print "PPSatBrace?\n" if ($parserStackDebug);
		if (casecmp($part, $lbrace, $case_sensitive)) {
			print "parserState pushed onto stack[brace]\n" if ($parserStackDebug);
			# if ($pushParserStateAtBrace == 2) {
				# print "NOINSERT parserState: $parserState\n" if ($parserStackDebug);
				# $parserState->{hollow} = undef;
				# $parserState->{noInsert} = 1;
			# }
			$parserState->{lastTreeNode} = $treeCur;
			$curline = "";
			push(@parserStack, $parserState);
			$parserState = HeaderDoc::ParserState->new();
			$parserState->{skiptoken} = 1;
			$parserState->{lang} = $lang;
			$parserState->{inputCounter} = $inputCounter;
			$parserState->{initbsCount} = scalar(@braceStack);
			$parserState->{noInsert} = $setNoInsert;
			$setNoInsert = 0;
			$pushParserStateAtBrace = 0;
		} elsif ($pushParserStateAtBrace) {
			if ($part =~ /\;/) {
				# It's a class instance declaration.  Whoops.
				$pushParserStateAtBrace = 0;
				$parserState->{inClass} = 0;
			}
			# if ($part =~ /\S/) { $pushParserStateAtBrace = 0; }
		}
	    } else {
		if (!$parserState->{hollow}) {
		    my $tok = $part; # $treeCur->token();
		    print "parserState: NOT HOLLOW\n" if ($parserStackDebug);
		    print "IS: $parserState->{inString}\nICom: $parserState->{inComment}\nISLC: $parserState->{inInlineComment}\nIChar: $parserState->{inChar}\nSkipToken: $parserState->{skiptoken}\nHollowSkip: $hollowskip\n" if ($parserStackDebug);
		    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} || $parserState->{skiptoken} || $hollowskip)) {
			print "parserState: NOT STRING/CHAR/COMMENT\n" if ($parserStackDebug);
			if ($tok =~ /\S/) {
				print "parserState: PS IS $parserState\n" if ($parserStackDebug);
				print "parserState: NOT WHITESPACE : ".$parserState->{hollow}." -> $treeCur\n" if ($parserStackDebug);
				if (!casecmp($tok, $rbrace, $case_sensitive) && $part !~ /\)/) {
					$parserState->{hollow} = $treeCur;
					$HeaderDoc::curParserState = $parserState;
					print "parserState: WILL INSERT AT TOKEN ".$treeCur->token()."\n" if ($parserStackDebug);
				}
			}
		    }
		    $hollowskip = 0;
		    $parserState->{skiptoken} = 0;
		}
	    }

	    if (length($part) && $part =~ /\S/o) { $lastnspart = $part; }
	    if ($parserState->{seenTilde} && length($part) && $part !~ /\s/o) { $parserState->{seenTilde}--; }
	    $part = $nextpart;
	} # end foreach (parts of the current line)
    } # end while (continue && ...)

    # Format and insert curline into the declaration.  This handles the
    # trailing line.  (Deprecated.)

    if ($curline !~ /\n/) { $curline =~ s/^\s*//go; }
    if ($curline =~ /\S/o) {
	$scratch = nspaces($prespace);
	$declaration .= "$scratch$curline\n";
    }

    print "($parserState->{typestring}, $parserState->{basetype})\n" if ($localDebug || $listDebug);

    print "LS: $parserState->{lastsymbol}\n" if ($localDebug);

    $parserState->{lastTreeNode} = $treeCur;
    $parserState->{inputCounter} = $inputCounter;

print "PARSERSTATE: $parserState\n" if ($localDebug);

    if ($parserState->{inMacro} == 3) {
	cpp_add($treeTop);
    }

print "LEFTBPMAIN\n" if ($localDebug || $hangDebug);

    return blockParseReturnState($parserState, $treeTop, $argparse, $declaration, $inPrivateParamTypes, $publicDeclaration, $lastACS, $retDebug, $fileoffset);
}


sub blockParseReturnState
{
    my $parserState = shift;
    my $treeTop = shift;
    my $argparse = shift;
    my $declaration = shift; # optional
    my $inPrivateParamTypes = shift; # optional
    my $publicDeclaration = shift; # optional
    my $lastACS = shift; # optional
    my $forcedebug = shift; # optional
    my $fileoffset = shift; # optional

    if (!length($declaration)) {
	$declaration = $treeTop->textTree();
    }

# $forcedebug = 1;
    $forcedebug = $forcedebug || $HeaderDoc::fileDebug;

    my $localDebug = 0 || $forcedebug;
    my $sodDebug   = 0 || $forcedebug;
    my $parseDebug = 0 || $forcedebug;
    my $listDebug  = 0 || $forcedebug;
    my $parmDebug  = 0 || $forcedebug;
    my $retDebug   = 0 || $forcedebug;

    cluck("PARSERSTATE IS $parserState\n") if ($localDebug);

    if ($forcedebug) { $parserState->print(); }

if ($forcedebug) { print "FD\n"; }

    my $lang = $parserState->{lang};
    my $sublang = $parserState->{sublang};
    my $pascal = 0;
    if ($lang eq "pascal") { $pascal = 1; }
    my $perl_or_shell = 0;
    if ($lang eq "perl" || $lang eq "shell") {
	$perl_or_shell = 1;
    }

    my $inputCounter = $parserState->{inputCounter};

    print "PS: $parserState\n" if ($localDebug);

    my @parsedParamList = @{$parserState->{parsedParamList}};
    my @pplStack = @{$parserState->{pplStack}};
    my @freezeStack = @{$parserState->{freezeStack}};

    if ($parserState->{stackFrozen}) {
	print "RESTORING SODNAME: $parserState->{sodname} -> $parserState->{frozensodname}\n" if ($localDebug);
	$parserState->{sodname} = $parserState->{frozensodname};
    }

    # if ($parserState->{simpleTypedef}) { $parserState->{name} = ""; }

    # From here down is a bunch of code for determining which names
    # for a given type/function/whatever are legit and which aren't.
    # It is mostly a priority scheme.  The data type names are first,
    # and thus lowest priority.

    my $typelist = "";
    my $namelist = "";
    my @names = split(/[,\s;]/, $parserState->{lastsymbol});
    foreach my $insname (@names) {
	$insname =~ s/\s//so;
	$insname =~ s/^\*//sgo;
	if (length($insname)) {
	    $typelist .= " $parserState->{typestring}";
	    $namelist .= ",$insname";
	}
    }
    $typelist =~ s/^ //o;
    $namelist =~ s/^,//o;

    if ($pascal) {
	# Pascal only has one name for a type, and it follows the word "type"
	if (!length($typelist)) {
		$typelist .= "$parserState->{typestring}";
		$namelist .= "$parserState->{name}";
	}
    }

print "TL (PRE): $typelist\n" if ($localDebug);

    if (!length($parserState->{basetype})) { $parserState->{basetype} = $parserState->{typestring}; }
print "BT: $parserState->{basetype}\n" if ($localDebug);

print "NAME is $parserState->{name}\n" if ($localDebug || $listDebug);

# print $HeaderDoc::outerNamesOnly . " or " . length($namelist) . ".\n";

    # If the name field contains a value, and if we've seen at least one brace or parenthesis
    # (to avoid "typedef struct foo bar;" giving us an empty declaration for struct foo), and
    # if either we want tag names (foo in "struct foo { blah } foo_t") or there is no name
    # other than a tag name (foo in "struct foo {blah}"), then we give the tag name.  Scary
    # little bit of logic.  Sorry for the migraine.

    # Note: at least for argparse == 2 (used when handling nested headerdoc
    # markup), we don't want to return more than one name/type EVER.

    if (($parserState->{name} && length($parserState->{name}) && !$parserState->{simpleTypedef} && (!($HeaderDoc::outerNamesOnly || $argparse == 2) || !length($namelist))) || ($namelist !~ /\w/)) {


	# print "NM: $parserState->{name}\nSTD: $parserState->{simpleTypedef}\nONO: ".$HeaderDoc::outerNamesOnly."\nAP: $argparse\nLNL: ".length($namelist)."\n";

	my $quotename = quote($parserState->{name});
	if ($namelist !~ /$quotename/) {
		if (length($namelist)) {
			$namelist .= ",";
			$typelist .= " ";
		}
		$namelist .= "$parserState->{name}";
		$typelist .= "$parserState->{basetype}";
	}
    } else {
	# if we never found the name, it might be an anonymous enum,
	# struct, union, etc.

	if (!scalar(@names)) {
		print "Empty output ($parserState->{basetype}, $parserState->{typestring}).\n" if ($localDebug || $listDebug);
		$namelist = " ";
		$typelist = "$parserState->{basetype}";
	}

	print "NUMNAMES: ".scalar(@names)."\n" if ($localDebug || $listDebug);
    }

print "NL: \"$namelist\".\n" if ($localDebug || $listDebug);
print "TL: \"$typelist\".\n" if ($localDebug || $listDebug);
print "PT: \"$parserState->{posstypes}\"\n" if ($localDebug || $listDebug);
print "SN: \"$parserState->{sodname}\"\nST: \"$parserState->{sodtype}\"\nSC: \"$parserState->{sodclass}\"\n" if ($localDebug || $sodDebug);

    my $destructor = 0;
    if ($parserState->{sodtype} =~ s/\~$//s) {
	$parserState->{sodname} = "~" . $parserState->{sodname};
	$destructor = 1;
    }

    # If it's a callback, the other names and types are bogus.  Throw them away.

    $parserState->{callbackName} =~ s/^.*:://o;
    $parserState->{callbackName} =~ s/^\*+//o;
    print "CBN: \"$parserState->{callbackName}\"\n" if ($localDebug || $listDebug);
    if (length($parserState->{callbackName})) {
	$parserState->{name} = $parserState->{callbackName};
	print "DEC: \"$declaration\"\n" if ($localDebug || $listDebug);
	$namelist = $parserState->{name};
	if ($parserState->{callbackIsTypedef}) {
		$typelist = "typedef";
		$parserState->{posstypes} = "function";
	} else {
		$typelist = "function";
		$parserState->{posstypes} = "typedef";
	}
	print "NL: \"$namelist\".\n" if ($localDebug || $listDebug);
	print "TL: \"$typelist\".\n" if ($localDebug || $listDebug);
	print "PT: \"$parserState->{posstypes}\"\n" if ($localDebug || $listDebug);

	# my $newdec = "";
	# my $firstpart = 2;
	# foreach my $decpart (split(/\n/, $declaration)) {
		# if ($firstpart == 2) {
			# $newdec .= "$decpart ";
			# $firstpart--;
		# } elsif ($firstpart) {
			# $decpart =~ s/^\s*//o;
			# $newdec .= "$decpart\n";
			# $firstpart--;
		# } else {
			# $newdec .= "$decpart\n";
		# }
	# }
	# $declaration = $newdec;
    }

    if (length($parserState->{preTemplateSymbol}) && ($parserState->{sodclass} eq "function")) {
	$parserState->{sodname} = $parserState->{preTemplateSymbol};
	$parserState->{sodclass} = "ftmplt";
	$parserState->{posstypes} = "ftmplt function method"; # can it really be a method?
    }

    # If it isn't a constant, the value is something else.  Otherwise,
    # the variable name is whatever came before the equals sign.

    print "TVALUE: $parserState->{value}\n" if ($localDebug);
    if ($parserState->{sodclass} ne "constant") {
	$parserState->{value} = "";
    } elsif (length($parserState->{value})) {
	$parserState->{value} =~ s/^\s*//so;
	$parserState->{value} =~ s/\s*$//so;
	$parserState->{posstypes} = "constant";
	$parserState->{sodname} = $parserState->{preEqualsSymbol};
    }

    # We lock in the name prior to walking through parameter names for
    # K&R C-style declarations.  Restore that name first.
    if (length($parserState->{kr_c_name})) { $parserState->{sodname} = $parserState->{kr_c_name}; $parserState->{sodclass} = "function"; }

    # Okay, so now if we're not an objective C method and the sod code decided
    # to specify a name for this function, it takes precendence over other naming.

    if (length($parserState->{sodname}) && !$parserState->{occmethod}) {
	if (!length($parserState->{callbackName})) { # && $parserState->{callbackIsTypedef}
	    if (!$perl_or_shell) {
		$parserState->{name} = $parserState->{sodname};
		$namelist = $parserState->{name};
	    }
	    $typelist = "$parserState->{sodclass}";
	    if (!length($parserState->{preTemplateSymbol})) {
	        $parserState->{posstypes} = "$parserState->{sodclass}";
	    }
	    print "SETTING NAME/TYPE TO $parserState->{sodname}, $parserState->{sodclass}\n" if ($sodDebug);
	    if ($parserState->{sodclass} eq "function") {
		$parserState->{posstypes} .= " method";
	    }
	}
    }

    # If we're an objective C method, obliterate everything and just
    # shove in the right values.

    print "DEC: $declaration\n" if ($sodDebug || $localDebug);
    if ($parserState->{occmethod}) {
	$typelist = "method";
	$parserState->{posstypes} = "method function";
	if ($parserState->{occmethod} == 2) {
		$namelist = "$parserState->{occmethodname}";
	}
    }

    # If we're a macro... well, this gets ugly.  We rebuild the parsed
    # parameter list from the declaration and otherwise use the name grabbed
    # by the sod code.
    if ($parserState->{inMacro} == 3) {
	$typelist = "#define";
	$parserState->{posstypes} = "function method";
	$namelist = $parserState->{sodname};
	$parserState->{value} = "";
	@parsedParamList = ();
	my $declaration = $treeTop->textTree();
	if ($declaration =~ /#define\s+\w+\(/o) {
		my $pplref = defParmParse($declaration, $inputCounter);
		print "parsedParamList replaced\n" if ($parmDebug);
		@parsedParamList = @{$pplref};
	} else {
		# It can't be a function-like macro, but it could be
		# a constant.
		$parserState->{posstypes} = "constant";
	}
    } elsif ($parserState->{inMacro} == 4) { 
	$typelist = "MACRO";
	$parserState->{posstypes} = "MACRO";
	$parserState->{value} = "";
	@parsedParamList = ();
    }

    # If we're an operator, our type is 'operator', not 'function'.  Our fallback
    # name is 'function'.
    if ($parserState->{inOperator}) {
	$typelist = "operator";
	$parserState->{posstypes} = "function";
    }

    # if we saw private parameter types, restore the first declaration (the
    # public part) and store the rest for later.  Note that the parse tree
    # code makes this deprecated.

    my $privateDeclaration = "";
    if ($inPrivateParamTypes) {
	$privateDeclaration = $declaration;
	$declaration = $publicDeclaration;
    }

print "TYPELIST WAS \"$typelist\"\n" if ($localDebug);;
# warn("left blockParse (macro)\n");
# print "NumPPs: ".scalar(@parsedParamList)."\n";

# $treeTop->printTree();

    # If we have parsed parameters that haven't been pushed onto
    # the stack of parsed parameters, push them now.


    if (scalar(@parsedParamList)) {
		foreach my $stackitem (@parsedParamList) {
			$stackitem =~ s/^\s*//so;
			$stackitem =~ s/\s*$//so;
			if (length($stackitem)) {
				push(@pplStack, $stackitem);
			}
		}
    }

    # Restore the frozen stack (to avoid bogus parameters after
    # the curly brace for inline functions/methods)
    if ($parserState->{stackFrozen}) {
	@pplStack = @freezeStack;
    }

    if ($localDebug) {
	foreach my $stackitem (@pplStack) {
		print "stack contained $stackitem\n";
	}
    }

    # If we have a simple typedef, do some formatting on the contents.
    # This is used by the upper layers so that if you have
    # "typedef struct myStruct;", you can associate the fields from
    # "struct myStruct" with the typedef, thus allowing more
    # flexibility in tagged/parsed parameter comparison.
    # 
    $parserState->{simpleTDcontents} =~ s/^\s*//so;
    $parserState->{simpleTDcontents} =~ s/\s*;\s*$//so;
    if ($parserState->{simpleTDcontents} =~ s/\s*\w+$//so) {
	my $continue = 1;
	while ($parserState->{simpleTDcontents} =~ s/\s*,\s*$//so) {
		$parserState->{simpleTDcontents} =~ s/\s*\w+$//so;
	}
    }
    if (length($parserState->{simpleTDcontents})) {
	print "SIMPLETYPEDEF: $inputCounter, $declaration, $typelist, $namelist, $parserState->{posstypes}, $parserState->{value}, OMITTED pplStack, $parserState->{returntype}, $privateDeclaration, $treeTop, $parserState->{simpleTDcontents}, $parserState->{availability}\n" if ($parseDebug || $sodDebug || $localDebug);
	$typelist = "typedef";
	$namelist = $parserState->{sodname};
	$parserState->{posstypes} = "";
    }

    # If we have a class, do some funky stuff.
    if ($parserState->{inClass}) {
	print "INCLASS!\n" if ($localDebug);
	print "PTS: $parserState->{preTemplateSymbol}\n" if ($localDebug);
	$parserState->{sodtype} =~ s/^\s*//s;
	my @classparts = split(/\s/, $parserState->{sodtype}, 2);
	my $classname = "";
	my $superclass = "";
	# print "RETURNING CLASS: SODTYPE IS $parserState->{sodtype}\n";
	if (scalar(@classparts)) {
		print "CLASSPARTS FOUND.\n" if ($localDebug);
		$classname = $classparts[0];
		$superclass = $parserState->{sodname};
		# $classparts[0]." : $parserState->{sodname}";
	} else {
		print "NO CLASSPARTS FOUND.\n" if ($localDebug);
		$classname = $parserState->{sodname};
		$superclass = "";
	}
	$namelist = $classname;
	# print "RETURNING CLASS: NAMELIST IS $classname\n";
	$typelist = "$parserState->{classtype}";
	$parserState->{posstypes} = "$superclass";
	print "SUPER WAS \"$superclass\"\n" if ($localDebug);

	print "categoryClass is $parserState->{categoryClass}\n" if ($localDebug || $parseDebug);
	if (length($parserState->{categoryClass})) {
		$parserState->{posstypes} = $namelist;
		$parserState->{posstypes} =~ s/\s*//sg;
		$namelist .= $parserState->{categoryClass};
		$namelist =~ s/\s*//sg;
		print "NL: $namelist\n" if ($localDebug || $parseDebug);
	}
    }
    if ($parserState->{forceClassName}) { 
	$namelist = $parserState->{forceClassName};
	$parserState->{posstypes} = cppsupers($parserState->{forceClassSuper}, $lang, $sublang);
	print "CPPSUPERS: $parserState->{posstypes}\n" if ($localDebug);
    }
    if ($parserState->{occSuper}) {
	$parserState->{posstypes} = $parserState->{occSuper};
    }

    $parserState->{returntype} = decomment($parserState->{returntype});

    # print "Return type was: $parserState->{returntype}\n" if ($argparse || $parserState->{sodclass} eq "function" || $parserState->{occmethod});
    if (length($parserState->{sodtype}) && !$parserState->{occmethod}) {
	$parserState->{returntype} = $parserState->{sodtype};
    }
    # print "Return type: $parserState->{returntype}\n" if ($argparse || $parserState->{sodclass} eq "function" || $parserState->{occmethod});
    # print "DEC: $declaration\n" if ($argparse || $parserState->{sodclass} eq "function" || $parserState->{occmethod});
    print "PTDEC: ".$treeTop->textTree()."\n" if ($localDebug || $retDebug);

    print "RETURNING NAMELIST: \"$namelist\"\nRETURNING TYPELIST: \"$typelist\"\n" if ($localDebug || $retDebug);

    if (length($lastACS)) {
	$HeaderDoc::AccessControlState = $lastACS;
    }

print "LEFTBP\n" if ($localDebug);

    # We're outta here.
    return ($inputCounter, $declaration, $typelist, $namelist, $parserState->{posstypes}, $parserState->{value}, \@pplStack, $parserState->{returntype}, $privateDeclaration, $treeTop, $parserState->{simpleTDcontents}, $parserState->{availability}, $fileoffset);
}


sub spacefix
{
my $curline = shift;
my $part = shift;
my $lastchar = shift;
my $soc = shift;
my $eoc = shift;
my $ilc = shift;
my $localDebug = 0;

if ($HeaderDoc::use_styles) { return $curline; }

print "SF: \"$curline\" \"$part\" \"$lastchar\"\n" if ($localDebug);

	if (($part !~ /[;,]/o)
	  && length($curline)) {
		# space before most tokens, but not [;,]
		if ($part eq $ilc) {
				if ($lastchar ne " ") {
					$curline .= " ";
				}
			}
		elsif ($part eq $soc) {
				if ($lastchar ne " ") {
					$curline .= " ";
				}
			}
		elsif ($part eq $eoc) {
				if ($lastchar ne " ") {
					$curline .= " ";
				}
			}
		elsif ($part =~ /\(/o) {
print "PAREN\n" if ($localDebug);
			if ($curline !~ /[\)\w\*]\s*$/o) {
				print "CASEA\n" if ($localDebug);
				if ($lastchar ne " ") {
					print "CASEB\n" if ($localDebug);
					$curline .= " ";
				}
			} else {
				print "CASEC\n" if ($localDebug);
				$curline =~ s/\s*$//o;
			}
		} elsif ($part =~ /^\w/o) {
			if ($lastchar eq "\$") {
				$curline =~ s/\s*$//o;
			} elsif ($part =~ /^\d/o && $curline =~ /-$/o) {
				$curline =~ s/\s*$//o;
			} elsif ($curline !~ /[\*\(]\s*$/o) {
				if ($lastchar ne " ") {
					$curline .= " ";
				}
			} else {
				$curline =~ s/\s*$//o;
			}
		} elsif ($lastchar =~ /\w/o) {
			#($part =~ /[=!+-\/\|\&\@\*/ etc.)
			$curline .= " ";
		}
	}

	if ($curline =~ /\/\*$/o) { $curline .= " "; }

	return $curline;
}

sub nspaces
{
    my $n = shift;
    my $string = "";

    while ($n-- > 0) { $string .= " "; }
    return $string;
}

sub pbs
{
    my @braceStack = shift;
    my $localDebug = 0;

    if ($localDebug) {
	print "BS: ";
	foreach my $p (@braceStack) { print "$p "; }
	print "ENDBS\n";
    }
}

# parse #define arguments
sub defParmParse
{
    my $declaration = shift;
    my $inputCounter = shift;
    my @myargs = ();
    my $localDebug = 0;
    my $curname = "";
    my $filename = "";

    $declaration =~ s/.*#define\s+\w+\s*\(//o;
    my @braceStack = ( "(" );

    my @tokens = split(/(\W)/, $declaration);
    foreach my $token (@tokens) {
	print "TOKEN: $token\n" if ($localDebug);
	if (!scalar(@braceStack)) { last; }
	if ($token =~ /[\(\[]/o) {
		print "open paren/bracket - $token\n" if ($localDebug);
		push(@braceStack, $token);
	} elsif ($token =~ /\)/o) {
		print "close paren\n" if ($localDebug);
		my $top = pop(@braceStack);
		if ($top !~ /\(/o) {
			warn("$filename:$inputCounter:Parentheses do not match (macro).\nWe may have a problem.\n");
		}
	} elsif ($token =~ /\]/o) {
		print "close bracket\n" if ($localDebug);
		my $top = pop(@braceStack);
		if ($top !~ /\[/o) {
			warn("$filename:$inputCounter:Braces do not match (macro).\nWe may have a problem.\n");
		}
	} elsif ($token =~ /,/o && (scalar(@braceStack) == 1)) {
		$curname =~ s/^\s*//sgo;
		$curname =~ s/\s*$//sgo;
		push(@myargs, $curname);
		print "pushed \"$curname\"\n" if ($localDebug);
		$curname = "";
	} else {
		$curname .= $token;
	}
    }
    $curname =~ s/^\s*//sgo;
    $curname =~ s/\s*$//sgo;
    if (length($curname)) {
	print "pushed \"$curname\"\n" if ($localDebug);
	push(@myargs, $curname);
    }

    return \@myargs;
}

sub ignore
{
    my $part = shift;
    my $ignorelistref = shift;
    my %ignorelist = %{$ignorelistref};
    my $phignorelistref = shift;
    my %perheaderignorelist = %{$phignorelistref};
    my $localDebug = 0;

    # if ($part =~ /AVAILABLE/o) {
	# $localDebug = 1;
    # }

    my $def = $HeaderDoc::availability_defs{$part};
    if ($def && length($def)) { return $def; }

    if ($ignorelist{$part}) {
	    print "IGNORING $part\n" if ($localDebug);
	    return 1;
    }
    if ($perheaderignorelist{$part}) {
	    print "IGNORING $part\n" if ($localDebug);
	    return 1;
    }
    print "NO MATCH FOUND\n" if ($localDebug);
    return 0;
}

sub blockParseOutside
{
    my $apiOwner = shift;
    my $inFunction = shift;
    my $inUnknown = shift;
    my $inTypedef = shift;
    my $inStruct = shift;
    my $inEnum = shift;
    my $inUnion = shift;
    my $inConstant = shift;
    my $inVar = shift;
    my $inMethod = shift;
    my $inPDefine = shift;
    my $inClass = shift;
    my $inInterface = shift;


    # my $blockDec = shift;
    # my $blockmode = shift;
    my $blockOffset = shift;
    # my $case_sensitive = shift;
    my $categoryObjectsref = shift;
    my $classObjectsref = shift;
    my $classType = shift;
    my $cppAccessControlState = shift;
    # my $curtype = shift;
    # my $declaration = shift;
    my $fieldsref = shift;
    my $filename = shift;
    my $functionGroup = shift;
    my $headerObject = shift;
    # my $inClass = shift;
    # my $innertype = shift;
    my $inputCounter = shift;
    my $inputlinesref = shift;
    # my $keywordhashref = shift;
    my $lang = shift;
    # my $namelist = shift;
    # my $newcount = shift;
    my $nlines = shift;
    # my $outertype = shift;
    # my $posstypes = shift;
    my $preAtPart = shift;
    # my $typedefname = shift;
    # my $typelist = shift;
    # my $varIsConstant = shift;
    my $xml_output = shift;

    my $localDebug = shift;
    my $hangDebug = shift;
    my $parmDebug = shift;
    my $blockDebug = shift;

    my $subparse = shift;
    my $subparseTree = shift;
    # my $outputdir = shift;
    my $nodec = shift;

# $localDebug = 1; $blockDebug = 1;


    if ($localDebug) { if ($subparse) { print "SUBPARSE\n"; } else { print "PARSE\n"; } }

    my $lastParseTree = undef;
    my $slowokay = ($subparse == 2) ? 1 : 0;

    my $old_enable_cpp = $HeaderDoc::enable_cpp;
    if ($subparse) {
	# We don't want to remove #define macros just because they appeared
	# within a class declaration....  :-)
	$HeaderDoc::enable_cpp = -1;
    }

    # Args here

    my @classObjects = @{$classObjectsref};
    my @categoryObjects = ();
    if ($categoryObjectsref) {
	# print "GOT categoryObjectsRef!\n";
	@categoryObjects = @{$categoryObjectsref};
    }
    my @fields = @{$fieldsref};
    my @inputLines = @{$inputlinesref};
    my @parseTrees = ();
    my $methods_with_new_parser = 1;
    my $curObj = undef;
    my $classKeyword = "auto";

# print "BPO: FIELDS: \n";
# foreach my $field (@fields) {
	# print "FIELD: $field\n";
# }
# print "END FIELDS\n";

print "blockParseOutside: APIOWNER IS $apiOwner\n" if ($localDebug);

				if ($inUnknown || $inTypedef || $inStruct || $inEnum || $inUnion || $inConstant || $inVar || $inFunction || ($inMethod && $methods_with_new_parser) || $inPDefine || $inClass) {
				    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
					$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
					$typedefname, $varname, $constname, $structisbrace, $macronameref)
					= parseTokens($lang, $HeaderDoc::sublang);
				    my $varIsConstant = 0;
				    my $blockmode = 0;
				    my $curtype = "";
				    my $warntype = "";
				    my $blockDebug = 0 || $localDebug;
				    my $parmDebug = 0 || $localDebug;
				    # my $localDebug = 1;

				    if ($inPDefine == 2) { $blockmode = 1; }
				    if ($inFunction || $inMethod) {
					if ($localDebug) {
						if ($inMethod) {
							print "inMethod\n";
						} else {
							print "inFunction\n";
						}
					}
					# @@@ FIXME DAG (OBJC)
					my $method = 0;
					if ($classType eq "occ" ||
						$classType eq "intf" ||
						$classType eq "occCat") {
							$method = 1;
					}
					if ($method) {
						$curObj = HeaderDoc::Method->new;
						$curtype = "method";
					} else {
						$curObj = HeaderDoc::Function->new;
						$curtype = "function";
					}
					$curObj->apiOwner($apiOwner);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
					if (length($functionGroup)) {
						$curObj->group($functionGroup);
					} else {
						$curObj->group($HeaderDoc::globalGroup);
					}
                                        $curObj->filename($filename);
					$curObj->linenum($inputCounter+$blockOffset);
					if ($method) {
						$curObj->processComment(\@fields);
					} else {
						$curObj->processComment(\@fields);
					}
				    } elsif ($inPDefine) {
					print "inPDefine\n" if ($localDebug);
					$curtype = "#define";
					if ($blockmode) { $warntype = "defineblock"; }
					$curObj = HeaderDoc::PDefine->new;
					$curObj->group($HeaderDoc::globalGroup);
					$curObj->apiOwner($apiOwner);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
					$curObj->inDefineBlock($blockmode);
                                        $curObj->filename($filename);
					$curObj->linenum($inputCounter+$blockOffset);
					$curObj->processComment(\@fields);
				    } elsif ($inVar) {
# print "inVar!!\n";
					print "inVar\n" if ($localDebug);
					$curtype = "constant";
					$varIsConstant = 0;
					$curObj = HeaderDoc::Var->new;
					$curObj->group($HeaderDoc::globalGroup);
					$curObj->apiOwner($apiOwner);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
                                        $curObj->filename($filename);
					$curObj->linenum($inputCounter+$blockOffset);
					$curObj->processComment(\@fields);
				    } elsif ($inConstant) {
					print "inConstant\n" if ($localDebug);
					$curtype = "constant";
					$varIsConstant = 1;
					$curObj = HeaderDoc::Constant->new;
					$curObj->group($HeaderDoc::globalGroup);
					$curObj->apiOwner($apiOwner);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
                                        $curObj->filename($filename);
					$curObj->linenum($inputCounter+$blockOffset);
					$curObj->processComment(\@fields);
				    } elsif ($inUnknown || $inClass) {
					if ($localDebug) {
						if ($inUnknown) {
							print "inUnknown\n";
						} else {
							print "inClass\n";
						}
					}
					$curtype = "UNKNOWN";
					if ($inUnknown) {
						$curObj = HeaderDoc::HeaderElement->new;
						$classKeyword = "auto";
					} else {
						$curObj = HeaderDoc::APIOwner->new;
						$classKeyword = @fields[0];
						$classKeyword =~ s/^\s*\/\*\!\s*//s;
						if (!length($classKeyword)) {
							$classKeyword = @fields[1];
						}
					}
					$curObj->group($HeaderDoc::globalGroup);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
					warnHDComment(\@inputLines, $inputCounter, $blockOffset, "unknown", "11");
				    } elsif ($inTypedef) {
# print "inTypedef\n"; $localDebug = 1;
					print "inTypedef\n" if ($localDebug);
					$curtype = $typedefname;
					# if ($lang eq "pascal") {
						# $curtype = "type";
					# } else {
						# $curtype = "typedef";
					# }
					$curObj = HeaderDoc::Typedef->new;
					$curObj->group($HeaderDoc::globalGroup);
					$curObj->apiOwner($apiOwner);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
                                        $curObj->filename($filename);
					$curObj->linenum($inputCounter+$blockOffset);
					$curObj->processComment(\@fields);
					$curObj->masterEnum(0);
					
					warnHDComment(\@inputLines, $inputCounter, $blockOffset, "enum", "11a");
					# if a struct declaration precedes the typedef, suck it up
				} elsif ($inStruct || $inUnion) {
					if ($localDebug) {
						if ($inUnion) {
							print "inUnion\n";
						} else {
							print "inStruct\n";
						}
					}
					if ($inUnion) {
						$curtype = "union";
					} else {
						$curtype = "struct";
					}
                                        $curObj = HeaderDoc::Struct->new;
					$curObj->group($HeaderDoc::globalGroup);
                                        $curObj->apiOwner($apiOwner);
                                        if ($inUnion) {     
                                            $curObj->isUnion(1);
                                        } else {
                                            $curObj->isUnion(0);
                                        }
                                        if ($xml_output) {
                                            $curObj->outputformat("hdxml");
                                        } else {
                                            $curObj->outputformat("html");
                                        }
                                        $curObj->filename($filename);
					$curObj->linenum($inputCounter+$blockOffset);
                                        $curObj->processComment(\@fields);
					warnHDComment(\@inputLines, $inputCounter, $blockOffset, "$curtype", "11b");
				} elsif ($inEnum) {
					print "inEnum\n" if ($localDebug);
					$curtype = "enum";
                                        $curObj = HeaderDoc::Enum->new;
					$curObj->masterEnum(1);
					$curObj->group($HeaderDoc::globalGroup);
                                        $curObj->apiOwner($apiOwner);
                                        if ($xml_output) {
                                            $curObj->outputformat("hdxml");
                                        } else {
                                            $curObj->outputformat("html");
                                        }
                                        $curObj->filename($filename);
					$curObj->linenum($inputCounter+$blockOffset);
                                        $curObj->processComment(\@fields);
					warnHDComment(\@inputLines, $inputCounter, $blockOffset, "$curtype", "11c");
				}
				if (!length($warntype)) { $warntype = $curtype; }
                                while (($inputLines[$inputCounter] !~ /\w/o)  && ($inputCounter <= $nlines)){
                                	# print "BLANKLINE IS $inputLines[$inputCounter]\n";
                                	$inputCounter++;
# print "warntype is $warntype\n";
                                	warnHDComment(\@inputLines, $inputCounter, $blockOffset, "$warntype", "12");
                                	print "Input line number[7]: $inputCounter\n" if ($localDebug);
                                };
                                # my  $declaration = $inputLines[$inputCounter];

print "NEXT LINE is ".$inputLines[$inputCounter].".\n" if ($localDebug);

	my $outertype = ""; my $newcount = 0; my $declaration = ""; my $namelist = "";

	my ($case_sensitive, $keywordhashref) = $curObj->keywords();
	my $typelist = ""; my $innertype = ""; my $posstypes = "";
	print "PTCT: $posstypes =? $curtype\n" if ($localDebug || $blockDebug);
	my $blockDec = "";
	my $hangDebug = 0 || $localDebug;
	while (($blockmode || ($outertype ne $curtype && $innertype ne $curtype && $posstypes !~ /$curtype/)) && ($inputCounter <= $nlines)) { # ($typestring !~ /$typedefname/)

		if ($hangDebug) { print "In Block Loop\n"; }

		# while ($inputLines[$inputCounter] !~ /\S/o && ($inputCounter <= $nlines)) { $inputCounter++; }
		# if (warnHDComment(\@inputLines, $inputCounter, 0, "blockParse:$outertype", "18b")) {
			# last;
		# } else { print "OK\n"; }
		print "DOING SOMETHING\n" if ($localDebug);
		$HeaderDoc::ignore_apiuid_errors = 1;
		my $junk = $curObj->apirefSetup();
		$HeaderDoc::ignore_apiuid_errors = 0;
		# the value of a constant
		my $value = "";
		my $pplref = undef;
		my $returntype = undef;
		my $pridec = "";
		my $parseTree = undef;
		my $simpleTDcontents = "";
		my $bpavail = "";
		print "Entering blockParse\n" if ($hangDebug);
		if ($subparse) {
			if ($lastParseTree) {
				# print "GOING FOR ANOTHER PARSE TREE.\n";
				$parseTree = $lastParseTree->nextTokenNoComments($soc, $ilc);
				if (!$parseTree) {
					my $curname = $curObj->name();
					warn("End of parse tree reached while searching for matching declaration.\n");
					warn "No matching declaration found.  Last name was $curname\n";
					warn "$outertype ne $curtype && $innertype ne $curtype && $posstypes !~ $curtype\n";
					$HeaderDoc::enable_cpp = $old_enable_cpp;
					return ($inputCounter, $cppAccessControlState);
				}
				$lastParseTree = $parseTree;
			} else {
				# print "FIRST PARSE TREE.\n";
				$parseTree = $subparseTree;;
				$lastParseTree = $parseTree;
				if (!$parseTree) {
					my $curname = $curObj->name();
					warn("End of parse tree reached while searching for matching declaration.\n");
					warn "No matching declaration found.  Last name was $curname\n";
					warn "$outertype ne $curtype && $innertype ne $curtype && $posstypes !~ $curtype\n";
					$HeaderDoc::enable_cpp = $old_enable_cpp;
					return ($inputCounter, $cppAccessControlState);
				}
			}
			my @ppl = ();
			@ppl = $parseTree->parsedParams();
			$pplref = \@ppl;
			print "PPLREF is $pplref\n" if ($localDebug);
			my $treestring = $parseTree->textTree();
			if ($treestring !~ /\w/) {
				my $curname = $curObj->name();
				warn("End of parse tree reached while searching for matching declaration.\n");
				warn "No matching declaration found.  Last name was $curname\n";
				warn "$outertype ne $curtype && $innertype ne $curtype && $posstypes !~ $curtype\n";
				$HeaderDoc::enable_cpp = $old_enable_cpp;
				return ($inputCounter, $cppAccessControlState);
			}
			@inputLines = ();
			foreach my $line (split(/\n/, $treestring)) {
				push(@inputLines, "$line\n");
			}
			$nlines = scalar(@inputLines) + 1000;
			$inputCounter = 0;
			$HeaderDoc::AccessControlState = $parseTree->acs();
			$cppAccessControlState = $parseTree->acs();

			my $parserState = $parseTree->parserState();
			my $findstate = $parseTree;
			while (!defined($parserState) && $findstate) {
				print "POS IS $findstate\n" if ($localDebug);
				print "POSTOKE IS ".$findstate->token()."\n" if ($localDebug);
				$parserState = $findstate->parserState;
				$findstate = $findstate->next();
			}
			if (!$parserState) {
				if (!$slowokay) {
					warn("Couldn't find parser state.  Using slow method.\n");
					$localDebug = 1;
					$hangDebug = 1;
				}
				($newcount, $declaration, $typelist, $namelist, $posstypes, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $blockOffset) = &blockParse($filename, $blockOffset, \@inputLines, $inputCounter, 0, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);
print "NC: $newcount\n" if ($localDebug);
				print "newcount IS $newcount\n" if ($localDebug);
				print "dec is $declaration\n" if ($localDebug);
				print "TYPELIST IS \"$typelist\"\n" if ($localDebug);
				print "NAMELIST IS \"$namelist\"\n" if ($localDebug);
				print "POSSTYPES IS \"$posstypes\"\n" if ($localDebug);
				print "PPLREF IS $pplref\n" if ($localDebug);
				print "RETURNTYPE IS \"$returntype\"\n" if ($localDebug);
				print "parseTree IS \"$parseTree\"\n" if ($localDebug);
			} else {
				my $bogusblockoffset;
				($newcount, $declaration, $typelist, $namelist, $posstypes, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $bogusblockoffset) = &blockParseReturnState($parserState, $parseTree, 0, "", 0, "", "");
				$newcount = 1;

				print "newcount IS $newcount\n" if ($localDebug);
				print "dec is $declaration\n" if ($localDebug);
				print "TYPELIST IS \"$typelist\"\n" if ($localDebug);
				print "NAMELIST IS \"$namelist\"\n" if ($localDebug);
				print "POSSTYPES IS \"$posstypes\"\n" if ($localDebug);
				print "PPLREF IS $pplref\n" if ($localDebug);
				print "RETURNTYPE IS \"$returntype\"\n" if ($localDebug);
				print "parseTree IS \"$parseTree\"\n" if ($localDebug);
			}
		} else {
			if ($nodec) {
				# print "NODEC\n";
				$declaration = "";
				$parseTree = HeaderDoc::ParseTree->new();
				my @ppl = ();
				$pplref = \@ppl;
				$newcount = $inputCounter;
			} else {
				($newcount, $declaration, $typelist, $namelist, $posstypes, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $blockOffset) = &blockParse($filename, $blockOffset, \@inputLines, $inputCounter, 0, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);
			}
		}

		if ($hangDebug) {
			print "DUMPING PARSE TREE:\n";$parseTree->dbprint();print "PARSETREEDONE\n";
		}

		if ($bpavail && length($bpavail)) { $curObj->availability($bpavail); }
		# print "BPAVAIL ($namelist): $bpavail\n";

		print "Left blockParse\n" if ($hangDebug);

		if ($outertype eq $curtype || $innertype eq $curtype || $posstypes =~ /$curtype/) {
			# Make sure we have the right UID for methods
			$curObj->declaration($declaration);
			$HeaderDoc::ignore_apiuid_errors = 1;
			my $junk = $curObj->apirefSetup(1);
			$HeaderDoc::ignore_apiuid_errors = 0;
		}
		$curObj->privateDeclaration($pridec);
		$parseTree->addAPIOwner($curObj);
		$curObj->parseTree(\$parseTree);
		# print "PT: $parseTree\n";
		# print "PT IS A $parseTree\n";
		# $parseTree->htmlTree();
		# $parseTree->processEmbeddedTags();

		my @parsedParamList = @{$pplref};
		print "VALUE IS $value\n" if ($localDebug);
		# warn("nc: $newcount.  ts: $typestring.  nl: $namelist\nBEGIN:\n$declaration\nEND.\n");

		my $method = 0;
		if ($classType eq "occ" ||
			$classType eq "intf" ||
			$classType eq "occCat") {
				$method = 1;
		}

		$declaration =~ s/^\s*//so;
		# if (!length($declaration)) { next; }

	print "obtained declaration\n" if ($localDebug);
		if ($localDebug) {
			print("IC: $inputCounter\n");
			print("DC: \"$declaration\"\n");
			print("TL: \"$typelist\"\n");
			print("DC: \"$namelist\"\n");
			print("DC: \"$posstypes\"\n");
		}

		$inputCounter = $newcount;

		my @oldnames = split(/[,;]/, $namelist);
		my @oldtypes = split(/ /, $typelist);

		$outertype = $oldtypes[0];
		if ($outertype eq "") {
			$outertype = $curtype;
			my $linenum = $inputCounter + $blockOffset;
			if (!$nodec) {
				warn("$filename:$linenum:Parser bug: empty outer type\n");
				warn("IC: $inputCounter\n");
				warn("DC: \"$declaration\"\n");
				warn("TL: \"$typelist\"\n");
				warn("NL: \"$namelist\"\n");
				warn("PT: \"$posstypes\"\n");
			}
		}
		$nodec = 0;
		$innertype = $oldtypes[scalar(@oldtypes)-1];

		if ($localDebug) {
			foreach my $ot (@oldtypes) {
				print "TYPE: \"$ot\"\n";
			}
		}

		my $explicit_name = 1;
		my $curname = $curObj->name();
		$curname =~ s/^\s*//o;
		$curname =~ s/\s*$//o;
		my @names = ( ); #$curname
		my @types = ( ); #$outertype

		my $nameDebug = 0;
		print "names:\n" if ($nameDebug);
		if ($curname !~ /:$/o) {
		    foreach my $name (@oldnames) {
			my $NCname = $name;
			my $NCcurname = $curname;
			$NCname =~ s/:$//so;
			$NCcurname =~ s/:$//so;
			print "NM \"$name\" CN: \"$curname\"\n" if ($nameDebug);
			if ($NCname eq $NCcurname && $name ne $curname) {
			    $curname .= ":";
			    $curObj->name($curname);
			    $curObj->rawname($curname);
			}
		    }
		}
		print "endnames\n" if ($nameDebug);

		if (length($curname) && length($curtype)) {
			push(@names, $curname);
			push(@types, $outertype);
		}
		my $count = 0;
		my $operator = 0;
		if ($typelist eq "operator") {
			$operator = 1;
		}
		foreach my $name (@oldnames) {
			if ($operator) {
				$name =~ s/^\s*operator\s*//so;
				$name = "operator $name";
				$curname =~ s/^operator(\s*)(\S+)/operator $2/so;
			}
# print "NAME \"$name\"\nCURNAME \"$curname\"\n";
			if (($name eq $curname) && ($oldtypes[$count] eq $outertype)) {
				$explicit_name = 0;
				$count++;
			} else {
				push(@names, $name);
				push(@types, $oldtypes[$count++]);
			}
		}
		# foreach my $xname (@names) { print "XNAME: $xname\n"; }
		if ($hangDebug) { print "Point A\n"; }
		# $explicit_name = 0;
		my $count = 0;
		foreach my $name (@names) {
		    # my $localDebug = 0;
		    my $typestring = $types[$count++];

		    print "NAME IS $name\n" if ($localDebug);
		    print "CURNAME IS $curname\n" if ($localDebug);
		    print "TYPESTRING IS $typestring\n" if ($localDebug);
		    print "CURTYPE IS $curtype\n" if ($localDebug);
			print "MATCH: $name IS A $typestring.\n" if ($localDebug);

print "DEC ($name / $typestring): $declaration\n" if ($localDebug);

		    $name =~ s/\s*$//go;
		    $name =~ s/^\s*//go;
		    my $cmpname = $name;
		    my $cmpcurname = $curname;
		    $cmpname =~ s/:$//so;
		    $cmpcurname =~ s/:$//so;
		    if (!length($name)) { next; }
			print "Got $name ($curname)\n" if ($localDebug);

			my $extra = undef;

			if ($typestring eq $curtype && ($cmpname eq $cmpcurname || !length($curname))) {
				print "$curtype = $typestring\n" if ($localDebug);
				$extra = $curObj;
# print "E=C\n$extra\n$curObj\n";
				if ($blockmode) {
					$blockDec .= $declaration;
					print "SPDF[1]\n" if ($hangDebug);
					$curObj->isBlock(1);
					# $curObj->setPDefineDeclaration($blockDec);
					print "END SPDF[1]\n" if ($hangDebug);
					# $declaration = $curObj->declaration() . $declaration;
				}
			} else {
				print "NAME IS $name\n" if ($localDebug);
			    if ($curtype eq "function" && $posstypes =~ /function/o) {
				$curtype = "UNKNOWN";
print "setting curtype to UNKNOWN\n" if ($localDebug);
			    }
			    if ($typestring eq $outertype || !$HeaderDoc::outerNamesOnly) {
				if ($typestring =~ /^(class|\@class|\@interface|\@protocol)/ || $inClass) {
                                        print "blockParse returned class\n" if ($localDebug);
					$classType = classTypeFromFieldAndBPinfo($classKeyword, $typestring." ".$posstypes, $declaration, $filename, $inputCounter+$blockOffset, $HeaderDoc::sublang);
					if ($classType eq "intf") {
						$extra = HeaderDoc::ObjCProtocol->new();
					} elsif ($classType eq "occCat") {
						$extra = HeaderDoc::ObjCCategory->new();
					} elsif ($classType eq "occ") {
						$extra = HeaderDoc::ObjCClass->new();
					} else {
						$extra = HeaderDoc::CPPClass->new();
						if ($inInterface || $typestring =~ /typedef/) {
							$inInterface = 0;
							$extra->isCOMInterface(1);
							$extra->tocTitlePrefix('COM Interface:');
						}
					}
					if ($typestring =~ /typedef/) {
						$extra->CClass(1);
					}

					$extra->group($HeaderDoc::globalGroup);
                                        $extra->filename($filename);
					$extra->linenum($inputCounter+$blockOffset);
					$extra->headerObject($HeaderDoc::headerObject);
					# my $superclass = &get_super($classType, $declaration);
					my $class = ref($extra) || $extra;
					my $superclass = $posstypes;
					my $superclassfieldname = "Superclass";
					if ($class =~ /HeaderDoc::ObjCCategory/) {
						$superclassfieldname = "Extends&nbsp;Class";
					}
					if (length($superclass) && (!($extra->checkShortLongAttributes($superclassfieldname))) && !$extra->CClass()) {
						$extra->attribute($superclassfieldname, $superclass, 0);
					}
					# $extra->declaration($declaration);
					# $extra->declarationInHTML($declaration);
					# if ($curtype eq "UNKNOWN") {
						$extra->processComment(\@fields);
					# }

					# if ($typestring eq "\@protocol") {
						# push (@classObjects, $extra);
						# $headerObject->addToProtocols($extra);
					# } elsif ($typestring eq "\@interface") {
						# push (@categoryObjects, $extra);
						# headerObject->addToCategories($extra);
					# } elsif ($typestring eq "\@class") {
						# push (@classObjects, $extra);
						# $headerObject->addToClasses($extra);
					# } else {
						# # class or typedef struct
						# push (@classObjects, $extra);
						# $headerObject->addToClasses($extra);
					# }

				} elsif ($typestring =~ /^$typedefname/ && length($typedefname)) {
					print "blockParse returned $typedefname\n" if ($localDebug);
					$extra = HeaderDoc::Typedef->new;
					$extra->group($HeaderDoc::globalGroup);
                                        $extra->filename($filename);
					$extra->linenum($inputCounter+$blockOffset);
					$curObj->masterEnum(1);
					if ($curtype eq "UNKNOWN") {
						$extra->processComment(\@fields);
					}
				} elsif ($typestring =~ /^struct/o || $typestring =~ /^union/o || ($lang eq "pascal" && $typestring =~ /^record/o)) {
					print "blockParse returned struct or union ($typestring)\n" if ($localDebug);
					$extra = HeaderDoc::Struct->new;
					$extra->group($HeaderDoc::globalGroup);
                                        $extra->filename($filename);
					$extra->linenum($inputCounter+$blockOffset);
					if ($typestring =~ /union/o) {
						$extra->isUnion(1);
					}
					if ($curtype eq "UNKNOWN") {
						$extra->processComment(\@fields);
					}
				} elsif ($typestring =~ /^enum/o) {
					print "blockParse returned enum\n" if ($localDebug);
					$extra = HeaderDoc::Enum->new;
					$extra->group($HeaderDoc::globalGroup);
                                        $extra->filename($filename);
					$extra->linenum($inputCounter+$blockOffset);
					if ($curtype eq "UNKNOWN") {
						$extra->processComment(\@fields);
					}
					if ($curtype eq "enum" || $curtype eq "typedef") {
						$extra->masterEnum(0);
					} else {
						$extra->masterEnum(1);
					}
				} elsif ($typestring =~ /^MACRO/o) {
					print "blockParse returned MACRO\n" if ($localDebug);
					# silently ignore this noise.
				} elsif ($typestring =~ /^\#define/o) {
					print "blockParse returned #define\n" if ($localDebug);
					$extra = HeaderDoc::PDefine->new;
					$extra->inDefineBlock($blockmode);
					$extra->group($HeaderDoc::globalGroup);
                                        $extra->filename($filename);
					$extra->linenum($inputCounter+$blockOffset);
					if ($curtype eq "UNKNOWN") {
						$extra->processComment(\@fields);
					}
				} elsif ($typestring =~ /^constant/o) {
					if ($declaration =~ /\s+const\s+/o) {
						$varIsConstant = 1;
						print "blockParse returned constant\n" if ($localDebug);
						$extra = HeaderDoc::Constant->new;
						$extra->group($HeaderDoc::globalGroup);
                                        	$extra->filename($filename);
						$extra->linenum($inputCounter+$blockOffset);
						if ($curtype eq "UNKNOWN") {
							$extra->processComment(\@fields);
						}
					} else {
						$varIsConstant = 0;
						print "blockParse returned variable\n" if ($localDebug);
						$extra = HeaderDoc::Var->new;
						$extra->group($HeaderDoc::globalGroup);
                                        	$extra->filename($filename);
						$extra->linenum($inputCounter+$blockOffset);
						if ($curtype eq "UNKNOWN") {
							$extra->processComment(\@fields);
						}
					}
				} elsif ($typestring =~ /^(function|method|operator|ftmplt)/o) {
					print "blockParse returned function or method\n" if ($localDebug);
					if ($method) {
						$extra = HeaderDoc::Method->new;
						if (length($functionGroup)) {
							$extra->group($functionGroup);
						} else {
							$extra->group($HeaderDoc::globalGroup);
						}
                                        	$extra->filename($filename);
						$extra->linenum($inputCounter+$blockOffset);
						if ($curtype eq "UNKNOWN") {
							$extra->processComment(\@fields);
						}
					} else {
						$extra = HeaderDoc::Function->new;
						if (length($functionGroup)) {
							$extra->group($functionGroup);
						} else {
							$extra->group($HeaderDoc::globalGroup);
						}
                                        	$extra->filename($filename);
						$extra->linenum($inputCounter+$blockOffset);
						if ($curtype eq "UNKNOWN") {
							$extra->processComment(\@fields);
						}
					}
					if ($typestring eq "ftmplt") {
						$extra->isTemplate(1);
					}
				} else {
					my $linenum = $inputCounter + $blockOffset;
					warn("$filename:$linenum:Unknown keyword $typestring in block-parsed declaration\n");
				}
			    } else {
				print "Dropping alternate name\n" if ($localDebug);
			    }
			}
			# print "HERE\n";
			if ($hangDebug) { print "Point NEWB\n"; }
			if ($curtype eq "UNKNOWN" && $extra) {
				my $orig_parsetree_ref = $curObj->parseTree();
				bless($orig_parsetree_ref, "HeaderDoc::ParseTree");
				my $pt = ${$orig_parsetree_ref};
				$curObj = $extra;
				$pt->addAPIOwner($extra);
				$curObj->parseTree($orig_parsetree_ref);
			}
			if ($extra) {
				print "Processing \"extra\" ($extra).\n" if ($localDebug);
				if ($bpavail && length($bpavail)) {
					$extra->availability($bpavail);
				}
				my $cleantypename = "$typestring $name";
				$cleantypename =~ s/\s+/ /sgo;
				$cleantypename =~ s/^\s*//so;
				$cleantypename =~ s/\s*$//so;
				if (length($cleantypename)) {
					$HeaderDoc::namerefs{$cleantypename} = $extra;
				}
				my $extraclass = ref($extra) || $extra;
				my $abstract = $curObj->abstract();
				my $discussion = $curObj->discussion();
				my $pridec = $curObj->privateDeclaration();
				$extra->privateDeclaration($pridec);

				if ($curObj != $extra) {
					my $orig_parsetree_ref = $curObj->parseTree();
					# my $orig_parsetree = ${$orig_parsetree_ref};
					bless($orig_parsetree_ref, "HeaderDoc::ParseTree");
					$$orig_parsetree_ref->addAPIOwner($extra);
					$extra->parseTree($orig_parsetree_ref); # ->clone());
					# my $new_parsetree = $extra->parseTree();
					# bless($new_parsetree, "HeaderDoc::ParseTree");
					# $new_parsetree->addAPIOwner($extra);
					# $new_parsetree->processEmbeddedTags();
				}
				# print "PROCESSING CO $curObj EX $extra\n";
				# print "PT: ".$curObj->parseTree()."\n";

				if ($blockmode) {
					my $altDiscussionRef = $curObj->checkAttributeLists("Included Defines");
					my $discussionParam = $curObj->taggedParamMatching($name);
					# print "got $discussionParam\n";
	# print "DP: $discussionParam ADP: $altDiscussionRef\n";
					if ($discussionParam) {
						$discussion = $discussionParam->discussion;
					} elsif ($altDiscussionRef) {
						my @altDiscEntries = @{$altDiscussionRef};
						foreach my $field (@altDiscEntries) {
						    my ($dname, $ddisc) = &getAPINameAndDisc($field);
						    if ($name eq $dname) {
						    	$discussion = $ddisc;
						    }
						}
					}
					if ($curObj != $extra) {
						# we use the parsed parms to
						# hold subdefines.
						$curObj->addParsedParameter($extra);
					}
				}

				print "Point B1\n" if ($hangDebug);
				if ($extraclass ne "HeaderDoc::Method" && !$extra->isAPIOwner()) {
					print "Point B2\n" if ($hangDebug);
					my $paramName = "";
					my $position = 0;
					my $type = "";
					if ($extraclass eq "HeaderDoc::Function") {
						$extra->returntype($returntype);
					}
					my @tempPPL = @parsedParamList;
					foreach my $parsedParam (@tempPPL) {
					    if (0) {
						# temp code
						print "PARSED PARAM: \"$parsedParam\"\n" if ($parmDebug);
						if ($parsedParam =~ s/(\w+\)*)$//so) {
							$paramName = $1;
						} else {
							$paramName = "";
						}

						$parsedParam =~ s/\s*$//so;
						if (!length($parsedParam)) {
							$type = $paramName;
							$paramName = "";
						} else {
							$type = $parsedParam;
						}
						print "NAME: $paramName\nType: $type\n" if ($parmDebug);

						my $param = HeaderDoc::MinorAPIElement->new();
						$param->linenum($inputCounter+$blockOffset);
						$param->outputformat($extra->outputformat);
						$param->name($paramName);
						$param->position($position++);
						$param->type($type);
						$extra->addParsedParameter($param);
					    } else {
						# the real code
						my $ppDebug = 0 || $parmDebug;

						print "PARSED PARAM: \"$parsedParam\"\n" if ($ppDebug);

						my $ppstring = $parsedParam;
						$ppstring =~ s/^\s*//sgo;
						$ppstring =~ s/\s*$//sgo;

						my $foo;
						my $dec;
						my $pridec;
						my $type;
						my $name;
						my $pt;
						my $value;
						my $pplref;
						my $returntype;
						if ($ppstring eq "...") {
							$name = $ppstring;
							$type = "";
							$pt = "";
						} else {
							$ppstring .= ";";
							my @array = ( $ppstring );

							my $parseTree = undef;
							my $simpleTDcontents = "";
							my $bpavail = "";
							my $bogusblockoffset;
							($foo, $dec, $type, $name, $pt, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $bogusblockoffset) = &blockParse($filename, $extra->linenum(), \@array, 0, 1, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);
						}
						if ($ppDebug) {
							print "NAME: $name\n";
							print "TYPE: $type\n";
							print "PT:   $pt\n";
							print "RT:   $returntype\n";
						}
						
						my $param = HeaderDoc::MinorAPIElement->new();
						$param->linenum($inputCounter+$blockOffset);
						$param->outputformat($extra->outputformat);
						$returntype =~ s/^\s*//s;
						$returntype =~ s/\s*$//s;
						if ($returntype =~ /(struct|union|enum|record|typedef)$/) {
							$returntype .= " $name";
							$name = "";
						} elsif (!length($returntype)) {
							$returntype .= " $name";
							if ($name !~ /\.\.\./) {
								$name = "anonymous$name";
							}
						}
						print "NM: $name RT $returntype\n" if ($ppDebug);
						$param->name($name);
						$param->position($position++);
						$param->type($returntype);
						$extra->addParsedParameter($param);
					    }
					}
				} else {
					# we're a method
					$extra->returntype($returntype);
					my @newpps = $parseTree->objCparsedParams();
					foreach my $newpp (@newpps) {
						$extra->addParsedParameter($newpp);
					}
				}
				my $extradirty = 0;
				if (length($simpleTDcontents)) {
					$extra->typedefContents($simpleTDcontents);
				}
				print "Point B3\n" if ($hangDebug);
				if (length($preAtPart)) {
					print "preAtPart: $preAtPart\n" if ($localDebug);
					$extra->discussion($preAtPart);
				} elsif ($extra != $curObj) {
					# Otherwise this would be bad....
					$extra->discussion($discussion);

				}
				print "Point B4\n" if ($hangDebug);
				$extra->abstract($abstract);
				if (length($value)) { $extra->value($value); }
				if ($extra != $curObj || !length($curObj->name())) {
					$name =~ s/^(\s|\*)*//sgo;
				}
				print "NAME IS $name\n" if ($localDebug);
				$extra->rawname($name);
				# my $namestring = $curObj->name();
				# if ($explicit_name && 0) {
					# $extra->name("$name ($namestring)");
				# } else {
					# $extra->name($name);
				# }
				print "Point B5\n" if ($hangDebug);
				$HeaderDoc::ignore_apiuid_errors = 1;
				$extra->name($name);
				my $junk = $extra->apirefSetup();
				$HeaderDoc::ignore_apiuid_errors = 0;


				# print "NAMES: \"".$curObj->name()."\" & \"".$extra->name()."\"\n";
				# print "ADDYS: ".$curObj." & ".$extra."\n";

				if ($extra != $curObj) {
				    my @params = $curObj->taggedParameters();
				    foreach my $param (@params) {
					$extradirty = 1;
					# print "CONSTANT $param\n";
					$extra->addTaggedParameter($param->clone());
				    }
				    my @constants = $curObj->constants();
				    foreach my $constant (@constants) {
					$extradirty = 1;
					# print "CONSTANT $constant\n";
					if ($extra->can("addToConstants")) {
					    $extra->addToConstants($constant->clone());
					    # print "ATC\n";
					} elsif ($extra->can("addConstant")) {
					    $extra->addConstant($constant->clone());
					    # print "AC\n";
					}
				    }

				    print "Point B6\n" if ($hangDebug);
				    if (length($curObj->name())) {
	# my $a = $extra->rawname(); my $b = $curObj->rawname(); my $c = $curObj->name();
	# print "EXTRA RAWNAME: $a\nCUROBJ RAWNAME: $b\nCUROBJ NAME: $c\n";
					# change whitespace to ctrl-d to
					# allow multi-word names.
					my $ern = $extra->rawname();
					if ($ern =~ /\s/o && $localDebug) {
						print "changed space to ctrl-d\n";
						print "ref is ".$extra->apiuid()."\n";
					}
					$ern =~ s/\s/\cD/sgo;
					my $crn = $curObj->rawname();
					if ($crn =~ /\s/o && $localDebug) {
						print "changed space to ctrl-d\n";
						print "ref is ".$curObj->apiuid()."\n";
					}
					$crn =~ s/\s/\cD/sgo;
					$curObj->attributelist("See Also", $ern." ".$extra->apiuid());
					$extra->attributelist("See Also", $crn." ".$curObj->apiuid());
				    }
				}
				print "Point B7 TS = $typestring\n" if ($hangDebug);
				if (ref($apiOwner) ne "HeaderDoc::Header") {
					$extra->accessControl($cppAccessControlState); # @@@ FIXME DAG CHECK FOR OBJC
				}
				if ($extra != $curObj && $curtype ne "UNKNOWN" && $curObj->can("fields") && $extra->can("fields")) {
					my @fields = $curObj->fields();
					print "B7COPY\n" if ($localDebug);

					foreach my $field (@fields) {
						bless($field, "HeaderDoc::MinorAPIElement");
						my $newfield = $field->clone();
						$extradirty = 1;
						$extra->addField($newfield);
						# print "Added field ".$newfield->name()." to $extra ".$extra->name()."\n";
					}
				}
				$extra->apiOwner($apiOwner);
				if ($xml_output) {
				    $extra->outputformat("hdxml");
				} else { 
				    $extra->outputformat("html");
				}
				$extra->filename($filename);

		# warn("Added ".$extra->name()." ".$extra->apiuid().".\n");

				print "B8X blockmode=$blockmode ts=$typestring\n" if ($localDebug || $hangDebug);
				if ($typestring =~ /^(class|\@class|\@interface|\@protocol)/ || $inClass) {
					print "ITSACLASS!\n" if ($localDebug);
					$extra->declaration($declaration);
					$extra->declarationInHTML($declaration);

					if (!$subparse || ($extra != $curObj)) {
					    # my $localDebug = 1;
						print "ADDING TO CLASSES/PROTOCOLS/*\n" if ($localDebug);
						$classType = classTypeFromFieldAndBPinfo($classKeyword, $typestring." ".$posstypes, $declaration, $filename, $inputCounter+$blockOffset, $HeaderDoc::sublang);
						if ($classType eq "intf") {
							push (@classObjects, $extra);
							print "intf\n" if ($localDebug);
							$apiOwner->addToProtocols($extra);
						} elsif ($classType eq "occCat") {
							push (@categoryObjects, $extra);
							print "occCat\n" if ($localDebug);
							$apiOwner->addToCategories($extra);
						} elsif ($classType eq "occ") {
							push (@classObjects, $extra);
							print "occ\n" if ($localDebug);
							$apiOwner->addToClasses($extra);
						} else {
							if ($classType ne "C" || $extra->isCOMInterface()) {
								# print "CT: $classType\n";
								# class or typedef struct
								# (java, C, cpp, etc.)
								push (@classObjects, $extra);
								print "other ($classType)\n" if ($localDebug);
								$apiOwner->addToClasses($extra);
								# print "ADDING CLASS\n";
							}
						}
					}
				} elsif ($typestring =~ /$typedefname/ && length($typedefname)) {
	                		if (length($declaration)) {
                        			$extra->setTypedefDeclaration($declaration);
					}
					if (length($extra->name())) {
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToTypedefs($extra); }
				    	}
				} elsif ($typestring =~ /MACRO/o) {
					# throw these away.
					# $extra->setPDefineDeclaration($declaration);
					# $apiOwner->addToPDefines($extra);
				} elsif ($typestring =~ /#define/o) {
					print "SPDF[2]\n" if ($hangDebug);
					$extra->setPDefineDeclaration($declaration);
# print "DEC:$declaration\n" if ($hangDebug);
					print "END SPDF[2]\n" if ($hangDebug);
					if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToPDefines($extra); }
				} elsif ($typestring =~ /struct/o || $typestring =~ /union/o || ($lang eq "pascal" && $typestring =~ /record/o)) {
					if ($typestring =~ /union/o) {
						$extra->isUnion(1);
					} else {
						$extra->isUnion(0);
					}
					# $extra->declaration($declaration);
# print "PRE (DEC IS $declaration)\n";
					$extra->setStructDeclaration($declaration);
# print "POST\n";
					if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToStructs($extra); }
				} elsif ($typestring =~ /enum/o) {
					$extra->declaration($declaration);
					$extra->declarationInHTML($extra->getEnumDeclaration($declaration));
print "B8ENUM\n" if ($localDebug || $hangDebug);
					if (($blockmode != 2) || ($extra != $curObj)) {
print "B8ENUMINSERT apio=$apiOwner\n" if ($localDebug || $hangDebug);
 $apiOwner->addToEnums($extra); }
				} elsif ($typestring =~ /\#define/o) {
					print "SPDF[3]\n" if ($hangDebug);
					$extra->setPDefineDeclaration($declaration);
					print "END SPDF[3]\n" if ($hangDebug);
					if (($blockmode != 2) || ($extra != $curObj)) { $headerObject->addToPDefines($extra); }
				} elsif ($typestring =~ /(function|method|operator|ftmplt)/o) {
					if ($method) {
						$extra->setMethodDeclaration($declaration);
						$HeaderDoc::ignore_apiuid_errors = 1;
						my $junk = $extra->apirefSetup(1);
						$extradirty = 0;
						$HeaderDoc::ignore_apiuid_errors = 0;
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToMethods($extra); }
					} else {
						print "SFD\n" if ($hangDebug);
						$extra->setFunctionDeclaration($declaration);
						print "END SFD\n" if ($hangDebug);
						$HeaderDoc::ignore_apiuid_errors = 1;
						my $junk = $extra->apirefSetup(1);
						$extradirty = 0;
						$HeaderDoc::ignore_apiuid_errors = 0;
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToFunctions($extra); }
					}
					if ($typestring eq "ftmplt") {
						$extra->isTemplate(1);
					}
				} elsif ($typestring =~ /constant/o) {
					$extra->declaration($declaration);
					if ($varIsConstant) {
					    $extra->setConstantDeclaration($declaration);
                                            if (length($extra->name())) {
                                                    if (ref($apiOwner) ne "HeaderDoc::Header") {
                                                        $extra->accessControl($cppAccessControlState);
                                                        if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToVars($extra); }
                                                    } else { # headers group by type
                                                            if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToConstants($extra); }
                                                }
                                            }
					} else {
						$extra->setVarDeclaration($declaration);
                                                if (ref($apiOwner) ne "HeaderDoc::Header") {
                                                    $extra->accessControl($cppAccessControlState);
						}
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToVars($extra); }
					}
				} else {
					my $linenum = $inputCounter + $blockOffset;
					warn("$filename:$linenum:Unknown typestring $typestring returned by blockParse\n");
				}
				print "B9 blockmode=$blockmode ts=$typestring\n" if ($localDebug || $hangDebug);
				$extra->checkDeclaration();
				$HeaderDoc::ignore_apiuid_errors = 1;
				my $junk = $extra->apirefSetup($extradirty);
				$HeaderDoc::ignore_apiuid_errors = 0;
			}
		}
		if ($hangDebug) {
			print "Point C\n";
			print "inputCounter is $inputCounter, #inputLines is $nlines\n";
		}

		while ($inputLines[$inputCounter] !~ /\S/o && ($inputCounter <= $nlines)) { $inputCounter++; }
		if ($hangDebug) { print "Point D\n"; }
		if ($curtype eq "UNKNOWN") { $curtype = $outertype; }
		if ((($outertype ne $curtype && $innertype ne $curtype && $posstypes !~ /$curtype/)) && (($inputCounter > $nlines) || warnHDComment(\@inputLines, $inputCounter, $blockOffset, "blockParse:$outertype", "18a"))) {
			warn "No matching declaration found.  Last name was $curname\n";
			warn "$outertype ne $curtype && $innertype ne $curtype && $posstypes !~ $curtype\n";
			last;
		}
		if ($hangDebug) { print "Point E\n"; }
		if ($blockmode) {
			warn "next line: ".$inputLines[$inputCounter]."\n" if ($hangDebug);
			$blockmode = 2;
			$HeaderDoc::ignore_apiuid_errors = 1;
			if (warnHDComment(\@inputLines, $inputCounter, $blockOffset, "blockMode:$outertype", "18a") == 1) {
				# print "OT: $outertype\n";
				$blockmode = 0;
				warn "Block Mode Ending\n" if ($hangDebug);
			}
			$HeaderDoc::ignore_apiuid_errors = 0;
		}
		print "PTCT: $posstypes =? $curtype\n" if ($localDebug || $blockDebug);
	}
	if (length($blockDec)) {
		$curObj->declaration($blockDec);
		$curObj->declarationInHTML($blockDec);
	}
	if ($hangDebug) { print "Point F\n"; }
	print "Out of Block\n" if ($localDebug || $blockDebug);
	# the end of this block assumes that inputCounter points
	# to the last line grabbed, but right now it points to the
	# next line available.  Back it up by one.
	$inputCounter--;
	# warn("NEWDEC:\n$declaration\nEND NEWDEC\n");

				}  ## end blockParse handler
	if ($subparse) {
		$HeaderDoc::enable_cpp = $old_enable_cpp;
	}
	return ($inputCounter, $cppAccessControlState, $classType, \@classObjects, \@categoryObjects, $blockOffset);
}

sub cpp_add($)
{
    my $parseTree = shift;
    my $string = $parseTree->textTree($HeaderDoc::lang, $HeaderDoc::sublang);
    my $localDebug = 0;

    my $slstring = $string;
    $slstring =~ s/\\\n/ /sg;

    print "cpp_add: STRING WAS $string\n" if ($cppDebug || $localDebug);
    print "SLSTRING: $slstring\n" if ($localDebug || $cppDebug);

    # if ($slstring =~ s/^\s*#define\s+(\w+)\(//s) {
    if ($slstring =~ s/^(?:\/\*.*?\*\/)?\s*#define\s+((?:\w|::|->)+)(\s|\(|\{)//s) {
	my $name = $1;
	my $namequot = quote($name);
	# my $firsttoken = $2;

	print "CPP ADDING FUNC-LIKE MACRO\n" if ($localDebug || $cppDebug);

	print "GOT NAME $name\n" if ($localDebug || $cppDebug);

	$string =~ s/^.*?$namequot//s;

	print "POST-STRIP: STRING IS \"$string\"\n" if ($localDebug || $cppDebug);

	my @tokens = split(/(\/\/|\/\*|\*\/|\W)/, $string);

	my $firstpart = "";
	my $lastpart = "";
	my $fpdone = 0;
	my $lasttoken = "";
	my $inChar = 0; my $inString = 0; my $inComment = 0; my $inSLC = 0;
	my $inParen = 0;
	foreach my $token (@tokens) {
	    print "TOK: $token LAS: $lasttoken ICH: $inChar ICO: $inComment ISL: $inSLC IST: $inString\n" if ($localDebug || $cppDebug);
	    if (!$fpdone) {
		if (!$inParen && $token =~ /\w/) {
			$lastpart .= $token;
			$fpdone = 1;
			next;
		} elsif ($token eq "//" && !$inComment) {
			# Since we don't strip single-line comments, we have to avoid breaking the parse
			# when a macro is included.  (Note that this makes us consistent which GNU cpp,
			# but other C preprocessors will choke on any code that trips this case.)
			$inSLC = 1;
			$token = "/*";
			$lastpart .= $token;
			$fpdone = 1;
			next;
		} elsif ($token eq "/*" && !$inChar && !$inString && !$inSLC) {
			$inComment = 1;
		} elsif ($token eq "*/" && !$inChar && !$inString && !$inSLC) {
			$inComment = 0;
		} elsif ($token eq '\\') {
			if ($lasttoken eq '\\') { $lasttoken = ""; }
			else { $lasttoken = $token; }
		} elsif ($token eq '"') {
			if ($lasttoken ne '\\') {
				if (!$inChar && !$inComment && !$inSLC) {
					$inString = !$inString;
				}
			}
			$lasttoken = $token;
		} elsif ($token eq "'") {
			if ($lasttoken ne '\\') {
				if (!$inString && !$inComment && !$inSLC) {
					$inChar = !$inChar;
				}
			}
			$lasttoken = $token;
		} elsif (!$inChar && !$inString && !$inComment && !$inSLC) {
			if ($token eq "(") {
				$inParen++;
			} elsif ($token eq ")") {
				$inParen--;
			} elsif ($token =~ /\s/) {
				if (!$inParen) {
					$fpdone = 1;
				}
			}
			$lasttoken = $token;
		}
		$firstpart .= $token;
	    } else {
		if ($token eq "//" && !$inComment) {
			$inSLC = 1;
			$token = "/*";
		} elsif ($token eq "/*" && !$inChar && !$inString && !$inSLC) {
			$inComment = 1;
		} elsif ($token eq "*/" && !$inChar && !$inString && !$inSLC) {
			$inComment = 0;
		}
		$lastpart .= $token;
	    }
	}
	$firstpart =~ s/^\(//s;
	$firstpart =~ s/\s*$//s;
	$firstpart =~ s/\)$//s;

	if ($inSLC) {
		# See comment about single-line comments above.
		$lastpart .= "*/";
	}

	print "FP: \"$firstpart\"\nLP: \"$lastpart\"\nFPLPEND\n" if ($cppDebug || $localDebug);

	if ($lastpart) {
		my @lines = split(/[\r\n]/, $lastpart);
		my $lastline = pop(@lines);
		my $definition = "";

		foreach my $line (@lines) {
			$line =~ s/\\\s*$//s;
			$line .= "\n";
			$definition .= $line;
		}
		$lastline .= "\n";

		push(@lines, $lastline);
		$definition .= "$lastline\n";

		print "ADDING NAME=\"$name\" ARGS=\"$firstpart\" DEFINITION=\"$definition\"\n" if ($cppDebug);

		if ($CPP_HASH{$name}) {
			warn "WARNING: Multiple definitions for $name\n" if (($cppDebug || $warnAllMultipleDefinitions) && $HeaderDoc::enable_cpp);
		} else {
			$CPP_HASH{$name} = $definition;
			if (length($firstpart)) {
				$CPP_ARG_HASH{$name} = $firstpart;
			}
		}
	} else {
		# This is defining a function-like macro to wipe.
		# warn("Unable to process #define macro \"$name\".\n");
		if ($CPP_HASH{$name}) {
			warn "WARNING: Multiple definitions for $name\n" if ($cppDebug || $warnAllMultipleDefinitions);
		} else {
			$CPP_HASH{$name} = "";
			if (length($firstpart)) {
				$CPP_ARG_HASH{$name} = "";
			}
		}
	}
    } else {
	warn "WARNING: CAN'T HANDLE \"$string\".\n" if ($HeaderDoc::enable_cpp);
    }
}

sub cpp_preprocess
{
    my $part = shift;
    my $linenum = shift;
    # my $hashlistref = shift;
    # my $arghashlistref = shift;

    my $hasargs = 0;

    # my @hashlist = ();
    # my @arghashlist = ();
    # if ($hashlistref) { @hashlist = @{$hashlistref}; }
    # if ($arghashlistref) { @arghashlist = @{$arghashlistref}; }

    my $count = 0;
    if ($HeaderDoc::enable_cpp > 0) {
# print "CPP ENABLE\n";
      foreach my $hashhashref (@HeaderDoc::cppHashList) {
# print "HASHREFCHECK\n";
	my $hashref = $hashhashref->{HASHREF};
	print "HASHREF: $hashref\n" if ($cppDebug);
	if (!$hashref) {
		warn "Empty hashref object!\n";
		next;
	}
	my %hash = %{$hashhashref->{HASHREF}};
	if ($linenum <= $hashhashref->{LINENUM}) {
		print "Skiping hash $hashhashref->{FILENAME}.  Line not reached.\n" if ($cppDebug);
		next;
	}
	print "COUNT: $count\nNARGHASHES: ".scalar(@HeaderDoc::cppArgHashList)."\n" if ($cppDebug);
	print "NHASHES: ".scalar(@HeaderDoc::cppHashList)."\n" if ($cppDebug);
	my $arghashref = $HeaderDoc::cppArgHashList[$count++];
	my %arghash = %{$arghashref};

	my $altpart = $hash{$part};
	if ($altpart && length($altpart)) {
		print "EXTHASH FOUND NAME=\"$part\" REPLACEMENT=\"$altpart\"\n" if ($cppDebug);
		if ($arghash{$part}) { $hasargs = 1; }
		print "HASARGS: $hasargs\n" if ($cppDebug);
		return ($altpart, $hasargs, $arghash{$part});
	}
      }

      my $altpart = $CPP_HASH{$part};
      if ($altpart && length($altpart)) {
	print "FOUND NAME=\"$part\" REPLACEMENT=\"$altpart\"\n" if ($cppDebug);
	if ($CPP_ARG_HASH{$part}) { $hasargs = 1; }
	print "HASARGS: $hasargs\n" if ($cppDebug);
	return ($altpart, $hasargs, $CPP_ARG_HASH{$part});
      }
    }

    # If we got here, either CPP is disabled or we didn't find anything.
    if ($HeaderDoc::enable_cpp != -1) {
	print "Checking token \"$part\" for ignored macros\n" if ($cppDebug);
	my $altpart = $HeaderDoc::perHeaderIgnoreFuncMacros{$part};
	if ($altpart && length($altpart)) {
		print "Found token \"$part\" among gnored macros\n" if ($cppDebug);
		$hasargs = 2;
		$part = "";
	}
    }

    return ($part, $hasargs, "");
}

sub getAndClearCPPHash
{
    my %newhash = %CPP_HASH;
    my %newarghash = %CPP_ARG_HASH;
    %CPP_HASH = ();
    %CPP_ARG_HASH = ();
    return (\%newhash, \%newarghash);
}

sub cpp_argparse
{
    my $name = shift;
    my $linenum = shift;
    my $arglistref = shift;
    # my $cpphashref = shift;
    # my $cpparghashref = shift;

    my @arglist = ();
    if ($arglistref) { @arglist = @{$arglistref}; }

    my %arghash = ();

    if ($cppDebug) {
	print "CPP_ARGPARSE: NM $name ARGS:\n";
	foreach my $arg (@arglist) {
		print "$arg\n";
		$arg->printTree();
	}
	print "ENDARGS\n";
    }

    print "SEARCHING FOR NAME \"$name\"\n" if ($cppDebug);
    my ($newtoken, $has_args, $pattern) = cpp_preprocess($name, $linenum); # , $cpphashref, $cpparghashref);

    print "PATTERN WAS \"$pattern\"\n" if ($cppDebug);

    my @parts = split(/,/, $pattern);
    my $count = 0;

    while ($count < scalar(@parts)) {
	my $part = $parts[$count];
	print "ORIGPART WAS $part\n" if ($cppDebug);
	$part =~ s/\s//sg;
	if (!$arglist[$count]) {
		warn "Not enough arguments to macro $name\n";
	} else {
		print "CALLING ON ".$arglist[$count]."\n" if ($cppDebug);
		$arghash{$part} = cpp_subparse($arglist[$count]); #, $cpphashref, $cpparghashref);
	}
	print "POINTS TO $arghash{$part}\n" if ($cppDebug);
	$count++;
    }

    my $retstring = "";
    my @ntparts = split(/(\W)/, $newtoken);
    foreach my $part (@ntparts) {
	if (length($part)) {
		print "PART WAS $part\n" if ($cppDebug);
		if (defined($arghash{$part})) {
			$retstring .= $arghash{$part};
		} else {
			$retstring .= $part;
		}
	}
    }
    return $retstring;
}

sub cpp_subparse($$)
{
    my $tree = shift;
    # my $hashlistref = shift;
    # my $arghashlistref = shift;

    my ($newtoken, $has_args) = cpp_preprocess($tree->token(), $tree->linenum()); #, $hashlistref, $arghashlistref);
    if ($has_args) {
	my $name = $tree->token();
	my $paren = $tree->next();
	if (!$paren) { return; }
	if ($paren->token() =~ /\(/) {
		# Recurse.
		my @parts = ();
		my $fc = $paren->firstchild;
		my $subparsetop = HeaderDoc::ParseTree->new();
		my $subparsecur = $subparsetop;
		while ($fc && $fc->next) { # drop closing ')'
			if ($fc == ',') {
				push(@parts, $subparsetop);
				$subparsetop = $subparsecur = HeaderDoc::ParseTree->new();
			} else {
				$subparsecur = $subparsecur->next(HeaderDoc::ParseTree->new());
				$subparsecur->token($fc);
			}
		}
		push(@parts, $subparsetop);

		cpp_argparse($name, $tree->linenum(), \@parts); #, $hashlistref, $arghashlistref);
	}
    } else {
	$tree->token($newtoken);
    }
    my $fc = $tree->firstchild();
    if ($fc) {
	cpp_subparse($fc);
    }
    my $n = $tree->next();
    if ($n) {
	cpp_subparse($n);
    }
    return $tree->textTree();
}

sub cppsupers
{
    my $string = shift;
    my $lang = shift;
    my $sublang = shift;

    my $localDebug = 0;

    my @parts = split(/(\W)/, $string);
    my $superlist = "";
    my $cursuper = "";
    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
	$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$typedefname, $varname, $constname, $structisbrace, $macronameref,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp)
		= parseTokens($lang, $sublang);

    my $inTemplate = 0;

    foreach my $part (@parts) {
	if ($part eq "<") {
		$inTemplate = 1;
		$cursuper .= $part;
	} elsif ($part eq ">") {
		$inTemplate = 0;
		$cursuper .= $part;
	} elsif (!$inTemplate && $part eq ",") {
		$superlist .= "\cA".$cursuper;
		$cursuper = "";
	} elsif ($part =~ /\cA/) {
		# drop
	} elsif (!length($accessregexp) || $part !~ /$accessregexp/) {
		$cursuper .= $part;
	}
    }

    $superlist .= "\cA".$cursuper;
    $superlist =~ s/^\cA//s;

	print "SUPERLIST IS $superlist\n" if ($localDebug);

    return $superlist;
}

# /*! This should only be used when handling return types.  It does not handle
#     strings or anything requiring actual parsing.  It strictly rips out
#     C comments (both single-line and standard).
#  */
sub decomment
{
    my $string = shift;
    my $newstring = "";

    my @lines = split(/\n/, $string);
    foreach my $line (@lines) {
	$line =~ s/\/\/.*$//g;
	if (length($line)) {
		$newstring .= $line;
	}
    }

    $newstring =~ s/\/\*.*?\*\///sg;

    return $newstring;

}

1;

