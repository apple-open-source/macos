#! /usr/bin/perl -w
#
# Module name: BlockParse
# Synopsis: Block parser code
#
# Last Updated: $Date: 2009/04/10 07:00:27 $
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
use HeaderDoc::TypeHelper;
use Exporter;
foreach (qw(Mac::Files Mac::MoreFiles)) {
    eval "use $_";
}

$HeaderDoc::disable_parms = 0;

@ISA = qw(Exporter);
@EXPORT = qw(blockParse nspaces blockParseOutside getAndClearCPPHash cpp_remove buildCommentFromFields bracematching peekmatch);

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash quote parseTokens isKeyword warnHDComment classTypeFromFieldAndBPinfo casecmp get_super addAvailabilityMacro);

my $cpp_debug_file = "";
$cpp_debug_file = "/tmp/cpp_debug";
my $cpp_debug_lastfile = "";

use strict;
use vars qw($VERSION @ISA);
use File::Basename qw(basename);
$HeaderDoc::BlockParse::VERSION = '$Revision: 1.59 $';

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
my $cppDebugFromToken = "";
my $nestedcommentwarn = 0;
my $warnAllMultipleDefinitions = 1;

my $modules_are_special = 1;

# Change this to 0 if you want to hide the parameter name
# for unlabeled parameters (old behavior)
$HeaderDoc::useParmNameForUnlabeledParms = 1;

$HeaderDoc::inputCounterDebug = 0;

# Change this to 0 if you don't want to hide IDL attributes (e.g. [foo])
$HeaderDoc::hideIDLAttributes = 1;

sub cpp_subparse($);

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
    } elsif ($classtype =~ /interface/) {
	$sublang = "IDL";
    } elsif ($classtype =~ /module/) {
	$sublang = "IDL";
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
	my $fullpath = shift;
	my $linenum = shift;
	my @stack = @{$ref};
	my $tos = pop(@stack);
	push(@stack, $tos);

	if (!$tos) {
		# popped off top of stack.  Outside a macro, this is an error.
		return "";
	}

	return bracematching($tos, $fullpath, $linenum);
}

# /*!
#     This is used by peekmatch (and by other bits of code) to find the
#     ending token that matches a starting token for braces, etc.
#  */
sub bracematching
{
	my $tos = shift;
	my $calledByParser = 0;
	my $fullpath = "";
	my $linenum = "";
	if (@_) {
		$calledByParser = 1;
		$fullpath = shift;
		$linenum = shift;
	}

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
	    ($tos eq "\@implementation") && do {
			return "\@end";
		};
	    ($tos eq "\@protocol") && do {
			return "\@end";
		};
	    {
		# default case
		if ($calledByParser) {
			# The parser would prefer to always get something back here.
			warn "$fullpath:$linenum: warning: Unknown block delimiter \"$tos\".  Please file a bug.\n";
			return $tos;
		}
		# Other code would prefer an error (an empty return value).
		return "";
	    };
	}
}

# /*! The blockParse function is the core of HeaderDoc's parse engine.
#     @param fullpath the path to the file being parsed.
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
    my $fullpath = shift;
    my $fileoffset = shift;
    my $inputLinesRef = shift;
    my $inputCounter = shift;
    my $argparse = shift;
    my $ignoreref = shift;
    my $perheaderignoreref = shift;
    my $perheaderignorefuncmacrosref = shift;
    my $keywordhashref = shift;
    my $case_sensitive = shift;

    my $apwarn = 0;
    if ($argparse && $apwarn) {
	print STDERR "argparse\n";
    }

    # Initialize stuff
    my @inputLines = @{$inputLinesRef};
    my $declaration = "";
    my $publicDeclaration = "";

# $HeaderDoc::fileDebug = 1;

    # Debugging switches
    my $retDebug                = 0;
    my $localDebug              = 0 || $HeaderDoc::fileDebug;
    my $operatorDebug           = 0;
    my $listDebug               = 0;
    my $parseDebug              = 0 || $HeaderDoc::fileDebug;
    my $sodDebug                = 0 || $HeaderDoc::fileDebug;
    my $valueDebug              = 0;
    my $parmDebug               = 0;
    my $cbnDebug                = 0;
    my $macroDebug              = 0;
    my $apDebug                 = 0;
    my $tsDebug                 = 0;
    my $treeDebug               = 0;
    my $ilcDebug                = 0;
    my $regexpDebug             = 0;
    my $parserStackDebug        = 0 || $HeaderDoc::fileDebug;
    my $braceDebug              = 0 || $HeaderDoc::fileDebug;
    my $hangDebug               = 0;
    my $offsetDebug             = 0;
    my $classDebug              = 0; # prints changes to inClass, etc.
    my $gccAttributeDebug       = 0; # also for availability macro argument handling.
    my $occMethodNameDebug      = 0;
    my $moduleDebug             = 0; # prints changes to INMODULE
    my $liteDebug               = 0 || $HeaderDoc::fileDebug; # Just prints the tokens.
    my $functionContentsDebug   = 0;
    my $continueDebug           = 0;
    my $parserStateInsertDebug  = 0;

    $cppDebug = $cppDebugDefault || $HeaderDoc::fileDebug;

    # State variables (part 1 of 3)
    # my $typestring = "";
    my $continue = 1; # set low when we're done.
    my $continue_no_return = 0; # set high if should restart block parser from the next line.
    # my $parserState->{parsedParamParse} = 0; # set high when current token is part of param.  * now in state *
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
    # my $treePopTwo = 0;       # set to 1 for tokens that nest, but have no
                              # explicit ending token ([+-:]).
    my $treePopOnNewLine = 0; # set to 1 for single-line comments, macros.
    my @treeStack = ();       # stack of parse trees.  Used for popping
                              # our way up the tree to simplify tree structure.

    # Leak a node here so that every real node has a parent.
    $treeCur = $treeCur->addChild("");
    $treeTop = $treeCur;

    # print STDERR "TREE TOP GOING IN IS $treeTop\n" if ($localDebug);

    my $lastACS = "";

    # The argparse switch is a trigger....
    if ($argparse && $apDebug) { 
	$localDebug   = 1;
	$retDebug     = 1;
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

    my $spaceDebug = 0;

    if ($localDebug || $apDebug || $liteDebug || $parseDebug) {
	print STDERR "ENTERED BLOCKPARSE\n";
    }

    my $disable_cpp = 0;
    if ($argparse && ($localDebug || $apDebug || $liteDebug)) {
	print STDERR "ARGPARSE MODE!\n";
	print STDERR "IPC: $inputCounter\nNLINES: ".$#inputLines."\n";
	cluck("Call backtrace\n");
    }

    print STDERR "INBP\n" if ($localDebug);

    if ($argparse) {
	# Avoid double-processing macro inclusions.
	$disable_cpp = 1;
    }
    if ($lang ne "C" || $sublang eq "PHP") { # || $sublang eq "IDL")
	$disable_cpp = 1;
    }

    print STDERR "INITIAL LANG: $lang INITIAL SUBLANG: $sublang\n" if ($localDebug);

# warn("in BlockParse\n");

    # State variables (part 2 of 3)
    my $parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
    # $parserState->{hollow} = $treeTop;
    setHollowWithLineNumbers(\$parserState, $treeTop, $fileoffset, $inputCounter);
    $parserState->{lang} = $lang;
    $parserState->{inputCounter} = $inputCounter;
    $parserState->{initbsCount} = 0; # included for consistency....
    my @parserStack = ();

    # print STDERR "TEST: ";
    # if (defined($parserState->{parsedParamList})) {
	# print STDERR "defined\n"
    # } else { print STDERR "undefined.\n"; }
    # print STDERR "\n";

    # my $inComment = 0;
    # my $inInlineComment = 0;
    # my $inString = 0;
    # my $inChar = 0;
    # my $inTemplate = 0;
    my @braceStack = ();
    my @parsedParamParseStack = ();
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
    my $inRegexpFirstPart = 0;    # 2 if in leading chars, 1 after first / or
				  # whatever, 0 after second / or outside regexp.
    my $inRegexpCharClass = 0;    # 3 until end of loop.
				  # 2 for first char of regexp character class,
				  # 1 elsewhere in character class, 0 otherwise.
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
    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
	$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
	$typedefname, $varname, $constname, $structisbrace, $macronameref,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
	$requiredregexp, $propname, $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename) = parseTokens($lang, $sublang);
    my $macrore_pound = macroRegexpFromList($macronameref, 1);
    my $macrore_nopound = macroRegexpFromList($macronameref, 2);
	# print STDERR "LANG: $lang SUBLANG: $sublang";
    print STDERR "MACRORE_POUND: \"$macrore_pound\"\n" if ($localDebug || $parseDebug);
    print STDERR "MACRORE_NOPOUND: \"$macrore_nopound\"\n" if ($localDebug || $parseDebug);
# print STDERR "INITIAL PROPNAME $propname\n";

    if ($parseDebug) {
	print STDERR "SOT: $sotemplate EOF: $eotemplate OP: $operator SOC: $soc EOC: $eoc ILC: $ilc ILC_B: $ilc_b\n";
	print STDERR "SOFUNC: $sofunction SOPROC: $soprocedure SOPREPROC: $sopreproc LBRACE: $lbrace RBRACE:  $rbrace\n";
 	print STDERR "UNION: $unionname STRUCT: $structname TYPEDEF: $typedefname VAR: $varname CONST: $constname\n";
 	print STDERR "STRUCTISBRACE: $structisbrace MACRONAMEREF: $macronameref CLASSRE: $classregexp\n";
	print STDERR "CLASSBRACERE: $classbraceregexp CLASSCLOSEBRACERE: $classclosebraceregexp ACCESSRE: $accessregexp\n";
	print STDERR "MODULERE: $moduleregexp\n";
    }
    

    # Set up regexp patterns for perl, variable for perl or shell.
    if ($lang eq "perl" || $lang eq "shell") {
	$perl_or_shell = 1;
	if ($lang eq "perl") {
		$regexpcharpattern = '\\{|\\#\\(|\\/|\\\'|\\"|\\<|\\[|\\`';
		# } vi bug workaround for previous line
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

    if (!$disable_cpp && (1 || $HeaderDoc::enable_cpp)) {
	if ($cpp_debug_file) {
	    if (basename($fullpath) ne $cpp_debug_lastfile) {
		open(CPP_DEBUG_FILE, ">>$cpp_debug_file");

		print CPP_DEBUG_FILE "DUMPING CPP INFORMATION FOR $fullpath\n";
		close(CPP_DEBUG_FILE);
		my $filename = basename($fullpath);
		$cpp_debug_lastfile = $filename;
	    }
	}
    }

    $HeaderDoc::hidetokens = 0;

    # Loop unti the end of file or until we've found a declaration,
    # processing one line at a time.
    my $nlines = $#inputLines;
    my $incrementoffsetatnewline = 0;
    print STDERR "INCOMING INPUTCOUNTER: $inputCounter\n" if ($HeaderDoc::inputCounterDebug);
    while ($continue && ($inputCounter <= $nlines)) {
	$HeaderDoc::CurLine = $inputCounter + $fileoffset;
	my $line = $inputLines[$inputCounter++];
	print STDERR "GOT LINE: $line\n" if (($localDebug && $apDebug) || $HeaderDoc::inputCounterDebug);
	print STDERR "INCREMENTED INPUTCOUNTER [1]\n" if ($HeaderDoc::inputCounterDebug);
	my @parts = ();

	# $line =~ s/^\s*//go; # Don't strip leading spaces, please.
	$line =~ s/\s*$//go;
	# $scratch = nspaces($prespace);
	# $line = "$scratch$line\n";
	# $curline .= $scratch;
	$line .= "\n";

	if ($lang eq "C" && $sublang eq "IDL") {
		if ($line =~ /cpp_quote\s*\(\s*\"(.*)\"\s*\)\s*$/) {
			print STDERR "CHANGED LINE FROM \"$line\" to " if ($localDebug || $liteDebug);
			$line = $1."\n";
			$line =~ s/\\\"/"/sg;
			print STDERR "\"$line\"\n" if ($localDebug || $liteDebug);
		}
	}

	print STDERR "LINE[$inputCounter] : $line\n" if ($offsetDebug);

	# The tokenizer
	if ($lang eq "perl" || $lang eq "shell") {
	    @parts = split(/("|'|\#|\{|\}|\(|\)|\s|;|\\|\W)/, $line);
	} else {
	    @parts = split(/("|'|\/\/|\/\*|\*\/|::|==|<=|>=|!=|\<\<|\>\>|\{|\}|\(|\)|\s|;|\\|\^|\W)/, $line);
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
		print STDERR "FOUND EXTRA NEWLINE\n" if ($offsetDebug);
		# $fileoffset++;
		$incrementoffsetatnewline++;
	    }
	    $xpart = $nextxpart;
	}
	pop(@parts);

	$parserState->{inInlineComment} = 0;
	print STDERR "inInlineComment -> 0\n" if ($ilcDebug);

        # warn("line $inputCounter\n");

	if ($localDebug || $cppDebug || $spaceDebug || $cppDebugFromToken) {
		foreach my $partlist (@parts) {
			print STDERR "PARTLIST: \"$partlist\"\n";
			if ($partlist eq $cppDebugFromToken) {
				$cppDebug = 1; $parseDebug = 1; $macroDebug = 1;
			}
		}
	}

	# We have to do the C preprocessing work up front because token substitution
	# must occur prior to actual parsing in order to do any good.  This block does
	# the work.
	my $cpp_in_argparse = 0;
	if (!$disable_cpp && (1 || $HeaderDoc::enable_cpp)) {
		my $newrawline = "";
		my $incppargs = 0;
		my $cppstring = "";
		my $cppname = "";
		my $lastcpppart = "";
		my @cppargs = ();
		my $inChar = 0; my $inString = 0; my $inComment = $parserState->{inComment}; my $inSLC = $parserState->{inInlineComment};
		my $inParen = 0;
		my $inMacro = $parserState->{inMacro};
		my $inCPPSpecial = $parserState->{inMacro} || $parserState->{inMacroLine};
		my $inMacroTail = 0;
		if ($parserState->{sodname} && ($parserState->{sodname} ne "")) {
			$inMacroTail = 1;
		}
		print STDERR "INMACROTAIL: $inMacroTail\n" if ($cppDebug);

		my @cpptrees;
		my $cpptreecur = HeaderDoc::ParseTree->new();
		my $cpptreetop = $cpptreecur;

		# print STDERR "CHECK LINE $line\n";
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
# print STDERR "PUSH HASH\n";
				push(@HeaderDoc::cppArgHashList, $HeaderDoc::HeaderFileCPPArgHashHash{$filename});
			}
		} elsif ($line =~ /^\s*$definename\s+/) {
			# print STDERR "inMacro -> 1\n";
			# print STDERR "inMacro -> 1\n" if ($macroDebug || $cppDebug);
			# This is a throwaway line.
			$inMacro = 1;
		}
		if ($macrore_pound ne "" && $line =~ /^\s*\#\s*$macrore_pound\s+/) {
			print STDERR "CPPSPECIAL -> 1\n" if ($macroDebug || $cppDebug);
			$inCPPSpecial = 1;
		}
		my $cppleaddebug = 0;
		do {
		    my $pos = 0;
		    my $dropargs = 0;
		    while ($pos < scalar(@parts)) {
			my $part = $parts[$pos];
			my $noCPPThisToken = 0;
			if (length($part)) {
			    if (!$inChar && !$inString && !$inComment && !$inSLC) {
				if ($parserState->{NEXTTOKENNOCPP} == 1) {
					# We're in an "if" block.
					if ($part eq "defined") {
						$parserState->{NEXTTOKENNOCPP} = 3;
					}
				} elsif ($parserState->{NEXTTOKENNOCPP} == 2) {
					# We're in an "ifdef"/"ifndef" block, so first word token
					# ends this mode completely.
					if ($part !~ /(\s|\()/) {
						$parserState->{NEXTTOKENNOCPP} = 0;
						$noCPPThisToken = 1;
					}
				} elsif ($parserState->{NEXTTOKENNOCPP} == 3) {
					# We're in an "if" block, so first word token
					# drops us back to default "if" block state.
					if ($part !~ /(\s|\()/) {
						$parserState->{NEXTTOKENNOCPP} = 1;
						$noCPPThisToken = 1;
					}
				}
				if ($inCPPSpecial && $part =~ /(ifdef|ifndef)/) {
					$parserState->{NEXTTOKENNOCPP} = 2;
				} elsif ($inCPPSpecial && $part =~ /if/) {
					$parserState->{NEXTTOKENNOCPP} = 1;
				}
			    }
			    print STDERR "TOKEN: $part NEXTTOKENNOCPP: ".$parserState->{NEXTTOKENNOCPP}." INMACRO: $inMacro INCPPSPECIAL: $inCPPSpecial\n" if ($cppleaddebug || $macroDebug || $cppDebug);

			    print STDERR "CPPLEADPART: $part\n"if ($cppleaddebug);
			    if (!$inString && !$inChar) {
				if ($inComment && $part eq $eoc) {
					print STDERR "EOC\n"if ($cppleaddebug);
					$inComment = 0;
				} elsif ($inSLC && $part =~ /[\r\n]/) {
					# Handle newline in single-line comments.
					print STDERR "EOSLC\n"if ($cppleaddebug);
					$inSLC = 0;
				} elsif (!$inSLC && $part eq $soc) {
					print STDERR "SOC\n"if ($cppleaddebug);
					$inComment = 1;
				} elsif (!$inComment && ($part eq $ilc || $part eq $ilc_b)) {
					print STDERR "INSLC\n"if ($cppleaddebug);
					$inSLC = 1;
				}
			    }
			    my $skip = 0;
			    if (!$incppargs) {
				my $newpart = $part;
				my $hasargs = 0;
				if (!$inComment && !$inSLC && !$noCPPThisToken) {
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
						print STDERR "Dropping arguments for ignored macro \"$part\"\n" if ($cppDebug);
					}
				} else {
					my $newpartnl = $newpart;
					my $newpartnlcount = ($newpartnl =~ tr/\n//);
					my $partnl = $part;
					my $partnlcount = ($partnl =~ tr/\n//);
					my $nlchange = ($newpartnlcount - $partnlcount);
					print STDERR "NLCHANGE: $nlchange (FILEOFFSET = $fileoffset)\n" if ($offsetDebug);
					$fileoffset -= $nlchange;
					if ($inMacro) {
						if ($newpart ne $part) {
							print STDERR "CHANGING NEWPART FROM \"$newpart\" TO " if ($cppDebug);
							$newpart =~ s/^\s*/ /s;
							$newpart =~ s/\s*$//s;
							$newpart =~ s/(.)\n/$1 \\\n/sg;
							$newpart =~ s/\\$/ /s;
							print STDERR "$newpart\n" if ($cppDebug);
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
					if (!$inMacro && ($lastcpppart eq '\\')) { $lastcpppart = ""; } # @@@ CHECKME.  inMacro test may not be needed.
					# else {
						# $lastcpppart = $part; 
						# if ($inMacro) {
# print STDERR "IMTEST\n" if ($cppDebug > 1);
							# my $npos = $pos + 1;
							# while ($npos < scalar(@parts)) {
							    # my $npart = $parts[$npos];
							    # if (length($npart)) {
# print STDERR "NEXTPART: \"".$parts[$npos]."\"\n" if ($cppDebug > 1);
								# if ($npart =~ /\s/) {
									# if ($npart =~ /[\n\r]/) {
# print STDERR "SKIP1\n" if ($cppDebug > 1);
										# $skip = 1; last;
									# } else {
# print STDERR "SPC\n" if ($cppDebug > 1);
									# }
								# } else {
# print STDERR "LAST\n" if ($cppDebug > 1);
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
						# Put in the token first, then nest.
						$cpptreecur = $cpptreecur->next(HeaderDoc::ParseTree->new());
						$cpptreecur->token($part);
						$skip = 1;

						$inParen++;
						push(@cpptrees, $cpptreecur);
						$cpptreecur = $cpptreecur->firstchild(HeaderDoc::ParseTree->new());
					} elsif ($part eq ")") {
						$inParen--;

						# Go out one nesting level, then
						# insert the token.
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
								print STDERR "CALLING ARGPARSE FROM blockParse() [1].\n" if ($cppDebug);
								my $addon = cpp_argparse($cppname, $HeaderDoc::CurLine, \@cppargs);
								if ($inMacro) {
									print STDERR "CHANGING ADDON FROM:\n\"$addon\"\nTO:\n" if ($cppDebug);
									$addon =~ s/^\s*/ /s;
									$addon =~ s/\s*$//s;
									$addon =~ s/(.)\n/$1 \\\n/sg;
									$addon =~ s/\\$/ /s;
									print STDERR "\"$addon\"\n" if ($cppDebug);
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
							print STDERR "CALLING ARGPARSE FROM blockParse() [2].\n" if ($cppDebug);
							my $addon = cpp_argparse($cppname, $HeaderDoc::CurLine, \@cppargs);
							if ($inMacro) {
									print STDERR "CHANGING ADDON FROM \"$addon\" TO " if ($cppDebug);
									$addon =~ s/^\s*/ /s;
									$addon =~ s/\s*$//s;
									$addon =~ s/(.)\n/$1 \\\n/sg;
									$addon =~ s/\\$/ /s;
									print STDERR "$addon\n" if ($cppDebug);
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

					# Strip newline in CPP argument list.
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
			# print STDERR "YO\n";
			if ($parserState->{inMacro} || $inMacro) {
			# print STDERR "YOYO\n";
				if ($cppstring !~ s/\\\s*$//s) {
print STDERR "CPPS: \"$cppstring\"\n";
					warn "Non-terminated macro.\n";
					$incppargs = 0;
				}
			}
		    }
		    if ($incppargs || $inComment) {
			print STDERR "Fetching new line ($incppargs, $inComment)\n" if ($cppleaddebug);
			$HeaderDoc::CurLine = $inputCounter + $fileoffset;
			$line = $inputLines[$inputCounter++];

			if ($lang eq "C" && $sublang eq "IDL") {
				if ($line =~ /cpp_quote\s*\(\s*\"(.*)\"\s*\)\s*$/) {
					print STDERR "CHANGED LINE FROM \"$line\" to " if ($localDebug || $liteDebug);
					$line = $1."\n";
					$line =~ s/\\\"/"/sg;
					print STDERR "\"$line\"\n" if ($localDebug || $liteDebug);
				}
			}

			print STDERR "INCREMENTED INPUTCOUNTER [2]\n" if ($HeaderDoc::inputCounterDebug);
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
	if (!$parserState->{inMacro}) {
		$parserState->{NEXTTOKENNOCPP} = 0;
	}

	# Throw away any empty entries caused by Perl seeing two
	# adjacent tokens that match the split regexp.  We don't
	# want them or care about them, and they break things
	# rather badly if we don't....
	my @stripparts = @parts;
	@parts = ();
	print STDERR "BEGIN PARTLIST 2:\n" if ($spaceDebug);
	foreach my $strippart (@stripparts) {
		if (length($strippart)) {
			print STDERR "MYPART: \"$strippart\"\n" if ($spaceDebug);
			push(@parts, $strippart);
		}
	}
	print STDERR "END PARTLIST 2.\n" if ($spaceDebug);

	if (!$disable_cpp && (1 || $HeaderDoc::enable_cpp)) {
	    if ($cpp_debug_file) {
		open(CPP_DEBUG_FILE, ">>$cpp_debug_file");

		# print CPP_DEBUG_FILE "DUMPING CPP INFORMATION FOR $fullpath\n";
		foreach my $part (@parts) {
			print CPP_DEBUG_FILE $part;
		}
		close(CPP_DEBUG_FILE);
	    }
	}

	# This bit of code needs a bit of explanation, I think.
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
	push(@parts, "BOGUSBOGUSBOGUSBOGUSBOGUS");

if ($localDebug || $cppDebug) {foreach my $partlist (@parts) {print STDERR "POSTCPPPARTLIST: \"$partlist\"\n"; }}

	foreach my $nextpart (@parts) {
	    my $hideTokenAndMaybeContents = 0;
	    my $bshandled = 0;
	    $treeSkip = 0;
	    # $treePopTwo = 0;
	    # $treePopOnNewLine = 0;

	    # The current token is now in "part", and the literal next
	    # token in "nextpart".  We can't just work with this as-is,
	    # though, because you can have multiple spaces, null
	    # tokens when two of the tokens in the split list occur
	    # consecutively, etc.

	    print STDERR "MYPART: \"$part\"\n" if ($localDebug || $spaceDebug);

	    $forcenobreak = 0;

	    # Convert CR/LF or LF/CR pair to LF
	    if ($part eq "\r" && $nextpart eq "\n") { $part = $nextpart ; next; }
	    if ($part eq "\n" && $nextpart eq "\r") { next; }

	    # Convert bare CR to LF
	    if ($nextpart eq "\r") { $nextpart = "\n"; }

	    if ($localDebug && $nextpart eq "\n") { print STDERR "NEXTPART IS NEWLINE!\n"; }
	    if ($localDebug && $part eq "\n") { print STDERR "PART IS NEWLINE!\n"; }

	    ### if ($nextpart ne "\n" && $nextpart =~ /\s/o) {
		### # Replace tabs with spaces.
		### $nextpart = " ";
	    ### }

	    # Replace tabs with spaces.
	    $part =~ s/\t/        /g;
	    $nextpart =~ s/\t/        /g;

	    if ($part ne "\n" && $part =~ /\s/o && $nextpart ne "\n" &&
		$nextpart =~ /\s/o) {
			# we're a space followed by a space.  Join the tokens.
			print STDERR "MERGED \"$part\" and \"$nextpart\" into " if ($spaceDebug);

			$nextpart = $part.$nextpart;

			print STDERR "\"$nextpart\".\n" if ($spaceDebug);

			$part = $nextpart;
			next;
	    }
	    print STDERR "PART IS \"$part\"\n" if ($localDebug || $parserStackDebug || $parseDebug || $liteDebug || $spaceDebug);
	    print STDERR "QUOTED: ".$parserState->isQuoted()."\n" if ($localDebug || $parserStackDebug || $parseDebug || $liteDebug || $spaceDebug);
	    print STDERR "CURLINE IS \"$curline\"\n" if ($localDebug || $hangDebug || $liteDebug);
	    print STDERR "RETURNTYPE IS \"$parserState->{returntype}\"\n" if ($localDebug || $hangDebug || $liteDebug);
	    print STDERR "INOP: ".$parserState->{inOperator}."\n" if ($operatorDebug);

	    if (!length($nextpart)) {
		print STDERR "SKIP NP\n" if ($localDebug);
		next;
	    }
	    if (!length($part)) {
		print STDERR "SKIP PART\n" if ($localDebug);
		$part = $nextpart;
		next;
	    }

	    if ($occPushParserStateOnWordTokenAfterNext > 1) {
		if ($part =~ /\w/) {
			$occPushParserStateOnWordTokenAfterNext--;
			print STDERR "occPushParserStateOnWordTokenAfterNext -> $occPushParserStateOnWordTokenAfterNext (--)\n" if ($localDebug || $parseDebug);
		}
	    } elsif ($occPushParserStateOnWordTokenAfterNext) {
		# if ($part !~ /(\s|<)/)
		if ($part =~ /(\/\/|\/\*|\-|\+|\w|\@)/) {
			$parserState->{lastTreeNode} = $treeCur;
			print STDERR "parserState pushed onto stack[occPushParserStateOnWordTokenAfterNext]\n" if ($parserStackDebug);
			$curline = "";
			$parserState->{storeDec} = $declaration;
			$parserState->{freezereturn} = 1;
			$declaration = "";
			push(@parserStack, $parserState);
			$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
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

	    print STDERR "IN LOOP LANG: $lang INITIAL SUBLANG: $sublang\n" if ($localDebug || $parseDebug);
	    if ($parseDebug) {
		print STDERR "PART: $part, type: $parserState->{typestring}, inComment: $parserState->{inComment}, inInlineComment: $parserState->{inInlineComment}, inChar: $parserState->{inChar}.\n" if ($localDebug);
		print STDERR "PART: inBrackets: $parserState->{inBrackets}\n" if ($localDebug);
		print STDERR "PART: onlyComments: $parserState->{onlyComments}, inClass: $parserState->{inClass}\n";
		print STDERR "PART: cbsodname: $parserState->{cbsodname}\n";
		print STDERR "PART: classIsObjC: $parserState->{classIsObjC}, PPSAT: $pushParserStateAfterToken, PPSAWordT: $pushParserStateAfterWordToken, PPSABrace: $pushParserStateAtBrace, occPPSOnWordTokenAfterNext: $occPushParserStateOnWordTokenAfterNext\n";
		print STDERR "PART: bracecount: " . scalar(@braceStack) . " (init was $parserState->{initbsCount}).\n";
		print STDERR "PART: inString: $parserState->{inString}, callbackNamePending: $parserState->{callbackNamePending}, namePending: $parserState->{namePending}, lastsymbol: $parserState->{lastsymbol}, lasttoken: $lasttoken, lastchar: $lastchar, SOL: $parserState->{startOfDec}\n" if ($localDebug);
		print STDERR "PART: sodclass: $parserState->{sodclass} sodname: $parserState->{sodname}\n";
		print STDERR "PART: sodtype: $parserState->{sodtype}\n";
		print STDERR "PART: simpleTypedef: $parserState->{simpleTypedef}\n";
		print STDERR "PART: posstypes: $parserState->{posstypes}\n";
		print STDERR "PART: seenBraces: $parserState->{seenBraces} inRegexp: $inRegexp inRegexpCharClass: $inRegexpCharClass\n";
		print STDERR "PART: regexpNoInterpolate: $regexpNoInterpolate\n";
		print STDERR "PART: seenTilde: $parserState->{seenTilde}\n";
		print STDERR "PART: CBN: $parserState->{callbackName}\n";
		print STDERR "PART: regexpStack is:";
		foreach my $token (@regexpStack) { print STDERR " $token"; }
		print STDERR "\n";
		print STDERR "PART: npplStack: ".scalar(@{$parserState->{pplStack}})." nparsedParamList: ".scalar(@{$parserState->{parsedParamList}})." nfreezeStack: ".scalar(@{$parserState->{freezeStack}})." frozen: $parserState->{stackFrozen}\n";
		print STDERR "PART: inMacro: $parserState->{inMacro} treePopOnNewLine: $treePopOnNewLine\n";
		print STDERR "PART: occmethod: $parserState->{occmethod} occmethodname: $parserState->{occmethodname}\n";
		print STDERR "PART: returntype is $parserState->{returntype}\n";
		print STDERR "length(declaration) = " . length($declaration) ."; length(curline) = " . length($curline) . "\n";
		print STDERR "REQUIREDREGEXP IS \"$requiredregexp\"\n";
		print STDERR "DEC: $declaration\n$curline\n";
	    } elsif ($tsDebug || $treeDebug) {
		print STDERR "BPPART: $part\n";
	    }
	    if ($parserStackDebug) {
		print STDERR "parserState: STACK CONTAINS ".scalar(@parserStack)." STATES\n";
		print STDERR "parserState is $parserState\n";
	    }

	    # The ignore function returns either null, an empty string,
	    # or a string that gives the text equivalent of an availability
            # macro.  If the token is non-null and the length is non-zero,
	    # it's an availability macro, so blow it in as if the comment
	    # contained an @availability tag.
	    # 
	    my $tempavail = ignore($part, $ignoreref, $perheaderignoreref);
	    printf("PART: $part TEMPAVAIL: $tempavail\n") if ($localDebug || $gccAttributeDebug);
	    if ($tempavail && ($tempavail ne "1") && ($tempavail ne "2") && ($tempavail ne "3")) {
		$parserState->{availability} = $tempavail;
	    } elsif ($tempavail eq "2" || $tempavail eq "3") {
		# Reusing the GCC attribute handling code because that does exactly what we need.
		print STDERR "Function-like availability macro detected.  Collecting.\n" if ($localDebug || $gccAttributeDebug);
		$parserState->{attributeState} = 1;
		$parserState->{attributeParts} = ();

		# Add __attribute__ as the next token.
		my $hidden = 0;
		if ($tempavail eq "3") {
			$hidden = 3;
		}

		$treeCur = $treeCur->addSibling($part, $hidden);
		# print STDERR "PUSHED $treeCur onto tree stack (__OSX_AVAIL...).\n";
		push(@treeStack, $treeCur);

		my @tempAvailabilityNodesArray = ();
		if ($parserState->{availabilityNodesArray}) {
			@tempAvailabilityNodesArray = @{$parserState->{availabilityNodesArray}};
		}

		push(@tempAvailabilityNodesArray, $treeCur);
		# print STDERR "ADDED $treeCur\n";
		# $treeCur->dbprint();

		$parserState->{availabilityNodesArray} = \@tempAvailabilityNodesArray;


		# Nest all contents one level lower.

		$treeCur = $treeCur->addChild("", 0);
		$part = $nextpart;
		next;
	    }

	print "IC: $parserState->{inClass} TOK: $part\n" if ($parseDebug || $classDebug);

	    my $externCDebug = 0;

	    # Handle the GCC "__attribute__" extension outside the context of
	    # the parser because it isn't part of the language and massively
	    # breaks the syntax.
	    # Same for asm
	    my $iskw = isKeyword($part, $keywordhashref, $case_sensitive);
	    if ($lang eq "C" && ($iskw == 2 || $iskw == 5 || $iskw == 6)) {
		print STDERR "GCC attribute/asm detected.  Collecting.\n" if ($localDebug || $gccAttributeDebug);
		$parserState->{attributeState} = 1;
		$parserState->{attributeParts} = ();

		# Add __attribute__ as the next token.
		$treeCur = $treeCur->addSibling($part, 0);
		push(@treeStack, $treeCur);

		# Nest all contents one level lower.
		$treeCur = $treeCur->addChild("", 0);
		$part = $nextpart;
		next;
	    } elsif ($lang eq "C" && ($iskw == 7)) {
		$parserState->rollbackSet();
		print STDERR "EXTERN C -> 1\n" if ($externCDebug);
		$parserState->{externC} = 1;
		$parserState->{preExternCcurline} = $curline;
		$parserState->{preExternCdeclaration} = $declaration;
		# $parserState->{preExternCsodname} = $parserState->{sodname};
		# $parserState->{preExternCsodclass} = $parserState->{sodclass};
		# $parserState->{preExternCsodtype} = $parserState->{sodtype};
		# $parserState->{preExternCreturntype} = $parserState->{returntype};
	    } elsif ($parserState->{attributeState} == 1) {
		if ($part eq "(") {
			print STDERR "GCC attribute open paren\n" if ($localDebug || $gccAttributeDebug);
			$parserState->{attributeState} = -1;
		}
		$treeCur = $treeCur->addSibling($part, 0);
		$part = $nextpart;
		next;
	    } elsif ($parserState->{attributeState} < 0) {
		if ($part eq "(") {
			$parserState->{attributeState}--;
			print STDERR "GCC attribute open paren, count=".(0-$parserState->{attributeState})."\n" if ($localDebug || $gccAttributeDebug);
		} elsif ($part eq ")") {
			$parserState->{attributeState}++;
			print STDERR "GCC attribute close paren, count=".(0-$parserState->{attributeState})."\n" if ($localDebug || $gccAttributeDebug);
		}
		$treeCur = $treeCur->addSibling($part, 0);
		if (!$parserState->{attributeState}) {
			print STDERR "GCC attribute: done collecting.\n" if ($localDebug || $gccAttributeDebug);

			# Get back to where we started.
			$treeCur = pop(@treeStack);
			# print STDERR "GOT TC: $treeCur\n";
		}
		$part = $nextpart;
		next;
	    } elsif ($parserState->{externC} == 1) {
		if ($part =~ /"/) {
			print STDERR "EXTERN C -> 2\n" if ($externCDebug);
			$parserState->{externC} = 2;
		} elsif ($part !~ /\s/) {
			print STDERR "EXTERN C -> 0[1]\n" if ($externCDebug);
			$parserState->{externC} = 0;
		}
	    } elsif ($parserState->{externC} == 2) {
		if ($part =~ /C/) {
			print STDERR "EXTERN C -> 3\n" if ($externCDebug);
			$parserState->{externC} = 3;
		} elsif ($part !~ /\s/) {
			print STDERR "EXTERN C -> 0[2]\n" if ($externCDebug);
			$parserState->{externC} = 0;
		}
	    } elsif ($parserState->{externC} == 3) {
		if ($part =~ /"/) {
			print STDERR "EXTERN C -> 0[Success]\n" if ($externCDebug);
			$parserState->{externC} = 0;
			$parserState->{onlyComments} = 1;
			$parserState->{rollbackPending} = 1;
			# $curline = $parserState->{preExternCcurline};
			# $declaration = $parserState->{preExternCdeclaration};
			# $parserState->{sodname} = $parserState->{preExternCsodname};
			# $parserState->{sodclass} = $parserState->{preExternCsodclass};
			# $parserState->{sodtype} = $parserState->{preExternCsodtype};
			# $parserState->{returntype} = $parserState->{preExternCreturntype};
		} elsif ($part !~ /\s/) {
			print STDERR "EXTERN C -> 0[3]\n" if ($externCDebug);
			$parserState->{externC} = 0;
		}
	    }

	    # Here be the parser.  Abandon all hope, ye who enter here.
	    $treepart = "";

	    if ($parserState->{inProtocol} == 1) {
		print STDERR "INPROTOCOL: 1\n" if ($parseDebug || $classDebug); 
		if ($part =~ /\w/) {
			print STDERR "INPROTOCOL: 1 -> 2\n" if ($parseDebug || $classDebug); 
			$parserState->{inProtocol} = 2;
		}
	    } elsif ($parserState->{inProtocol} == 2) {
		print STDERR "INPROTOCOL: 2\n" if ($parseDebug || $classDebug); 
		if ($part eq "<") {
			print STDERR "INPROTOCOL: 2 -> 3\n" if ($parseDebug || $classDebug); 
			$parserState->{extendsProtocol} = "";
			$parserState->{inProtocol} = 3;
		} elsif ($part =~ /\S/) {
			# PUSH PARSER STATE
			print STDERR "parserState pushed onto stack[PROTOCOL]\n" if ($parserStackDebug);
			$parserState->{inProtocol} = -1;
			$parserState->{lastTreeNode} = $treeCur;
			$curline = "";
			$parserState->{storeDec} = $declaration;
			$parserState->{freezereturn} = 1;
			$declaration = "";
			push(@parserStack, $parserState);
			$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
			$parserState->{skiptoken} = 1;
			$parserState->{lang} = $lang;
			$parserState->{inputCounter} = $inputCounter;
			$parserState->{initbsCount} = scalar(@braceStack);
			$pushParserStateAfterWordToken = 0;
		}
	    } elsif ($parserState->{inProtocol} == 3) {
		print STDERR "INPROTOCOL: 3\n" if ($parseDebug || $classDebug); 
		if ($part eq ">") {
			print STDERR "INPROTOCOL: 3 -> 2\n" if ($parseDebug || $classDebug); 
			$parserState->{inProtocol} = 2;
		} else {
			$parserState->{extendsProtocol} .= $part;
		}
	    }
	    if ($parserState->{inClass} == 3) {
		print STDERR "INCLASS3\n" if ($parseDebug || $classDebug);
		if ($part eq ")") {
			$parserState->{inClass} = 1;
			print STDERR "inClass -> 1 [1]\n" if ($classDebug);
			$parserState->{categoryClass} .= $part;
			print STDERR "parserState will be pushed onto stack[cparen3]\n" if ($parserStackDebug);
			# $parserState->{lastTreeNode} = $treeCur;
			# push(@parserStack, $parserState);
			# $parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
			# $parserState->{lang} = $lang;
			# $parserState->{inputCounter} = $inputCounter;
			# $parserState->{initbsCount} = scalar(@braceStack);
			$pushParserStateAfterToken = 1;
		} elsif ($part eq ":") {
			$parserState->{inClass} = 1;
			print STDERR "inClass -> 1 [2]\n" if ($classDebug);
			if ($parserState->{classIsObjC}) {
				print STDERR "occPushParserStateOnWordTokenAfterNext -> 2\n" if ($localDebug || $parseDebug);
				$occPushParserStateOnWordTokenAfterNext = 2;
			} else {
				$pushParserStateAfterWordToken = 1;
			}
			# if ($sublang eq "occ") {
				# $pushParserStateAtBrace = 2;
			# }
		} elsif ($part =~ /</ && $parserState->{classIsObjC}) {
			print STDERR "pushParserStateAfterWordToken -> 0 (Conforming)\n" if ($localDebug || $parseDebug);
			print STDERR "inClassConformingToProtocol -> 1\n" if ($localDebug || $parseDebug);
			$pushParserStateAfterWordToken = 0;
			$parserState->{inClassConformingToProtocol} = 1;
			$occPushParserStateOnWordTokenAfterNext = 0;
		} elsif ($part =~ />/ && $parserState->{classIsObjC} && $parserState->{inClassConformingToProtocol}) {
			print STDERR "inClassConformingToProtocol -> 0\n" if ($localDebug || $parseDebug);
			$pushParserStateAfterToken = 1;
			print STDERR "pushParserStateAfterWordToken -> 1 (Conforming)\n" if ($localDebug || $parseDebug);
			$parserState->{inClassConformingToProtocol} = 0;
		} else {
			$parserState->{categoryClass} .= $part;
		}
	    } elsif ($parserState->{inClass} == 2) {
		print STDERR "INCLASS2\n" if ($parseDebug || $classDebug);
		if ($part eq ")") {
			$parserState->{inClass} = 1;
			print STDERR "inClass -> 1 [3]\n" if ($classDebug);
			$parserState->{lastTreeNode} = $treeCur;
			print STDERR "parserState pushed onto stack[cparen2]\n" if ($parserStackDebug);
			$curline = "";
			$parserState->{storeDec} = $declaration;
			$parserState->{freezereturn} = 1;
			$declaration = "";
			push(@parserStack, $parserState);
			$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
			$parserState->{skiptoken} = 1;
			$parserState->{lang} = $lang;
			$parserState->{inputCounter} = $inputCounter;
			$parserState->{initbsCount} = scalar(@braceStack);
		} elsif ($part eq ":") {
			$parserState->{inClass} = 1;
			print STDERR "inClass -> 1 [4]\n" if ($classDebug);
			if ($parserState->{classIsObjC}) {
				print STDERR "occPushParserStateOnWordTokenAfterNext -> 2\n" if ($localDebug || $parseDebug);
				$occPushParserStateOnWordTokenAfterNext = 2;
			} else {
				$pushParserStateAfterWordToken = 2;
			}
		} elsif ($part =~ /\w/) {
			# skip the class name itself.
			$parserState->{inClass} = 3;
			print STDERR "inClass -> 3 [5]\n" if ($classDebug);
		}
	    } elsif ($parserState->{inClass} == 1) {
		print STDERR "INCLASS1\n" if ($parseDebug || $classDebug);
		# print STDERR "inclass Part is $part\n";
		print STDERR "IK: $part => ".isKeyword($part, $keywordhashref, $case_sensitive)."\n" if ($parseDebug || $classDebug);;
		if ($part eq ":") {
			print STDERR "INCLASS COLON\n" if ($parseDebug || $classDebug);
			if (!$parserState->{forceClassName}) {
				$parserState->{forceClassName} = $parserState->{sodname};
				$parserState->{forceClassSuper} = "";
			}
			# print STDERR "XSUPER: $parserState->{forceClassSuper}\n";
 		} elsif (isKeyword($part, $keywordhashref, $case_sensitive) == 3) {
			if (!$parserState->{forceClassName}) {
				$parserState->{forceClassName} = $parserState->{sodname};
			}
			$parserState->{inExtends} = 1;
			$parserState->{inImplements} = 0;
			if ($parserState->{extendsClass}) {
				$parserState->{extendsClass} .= " ";
			} else {
				$parserState->{extendsClass} = "";
			}
		} elsif (isKeyword($part, $keywordhashref, $case_sensitive) == 4) {
			if (!$parserState->{forceClassName}) {
				$parserState->{forceClassName} = $parserState->{sodname};
			}
			$parserState->{inImplements} = 1;
			$parserState->{inExtends} = 0;
			if ($parserState->{implementsClass}) {
				$parserState->{implementsClass} .= " ";
			} else {
				$parserState->{implementsClass} = "";
			}
		} elsif ($part eq "{" || $part eq ";") {
			print STDERR "INCLASS BRCSEMI\n" if ($parseDebug || $classDebug);
			$parserState->{forceClassDone} = 1;
			if ($parserState->{classIsObjC} && $part eq "{") {
				$parserState->{ISFORWARDDECLARATION} = 0;
				$parserState->{lastTreeNode} = $treeCur;
				print STDERR "parserState pushed onto stack[OCC-BRCSEMI]\n" if ($parserStackDebug);
				$curline = "";
				$parserState->{storeDec} = $declaration;
				$parserState->{freezereturn} = 1;
				$declaration = "";
				push(@parserStack, $parserState);
				$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
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

				if (!defined($parserState->{ISFORWARDDECLARATION})) {
					print STDERR "FORWARD DECLARATION DETECTED\n" if ($parseDebug || $localDebug || $liteDebug);
					# print STDERR "PREVIOUS FD STATE: ".$parserState->{ISFORWARDDECLARATION}."\n";
					$parserState->{ISFORWARDDECLARATION} = 1;
				}
				$pushParserStateAtBrace = 0;
				$occPushParserStateOnWordTokenAfterNext = 0;
				$pushParserStateAfterToken = 0;
			}
		} elsif ($parserState->{forceClassName} && !$parserState->{forceClassDone} && !$parserState->{inImplements} && !$parserState->{inExtends}) {
			print STDERR "INCLASS ADD\n" if ($parseDebug || $classDebug);
			if ($part =~ /[\n\r]/) {
				$parserState->{forceClassSuper} .= " ";
			} else {
				$parserState->{forceClassSuper} .= $part;
			}
			# print STDERR "SUPER IS $parserState->{forceClassSuper}\n";
		} elsif ($part =~ /</ && $parserState->{classIsObjC} && $occPushParserStateOnWordTokenAfterNext) {
			print STDERR "INCLASS <\n" if ($parseDebug || $classDebug);
			print STDERR "pushParserStateAfterWordToken -> 0 (Conforming)\n" if ($localDebug || $parseDebug);
			print STDERR "inClassConformingToProtocol -> 1\n" if ($localDebug || $parseDebug);
			$pushParserStateAfterWordToken = 0;
			$parserState->{inClassConformingToProtocol} = 1;
			$occPushParserStateOnWordTokenAfterNext = 0;
		} elsif ($part =~ />/ && $parserState->{classIsObjC} && $parserState->{inClassConformingToProtocol}) {
			print STDERR "INCLASS >\n" if ($parseDebug || $classDebug);
			print STDERR "inClassConformingToProtocol -> 0\n" if ($localDebug || $parseDebug);
			$pushParserStateAfterToken = 1;
			print STDERR "pushParserStateAfterWordToken -> 1 (Conforming)\n" if ($localDebug || $parseDebug);
			$parserState->{inClassConformingToProtocol} = 0;
		} elsif ($occPushParserStateOnWordTokenAfterNext && $part =~ /\w/) {
			print STDERR "INCLASS OCCSUPER\n" if ($parseDebug || $classDebug);
			$parserState->{occSuper} = $part;
			# $occPushParserStateOnWordTokenAfterNext = 0;
			# $pushParserStateAfterToken = 1;
		} elsif ($parserState->{inExtends}) {
			print STDERR "INCLASS INEXTENDS (PART: \"$part\")\n" if ($parseDebug || $classDebug);
                        $parserState->{extendsClass} .= $part;
		} elsif ($parserState->{inImplements}) {
			print STDERR "INCLASS INIMPLEMENTS (PART: \"$part\")\n" if ($parseDebug || $classDebug);
                        $parserState->{implementsClass} .= $part;
		} elsif (!$parserState->{classIsObjC}) {
			print STDERR "INCLASS NOTOBJC (OTHER)\n" if ($parseDebug || $classDebug);
			if (!(scalar(@braceStack) - $parserState->{initbsCount})) {
				if ($part =~ /[*(^]/) {
					print STDERR "INCLASS DROP\n" if ($parseDebug || $classDebug);
					$parserState->{inClass} = 0; # We're an instance.  Either a variable or a function.
					print STDERR "inClass -> 0 [6]\n" if ($classDebug);
					$parserState->{sodtype} = $parserState->{preclasssodtype} . $parserState->{sodtype};
				} elsif ($part =~ /\w/) {
					print STDERR "INCLASS GOT WORD TOKEN\n" if ($parseDebug || $classDebug);
					if ($parserState->{classNameFound} && !$parserState->{forceClassName}) {
						print STDERR "INCLASS NOT A CLASS.  IT IS A VARIABLE OR TYPEDEF.\n" if ($parseDebug || $classDebug);
						$parserState->{inClass} = 0;
					} else {
						print STDERR "INCLASS WORD TOKEN IS CLASS NAME\n" if ($parseDebug || $classDebug);
						$parserState->{classNameFound} = 1;
					}
				}
			}
		# } else {
			# print STDERR "BUG\n";
		}
	    };
	    if ($parserState->{inClassConformingToProtocol} == 1) {
		$parserState->{inClassConformingToProtocol} = 2;
	    } elsif ($parserState->{inClassConformingToProtocol}) {
		$parserState->{conformsToList} .= $part;
	    }
	if ($macroDebug) {
		print STDERR "MNT: ".$parserState->{macroNoTrunc}."\n";
	}

		# if (($part eq $ilc || $part eq $ilc_b) && ($lang ne "perl" || $lasttoken ne "\$")) {
			# print STDERR "should be ILC?\n";
		# } else {
			# print STDERR "NO CHANEC: PART \"$part\" ILC \"$ilc\" ILC_B: \"ilc_b\" LANG: \"$lang\" LASTTOKEN: \"$lasttoken\"\n";
		# }

	my $tempInIf = 0;
	if ($HeaderDoc::parseIfElse && (!$parserState->{inMacro} && !$parserState->{inMacroLine} && !($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}))) {
	    if (!(scalar(@braceStack) - $parserState->{initbsCount})) {
		if ($part eq "if" || $part eq "else") {
			print STDERR "INIF OUTSIDE CHANGE -> 1 FOR PART \"$part\" IN $parserState\n" if ($continueDebug);
			if ($part eq "if") {
				$parserState->{seenIf} = 1; # Hint for code that uses results.
				$parserState->{INIF} = 1;
			} else {
				# print STDERR "SETTING IFCONTENTS TO ".$parserState->{functionContents}."\n";
				$parserState->{ifContents} = $parserState->{functionContents};
				$parserState->{functionContents} = "";
				$parserState->{seenElse} = 1; # Hint for code that uses results.
				$parserState->{INIF} = 1;
			}

			print STDERR "CONTINUE -> 1 [INIF]\n" if ($parseDebug || $cppDebug || $macroDebug || $localDebug || $continueDebug);
			$continue = 1;
			# $pushParserStateAtBrace = 1;
		} elsif ($part eq ";") {
			# Not in "if" after the close curly brace.
			print STDERR "INIF OUTSIDE CHANGE -> 0 FOR PART \"$part\" IN $parserState\n" if ($continueDebug);
			$parserState->{INIF} = 0;
		} elsif ($part eq "{") {
			# Not in "if" after the close curly brace.
			print STDERR "INIF OUTSIDE CHANGE -> 0 FOR PART \"$part\" IN $parserState\n" if ($continueDebug);
			$tempInIf = $parserState->{INIF};
			print STDERR "TEMPINIF: $tempInIf\n" if ($continueDebug);
			$parserState->{INIF} = 0;
		} elsif ($part =~ /\S/ && $part ne "(" && $parserState->{INIF}) {
			$parserState->{INIF}++;
			$parserState->{seenBraces} = 1;
		} else {
			print STDERR "INIF OUTSIDE NC (".$parserState->{INIF}.") FOR PART \"$part\" IN $parserState\n" if ($continueDebug);
		}
		# print STDERR "DEBUG $pushParserStateAfterToken $pushParserStateAfterWordToken $pushParserStateAtBrace $occPushParserStateOnWordTokenAfterNext\n";
	} else {
		print STDERR "INIF INSIDE NC (".$parserState->{INIF}.") FOR PART \"$part\" IN $parserState\n" if ($continueDebug);
		# print STDERR "DEBUG $pushParserStateAfterToken $pushParserStateAfterWordToken $pushParserStateAtBrace $occPushParserStateOnWordTokenAfterNext\n";
	    }
	}

	    SWITCH: {
		# Blank declaration handlers (mostly for misuse of
		# OSMetaClassDeclareReservedUsed and similar)

		(($part eq ";") && ($parserState->{startOfDec} == 1) && !$parserState->{inMacro} && !$parserState->{inMacroLine} && !($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) && do {
			print STDERR "LEADING SEMICOLON: CASE 01\n" if ($liteDebug);
			print STDERR "Dropping empty declaration\n" if ($localDebug || $parseDebug);
			$part = "";
			last SWITCH;
		};

		# Macro handlers

		(($parserState->{inMacro} == 1) && ($part eq "define")) && do {
			print STDERR "INMACRO/DEFINE: CASE 02\n" if ($liteDebug);
			# define may be a multi-line macro
			print STDERR "INMACRO AND DEFINE\n" if ($parseDebug || $localDebug);
			$parserState->{inMacro} = 3;
			print STDERR "inMacro -> 3\n" if ($macroDebug || $cppDebug);
			$parserState->{sodname} = "";
			my $pound = $treeCur->token();
			if ($pound eq "$sopreproc") {
				$treeNest = 2;
				if ($treeDebug) { print STDERR "TS TREENEST -> 2 [1]\n"; }
				$treePopOnNewLine = 2;
				$pound .= $part;
				$treeCur->token($pound);
			}
			last SWITCH;
		};
		# (($parserState->{inMacro} == 1 && $macrore_pound ne "" && $part =~ /(if|ifdef|ifndef|endif|else|undef|elif|error|warning|pragma|import|include)/ && ($part ne $definename)) || ($parserState->{inMacro} == 0 && $macrore_nopound ne "" && $part =~ /$macrore_nopound/)) 
		# (($parserState->{inMacro} == 1 && $part =~ /(if|ifdef|ifndef|endif|else|undef|elif|error|warning|pragma|import|include)/ )) && do
		(!$parserState->{inComment} && (($parserState->{inMacro} == 1 && $macrore_pound ne "" && $part =~ /^$macrore_pound$/ && ($part ne $definename)) || ($parserState->{inMacro} == 0 && $macrore_nopound ne "" && $part =~ /^$macrore_nopound$/))) && do {
			print STDERR "MACRORE-v: \"$macrore_pound\"\n" if ($macroDebug);
			print STDERR "MACRORE-r: \"(if|ifdef|ifndef|endif|else|undef|elif|error|warning|pragma|import|include)\"\n" if ($macroDebug);
			print STDERR "MACRORE-n: \"$macrore_nopound\"\n" if ($macroDebug);
			print STDERR "INMACRO/IF: CASE 03\n" if ($liteDebug);
			print STDERR "INMACRO AND IF/IFDEF/IFNDEF/ENDIF/ELSE/PRAGMA/IMPORT/INCLUDE\n"  if ($parseDebug || $localDebug);
			# these are all single-line macros

			$parserState->{inMacro} = 4;
			print STDERR "inMacro -> 4\n" if ($macroDebug || $cppDebug);
			$parserState->{sodname} = "";
			my $pound = $treeCur->token();
			if ($pound eq "$sopreproc") {
				$treeNest = 2;
				if ($treeDebug) { print STDERR "TS TREENEST -> 2 [2]\n"; }
				$treePopOnNewLine = 1;
				$pound .= $part;
				$treeCur->token($pound);
				if ($part eq "endif") {
					# the rest of the line is not part of the macro
					# NOTE: Do not change treeCur in the
					# next line.
					$treeCur->addChild("\n", 0);
					$treeNest = 0;
					if ($treeDebug) { print STDERR "TS TREENEST -> 0 [3]\n"; }
					$treePopOnNewLine = 0;
					$treeSkip = 1;
				}
			}
			last SWITCH;
		};
		(($parserState->{inMacroLine} == 1) && ($part =~ /(if|ifdef|ifndef|endif|else|undef|elif|error|warning|pragma|import|include|define)/o)) && do {
			print STDERR "INMACROLINE/IF: CASE 04\n" if ($liteDebug);
			print STDERR "INMACROLINE AND IF/IFDEF/IFNDEF/ENDIF/ELSE/PRAGMA/IMPORT/INCLUDE\n" if ($parseDebug || $localDebug);
			my $pound = $treeCur->token();
			if ($pound eq "$sopreproc") {
				$pound .= $part;
				$treeCur->token($pound);
				if ($part =~ /define/o) {
					$treeNest = 2;
					if ($treeDebug) { print STDERR "TS TREENEST -> 2 [4]\n"; }
					$treePopOnNewLine = 2;
				} elsif ($part eq "endif") {
					# the rest of the line is not part of the macro
					# NOTE: Do not change treeCur in the
					# next line.
					$treeCur->addChild("\n", 0);
					$treeNest = 0;
					if ($treeDebug) { print STDERR "TS TREENEST -> 0 [5]\n"; }
					$treePopOnNewLine = 0;
					$treeSkip = 1;
				} else {
					$treeNest = 2;
					if ($treeDebug) { print STDERR "TS TREENEST -> 2 [6]\n"; }
					$treePopOnNewLine = 1;
				}
			}
			last SWITCH;
		};
		($parserState->{inMacro} == 1 && ($part ne $soc) && ($part ne $eoc) && $part =~ /\s/) && do {
			$treepart = $part; $part = "";
			last SWITCH;
		};
		($parserState->{inMacro} == 1 && ($part ne $soc) && ($part ne $eoc)) && do {
			print STDERR "INMACRO PPTOKEN: CASE 05\n" if ($liteDebug);
			print STDERR "INMACRO IS 1, CHANGING TO 2 (NO PROCESSING)\n" if ($parseDebug || $localDebug);
			# error case.
			$parserState->{inMacro} = 2;
			print STDERR "inMacro -> 2\n" if ($macroDebug || $cppDebug);
			last SWITCH;
		};
		($parserState->{inMacro} > 1 && $part ne "//" && $part !~ /[\n\r]/ && ($part ne $soc) && ($part ne $eoc)) && do {
			print STDERR "INMACRO OTHERTOKEN: CASE 06\n" if ($liteDebug);
			print STDERR "INMACRO > 1, PART NE //" if ($parseDebug || $localDebug);
			if ($cppDebug || $parseDebug) {
				print STDERR "\nISQUOTED: ".$parserState->isQuoted()."\n" if ($parseDebug || $localDebug || $cppDebug || $macroDebug);
			}
			if ($part eq "\\") {
				print STDERR "BS ADD\n" if ($parseDebug || $localDebug || $cppDebug || $macroDebug);
				$parserState->addBackslash();
				$bshandled = 1;
				print STDERR "ISQUOTED NOW: ".$parserState->isQuoted()."\n" if ($parseDebug || $localDebug || $cppDebug || $macroDebug);
			} elsif ($part !~ /[ \t]/) {
				print STDERR "BS RESET\n" if ($parseDebug || $localDebug || $cppDebug || $macroDebug);
				$parserState->resetBackslash();
				$bshandled = 1;
				print STDERR "ISQUOTED NOW: ".$parserState->isQuoted()."\n" if ($parseDebug || $localDebug || $cppDebug || $macroDebug);
			}
			print STDERR "PART: $part\n" if ($macroDebug);
			if ($parserState->{seenMacroPart}) {
				print STDERR "MACRO: SMP&TI\n" if ($macroDebug);
				if (!(scalar(@braceStack) - $parserState->{initbsCount})) {
					print STDERR "MACRO: NOSTACK\n" if ($macroDebug);
					if ($part =~ /\s/o && $parserState->{macroNoTrunc} == 1) {
						print STDERR "MACRO: ENDOFNAME\n" if ($macroDebug);
						$parserState->{macroNoTrunc} = 0;
						$treeCur->{HIDEMACROLASTTOKEN} = 1;
					} elsif ($part =~ /[\{\(]/o) {
						print STDERR "MACRO: BRACE\n" if ($macroDebug);
						if (!$parserState->{macroNoTrunc}) {
							# $parserState->{seenBraces} = 1;
							print STDERR "END OF MACRO\n" if ($macroDebug);
							if ($HeaderDoc::truncate_inline) {
								$HeaderDoc::hidetokens = 3;
							} else {
								$treeCur->{HIDEMACROLASTTOKEN} = 2;
							}
						}
					} else {
						print STDERR "MACRO: OTHERTOKEN\n" if ($macroDebug);
						$parserState->{macroNoTrunc} = 2;
					}
				}
			}
			if ($part =~ /[\{\(]/o) {
				push(@braceStack, $part);
				push(@parsedParamParseStack, $parserState->{parsedParamParse});
				print STDERR "PUSHED $part ONTO BRACESTACK\n" if ($macroDebug || $braceDebug);
			} elsif ($part =~ /[\}\)]/o) {
				if ($part ne peekmatch(\@braceStack, $fullpath, $inputCounter)) {
					if ($parserState->{macroNoTrunc} == 1) {
						# We haven't reached the end of the first part of the declaration, so this is an error.
						warn("$fullpath:$inputCounter: warning: Initial braces in macro name do not match.\nWe may have a problem.\n");
					}
				}
				my $temp = pop(@braceStack);
				$parserState->{parsedParamParse} = pop(@parsedParamParseStack);
				print STDERR "POPPED $temp FROM BRACESTACK\n" if ($macroDebug || $braceDebug);
			}

			if ($part =~ /\S/o) {
				$parserState->{seenMacroPart} = 1;
				$parserState->{lastsymbol} = $part;
				if (($parserState->{sodname} eq "") && ($parserState->{inMacro} == 3)) {
					print STDERR "DEFINE NAME IS $part\n" if ($macroDebug);
					$parserState->{sodname} = $part;
				}
			}
			$lastchar = $part;
			last SWITCH;
		};

		# Regular expression handlers

		(length($varname) && $part eq $varname && !($inRegexp || $inRegexpTrailer || $parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && !$parserState->{sodclass})  && do {
			$parserState->{sodclass} = "variable";
			print STDERR "sodclass -> variable (explicit[1])\n" if ($sodDebug);
			print STDERR "DETECTED VARIABLE KEYWORD\n" if ($localDebug || $parseDebug);

			# Fall through.
		};
		(length($constname) && $part eq $constname && !($inRegexp || $inRegexpTrailer || $parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && (!(scalar(@braceStack) - $parserState->{initbsCount}))) && do {
			$parserState->{constKeywordFound} = 1;
			print STDERR "DETECTED CONSTANT KEYWORD\n" if ($localDebug || $parseDebug);

			# Fall through.
		};
		# print STDERR "IRE: $inRegexp IRT: $inRegexpTrailer IS: $parserState->{inString} ICo $parserState->{inComment} ILC: $parserState->{inInlineComment} ICh $parserState->{inChar}\n";
		# print STDERR "IRE: $inRegexp IRT: $inRegexpTrailer IS: $parserState->{inString} ICo $parserState->{inComment} ILC: $parserState->{inInlineComment} ICh $parserState->{inChar}\n";
		(length($regexppattern) && $part =~ /^($regexppattern)$/ && !($inRegexp || $inRegexpTrailer || $parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) && do {
			print STDERR "REGEXP PATTERN: CASE 07\n" if ($liteDebug);
			my $match = $1;
			print STDERR "REGEXP WITH PREFIX\n" if ($regexpDebug);
			$regexpNoInterpolate = 0;
			if ($match =~ /^($singleregexppattern)$/) {
				# e.g. perl PATTERN?
				$inRegexp = 2;
			} else {
				$inRegexp = 4;
				# print STDERR "REGEXP PART IS \"$part\"\n";
				if ($part eq "tr") { $regexpNoInterpolate = 1; }
				# if ($part =~ /tr/) { $regexpNoInterpolate = 1; }
			}
			$inRegexpFirstPart = 2;
			last SWITCH;
		}; # end regexppattern
		(($inRegexp || $parserState->{lastsymbol} eq "~" ||
		  ((!$inRegexp) && $part =~ /\// &&
		   length($regexpcharpattern) &&
		   !($inRegexp || $inRegexpTrailer ||
		     $parserState->{inString} || $parserState->{inComment} ||
		     $parserState->{inInlineComment} || $parserState->{inChar})
		  )
		 ) &&
		 (length($regexpcharpattern) &&
		  $part =~ /^($regexpcharpattern)$/ &&
		  (!$inRegexpCharClass) &&
		  (!scalar(@regexpStack) || $part eq peekmatch(\@regexpStack, $fullpath, $inputCounter))
		 )
		) && do {
			print STDERR "REGEXP CHARACTER: CASE 08\n" if ($liteDebug);
			print STDERR "REGEXP?\n" if ($regexpDebug);

			# if ($lasttoken eq "\\") 
			if ($parserState->isQuoted($lang. $sublang)) {
				# jump to next match.
				$lasttoken = $part;
				$parserState->{lastsymbol} = $part;
			} else {
				print STDERR "REGEXP POINT A\n" if ($regexpDebug);

				if (!$inRegexp) {
					$inRegexp = 2;
					$inRegexpFirstPart = 1;
				} elsif ($inRegexpFirstPart != 2) {
					$inRegexpFirstPart = 0;
				} else {
					$inRegexpFirstPart = 1;
				}
	
				$lasttoken = $part;
				$parserState->{lastsymbol} = $part;

				my $is_a_comment = 0;
				if ($part eq "#" &&
				    ((scalar(@regexpStack) != 1) || 
				     (peekmatch(\@regexpStack, $fullpath, $inputCounter) ne "#"))) {
					if ($nextpart =~ /^\s/o) {
						# it's a comment.  jump to next match.
						$is_a_comment = 1;
					}
				}
				if (!$is_a_comment) {
					my $fall_through = 0;

					print STDERR "REGEXP POINT B\n" if ($regexpDebug);
	
					if (!scalar(@regexpStack)) {
						print STDERR "PUSHING $part ONTO REGEXPSTACK\n" if ($localDebug || $parseDebug);
						push(@regexpStack, $part);
						$inRegexp--;
					} else {
						my $match = peekmatch(\@regexpStack, $fullpath, $inputCounter);
						my $tos = pop(@regexpStack);
						print STDERR "popped $tos FROM REGEXPSTACK\n" if ($localDebug || $parseDebug);

						if (!scalar(@regexpStack) && ($match eq $part)) {
							$inRegexp--;
							if ($inRegexp == 2 && $tos eq "/") {
								# we don't double the slash in the
								# middle of a s/foo/bar/g style
								# expression.
								$inRegexp--;
							}
							if ($inRegexp) {
								print STDERR "PUSHING $tos ONTO REGEXPSTACK\n" if ($localDebug || $parseDebug);
								push(@regexpStack, $tos);
							}
						} elsif (scalar(@regexpStack) == 1) {
							print STDERR "PUSHING $tos ONTO REGEXPSTACK\n" if ($localDebug || $parseDebug);
							push(@regexpStack, $tos);
							if ($tos =~ /['"`]/o || $regexpNoInterpolate) {
								# these don't interpolate.
								$fall_through = 1;
							}
						} else {
							print STDERR "PUSHING $tos ONTO REGEXPSTACK\n" if ($localDebug || $parseDebug);
							push(@regexpStack, $tos);
							if ($tos =~ /['"`]/o || $regexpNoInterpolate) {
								# these don't interpolate.
								$fall_through = 1;
							} else {
								print STDERR "PUSHING $part ONTO REGEXPSTACK\n" if ($localDebug || $parseDebug);
								push(@regexpStack, $part);
							}
						}
					}
					if (!$fall_through) {
						print STDERR "REGEXP POINT C\n" if ($regexpDebug);
						if (!$inRegexp) {
							$inRegexpTrailer = 2;
						}
						last SWITCH;
					}
				}
			}
		}; # end regexpcharpattern

		# Start of preprocessor macros

		($part eq "$sopreproc") && do {
			print STDERR "SOPREPROC: CASE 09\n" if ($liteDebug);
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				if ($parserState->{onlyComments}) {
					print STDERR "inMacro -> 1\n" if ($macroDebug || $cppDebug);
					$parserState->{inMacro} = 1;

					## @@@ FIXME DAG NEXT TWO LINES NEEDED FOR IDL TO AVOID
					## "warning: Declaration starts with # but is not preprocessor macro"
					## ERROR MESSAGE, BUT THIS BREAKS C/C++.
					## WHY !?!?!
					## 
					## if ($$treepart = " ";
					## $nextpart = $part.$nextpart;
					##
					## END IDL-ONLY BLOCK

					# $continue = 0;
		    			# print STDERR "CONTINUE -> 0 [1]\n" if ($localDebug || $macroDebug || $continueDebug);
				} elsif ($curline =~ /^\s*$/o) {
					$parserState->{inMacroLine} = 1;
					print STDERR "IML\n" if ($localDebug);
				} elsif ($postPossNL) {
					print STDERR "PRE-IML \"$curline\"\n" if ($localDebug || $macroDebug);
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
			print STDERR "SOFUNC: CASE 10\n" if ($liteDebug);
				$parserState->{sodclass} = "function";
				print STDERR "sodclass -> function (explicit[2])\n" if ($sodDebug);
				print STDERR "FUNCTION OR PROCEDURE FOUND [1].\n" if ($localDebug);
				if (!$pascal) {
					$parserState->{kr_c_function} = 1;
				}
				$parserState->{typestring} = "function";
				$parserState->{startOfDec} = 2;
				print STDERR "startOfDec -> 2 [1]\n" if ($localDebug);
				$parserState->{namePending} = 1;
				# if (!$parserState->{seenBraces}) { # TREEDONE
					# $treeNest = 1;
					# $treePopTwo++;
					# push(@treeStack, $treeCur);
					# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
					# bless($treeCur, "HeaderDoc::ParseTree");
				# }
				print STDERR "namePending -> 1 [1]\n" if ($parseDebug);
				last SWITCH;
			};

		# C++ destructor handler.

		($part =~ /\~/o && $lang eq "C" && $sublang eq "cpp" && !!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) && do {
			print STDERR "C++ DESTRUCTOR: CASE 11\n" if ($liteDebug);
				print STDERR "TILDE\n" if ($localDebug);
				$parserState->{seenTilde} = 2;
				$lastchar = $part;
				$parserState->{onlyComments} = 0;
				# $name .= '~';
				last SWITCH;
			};

		# Objective-C method handler.

		($part =~ /[-+]/o && $parserState->{onlyComments}) && do {
			print STDERR "OBJC METHOD: CASE 12\n" if ($liteDebug);
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				print STDERR "OCCMETHOD\n" if ($localDebug);
				# Objective C Method.
				$parserState->{occmethod} = 1;
				$parserState->{occmethodtype} = $part;
				$parserState->{occmethodreturntype} = "";
				$lastchar = $part;
				$parserState->{onlyComments} = 0;
				print STDERR "[a]onlyComments -> 0\n" if ($macroDebug);
				if (!$parserState->{seenBraces}) { # TREEDONE
				    if (!$parserState->{hollow}) {
					print STDERR "SETHOLLOW -> 1\n" if ($parserStackDebug);
					$sethollow = 1;
				    }
				    $treeNest = 1;
					if ($treeDebug) { print STDERR "TS TREENEST -> 1 [7]\n"; }
				    $parserState->{treePopTwo} = 1;
				}
			    }
			    last SWITCH;
			};

		# newline handler.
		($part =~ /[\n\r]/o) && do {
			print STDERR "NEWLINE: CASE 13\n" if ($liteDebug);
			# NEWLINE FOUND
				$treepart = $part;
				if ($inRegexp) {
					warn "$fullpath:$inputCounter: warning: multi-line regular expression\n";
				}
				print STDERR "NLCR\n" if ($tsDebug || $treeDebug || $localDebug);
				if ($lastchar !~ /[\,\;\{\(\)\}]/o && $nextpart !~ /[\{\}\(\)]/o) {
					if ($lastchar ne "*/" && $nextpart ne "/*") {
						if (!$parserState->{inMacro} && !$parserState->{inMacroLine} && !$treePopOnNewLine) {
							print STDERR "NL->SPC\n" if ($localDebug);
							$part = " ";
							print STDERR "LC: $lastchar\n" if ($localDebug);
							print STDERR "NP: $nextpart\n" if ($localDebug);
							$postPossNL = 2;
						} else {
							$parserState->{inMacroLine} = 0;
							# Don't push parsed parameter here.  Just clear it.
							# push(@{$parserState->{parsedParamList}}, $parsedParam);
							# print STDERR "pushed $parsedParam into parsedParamList [1]\n" if ($parmDebug);
							print STDERR "skipped pushing CPP directive $parsedParam into parsedParamList [1]\n" if ($parmDebug || $cppDebug || $localDebug);
							$parsedParam = "";
						}
					}
				}
				if ($lang eq "shell" && $parserState->{sodclass} ne "function") {
					print STDERR "SC: ".$parserState->{sodclass}."\n" if ($localDebug || $parseDebug);
					if (!($parserState->{inString} || $parserState->{inChar})) {
						if ($parserState->{lastsymbol} ne "\\") {
							print STDERR "LS: ".$parserState->{lastsymbol}."\n" if ($localDebug || $parseDebug);
							print STDERR "CONTINUE -> 0 [1aa]\n" if ($parseDebug || $cppDebug || $macroDebug || $localDebug || $continueDebug);
							$continue = 0;
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
						warn "$fullpath:$inputCounter: warning: Attempted to pop off top of tree\n";
					}
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print STDERR "TSPOP [1]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
				}
				if ($treePopOnNewLine == 1 || ($treePopOnNewLine && !$parserState->isQuoted())) {
					# $parserState->{lastsymbol} ne "\\"
					$treeCur = $treeCur->addSibling($treepart, 0);
					bless($treeCur, "HeaderDoc::ParseTree");
					# push(@treeStack, $treeCur);
					$treeSkip = 1;
					$treeCur = pop(@treeStack);
					if (!$treeCur) {
						$treeCur = $treeTop;
						warn "$fullpath:$inputCounter: warning: Attempted to pop off top of tree\n";
					}
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->addSibling("", 0); # empty token
					print STDERR "TSPOP [1a]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
					$treePopOnNewLine = 0;
					$HeaderDoc::hidetokens = 0;
				} else {
					print STDERR "Not popping from tree.  Probably quoted.\n" if ($localDebug || $parseDebug);
				}
				next SWITCH;
			};

		# C++ template handlers

		($part eq $sotemplate && !$parserState->{seenBraces} && ($parserState->{inOperator} != 1)) && do {
			print STDERR "C++ TEMPLATE: CASE 14\n" if ($liteDebug);
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				if ($HeaderDoc::hideIDLAttributes && $lang eq "C" && $sublang eq "IDL") { $hideTokenAndMaybeContents = 3; }
				print STDERR "inTemplate -> ".($parserState->{inTemplate}+1)."\n" if ($localDebug);
	print STDERR "SBS: " . scalar(@braceStack) . ".\n" if ($localDebug);
				$parserState->{inTemplate}++;
				if (!(scalar(@braceStack) - $parserState->{initbsCount})) {
					$parserState->{preTemplateSymbol} = $parserState->{lastsymbol};
				}
				$parserState->{lastsymbol} = "";
				$lastchar = $part;
				$parserState->{onlyComments} = 0;
				push(@parsedParamParseStack, $parserState->{parsedParamParse});
				push(@braceStack, $part); pbs(@braceStack);
				print STDERR "PUSHED $part ONTO BRACESTACK\n" if ($macroDebug || $braceDebug);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeNest = 1;
					if (!$parserState->{hollow}) { $sethollow = 1; } # IDL can have this at the start of declaration.
					if ($treeDebug) { print STDERR "TS TREENEST -> 1 [8]\n"; }
					# push(@treeStack, $treeCur);
					# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
					# bless($treeCur, "HeaderDoc::ParseTree");
				}
				print STDERR "[b]onlyComments -> 0\n" if ($macroDebug);
			    }
			    last SWITCH;
			};
		($part eq $eotemplate && !$parserState->{seenBraces} && ($parserState->{inOperator} != 1)) && do {
			print STDERR "C++ TEMPLATE END: CASE 15\n" if ($liteDebug);
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && (!(scalar(@braceStack)-$parserState->{initbsCount}) || $parserState->{inTemplate})) {
				if ($parserState->{inTemplate})  {
					print STDERR "parserState->{inTemplate} -> ".($parserState->{inTemplate}-1)."\n" if ($localDebug);
					$parserState->{inTemplate}--;
					$parserState->{lastsymbol} = "";
					$lastchar = $part;
					$curline .= " ";
					$parserState->{onlyComments} = 0;
					print STDERR "[c]onlyComments -> 0\n" if ($macroDebug);
				}
				my $top = pop(@braceStack);
				$parserState->{parsedParamParse} = pop(@parsedParamParseStack);
				print STDERR "POPPED $top FROM BRACESTACK\n" if ($macroDebug || $braceDebug);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeCur->addSibling($part, 0); $treeSkip = 1;
					$treeCur = pop(@treeStack) || $treeTop;
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print STDERR "TSPOP [2]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
				}
				if ($top ne "$sotemplate") {
					warn("$fullpath:$inputCounter: warning: Template (angle) brackets do not match.\nWe may have a problem.\n");
				}
			    }
			    last SWITCH;
			};

		#
		# Handles C++ access control state, e.g. "public:"
		#

		($part eq ":") && do {
			print STDERR "Access control colon: CASE 16\n" if ($liteDebug);
			print STDERR "TS IS \"$parserState->{typestring}\"\n" if ($localDebug || $parseDebug);
			# fall through to next colon handling case if we fail.
			if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {

				if ($pascal && (!(scalar(@braceStack)-$parserState->{initbsCount}))) {
					print STDERR "Clearing SOD (pascal variable)\n" if ($sodDebug || $localDebug);
					$parserState->{startOfDec} = 1;
					print STDERR "startOfDec -> 1 [2]\n" if ($localDebug);
					print STDERR "SODCLASS: \"$parserState->{sodclass}\"\n" if ($sodDebug || $localDebug);
					if ($parserState->{sodclass} ne "function") {
						print STDERR "sodclass -> function (implicit)[3]\n" if ($sodDebug);
						$parserState->{sodclass} = "variable";
					}
					$parserState->{nameList} = $parserState->{sodtype}." ".$parserState->{sodname};
					print STDERR "NL: $parserState->{nameList}\n" if ($sodDebug || $localDebug);
					$parserState->{sodname} = "";
					$parserState->{sodtype} = "";
					$parserState->{waitingForTypeInformation} = 2;
					print STDERR "CLEARING CURLINE ($curline)\n" if ($sodDebug || $localDebug);
					$curline = "";
				}

				if (length($accessregexp) && ($lastnspart =~ /$accessregexp/)) {
					# We're special.
					print STDERR "PERMANENT ACS CHANGE from $HeaderDoc::AccessControlState to $1\n" if ($localDebug);
					$parserState->{sodname} = "";
					$parserState->{typestring} = "";
					print STDERR "RESET HOLLOW at $part\n" if ($parserStackDebug);
					if ($parserState->{hollow}) {
						my $node = $parserState->{hollow};
						$node->{BLOCKOFFSET} = undef;
						$node->{INPUTCOUNTER} = undef;
					}

					$parserState->{hollow} = undef;
					$parserState->{returntype} = ""; # @@@ DELETE THIS LINE IF IT CAUSES TEST FAILURES.
					$curline = "";
					$parserState->{onlyComments} = 1;
					print STDERR "SET onlyComments to 1\n" if ($parserStateInsertDebug || $parseDebug);
					print STDERR "hollowskip -> 1 (ACS)\n" if ($parserStateInsertDebug);
					$hollowskip = 1;
					$HeaderDoc::AccessControlState = $1;
					$lastACS = $1;
					last SWITCH;
				} elsif ($structname && $parserState->{typestring} eq $structname) {
					if (!(scalar(@braceStack) - $parserState->{initbsCount})) {
						if (!$parserState->{structClassName}) {
							$parserState->{structClassName} = $parserState->{lastsymbol};
							$parserState->{bracePending} = 2;
						}
					}
				}
			}
		    };

		(length($accessregexp) && ($part =~ /$accessregexp/)) && do {
			print STDERR "Access regexp: CASE 17\n" if ($liteDebug);
			if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				# We're special.
				if ($part =~ /^\@(.*)$/) {
					print STDERR "PERMANENT ACS CHANGE from $HeaderDoc::AccessControlState to $1\n" if ($localDebug);
					$parserState->{sodname} = "";
					$parserState->{typestring} = "";
					print STDERR "RESET HOLLOW at $part\n" if ($parserStackDebug);
					$parserState->{hollow} = undef;
					$parserState->{onlyComments} = 1;
					$hollowskip = 1;
					print STDERR "hollowskip -> 1 (\@ACS)\n" if ($parserStateInsertDebug);
					$HeaderDoc::AccessControlState = $1;
					$lastACS = $1;
					last SWITCH;
				} else {
					print STDERR "TEMPORARY ACS CHANGE from $HeaderDoc::AccessControlState to $1\n" if ($localDebug);
					$parserState->{sodname} = "";
					$lastACS = $HeaderDoc::AccessControlState;
					$HeaderDoc::AccessControlState = $1;
				}
			} else {
				next SWITCH;
			}
		};
		(length($requiredregexp) && $part =~ /$requiredregexp/) && do {
			print STDERR "REQUIRED REGEXP MATCH: \"$part\"\n" if ($localDebug || $parseDebug);
			$hollowskip = 1;
			print STDERR "hollowskip -> 1 (requiredregexp)\n" if ($parserStateInsertDebug);

			last SWITCH;
		};

		#
		# C++ copy constructor handler.  For example:
		# 
		# char *class(void *a, void *b) :
		#       class(pri_type, pri_type);
		#
		($part eq ":") && do {
			print STDERR "Copy constructor: CASE 18\n" if ($liteDebug);
			if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				if ($parserState->{occmethod}) {
				    $parserState->{name} = $parserState->{lastsymbol};
				    if ($parserState->{occparmlabelfound}) {
					$parserState->{occmethodname} .= "$parserState->{lastsymbol}:";
					if ($occMethodNameDebug) {
						print STDERR "OCC method name now ".$parserState->{occmethodname}." (lastsymbol was \"".$parserState->{lastsymbol}."\").\n";
					}
					$parserState->{occparmlabelfound} = -1; # next token is name of this parameter, followed by label for next parameter.
				    } else {
					if ($occMethodNameDebug) {
						print STDERR "OCC method name missing.\n";
						print STDERR "OCC method name still ".$parserState->{occmethodname}." (lastsymbol was \"".$parserState->{lastsymbol}."\").\n";
					}
					$parserState->{occparmlabelfound} = -2; # Special case: grab the parameter name instead because parameter has no label.
				    }
				    # Start doing line splitting here.
				    # Also, capture the method's name.
				    if ($parserState->{occmethod} == 1) {
					$parserState->{occmethod} = 2;
					if (!$prespace) { $prespaceadjust = 4; }
					$parserState->{onlyComments} = 0;
					print STDERR "[d]onlyComments -> 0\n" if ($macroDebug);
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
						    print STDERR "parsedParamList pushed\n" if ($parmDebug);
						}
						# print STDERR "SEOPPLS\n";
						# for my $item (@{$parserState->{pplStack}}) {
							# print STDERR "PPLS: $item\n";
						# }
						# print STDERR "OEOPPLS\n";
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
					if ($treeDebug) { print STDERR "TS TREENEST -> 1 [9]\n"; }
				    $parserState->{treePopTwo} = 1;
				    # $treeCur = pop(@treeStack) || $treeTop;
				    # bless($treeCur, "HeaderDoc::ParseTree");
			    }
			    last SWITCH;
			    } else {
				print STDERR "NOPE\n" if ($liteDebug);
				next SWITCH;
			    }
			};

		# Non-newline, non-carriage-return whitespace handler.

		($part =~ /\s/o) && do {
			print STDERR "Whitespace: CASE 19\n" if ($liteDebug);
				# just add white space silently.
				# if ($part eq "\n") { $parserState->{lastsymbol} = ""; };
				$lastchar = $part;
				last SWITCH;
		};

		# backslash handler (largely useful for macros, strings).

		($part =~ /\\/o) && do {
			print STDERR "BACKSLASH: CASE 20\n" if ($liteDebug);
			$parserState->{lastsymbol} = $part; $lastchar = $part;
			$parserState->addBackslash();
		};

		# quote and bracket handlers.

		($part eq "\"") && do {
			print STDERR "DOUBLE QUOTE: CASE 21\n" if ($liteDebug);
				print STDERR "dquo\n" if ($localDebug);

				# print STDERR "QUOTEDEBUG: CURSTRING IS '$curstring'\n";
				# print STDERR "QUOTEDEBUG: CURLINE IS '$curline'\n";
				if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} || $inRegexp)) {
					$parserState->{onlyComments} = 0;
					print STDERR "[e]onlyComments -> 0\n" if ($macroDebug);
					print STDERR "LASTTOKEN: $lasttoken\nCS: $curstring\n" if ($localDebug);
					# if (($lasttoken !~ /\\$/o) && ($curstring !~ /\\$/o))
					if (!$parserState->isQuoted($lang, $sublang)) {
						if (!$parserState->{inString}) {
						    if (!$parserState->{seenBraces}) { # TREEDONE
							$treeNest = 1;
					if ($treeDebug) { print STDERR "TS TREENEST -> 1 [10]\n"; }
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
							print STDERR "TSPOP [3]\n" if ($tsDebug || $treeDebug);
							bless($treeCur, "HeaderDoc::ParseTree");
						    }
						}
						$parserState->{inString} = (1-$parserState->{inString});
						print STDERR "INSTRING -> ".$parserState->{inString} ."\n" if ($liteDebug || $parseDebug || $localDebug);
					}
				}
				$lastchar = $part;
				$parserState->{lastsymbol} = "";

				last SWITCH;
			};
		($part eq "[") && do {
			print STDERR "LEFT BRACKET: CASE 22\n" if ($liteDebug);
			    # left square bracket (square brace)
			    my $fall_through = 0;
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				if ($inRegexp && $inRegexpCharClass) {
					# jump to next match.
					$lasttoken = $part;
					$parserState->{lastsymbol} = $part;
					$fall_through = 1;
				} else {
					if ($inRegexp) {
						$inRegexpCharClass = 3;
					}
					print STDERR "lbracket\n" if ($localDebug);
	
					print STDERR "LBRACKET DEBUG TRACE: SODNAME: ".$parserState->{sodname}." SODTYPE: ".$parserState->{sodtype}." SIMPLETDCONTENTS: ".$parserState->{simpleTDcontents}."\n" if ($localDebug);
					if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inString})) {
						$parserState->{onlyComments} = 0;
						print STDERR "[f]onlyComments -> 0\n" if ($macroDebug);
					}
					push(@parsedParamParseStack, $parserState->{parsedParamParse});
					push(@braceStack, $part); pbs(@braceStack);
					print STDERR "PUSHED $part ONTO BRACESTACK\n" if ($macroDebug || $braceDebug);
					if (!$parserState->{seenBraces}) { # TREEDONE
						$treeNest = 1;
						if ($treeDebug) { print STDERR "TS TREENEST -> 1 [11]\n"; }
						# push(@treeStack, $treeCur);
						# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
						# bless($treeCur, "HeaderDoc::ParseTree");
					}
					$curline = spacefix($curline, $part, $lastchar);
					$parserState->{lastsymbol} = "";
					$parserState->{inBrackets} += 1;
				}
			    }
			    if (!$fall_through) {
				$lastchar = $part;

				if ($parserState->{startOfDec} == 2) {
					if (!$parserState->{inTemplate}) {
						if (!$parserState->{sodbrackets}) {
							$parserState->{sodbrackets} = $part;
						} else {
							$parserState->{sodbrackets} .= $part;
						}
						print STDERR "sodbrackets set to ".$parserState->{sodbrackets}."\n" if ($sodDebug);
					} else {
						print STDERR "Not adjusting startOfDec or sodname because in template.\n" if ($localDebug || $sodDebug);
					}
				}


				last SWITCH;
			    }
			};
		($part eq "]") && do {
			print STDERR "CLOSE BRACKET: CASE 23\n" if ($liteDebug);
			    # right square bracket (square brace)
			    if ($inRegexp && ($inRegexpCharClass == 2)) {
				print STDERR "First in character class.  Treating as literal.\n" if ($localDebug || $parseDebug);
				# A close bracket as first character in a
				# character class is treated as a literal.
				# jump to next match.
				$lasttoken = $part;
				$parserState->{lastsymbol} = $part;

			    } elsif ($inRegexpCharClass && ($parserState->isQuoted($lang, $sublang))) {
				# At least in Perl, \] is treated as a literal
				# even in a character class....
				print STDERR "Quoted ] in character class.  Treating as literal.\n" if ($localDebug || $parseDebug);
				$lasttoken = $part;
				$parserState->{lastsymbol} = $part;

			    } else {
			      if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				print STDERR "rbracket\n" if ($localDebug);

				if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inString})) {
					$parserState->{onlyComments} = 0;
					print STDERR "[g]onlyComments -> 0\n" if ($macroDebug);
				}
				my $top = pop(@braceStack);
				$parserState->{parsedParamParse} = pop(@parsedParamParseStack);
				print STDERR "POPPED $top FROM BRACESTACK\n" if ($macroDebug || $braceDebug);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeCur->addSibling($part, 0); $treeSkip = 1;
					$treeCur = pop(@treeStack) || $treeTop;
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print STDERR "TSPOP [4]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
				}
				if ($top ne "[") {
					warn("$fullpath:$inputCounter: warning: Square brackets do not match.\nWe may have a problem.\n");
					warn("Declaration to date: $declaration$curline\n");
				}
				pbs(@braceStack);
				$curline = spacefix($curline, $part, $lastchar);
				$parserState->{lastsymbol} = "";
				$parserState->{inBrackets} -= 1;
			      }
			      if ($inRegexpCharClass) {
				$inRegexpCharClass = 0;
			      }
			      $lastchar = $part;

			      if ($parserState->{startOfDec} == 2) {
				if (!$parserState->{inTemplate}) {
					if (!$parserState->{sodbrackets}) {
						$parserState->{sodbrackets} = $part;
					} else {
						$parserState->{sodbrackets} .= $part;
					}
					print STDERR "sodbrackets set to ".$parserState->{sodbrackets}."\n" if ($sodDebug);
				} else {
					print STDERR "Not adjusting startOfDec or sodname because in template.\n" if ($localDebug || $sodDebug);
				}
			      }


			      last SWITCH;
			    }
			};
		($part eq "'") && do {
			print STDERR "SINGLE QUOTE: CASE 24\n" if ($liteDebug);
				print STDERR "squo\n" if ($localDebug);

				if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inString} || $inRegexp)) {
					# if (($lasttoken !~ /\\$/o) && ($curstring !~ /\\$/o))
					if (!$parserState->isQuoted($lang, $sublang)) {
						$parserState->{onlyComments} = 0;
						print STDERR "[h]onlyComments -> 0\n" if ($macroDebug);
						if (!$parserState->{inChar}) {
						    if (!$parserState->{seenBraces}) { # TREEDONE
							$treeNest = 1;
					if ($treeDebug) { print STDERR "TS TREENEST -> 1 [12]\n"; }
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
							print STDERR "TSPOP [5]\n" if ($tsDebug || $treeDebug);
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

		(($part eq $ilc || $part eq $ilc_b) && ($lang ne "perl" || $lasttoken ne "\$")) && do {
			print STDERR "SINGLE LINE COMMENT: CASE 25\n" if ($liteDebug);
				print STDERR "ILC\n" if ($localDebug || $ilcDebug);

				if (!($parserState->{inComment} || $parserState->{inChar} || $parserState->{inString} || $inRegexp)) {
					$parserState->{inInlineComment} = 4;
					print STDERR "inInlineComment -> 1\n" if ($ilcDebug);
					$curline = spacefix($curline, $part, $lastchar, $soc, $eoc, $ilc, $ilc_b);
					if (!$parserState->{seenBraces}) { # TREEDONE
						$treeNest = 1;
					if ($treeDebug) { print STDERR "TS TREENEST -> 1 [13]\n"; }

						if (!$treePopOnNewLine) {
							$treePopOnNewLine = 1;
						} else {
							$treePopOnNewLine = 0 - $treePopOnNewLine;
						}
						print STDERR "treePopOnNewLine -> $treePopOnNewLine\n" if ($ilcDebug);

						# $treeCur->addSibling($part, 0); $treeSkip = 1;
						# $treePopOnNewLine = 1;
						# $treeCur = pop(@treeStack) || $treeTop;
						# bless($treeCur, "HeaderDoc::ParseTree");
					}
				} elsif ($parserState->{inComment}) {
					my $linenum = $inputCounter + $fileoffset;
					if (!$cpp_in_argparse) {
						# We've already seen these.
						if ($nestedcommentwarn) {
							warn("$fullpath:$linenum: warning: Nested comment found [1].  Ignoring.\n");
						}
						# This isn't really a problem.
						# Don't warn to avoid bogus
						# warnings for apple_ref and
						# URL markup in comments.
					}
					# warn("XX $cpp_in_argparse XX $inputCounter XX $fileoffset XX\n");
				}
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				last SWITCH;
			};

		# Standard comment handlers: soc = start of comment,
		# eoc = end of comment.

		($part eq $soc) && do {
			print STDERR "START OF MULTILINE COMMENT: CASE 26\n" if ($liteDebug);
				print STDERR "SOC\n" if ($localDebug);

				if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} || $parserState->{inString})) {
					$parserState->{inComment} = 4; 
					$curline = spacefix($curline, $part, $lastchar);
					if (!$parserState->{seenBraces}) {
						$treeNest = 1;
					if ($treeDebug) { print STDERR "TS TREENEST -> 1 [14]\n"; }
						# print STDERR "TSPUSH\n" if ($tsDebug || $treeDebug);
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
						warn("$fullpath:$linenum: warning: Nested comment found [2].  Ignoring.\n");
					}
				}
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				last SWITCH;
			};
		($part eq $eoc) && do {
			print STDERR "END OF MULTILINE COMMENT: CASE 27\n" if ($liteDebug);
				print STDERR "EOC\n" if ($localDebug);

				if ($parserState->{inComment} && !($parserState->{inInlineComment} || $parserState->{inChar} || $parserState->{inString})) {
					$parserState->{inComment} = 0;
					$parserState->{leavingComment} = 1;
					$curline = spacefix($curline, $part, $lastchar);
					$ppSkipOneToken = 1;
					if (!$parserState->{seenBraces}) {
                                        	$treeCur->addSibling($part, 0); $treeSkip = 1;
                                        	$treeCur = pop(@treeStack) || $treeTop;
						$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
						$treeCur = $treeCur->lastSibling();
						$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
                                        	print STDERR "TSPOP [6]\n" if ($tsDebug || $treeDebug);
                                        	bless($treeCur, "HeaderDoc::ParseTree");
					}
				} elsif (!$parserState->{inComment} && !$parserState->{inInlineComment} && !$parserState->{inString} && !$parserState->{inChar} && !$inRegexp) {
					my $linenum = $inputCounter + $fileoffset;
					warn("$fullpath:$linenum: warning: Unmatched close comment tag found.  Ignoring.\n");
				} elsif ($parserState->{inInlineComment}) {
					my $linenum = $inputCounter + $fileoffset;
					# We'll leave this one on for now.
					if ((1 || $nestedcommentwarn) && (!$HeaderDoc::test_mode)) {
						warn("$fullpath:$linenum: warning: Nested comment found [3].  Ignoring.\n");
					}
				}
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				last SWITCH;
			};

		# Parenthesis and brace handlers.

		($part eq "(") && do {
			print STDERR "OPEN PAREN: CASE 28\n" if ($liteDebug);
			# if ($liteDebug) { $localDebug = 1; $parseDebug = 1; }
			    my @tempppl = undef;
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && !($inRegexp && $regexpNoInterpolate)) {
			      if ((!$parserState->isQuoted($lang, $sublang)) && (!$inRegexpCharClass)) {
				my $oldParsedParamParse = $parserState->{parsedParamParse};
			        if (!(scalar(@braceStack)-$parserState->{initbsCount})) {
					# start parameter parsing after this token
					# print STDERR "SUBLANG: \"$sublang\"\n";
					# print STDERR "SODCLASS: \"".$parserState->{sodclass}."\"\n";
					if ($parserState->{occmethod} && !$parserState->{occmethodreturntype}) {
						$parserState->{gatheringObjCReturnType}++;
					} elsif ($pascal && $parserState->{sodclass} eq "function") {
						# Pascal fuction parameters are semicolon-delimited.
						$parserState->{parsedParamParse} = 2;
					} elsif ($sublang eq "MIG") {
						# MIG fuction parameters are semicolon-delimited.
						$parserState->{parsedParamParse} = 2;
					} else {
						$parserState->{parsedParamParse} = 4;
					}
					print STDERR "parsedParamParse -> $parserState->{parsedParamParse}"."[lparen]\n" if ($parmDebug);
					print STDERR "parsedParamList wiped\n" if ($parmDebug);
					@tempppl = @{$parserState->{parsedParamList}};
					@{$parserState->{parsedParamList}} = ();
					$parsedParam = "";
			        }
				$parserState->{onlyComments} = 0;
				print STDERR "[i]onlyComments -> 0\n" if ($macroDebug);
				if ($parserState->{simpleTypedef} && !(scalar(@braceStack)- $parserState->{initbsCount})) {
					$parserState->{simpleTypedef} = 0;
					$parserState->{simpleTDcontents} = "";
					print STDERR "Setting typedef sodname to ".$parserState->{lastsymbol}."\n" if ($localDebug || $sodDebug);
					$parserState->{sodname} = $parserState->{lastsymbol};
					$parserState->{sodclass} = "function";
					print STDERR "sodclass -> function (implicit)[4]\n" if ($sodDebug);

					# DAG: changed to respect freezereturn
					# and hollow, but in the unlikely event
					# that we should start seeing any weird
					# "missing return type info" bugs,
					# this next line might need to be
					# put back in rather than the lines
					# that follow it.

					# $parserState->{returntype} = "$declaration$curline";

					if (!$parserState->{freezereturn} && $parserState->{hollow} && !$parserState->{inComment} && !$parserState->{leavingComment}) {
						$parserState->{returntype} = "$declaration$curline";
						print STDERR "APPENDING TO RETURNTYPE: NOW \"$parserState->{returntype}\".\n" if ($retDebug);
 	    				} elsif (!$parserState->{freezereturn} && !$parserState->{hollow} && !$parserState->{inComment} && !$parserState->{leavingComment}) {
						$parserState->{returntype} = "$curline";
						print STDERR "REPLACING RETURNTYPE: NOW \"$parserState->{returntype}\".\n" if ($retDebug);
						$declaration = "";
					}
				}
				$parserState->{posstypesPending} = 0;
				if ($parserState->{callbackNamePending} == 2) {
					$parserState->{callbackNamePending} = 3;
					print STDERR "callbackNamePending -> 3\n" if ($localDebug || $cbnDebug);
				}
				print STDERR "lparen\n" if ($localDebug);
				print STDERR "WFTI: ".$parserState->{waitingForTypeInformation}."\n" if ($localDebug || $parseDebug);
			        if ($pascal && ($parserState->{waitingForTypeInformation} == 1) && ((scalar(@braceStack)-$parserState->{initbsCount}) == 0)) {
					print STDERR "Setting sodclass to 'enum'." if ($sodDebug || $localDebug);
					$parserState->{waitingForTypeInformation} = 3;
					$parserState->{sodclass} = "enum";
					print STDERR "sodclass -> enum (implicit)[5]\n" if ($sodDebug);
				}
			        if ($parserState->{cbsodname} && (scalar(@braceStack)-$parserState->{initbsCount}) == 0) {

					if (!$parserState->{functionReturnsCallback}) {
						# At the top level, if we see a second open parenthesis after setting a callback
						# name, the first token in the first set of open parentheses is the name of
						# the callback, so clear cbsodname.
						# 
						# Until this point, the value in cbsodname was a copy of the already-cleared
						# sodname field, and would replace the callbackName field at the end of
						# processing.

						$parserState->{cbsodname} = "";
					} else {
						# If we are in a function that returns a callback, everything from here on
						# is a list of parameters for the callback, not the function, so the
						# previous parameter list should be discarded (though it is useful to
						# add these parameters as valid things to comment about)

						@{$parserState->{parsedParamList}} = @tempppl;
						$parserState->{functionReturnsCallback}--;
						print STDERR "parsedParamList restored\n" if ($parmDebug);
					}
				}
			        if ((scalar(@braceStack)-$parserState->{initbsCount}) == 1) {
					if ($parserState->{callbackName}) {
						$parserState->{cbsodname} = $parserState->{callbackName};
						$parserState->{sodclass} = "function";
						print STDERR "sodclass -> function (implicit)[6]\n" if ($sodDebug);
						# $parserState->{callbackName} = "";
						$parserState->{functionReturnsCallback}++;
						print STDERR "Function returning callback.  NAME: $parserState->{cbsodname}\n" if ($parmDebug || $localDebug || $parseDebug);
						print STDERR "parsedParamParse -> 4[callback]\n" if ($parmDebug);
						$parserState->{parsedParamParse} = 4;
						print STDERR "parsedParamList wiped\n" if ($parmDebug);
						@tempppl = @{$parserState->{parsedParamList}};
						@{$parserState->{parsedParamList}} = ();
						$parsedParam = "";
					}
				}

				if ($parserState->{inOperator} == 1) {
					$parserState->{inOperator} = 2;
				}
				push(@parsedParamParseStack, $oldParsedParamParse);
				push(@braceStack, $part); pbs(@braceStack);
				print STDERR "PUSHED $part ONTO BRACESTACK\n" if ($macroDebug || $braceDebug);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeNest = 1;
					if ($treeDebug) { print STDERR "TS TREENEST -> 1 [15]\n"; }
					# push(@treeStack, $treeCur);
					# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
					# bless($treeCur, "HeaderDoc::ParseTree");
				}
				$curline = spacefix($curline, $part, $lastchar);

				print STDERR "LASTCHARCHECK: \"$lastchar\" \"$lastnspart\" \"$curline\".\n" if ($localDebug);
				if ($lastnspart eq ")") {  # || $curline =~ /\)\s*$/so
print STDERR "HERE: DEC IS $declaration\nENDDEC\nCURLINE IS $curline\nENDCURLINE\n" if ($localDebug);
				    # print STDERR "CALLBACKMAYBE: $parserState->{callbackNamePending} $parserState->{sodclass} ".scalar(@braceStack)."\n";
				    print STDERR "SBS: ".scalar(@braceStack)."\n" if ($localDebug);
				    ### if (!$parserState->{callbackNamePending} && ($parserState->{sodclass} eq "function") && ((scalar(@braceStack)-$parserState->{initbsCount}) == 1)) { #  && $argparse
					### # Guess it must be a callback anyway.
					### my $temp = pop(@tempppl);
					### $parserState->{callbackName} = $temp;
					### $parserState->{name} = "";
					### $parserState->{sodclass} = "";
					### $parserState->{sodname} = "";
					### print STDERR "CALLBACKHERE ($temp)!\n" if ($cbnDebug || $parseDebug);
				    ### }
				    if ($declaration =~ /.*\n(.*?)\n$/so) {
					my $lastline = $1;
print STDERR "LL: $lastline\nLLDEC: $declaration" if ($localDebug);
					$declaration =~ s/(.*)\n(.*?)\n$/$1\n/so;
					$curline = "$lastline $curline";
					$curline =~ s/^\s*//so;
					$prespace -= 4;
					$prespaceadjust += 4;
					
					$forcenobreak = 1;
print STDERR "NEWDEC: $declaration\nNEWCURLINE: $curline\n" if ($localDebug);
				    } elsif (length($declaration) && $callback_typedef_and_name_on_one_line) {
print STDERR "SCARYCASE\n" if ($localDebug);
					$declaration =~ s/\n$//so;
					$curline = "$declaration $curline";
					$declaration = "";
					$prespace -= 4;
					$prespaceadjust += 4;
					
					$forcenobreak = 1;
				    }
				} else { print STDERR "OPARENLC: \"$lastchar\"\nCURLINE IS: \"$curline\"\n" if ($localDebug);}

				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				if ($parserState->{startOfDec} == 2) {
					$parserState->{sodclass} = "function";
					print STDERR "sodclass -> function (implicit)[7]\n" if ($sodDebug);
					$parserState->{freezereturn} = 1;
					$parserState->{returntype} =~ s/^\s*//so;
					$parserState->{returntype} =~ s/\s*$//so;
				}
				$parserState->{startOfDec} = 0;
				print STDERR "startOfDec -> 0 [3]\n" if ($localDebug);
				if ($curline !~ /\S/o) {
					# This is the first symbol on the line.
					# adjust immediately
					$prespace += 4;
					print STDERR "PS: $prespace immediate\n" if ($localDebug);
				} else {
					$prespaceadjust += 4;
					print STDERR "PSA: $prespaceadjust\n" if ($localDebug);
				}
				if ($inRegexp) {
					print STDERR "PUSHING $part ONTO REGEXPSTACK\n" if ($localDebug || $parseDebug);
					push(@regexpStack, $part);
				}
			      }
			    }
			    print STDERR "OUTGOING CURLINE: \"$curline\"\n" if ($localDebug);
			    last SWITCH;
			};
		($part eq ")") && do {
			print STDERR "CLOSE PAREN: CASE 29\n" if ($liteDebug);
			    print STDERR "TOP OF RE STACK IS: \"".peek(\@regexpStack)."\"\n" if ($localDebug || $parseDebug);
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && !($inRegexp && $regexpNoInterpolate && (peek(\@regexpStack) ne "("))) {
			      print STDERR "PAST FIRST CHECK\n" if ($localDebug || $parseDebug);
			      if ((!$parserState->isQuoted($lang, $sublang)) && (!$inRegexpCharClass)) {
			        if ((scalar(@braceStack)-$parserState->{initbsCount} - $parserState->{functionReturnsCallback}) == 1) {
				    # stop parameter parsing
				    if ($parserState->{gatheringObjCReturnType}) {
					$parserState->{gatheringObjCReturnType}--;
				    	if ($parserState->{gatheringObjCReturnType} == 1) {
						$parserState->{gatheringObjCReturnType} = 0;
					}
				    }
				    if ($parserState->{parsedParamParse}) {
					$parserState->{parsedParamParse} = 0;
					print STDERR "parsedParamParse -> 0[rparen]\n" if ($parmDebug);
					$parsedParam =~ s/^\s*//so; # trim leading space
					$parsedParam =~ s/\s*$//so; # trim trailing space

					if ($parsedParam ne "void") {
						# ignore foo(void)
						push(@{$parserState->{parsedParamList}}, $parsedParam);
						print STDERR "pushed $parsedParam into parsedParamList [1]\n" if ($parmDebug);
					}
					$parsedParam = "";
				    }
			        }
				$parserState->{onlyComments} = 0;
				print STDERR "[j]onlyComments -> 0\n" if ($macroDebug);
				print STDERR "rparen\n" if ($localDebug);


				my $test = pop(@braceStack); pbs(@braceStack);
				$parserState->{parsedParamParse} = pop(@parsedParamParseStack);
				print STDERR "POPPED $test FROM BRACESTACK\n" if ($macroDebug || $braceDebug);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeCur->addSibling($part, 0); $treeSkip = 1;
					$treeCur = pop(@treeStack) || $treeTop;
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print STDERR "TSPOP [6a]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
				}
				if (!($test eq "(")) {		# ) brace hack for vi
					warn("$fullpath:$inputCounter: warning: Parentheses do not match.\nWe may have a problem.\n");
					warn("Declaration to date: $declaration$curline\n");
					# cluck("backtrace follows\n");
				}
				$curline = spacefix($curline, $part, $lastchar);
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				$parserState->{startOfDec} = 0;
				print STDERR "startOfDec -> 0 [4]\n" if ($localDebug);
				if ($curline !~ /\S/o) {
					# This is the first symbol on the line.
					# adjust immediately
					$prespace -= 4;
					print STDERR "PS: $prespace immediate\n" if ($localDebug);
				} else {
					$prespaceadjust -= 4;
					print STDERR "PSA: $prespaceadjust\n" if ($localDebug);
				}
				if ($inRegexp) {
					my $temp = pop(@regexpStack);
					print STDERR "popped $temp FROM REGEXPSTACK\n" if ($localDebug || $parseDebug);
					if ($temp ne "(") {
						warn("Parentheses do not match in regular expression.  We may have a problem.\n");
					}
				}
			      }
			    }
			    last SWITCH;
			};
		(casecmp($part, $lbrace, $case_sensitive)) && do {
			my $oldParsedParamParse = $parserState->{parsedParamParse};
			print STDERR "LEFT BRACE: CASE 30\n" if ($liteDebug);
			    if ($parserState->{onlyComments} && !$parserState->{inComment} && !$parserState->{inInlineComment} && !$parserState->{inChar} && !$inRegexp && !scalar(@parserStack) && !$parserState->{INIF} && !$tempInIf) {
				print STDERR "BAILING NOW\n" if ($externCDebug);
				$continue_no_return = 1;
				print STDERR "CONTINUE -> 0 [NORETURN]\n" if ($parseDebug || $cppDebug || $macroDebug || $localDebug || $continueDebug);
				$continue = 0;
			    }
			    if ($parserState->{onlyComments} && !$parserState->{inComment} && !$parserState->{inInlineComment} && !$parserState->{inChar} && !$inRegexp && scalar(@parserStack)) {
				# Somebody put in a brace in the middle of
				# a class or else we're seeing ObjC private
				# class bits.  Either way, throw away the
				# curly brace.

				print STDERR "NOINSERT\n" if ($parserStackDebug);

				$pushParserStateAtBrace = 1;
				# $setNoInsert = 1;
				$parserState->{noInsert} = 1;
			    }
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} || $inRegexp)) {
				$parserState->{bracePending} = 0;
				print STDERR "bracePending -> 0 [brace]\n" if ($localDebug);
				$parserState->{onlyComments} = 0;
				print STDERR "[k]onlyComments -> 0\n" if ($macroDebug);
				if (scalar(@{$parserState->{parsedParamList}})) {
					foreach my $node (@{$parserState->{parsedParamList}}) {
						$node =~ s/^\s*//so;
						$node =~ s/\s*$//so;
						if (length($node)) {
							push(@{$parserState->{pplStack}}, $node)
						}
					}
					@{$parserState->{parsedParamList}} = ();
					print STDERR "parsedParamList pushed\n" if ($parmDebug);
				}

				# start parameter parsing after this token
				if (!$pushParserStateAtBrace && !$parserState->{inClass}) {
					if ($parserState->{inEnum}) {
						$parserState->{parsedParamParse} = 4;
					} else {
						$parserState->{parsedParamParse} = 2;
					}
					print STDERR "parsedParamParse -> $parserState->{parsedParamParse}"."[lbrace]\n" if ($parmDebug);
				}

				# print STDERR "statecheck: ".$parserState->{inClass}."X".$parserState->{sodclass}."X".$parserState->{inOperator}."X".$parserState->{occmethod}."\n"; # @@@ CHECKME - Do this for Obj-C methods too?
				if (!$parserState->{inClass} && ($parserState->{sodclass} eq "function" || $parserState->{inOperator} || $parserState->{occmethod})) {
					# This is the opening brace of a function.  Start ignoring everything
					# until the matching brace is encountered.
					print STDERR "seenBraces -> 1\n" if ($parseDebug);
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
				print STDERR "callbackNamePending -> -1\n" if ($localDebug || $cbnDebug);
				print STDERR "lbrace\n" if ($localDebug);

				push(@parsedParamParseStack, $oldParsedParamParse);
				push(@braceStack, $part); pbs(@braceStack);
				print STDERR "PUSHED $part ONTO BRACESTACK\n" if ($macroDebug || $braceDebug);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeNest = 1;
					if ($treeDebug) { print STDERR "TS TREENEST -> 1 [16]\n"; }
					print STDERR "TN -> 1\n" if ($localDebug);
					# push(@treeStack, $treeCur);
					# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
					# bless($treeCur, "HeaderDoc::ParseTree");
				}
				$curline = spacefix($curline, $part, $lastchar);
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				if ($parserState->{INMODULE} == 2) {
					# Drop token on the floor.
					$treepart = " "; 
				}

				$parserState->{startOfDec} = 0;
				print STDERR "startOfDec -> 0 [5]\n" if ($localDebug);
				if ($curline !~ /\S/o) {
					# This is the first symbol on the line.
					# adjust immediately
					$prespace += 4;
					print STDERR "PS: $prespace immediate\n" if ($localDebug);
				} else {
					$prespaceadjust += 4;
					print STDERR "PSA: $prespaceadjust\n" if ($localDebug);
				}
			    }
			    last SWITCH;
			};
		(casecmp($part, $rbrace, $case_sensitive)) && do {
			print STDERR "RIGHT BRACE: CASE 31\n" if ($liteDebug);
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} || $inRegexp)) {

				my $oldOC = $parserState->{onlyComments};
				print STDERR "rbrace???\n" if ($localDebug);
				# $parserState->{onlyComments} = 0;	# If this is all we've seen, there's either a bug or we're
									# unrolling a class or similar anyway.
				print STDERR "[l]onlyComments -> 0\n" if ($macroDebug);

				my $bsCount = scalar(@braceStack);
				if (scalar(@parserStack) && !($bsCount - $parserState->{initbsCount})) {
print STDERR "parserState: ENDOFSTATE\n" if ($parserStackDebug);
					if ($parserState->{noInsert} || $oldOC) {
						print STDERR "parserState insertion skipped[RBRACE]\n" if ($parserStackDebug || $parserStateInsertDebug);
					} elsif ($parserState->{hollow}) {
						print STDERR "inserted parser state into tree [RBRACE]\n" if ($parserStateInsertDebug);
						my $treeRef = $parserState->{hollow};

						$parserState->{lastTreeNode} = $treeCur;
						$treeRef->addRawParsedParams(\@{$parserState->{parsedParamList}});
						$treeRef->parserState($parserState);
					} else {
						warn "Couldn't insert info into parse tree[1].\n";
					}

					print STDERR "parserState popped from parserStack[rbrace]\n" if ($parserStackDebug);

					# print STDERR "PREINMODULE: ".$parserState->{INMODULE}."\n";

					$parserState = pop(@parserStack) || $parserState;
					$declaration = $parserState->{storeDec}.$declaration;

					$HeaderDoc::module = $parserState->{MODULE};

					# print STDERR "INMODULE: ".$parserState->{INMODULE}."\n";

					if ($parserState->{INMODULE} == 2) {
						# Drop token on the floor.
						print STDERR "CURRENT: ".$treeCur->{TOKEN}."\n" if ($localDebug);
						$part = "";
						print STDERR "INMODULE -> 3\n" if ($localDebug || $moduleDebug);
						$parserState->{INMODULE} = 3;
						print STDERR "CONTINUE -> 0 [1a]\n" if ($parseDebug || $cppDebug || $macroDebug || $localDebug || $continueDebug);
						$parserState->{noInsert} = 0;
						$continue = 0;
						print STDERR "AT END: REALPS IS ".$parserState->{REALPS}."\n" if ($parserStackDebug || $localDebug);
						print STDERR "STACK COUNT: ".scalar(@parserStack)."\n" if ($parserStackDebug || $localDebug);
					}
					if ($lang eq "php" || ($lang eq "C" && $sublang eq "IDL") || ($lang eq "java" && $sublang eq "java")) {
							# print STDERR "PHP OUT OF BRACES?: ".scalar(@braceStack)."\n";
						if (scalar(@braceStack) == 1) {
							# PHP, IDL, and Java classes end at
							# the brace.
							print STDERR "CONTINUE -> 0 [1a]\n" if ($parseDebug || $cppDebug || $macroDebug || $localDebug || $continueDebug);
							$continue = 0;
						}
					}
					if ($parserState->{noInsert} && scalar(@parserStack)) {
						# This is to handle the end of
						# the private vars in an
						# Objective C class.
						print STDERR "parserState: Hit me.\n" if ($localDebug);
						$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
						$parserState->{skiptoken} = 1;
						$parserState->{lang} = $lang;
						$parserState->{inputCounter} = $inputCounter;
						# It's about to go down by 1.
						$parserState->{initbsCount} = scalar(@braceStack) - 1;
					}
					# $parserState->{onlyComments} = 1;
				} else {
					print STDERR "NO CHANGE IN PARSER STATE STACK (nPARSERSTACK = ".scalar(@parserStack).", $bsCount != $parserState->{initbsCount})\n" if ($parseDebug || $parserStackDebug);
				}

				if ((scalar(@braceStack)-$parserState->{initbsCount}) == 1) {
					# stop parameter parsing
					$parserState->{parsedParamParse} = 0;
					print STDERR "parsedParamParse -> 0[rbrace]\n" if ($parmDebug);
					$parsedParam =~ s/^\s*//so; # trim leading space
					$parsedParam =~ s/\s*$//so; # trim trailing space

					if (length($parsedParam)) {
						# ignore foo(void)
						push(@{$parserState->{parsedParamList}}, $parsedParam);
						print STDERR "pushed $parsedParam into parsedParamList [1b]\n" if ($parmDebug);
					}
					$parsedParam = "";
				} else {
					# start parameter parsing after this token
					# grabbing end names for typedefs.
					$parserState->{parsedParamParse} = 4;
					print STDERR "parsedParamParse -> $parserState->{parsedParamParse}"."[rbrace2]\n" if ($parmDebug);
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
					print STDERR "parsedParamList pushed\n" if ($parmDebug);
				}

				print STDERR "rbrace\n" if ($localDebug);

				my $test = pop(@braceStack); pbs(@braceStack);
				my $temp = pop(@parsedParamParseStack);
				if ($temp) {
					if ($temp == 1) { $temp = 2; }
					elsif ($temp == 3) { $temp = 4; }

					if ($temp != $parserState->{parsedParamParse} && $parmDebug) {
						warn("Changing PPP from ".$parserState->{parsedParamParse}." to $temp\n");
					}
					$parserState->{parsedParamParse} = $temp;
				}
				print STDERR "POPPED $test FROM BRACESTACK\n" if ($macroDebug || $braceDebug);
				if (!$parserState->{seenBraces}) { # TREEDONE
					$treeCur->addSibling($part, 0); $treeSkip = 1;
					$treeCur = pop(@treeStack) || $treeTop;
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print STDERR "TSPOP [7]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
				}
				if (!($test eq "$lbrace") && (!length($structname) || (!($test eq $structname) && $structisbrace))) {
					warn("$fullpath:$inputCounter: warning: Braces do not match (top of brace stack\nis \"$test\").We may have a problem.\n");
					warn("Declaration to date: $declaration$curline\n");
				}
				$curline = spacefix($curline, $part, $lastchar);
				$parserState->{lastsymbol} = "";
				$lastchar = $part;

				$parserState->{startOfDec} = 0;
				print STDERR "startOfDec -> 0 [6]\n" if ($localDebug);
				if ($curline !~ /\S/o) {
					# This is the first symbol on the line.
					# adjust immediately
					$prespace -= 4;
					print STDERR "PS: $prespace immediate\n" if ($localDebug);
				} else {
					$prespaceadjust -= 4;
					print STDERR "PSA: $prespaceadjust\n" if ($localDebug);
				}
			    }
			    last SWITCH;
			};

		# Typedef, struct, enum, and union handlers.

		# Merge the '@' symbol onto @protocol, @property, @public, and similar.
		(length($part) && length($nextpart) && ((length($propname) && $propname =~ /\@/) || length($objcdynamicname) || length($objcsynthesizename) || length($classregexp) || (length($accessregexp) && $accessregexp =~ /\@/)) && $part =~ /^\@$/ && !$parserState->{inComment} && !$parserState->{inChar} && !$parserState->{inString} && !$parserState->{inInlineComment}) && do {
			print STDERR "PROPERTY PREPEND AT (\@): CASE 32\n" if ($liteDebug);
				my $temp = "\@".$nextpart;
				# print STDERR "TEMP IS $temp PROPNAME is $propname\n";
				if ($temp =~ /$accessregexp/) {
					print STDERR "MERGE $part $nextpart\n" if ($localDebug);
					$nextpart = "\@".$nextpart;
					$parserState->{classIsObjC} = 1;
				} elsif ($temp =~ /$classregexp/) {
					$nextpart = "\@".$nextpart;
					$parserState->{classIsObjC} = 1;
				} elsif ($temp =~ /$classclosebraceregexp/) {
					$nextpart = "\@".$nextpart;
				} elsif ($temp eq $propname) {
					# This shows up in a declaration, so delete the token
					$part = "";
					print STDERR "MERGE $part $nextpart\n" if ($localDebug);
					$nextpart = "\@".$nextpart;
				} elsif (length($requiredregexp) && $temp =~ /$requiredregexp/) {
					# This shows up in a declaration, so delete the token
					$part = "";
					print STDERR "MERGE $part $nextpart\n" if ($localDebug);
					$nextpart = "\@".$nextpart;
				} elsif ($temp eq $objcdynamicname) {
					# This shows up in a declaration, so delete the token
					$part = "";
					print STDERR "MERGE $part $nextpart\n" if ($localDebug);
					$nextpart = "\@".$nextpart;
				} elsif ($temp eq $objcsynthesizename) {
					# This shows up in a declaration, so delete the token
					$part = "";
					print STDERR "MERGE $part $nextpart\n" if ($localDebug);
					$nextpart = "\@".$nextpart;
				}
				next SWITCH;
			};
		($modules_are_special && !$parserState->{inTemplate} && !$parserState->{inComment} && !$parserState->{inInlineComment} && !$parserState->{inString} && !$parserState->{inChar} && length($moduleregexp) && $part =~ /$moduleregexp/) && do {
			print STDERR "INMODULE -> 1\n" if ($localDebug || $moduleDebug);
			$parserState->{INMODULE} = 1;
			print STDERR "MODULE START TOKEN: CASE 32-M-1\n" if ($localDebug || $liteDebug);
		};

		(length($classclosebraceregexp) && ($part =~ /$classclosebraceregexp/) && !$parserState->{inComment} && !$parserState->{inChar} && !$parserState->{inString} && !$parserState->{inInlineComment}) && do {
			print STDERR "CLASS CLOSE BRACE: CASE 33\n" if ($liteDebug);
				if ($part ne peekmatch(\@braceStack, $fullpath, $inputCounter)) {
					warn("$fullpath:inputCounter: warning: Class braces do not match.\nWe may have a problem.\n");
				}
				$parserState->{seenBraces} = 1;
				my $temp = pop(@braceStack);
				$parserState->{parsedParamParse} = pop(@parsedParamParseStack);
				print STDERR "POPPED $temp FROM BRACESTACK\n" if ($macroDebug || $braceDebug);
				$treeCur->addSibling($part, 0); $treeSkip = 1;
				$treeCur = pop(@treeStack) || $treeTop;
				$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
				$treeCur = $treeCur->lastSibling();
				$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
				print STDERR "TSPOP [6]\n" if ($tsDebug || $treeDebug);
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
						$declaration = $parserState->{storeDec}.$declaration;
						$HeaderDoc::module = $parserState->{MODULE};

						$continue = 0;
						print STDERR "CONTINUE -> 0 [occend]\n" if ($parseDebug || $cppDebug || $macroDebug || $localDebug || $continueDebug);
					} else {
					    if (!$parserState->{onlyComments}) {
						# Process entry here
						if ($parserState->{noInsert}) {
							print STDERR "parserState insertion skipped[\@end]\n" if ($parserStackDebug);
						} elsif ($parserState->{hollow}) {
							print STDERR "inserted parser state into tree [\@end]\n" if ($parserStateInsertDebug);
							my $treeRef = $parserState->{hollow};
							$parserState->{lastTreeNode} = $treeCur;

							$treeRef->addRawParsedParams(\@{$parserState->{parsedParamList}});
							$treeRef->parserState($parserState);
						} else {
							warn "Couldn't insert info into parse tree[2].\n";
						}

						print STDERR "parserState: Created parser state[1].\n" if ($parserStackDebug);
						print STDERR "CURLINE CLEAR[PRS2]\n" if ($localDebug);
						$curline = "";
						$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
						$parserState->{skiptoken} = 1;
						$parserState->{lang} = $lang;
						$parserState->{inputCounter} = $inputCounter;
						$parserState->{initbsCount} = scalar(@braceStack);
					    }
					    print STDERR "parserState popped from parserStack[\@end]\n" if ($parserStackDebug);
					    $parserState = pop(@parserStack) || $parserState;
					    $declaration = $parserState->{storeDec}.$declaration;
					    $HeaderDoc::module = $parserState->{MODULE};
					}
				}
				# fall through to next case.  WHY???
			};
		(!$parserState->{inTemplate} && !$parserState->{inComment} && !$parserState->{inInlineComment} && !$parserState->{inString} && !$parserState->{inChar} && length($classregexp) && $part =~ /$classregexp/) && do {
			print STDERR "START OF CLASS: CASE 34\n" if ($liteDebug);
			### if ($parserState->{classIsObjC}) { $sublang = "occ"; }
			### else { $sublang = "cpp"; }
			### print STDERR "LANG $lang SUBLANG $sublang\n" if ($localDebug || $parseDebug || $classDebug);
			### # Update the class regular expressions because our language has changed.
			### ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
				### $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
				### $enumname,
				### $typedefname, $varname, $constname, $structisbrace, $macronameref,
				### $classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
				### $requiredregexp, $propname, $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename) = parseTokens($lang, $sublang);
			### print STDERR "PROPNAME NOW $propname\n" if ($localDebug || $parseDebug || $classDebug);
			my $localclasstype = $1;
			if ($part =~ /^\@/) { $part =~ s/^\@//s; }
			if (!(scalar(@braceStack)-$parserState->{initbsCount})) {
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				print STDERR "ITISACLASS\n" if ($localDebug);
				if (!length($parserState->{sodclass}) && !$parserState->{inTypedef}) {
					print STDERR "GOOD.\n" if ($localDebug);
					$parserState->{inClass} = 1;
					print STDERR "inClass -> 1 [7]\n" if ($classDebug);
					$pushParserStateAtBrace = 1;
					if ($localclasstype =~ /\@interface/) {
						$parserState->{inClass} = 2;
						print STDERR "inClass -> 2 [8]\n" if ($classDebug);
						$pushParserStateAtBrace = 0;
					} elsif ($localclasstype =~ /\@protocol/) {
						$pushParserStateAtBrace = 0;
						$pushParserStateAfterWordToken = 0;
						$parserState->{inClass} = 0;
						print STDERR "inClass -> 0 [9]\n" if ($classDebug);
						$parserState->{inProtocol} = 1;
					} elsif ($localclasstype =~ /\@implementation/) {
						$pushParserStateAtBrace = 0;
						$pushParserStateAfterWordToken = 2;
					}
			    		$parserState->{sodclass} = "class";
					print STDERR "sodclass -> class (explicit)[8]\n" if ($sodDebug);
					$parserState->{classtype} = $localclasstype;
					$parserState->{preclasssodtype} = $parserState->{sodtype} . $part;
					$parserState->{sodtype} = "";
			    		$parserState->{startOfDec} = 1;
					$parserState->{sodtypeclasstoken} = $part;
					print STDERR "startOfDec -> 1 [7]\n" if ($localDebug);

					$parserState->{onlyComments} = 0;
					print STDERR "[m]onlyComments -> 0\n" if ($macroDebug);
					$continuation = 1;
					# Get the parse tokens from Utilities.pm.

					if (length($classbraceregexp) && ($localclasstype =~ /$classbraceregexp/)) {
						print STDERR "CLASS ($localclasstype) IS A BRACE.\n" if ($localDebug);
						push(@parsedParamParseStack, $parserState->{parsedParamParse});
						push(@braceStack, $localclasstype); pbs(@braceStack);
						print STDERR "PUSHED $localclasstype ONTO BRACESTACK\n" if ($macroDebug || $braceDebug);
						$treeNest = 1;
					if ($treeDebug) { print STDERR "TS TREENEST -> 1 [17]\n"; }
					# } else {
						# print STDERR "CBRE: \"$classbraceregexp\"\n";
					}


					($lang, $sublang) = getLangAndSublangFromClassType($localclasstype);
					$HeaderDoc::lang = $lang;
					$HeaderDoc::sublang = $sublang;

					($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
						$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
						$enumname,
						$typedefname, $varname, $constname, $structisbrace, $macronameref,
						$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
						$requiredregexp, $propname, $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename) = parseTokens($lang, $sublang);
					my $macrore = macroRegexpFromList($macronameref);
# print STDERR "PROPNAME2: $propname\n";
					print STDERR "ARP: $accessregexp\n" if ($localDebug);


			    		last SWITCH;
				} else {
					($lang, $sublang) = getLangAndSublangFromClassType($localclasstype);
					$HeaderDoc::lang = $lang;
					$HeaderDoc::sublang = $sublang;

					($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
						$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
						$enumname,
						$typedefname, $varname, $constname, $structisbrace, $macronameref,
						$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
						$requiredregexp, $propname, $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename) = parseTokens($lang, $sublang);
					my $macrore = macroRegexpFromList($macronameref);
# print STDERR "PROPNAME2: $propname\n";
					print STDERR "ARP: $accessregexp\n" if ($localDebug);


			    		last SWITCH;
				}
			    }
			}
		};

		($part eq $objcdynamicname) && do {
			print STDERR "PROPERTY: CASE 35\n" if ($liteDebug);
			print STDERR "PROPERTY FOUND\n" if ($localDebug);
			$parserState->{isProperty} = 1; # Basically treat it like a normal variable, but flag it.
			last SWITCH;
		};
		($part eq $objcsynthesizename) && do {
			print STDERR "PROPERTY: CASE 35\n" if ($liteDebug);
			print STDERR "PROPERTY FOUND\n" if ($localDebug);
			$parserState->{isProperty} = 1; # Basically treat it like a normal variable, but flag it.
			last SWITCH;
		};
		($part eq $propname) && do {
			print STDERR "PROPERTY: CASE 35\n" if ($liteDebug);
			print STDERR "PROPERTY FOUND\n" if ($localDebug);
			$parserState->{isProperty} = 1; # Basically treat it like a normal variable, but flag it.
			last SWITCH;
		};
		($part eq $structname || $part eq $enumname || $part eq $unionname) && do {
			print STDERR "STRUCT/ENUM/UNION: CASE 36\n" if ($liteDebug);
			    if ((!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) &&
				(!(scalar(@braceStack)-$parserState->{initbsCount}))) {

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
                                	print STDERR "callbackNamePending -> -1\n" if ($localDebug || $cbnDebug);
                                	print STDERR "lbrace\n" if ($localDebug);

					push(@parsedParamParseStack, $parserState->{parsedParamParse});
                                	push(@braceStack, $part); pbs(@braceStack);
					print STDERR "PUSHED $part ONTO BRACESTACK\n" if ($macroDebug || $braceDebug);
					if (!$parserState->{seenBraces}) { # TREEDONE
						$treeNest = 1;
					if ($treeDebug) { print STDERR "TS TREENEST -> 1 [18]\n"; }
						# push(@treeStack, $treeCur);
						# $treeCur = $treeCur->addChild($part, 0); $treeSkip = 1;
						# bless($treeCur, "HeaderDoc::ParseTree");
					}
                                	$curline = spacefix($curline, $part, $lastchar);
                                	$parserState->{lastsymbol} = "";
                                	$lastchar = $part;

                                	$parserState->{startOfDec} = 0;
					print STDERR "startOfDec -> 0 [8]\n" if ($localDebug);
                                	if ($curline !~ /\S/o) {
                                        	# This is the first symbol on the line.
                                        	# adjust immediately
                                        	$prespace += 4;
                                        	print STDERR "PS: $prespace immediate\n" if ($localDebug);
                                	} else {
                                        	$prespaceadjust += 4;
                                        	print STDERR "PSA: $prespaceadjust\n" if ($localDebug);
                                	}
				} else {
					if (!$parserState->{simpleTypedef}) {
						print STDERR "simpleTypedef -> 2\n" if ($localDebug);
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
				if ($part eq $enumname) {
					$parserState->{inEnum} = 1;
					$parserState->{inUnion} = 0;
				} elsif ($part eq $unionname) {
					$parserState->{inEnum} = 0;
					$parserState->{inUnion} = 1;
				} else {
					$parserState->{inEnum} = 0;
					$parserState->{inUnion} = 0;
				}
				$parserState->{onlyComments} = 0;
				print STDERR "[n]onlyComments -> 0\n" if ($macroDebug);
				$continuation = 1;
				# $parserState->{simpleTypedef} = 0;
				if ($parserState->{basetype} eq "") { $parserState->{basetype} = $part; }
				# fall through to default case when we're done.
				if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inString} || $parserState->{inChar})) {
					$parserState->{namePending} = 2;
					print STDERR "namePending -> 2 [2]\n" if ($parseDebug);
					if ($parserState->{posstypesPending}) { $parserState->{posstypes} .=" $part"; }
				}
				if ($parserState->{sodclass} eq "") {
					$parserState->{startOfDec} = 0; $parserState->{sodname} = "";
					print STDERR "startOfDec -> 0 [9]\n" if ($localDebug);
print STDERR "sodname cleared (seu)\n" if ($sodDebug);
				}
				$lastchar = $part;
			    }; # end if
			}; # end do
		($part =~ /^$typedefname$/) && do {
			print STDERR "TYPEDEF: CASE 37\n" if ($liteDebug);
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				if (!(scalar(@braceStack)-$parserState->{initbsCount})) { $parserState->{callbackIsTypedef} = 1; $parserState->{inTypedef} = 1; }
				$parserState->{onlyComments} = 0;
				print STDERR "[o]onlyComments -> 0\n" if ($macroDebug);
				$continuation = 1;
				$parserState->{simpleTypedef} = 1; print STDERR "simpleTypedef -> 1\n" if ($localDebug);
				# previous case falls through, so be explicit.
				if ($part =~ /^$typedefname$/) {
				    if (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inString} || $parserState->{inChar})) {
					if ($pascal) {
					    $parserState->{namePending} = 2;
					    $inPType = 1;
					    print STDERR "namePending -> 2 [3]\n" if ($parseDebug);
					}
					if ($parserState->{posstypesPending}) { $parserState->{posstypes} .=" $part"; }
					if (!($parserState->{callbackNamePending})) {
						print STDERR "callbackNamePending -> 1\n" if ($localDebug || $cbnDebug);
						$parserState->{callbackNamePending} = 1;
					}
				    }
				}
				if ($parserState->{sodclass} eq "") {
					$parserState->{startOfDec} = 0; $parserState->{sodname} = "";
					print STDERR "startOfDec -> 0 [10]\n" if ($localDebug);
print STDERR "sodname cleared ($typedefname)\n" if ($sodDebug);
				}
				$lastchar = $part;
			    }; # end if
			}; # end do

		# C++ operator keyword handler

		($part eq "$operator") && do {
			print STDERR "OPERATOR KEYWORD: CASE 38\n" if ($liteDebug);
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				$parserState->{inOperator} = 1;
				$parserState->{sodname} = "";
				$parserState->{sodtype} = $parserState->{returntype};
			    }
			    $parserState->{lastsymbol} = $part;
			    $lastchar = $part;
			    last SWITCH;
			    # next;
			};

		# Punctuation handlers

		($part =~ /;/o) && do {
			print STDERR "SEMICOLON: CASE 39\n" if ($liteDebug);
			    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
				print STDERR "NOT IN STRING, CHAR, COMMENT, etc.\n" if ($localDebug);
				if ($parserState->{parsedParamParse}) {
					print STDERR "PPP IS $parserState->{parsedParamParse}.\n" if ($localDebug);
					$parsedParam =~ s/^\s*//so; # trim leading space
					$parsedParam =~ s/\s*$//so; # trim trailing space
					if (length($parsedParam)) { push(@{$parserState->{parsedParamList}}, $parsedParam); }
					print STDERR "pushed $parsedParam into parsedParamList [2semi]\n" if ($parmDebug);
					$parsedParam = "";
				}
				# skip this token
				print STDERR "PPP AT SEMI: $parserState->{parsedParamParse}\n" if ($parseDebug || $localDebug || $parmDebug);
				if ($parserState->{parsedParamParse} <= 2) {
					$parserState->{parsedParamParse} = 2;
					print STDERR "parsedParamParse -> 2[semi]\n" if ($parmDebug);
					$parserState->{freezereturn} = 1;
					# $parserState->{onlyComments} = 0;	# If this is all we've seen, there's either a bug or we're
										# unrolling a class or similar anyway.
					$parserState->{temponlyComments} = $parserState->{onlyComments};
					print STDERR "[p]onlyComments -> 0\n" if ($macroDebug);
					print STDERR "valuepending -> 0\n" if ($valueDebug);
					$parserState->{valuepending} = 0;
				} else {
					if ($parmDebug) {
						warn "WARNING: PPP AT SEMI NOT <= 2!\n";
					}
				}
				$continuation = 1;
				if ($parserState->{occmethod}) {
					$prespaceadjust = -$prespace;
				}
				# previous case falls through, so be explicit.
				if ($part =~ /;/o && !$parserState->{inMacroLine} && !$parserState->{inMacro}) {
				    my $bsCount = scalar(@braceStack)-$parserState->{initbsCount};
				    if (!$bsCount && !$parserState->{kr_c_function}) {
					if ($parserState->{startOfDec} == 2 && !$pascal) {
						$parserState->{sodclass} = "variable";
						print STDERR "sodclass -> variable (implicit)[9]\n" if ($sodDebug);
						$parserState->{startOfDec} = 1;
						print STDERR "startOfDec -> 1 [11]\n" if ($localDebug);

					} elsif (!($parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} || $parserState->{inString})) {
						$parserState->{startOfDec} = 1;
						print STDERR "startOfDec -> 1 [12]\n" if ($localDebug);

					}
					# $parserState->{lastsymbol} .= $part;
				    }
				    if (!$bsCount) {
					$treeCur = $treeCur->addSibling(";"); $treepart = " "; # $treeSkip = 1;
if (0) {
					$treeCur = pop(@treeStack) || $treeTop;
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print STDERR "TSPOP [8]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
}
					# $parserState->{lastTreeNode} = $treeCur;
					# print STDERR "LASTTREENODE -> $treeCur (".$treeCur->token().")\n";
					while ($parserState->{treePopTwo}--) {
						$treeCur = pop(@treeStack) || $treeTop;
						$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
						$treeCur = $treeCur->lastSibling();
						$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
						print STDERR "TSPOP [9]\n" if ($tsDebug || $treeDebug);
						bless($treeCur, "HeaderDoc::ParseTree");
					}
					$parserState->{treePopTwo} = 0;
				    }
				}
				$lastchar = $part;
			    }; # end if
			}; # end do
		($part eq "=" && ($parserState->{lastsymbol} ne "operator") && (!(($parserState->{inOperator} == 1) && $parserState->{lastsymbol} =~ /\W/ && $parserState->{lastsymbol} =~ /\S/)) && !($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) && do {
			print STDERR "EQUALS: CASE 40\n" if ($liteDebug);
				$parserState->{onlyComments} = 0;
				print STDERR "[q]onlyComments -> 0\n" if ($macroDebug);
				if ($part =~ /=/o && !(scalar(@braceStack)-$parserState->{initbsCount}) &&
				    $nextpart !~ /=/o && $lastchar !~ /=/o &&
				    $parserState->{sodclass} ne "function" && !$inPType) {
					print STDERR "valuepending -> 1\n" if ($valueDebug);
					$parserState->{valuepending} = 1;
					$parserState->{preEqualsSymbol} = $parserState->{lastsymbol};
					$parserState->{sodclass} = "variable";
					print STDERR "sodclass -> variable (implicit)[10]\n" if ($sodDebug);
					$parserState->{startOfDec} = 0;
					print STDERR "startOfDec -> 0 [13]\n" if ($localDebug);
				}; # end if
			}; # end do
		($part =~ /,/o) && do {
			print STDERR "COMMA: CASE 41\n" if ($liteDebug);
				if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
					$parserState->{onlyComments} = 0;
					print STDERR "[r]onlyComments -> 0\n" if ($macroDebug);
				}
				print STDERR "PPP AT COMMA: $parserState->{parsedParamParse}\n" if ($parseDebug || $localDebug || $parmDebug);
				if ($part =~ /,/o && ($parserState->{parsedParamParse} == 3 || $parserState->{parsedParamParse} == 4) && !($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) && ((scalar(@braceStack)-$parserState->{initbsCount}-$parserState->{functionReturnsCallback}) == 1) && (peek(\@braceStack) eq '(' || peek(\@braceStack) eq '{')) {
					print STDERR "$part is a comma\n" if ($localDebug || $parseDebug);
					$parsedParam =~ s/^\s*//so; # trim leading space
					$parsedParam =~ s/\s*$//so; # trim trailing space
					if (length($parsedParam)) { push(@{$parserState->{parsedParamList}}, $parsedParam); }
					print STDERR "pushed $parsedParam into parsedParamList [2]\n" if ($parmDebug);
					$parsedParam = "";
					# skip this token
					$parserState->{parsedParamParse} = 4;
					print STDERR "parsedParamParse -> 4[comma]\n" if ($parmDebug);
				} elsif ($parserState->{parsedParamParse}) {
					if ($parmDebug) {
						warn "WARNING: PPP AT COMMA NOT 3 or 4 (or maybe in string, comment, etc.)!\n";
					}
				}; # end if
			}; # end do
		($part =~ /[*^]/) && do {
				if ($lastnspart eq "(" &&  # ")"
					!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) &&
					!$parserState->{callbackNamePending} &&
					((scalar(@braceStack)-$parserState->{initbsCount}) == 1)) {
						# print STDERR "CBNP\n";
						$parserState->{callbackNamePending} = 3;
				}
				# Fall through to the default case.
			}; # end star/asterisk/caret case
		{ # SWITCH default case

		    print STDERR "DEFAULT CASE: CASE 42\n" if ($liteDebug);
		    # Handler for all other text (data types, string contents,
		    # comment contents, character contents, etc.)

		    print STDERR "DEFAULT CASE\n" if ($localDebug || $parseDebug);

	# print STDERR "TEST CURLINE IS \"$curline\".\n";
		    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar})) {
		      if (!ignore($part, $ignoreref, $perheaderignoreref)) {
			if ($part =~ /\S/o) {
				$parserState->{onlyComments} = 0;
				print STDERR "[s]onlyComments -> 0\n" if ($macroDebug);
			}
			if (!$continuation && !$occspace) {
				$curline = spacefix($curline, $part, $lastchar);
			} else {
				$continuation = 0;
				$occspace = 0;
			}
	# print STDERR "BAD CURLINE IS \"$curline\".\n";
			if (length($part) && !($parserState->{inComment} || $parserState->{inInlineComment})) {
				if ($localDebug && $lastchar eq ")") {print STDERR "LC: $lastchar\nPART: $part\n";}
	# print STDERR "XXX LC: $lastchar SC: $parserState->{sodclass} LG: $lang\n";
				if ($lastchar eq ")" && $parserState->{sodclass} eq "function" && ($lang eq "C" || $lang eq "Csource") && !(scalar(@braceStack)-$parserState->{initbsCount})) {
					if ($part !~ /^\s*;/o) {
						# warn "K&R C FUNCTION FOUND.\n";
						# warn "NAME: $parserState->{sodname}\n";
						if (!isKeyword($part, $keywordhashref, $case_sensitive)) {
							my $tempavail = ignore($part, $ignoreref, $perheaderignoreref);
							if (!$tempavail) {
								print STDERR "K&R C FUNCTION FOUND [2].\n" if ($localDebug);
								print STDERR "TOKEN: \"$part\"\n" if ($localDebug);
								print STDERR "TA: \"$tempavail\"\n" if ($localDebug);
								$parserState->{kr_c_function} = 1;
								$parserState->{kr_c_name} = $parserState->{sodname};
								$parserState->{parsedParamParse} = 1;
								print STDERR "parsedParamParse -> 1[default1]\n" if ($parmDebug);
							}
						}
					}
				}
				$lastchar = $part;
				if ($part =~ /\w/o || $part eq "::") {
				    if ($parserState->{callbackNamePending} == 1) {
					if (!($part eq $structname || $part eq $enumname || $part eq $unionname || $part eq $typedefname)) {
						# we've seen the initial type.  The name of
						# the callback is after the next open
						# parenthesis.
						print STDERR "callbackNamePending -> 2\n" if ($localDebug || $cbnDebug);
						$parserState->{callbackNamePending} = 2;
					}
				    } elsif ($parserState->{callbackNamePending} == 3) {
					print STDERR "callbackNamePending -> 4\n" if ($localDebug || $cbnDebug);
					$parserState->{callbackNamePending} = 4;
					$parserState->{callbackName} = $part;
					$parserState->{name} = "";
					$parserState->{sodclass} = "";
					print STDERR "sodclass -> \"\" (callback name pending)[11]\n" if ($sodDebug);
					$parserState->{cbsodname} = $parserState->{sodname};
					$parserState->{sodname} = "";
				    } elsif ($parserState->{callbackNamePending} == 4) {
					if ($part eq "::") {
						print STDERR "callbackNamePending -> 5\n" if ($localDebug || $cbnDebug);
						$parserState->{callbackNamePending} = 5;
						$parserState->{callbackName} .= $part;
					} elsif ($part !~ /\s/o) {
						print STDERR "callbackNamePending -> 0\n" if ($localDebug || $cbnDebug);
						$parserState->{callbackNamePending} = 0;
					}
				    } elsif ($parserState->{callbackNamePending} == 5) {
					if ($part !~ /\s/o) {
						print STDERR "callbackNamePending -> 4\n" if ($localDebug || $cbnDebug);
						if ($part !~ /\*/ && $part !~ /\^/) {
							$parserState->{callbackNamePending} = 4;
						}
						$parserState->{callbackName} .= $part;
					}
				    }
				    if ($parserState->{namePending} == 2) {
					$parserState->{namePending} = 1;
					print STDERR "namePending -> 1 [4]\n" if ($parseDebug);
					if (!(scalar(@braceStack)-$parserState->{initbsCount}) && ($parserState->{simpleTypedef} == 2)) {
						print STDERR "bracePending -> 1\n" if ($localDebug);
						$parserState->{bracePending} = 1;
					}
				    } elsif ($parserState->{namePending}) {
					if ($parserState->{name} eq "") { $parserState->{name} = $part; }
					$parserState->{namePending} = 0;
					print STDERR "namePending -> 0 [5]\n" if ($parseDebug);
				    } elsif ($parserState->{bracePending} == 1) {
					if ($part eq "::") {
						# struct foo::bar ....
						# "foo::bar" is the name of
						# the struct and should not
						# trigger this (though we might
						# trigger it on the following
						# word.
						print STDERR "bracePending -> 2 [classmember]\n" if ($localDebug);
						$parserState->{bracePending} = 2;
					} else {
						# Word token when brace pending.  It's
						# a variable.
						print STDERR "IT'S A VARIABLE!  NAME WAS \"$part\".\n" if ($localDebug);
						print STDERR "Word token before brace.  Setting sodname to ".$parserState->{lastsymbol}."\n" if ($localDebug || $sodDebug);
						$parserState->{sodname} = $part;
						# $parserState->{sodtype} = $parserState->{returntype}; #  . " " . $parserState->{name};
						$parserState->{sodtype} = "$declaration$curline";
						$parserState->{sodclass} = "variable";
						print STDERR "sodclass -> variable (implicit)[12]\n" if ($sodDebug);
						$parserState->{frozensodname} = $part;
						print STDERR "bracePending -> 0 [word]\n" if ($localDebug);
						$parserState->{bracePending} = 0;
					}
				    } elsif ($parserState->{bracePending} == 2) {
					$parserState->{bracePending}--;
				    }
				} # end if ($part =~ /\w/o)
				if ($part !~ /[,;\[\]]/o && !$parserState->{inBrackets})  {
					my $opttilde = "";
					if ($parserState->{seenTilde}) { $opttilde = "~"; }
					if ($parserState->{sodbrackets}) {
						print STDERR "SODNAME: $parserState->{sodname} SODTYPE: $parserState->{sodtype}\n" if ($sodDebug);

						# This is counterintuitive.  We append to sodname because it contain the
						# value that is about to be appended to sodtype.  The new, incoming word
						# token will eventually end up in sodname, but it isn't there yet.

						$parserState->{sodname} .= $parserState->{sodbrackets};
						$parserState->{sodbrackets} = "";
						print STDERR "sodbrackets appended and cleared\n" if ($sodDebug);
					}
					
					print STDERR "CHECKPOINT: INTEMPLATE IS ".$parserState->{inTemplate}." SOD IS ".$parserState->{startOfDec}."\n" if ($localDebug || $sodDebug);
					if ($parserState->{startOfDec} == 1) { # @@@ FIXME DAG.  This should not set sodname, but otherwise, we're losing classes!!!
						if (!$parserState->{inTemplate}) {
							if (!length($accessregexp) || ($part !~ /$accessregexp/)) {
								print STDERR "Setting sodname (maybe type) to \"$part\"\n" if ($sodDebug);
								$parserState->{sodname} = $opttilde.$part;
								if ($part =~ /\w/o) {
									$parserState->{startOfDec}++;
								}
							} else {
								print STDERR "Not adjusting startOfDec or sodname because in access (e.g. public/private/*).\n" if ($localDebug || $sodDebug);
							}
						} else {
							print STDERR "Not adjusting startOfDec or sodname because in template.\n" if ($localDebug || $sodDebug);
						}
					} elsif ($parserState->{startOfDec} == 2) {
						if ($part =~ /\w/o && !$parserState->{inTemplate}) {
							$parserState->{preTemplateSymbol} = "";
						}
						if (!$parserState->{inTemplate}) {
							if (isKeyword($part, $keywordhashref, $case_sensitive)) {
							    $parserState->{prekeywordsodname} = $parserState->{sodname};
							    $parserState->{prekeywordsodtype} = $parserState->{sodtype};
							} else {
							    $parserState->{prekeywordsodname} = "";
							    $parserState->{prekeywordsodtype} = "";
							}
							if ($parserState->{inOperator} == 1) {
							    $parserState->{sodname} .= $part;
							} else {
							    if (length($parserState->{sodname})) {
								my $spc = "";
								if ($parserState->{sodname} !~ /[][()]/) { $spc = " "; }
								$parserState->{sodtype} .= "$spc$parserState->{sodname}";
							    }
							    $parserState->{sodname} = $opttilde.$part;
							}

							print STDERR "sodname set to $part\n" if ($sodDebug);
						} else {
							print STDERR "Not adjusting startOfDec or sodname because in template.\n" if ($localDebug || $sodDebug);
						}
					} else {
						$parserState->{startOfDec} = 0;
						print STDERR "startOfDec -> 0 [14]\n" if ($localDebug);
					}
				} elsif ($part eq "[") { # if ($part !~ /[;\[\]]/o)
					$parserState->{inBrackets} += 1;
					print STDERR "inBrackets -> $parserState->{inBrackets}\n" if ($sodDebug);
					print STDERR "SOD AT BRACKET: ".$parserState->{startOfDec}."\n" if ($sodDebug);
				} elsif ($part eq "]") {
					$parserState->{inBrackets} -= 1;
					print STDERR "inBrackets -> $parserState->{inBrackets}\n" if ($sodDebug);
				} # end if ($part !~ /[;\[\]]/o)
				if (!($part eq $eoc)) {
					print STDERR "SETTING LS ($part)\n" if ($parseDebug);
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

	    if ($parserState->{occmethod} && $parserState->{gatheringObjCReturnType} == 1) {
		$parserState->{gatheringObjCReturnType} = 2;
	    } elsif ($parserState->{occmethod} && $parserState->{gatheringObjCReturnType} == 2) {
		$parserState->{occmethodreturntype} .= $part;
	    }

	    if ($parserState->{waitingForTypeInformation} == 1 && $part !~ /\s/) {
			$parserState->{waitingForTypeInformation} = -1;
	    } elsif ($parserState->{waitingForTypeInformation} == 2) {
			$parserState->{waitingForTypeInformation} = 1;
	    }
	    if ($inRegexpCharClass > 1) {
		$inRegexpCharClass--;
	    }

	    # Note: don't check tempInIf here.  We want the open brace included because
	    # the close brace is included.
	    if ($parserState->{seenBraces} && ($parserState->{INIF} != 1)) {
		# print STDERR "SEENBRACES. TP: $treepart PT: $part\n";
		if ($treepart) {
			print STDERR "ADDED \"$treepart\" to function contents\n" if ($functionContentsDebug);
			$parserState->{functionContents} .= $treepart;
		} else {
			print STDERR "ADDED \"$part\" to function contents\n" if ($functionContentsDebug);
			$parserState->{functionContents} .= $part;
		}
		# print STDERR "SEENBRACES. FC: ".$parserState->{functionContents}."\n";
	    } elsif ($parserState->{seenBraces}) {
			print STDERR "NOT ADDING \"$part\" to function contents (INIF: ".$parserState->{INIF}." TEMPINIF: $tempInIf\n" if ($functionContentsDebug);
	    } else {
			print STDERR "NOT ADDING \"$part\" to function contents (!seenBraces)\n" if ($functionContentsDebug);
	    }

	    if ($part !~ /\\/o) {
		if (!($parserState->{inMacro} || $parserState->{inMacroLine}) || $part !~ /\s/) {
			$parserState->resetBackslash();
		}
	    }

	    if (length($part)) { $lasttoken = $part; }
	    if (length($part) && $inRegexpTrailer) { --$inRegexpTrailer; }
	    if ($postPossNL) { --$postPossNL; }
	    if (($parserState->{simpleTypedef} == 1) && ($part ne $typedefname) &&
		   !($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} ||
		     $inRegexp)) {
		# print STDERR "NP: $parserState->{namePending} PTP: $parserState->{posstypesPending} PART: $part\n";
		$parserState->{simpleTDcontents} .= $part;
	    }

	    my $ignoretoken = ignore($part, $ignoreref, $perheaderignoreref);
	    my $hide = ( $hideTokenAndMaybeContents ||
				( $ignoretoken &&
					!( $parserState->{inString} || $parserState->{inComment} ||
					   $parserState->{inInlineComment} || $parserState->{inChar}
					 )
				)
	               );

	    print STDERR "TPONL: $treePopOnNewLine TPTWO: ".$parserState->{treePopTwo}."\n" if ($tsDebug);
	    print STDERR "TN: $treeNest TS: $treeSkip nTS: ".scalar(@treeStack)."\n" if ($tsDebug || $parserStateInsertDebug);
	    print STDERR "sethollow: $sethollow\n" if ($parserStateInsertDebug);
	    if (!$treeSkip) {
		if (!$parserState->{seenBraces}) { # TREEDONE
			if ($treeNest != 2) {
				# If we really want to skip and nest, set treeNest to 2.
				if (length($treepart)) {
					if ((($parserState->{inComment} == 1) || ($parserState->{inInlineComment} == 1)) && $treepart !~ /[\r\n!]/) {
						$treeCur->token($treeCur->token() . $treepart);
						# print STDERR "SHORT\n";
					} else {
						$treeCur = $treeCur->addSibling($treepart, $hide);
					}
					$treepart = "";
				} else {
					if ((($parserState->{inComment} == 1) || ($parserState->{inInlineComment} == 1)) && $treepart !~ /[\r\n!]/) {
						$treeCur->token($treeCur->token() . $part);
						# print STDERR "SHORT\n";
					} else {
						$treeCur = $treeCur->addSibling($part, $hide);
					}
				}
				bless($treeCur, "HeaderDoc::ParseTree");
			}
			# print STDERR "TC IS $treeCur\n";
			# $treeCur = %{$treeCur};
			if ($treeNest) {
				if ($sethollow) {
					print STDERR "WILL INSERT STATE $parserState (SETHOLLOW) at ".$treeCur->token()."\n" if ($parserStackDebug);
					# $parserState->{hollow} = $treeCur;
					setHollowWithLineNumbers(\$parserState, $treeCur, $fileoffset, $inputCounter);
					$sethollow = 0;
				}
				print STDERR "TSPUSH\n" if ($tsDebug || $treeDebug);
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
	    if ($treeDebug) { print STDERR "TS TREENEST -> 0 [19]\n"; }

	    if (!$parserState->{freezereturn} && $parserState->{hollow} && !$parserState->{inComment} && !$parserState->{leavingComment}) {
		# print STDERR "WARNING: RETURN TYPE CHANGE[A]".$parserState->{returntype}." CHANGED TO $declaration$curline.\n";
		$parserState->{returntype} = "$declaration$curline";
		print STDERR "APPENDING TO RETURNTYPE[2]: NOW \"$parserState->{returntype}\".\n" if ($retDebug);
 	    } elsif (!$parserState->{freezereturn} && !$parserState->{hollow} && !$parserState->{inComment} && !$parserState->{leavingComment}) {
		# print STDERR "WARNING: RETURN TYPE CHANGE[B]".$parserState->{returntype}." CHANGED TO $curline.\n";
		$parserState->{returntype} = "$curline";
		print STDERR "REPLACING RETURNTYPE[2]: NOW \"$parserState->{returntype}\".\n" if ($retDebug);
		$declaration = "";
	    # } else {
		# print STDERR "WARNING: LEAVING RETURN TYPE ALONE: ".$parserState->{returntype}." NOT CHANGED TO $curline.\n";
	    }

	    if ($pascal && $parserState->{waitingForTypeInformation}) {
		print STDERR "PASCAL END CASE RT: $parserState->{returntype}\nCL: $curline\n" if ($parseDebug || $localDebug || $retDebug);
		$parserState->{returntype} = $curline;
		print STDERR "REPLACING RETURNTYPE[3]: NOW \"$parserState->{returntype}\".\n" if ($retDebug);
	    }

	    # From here down is... magic.  This is where we figure out how
	    # to handle parsed parameters, K&R C types, and in general,
	    # determine whether we've received a complete declaration or not.
	    #
	    # About 90% of this is legacy code to handle proper spacing.
	    # Those bits got effectively replaced by the parseTree class.
	    # The only way you ever see this output is if you don't have
	    # any styles defined in your config file.

	print STDERR "TOKEN $part parsedParamParse going into end is $parserState->{parsedParamParse}\n" if ($parmDebug);
	    if (($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) ||
		!$ignoretoken) {
		if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar}) &&
                    !$ppSkipOneToken) {
	            if ($parserState->{parsedParamParse} == 1 || $parserState->{parsedParamParse} == 3) {
		        $parsedParam .= $part;
	            } elsif ($parserState->{parsedParamParse} == 2 || $parserState->{parsedParamParse} == 4) {
		        $parserState->{parsedParamParse}--;
		        print STDERR "parsedParamParse -> $parserState->{parsedParamParse}"."[endgame]\n" if ($parmDebug);
	            }
		}
		if ($ppSkipOneToken) {
			$hollowskip = $ppSkipOneToken;
			print STDERR "hollowskip -> $ppSkipOneToken (ppSkipOneToken)\n" if ($parserStateInsertDebug);
		}
		$ppSkipOneToken = 0;
		print STDERR "MIDPOINT CL: $curline\nDEC:$declaration\nSCR: \"$scratch\"\n" if ($localDebug);
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
					# Was != /\n/ which is clearly
					# wrong.  Suspect the next line
					# if we start losing leading spaces
					# where we shouldn't (or don't where
					# we should).  Also was just /g.
					if ($curline !~ /^\s*\n/so) { $curline =~ s/^\s*//sgo; }
					
					# NEWLINE INSERT
					print STDERR "CURLINE CLEAR [1]\n" if ($localDebug);
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
				# Was != /\n/ which is clearly
				# wrong.  Suspect the next line
				# if we start losing leading spaces
				# where we shouldn't (or don't where
				# we should).  Also was /g instead of /sg.
				if ($curline !~ /^\s*\n/so) { $curline =~ s/^\s*//sgo; }
				# NEWLINE INSERT
				$declaration .= "$scratch$curline\n";
				print STDERR "CURLINE CLEAR [2]\n" if ($localDebug);
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
					print STDERR "ZEROSCRATCH\n";
					print STDERR "zDec: \"$zDec\"\n";
				}
			}
			$declaration .= "$scratch$curline";
				print STDERR "CURLINE CLEAR [3]\n" if ($localDebug);
			$curline = "";
			# $curline = nspaces($prespace);
			print STDERR "PS: $prespace -> " . $prespace + $prespaceadjust . "\n" if ($localDebug);
			$prespace += $prespaceadjust;
			$prespaceadjust = 0;
		      } elsif ($part =~ /[\(;,]/o && $nextpart !~ /\n/o &&
                                      ($parserState->{occmethod} == 1)) {
			print STDERR "SPC\n" if ($localDebug);
			$curline .= " "; $occspace = 1;
		      } else {
			print STDERR "NOSPC: $part:$nextpart:$parserState->{occmethod}\n" if ($localDebug);
		      }
		    }
		}

		if ($parserState->{temponlyComments}) {
			# print STDERR "GOT TOC: ".$parserState->{temponlyComments}."\n";
			$parserState->{onlyComments} = $parserState->{temponlyComments};
			$parserState->{temponlyComments} = undef;
		}

	        print STDERR "CURLINE IS \"$curline\".\n" if ($localDebug);
	        my $bsCount = scalar(@braceStack);
		print STDERR "ENDTEST: $bsCount \"$parserState->{lastsymbol}\"\n" if ($localDebug);
		print STDERR "KRC: $parserState->{kr_c_function} SB: $parserState->{seenBraces}\n" if ($localDebug);
	        if (!($bsCount - $parserState->{initbsCount}) && $parserState->{lastsymbol} =~ /;\s*$/o) {
		    # print STDERR "DPA\n";
		    if ((!$parserState->{kr_c_function} || $parserState->{seenBraces}) && !$parserState->{inMacro}) {
		        # print STDERR "DPB\n";
			if (!scalar(@parserStack)) {
			    $continue = 0;
			    print STDERR "CONTINUE -> 0 [3]\n" if ($parseDebug || $cppDebug || $macroDebug || $localDebug || $continueDebug);
			} elsif (!$parserState->{onlyComments}) {
				# Process entry here
				if ($parserState->{noInsert}) {
					print STDERR "parserState insertion skipped[SEMI-1]\n" if ($parserStackDebug);
				} elsif ($parserState->{hollow}) {
					my $treeRef = $parserState->{hollow};

					$parserState->{lastTreeNode} = $treeCur;
					$treeRef->addRawParsedParams(\@{$parserState->{parsedParamList}});
					$treeRef->parserState($parserState);
				} elsif ($parserState->{classtype} && length($parserState->{classtype})) {
					warn "Couldn't insert info into parse tree[3class].\n" if ($localDebug);
				} else {
					warn "Couldn't insert info into parse tree[3].\n";
					print STDERR "Printing tree.\n";
					$parserState->print();
					$treeTop->dbprint();
				}

				print STDERR "parserState: Created parser state[2].\n" if ($parserStackDebug);
				$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
				$parserState->{skiptoken} = 1;
				$parserState->{lang} = $lang;
				$parserState->{inputCounter} = $inputCounter;
				$parserState->{initbsCount} = scalar(@braceStack);
				print STDERR "NEWRETURNTYPE: $parserState->{returntype}\n" if ($localDebug);
				print STDERR "CURLINE CLEAR[PRS2]\n" if ($localDebug);
				$curline = "";
			}
		    }
	        } else {
		    print STDERR "bsCount: $bsCount - $parserState->{initbsCount}, ls: $parserState->{lastsymbol}\n" if ($localDebug);
		    pbs(@braceStack);
	        }
	        if (!($bsCount - $parserState->{initbsCount}) && $parserState->{seenBraces} && ($parserState->{sodclass} eq "function" || $parserState->{inOperator}) && 
		    ($nextpart ne ";")) {
			# Function declarations end at the close curly brace.
			# No ';' necessary (though we'll eat it if it's there.

			if ($parserState->{treePopTwo}) {
				# Fix nesting.
				# print STDERR "LASTTREENODE -> $treeCur (".$treeCur->token().")\n";
				while ($parserState->{treePopTwo}--) {
					$treeCur = pop(@treeStack) || $treeTop;
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					$treeCur = $treeCur->lastSibling();
					$treeCur->parsedParamCopy(\@{$parserState->{parsedParamList}});
					print STDERR "TSPOP [13]\n" if ($tsDebug || $treeDebug);
					bless($treeCur, "HeaderDoc::ParseTree");
				}
				$treeCur = $treeCur->addSibling(";", 0);
				$parserState->{lastTreeNode} = $treeCur;
				$parserState->{treePopTwo} = 0;
			}

			print STDERR "CHECKINIF: ".$parserState->{INIF}."\n" if ($continueDebug);
			if (!scalar(@parserStack) && !$parserState->{INIF} && !$tempInIf) {
				$continue = 0;
				print STDERR "CONTINUE -> 0 [4]\n" if ($parseDebug || $cppDebug || $macroDebug || $localDebug || $continueDebug);
			} elsif (!$parserState->{onlyComments} && !$parserState->{INIF} && !$tempInIf) {
				# Process entry here
				if ($parserState->{noInsert}) {
					print STDERR "parserState insertion skipped[SEMI-2]\n" if ($parserStackDebug);
				} elsif ($parserState->{hollow}) {
					my $treeRef = $parserState->{hollow};

					$parserState->{lastTreeNode} = $treeCur;
					$treeRef->addRawParsedParams(\@{$parserState->{parsedParamList}});
					$treeRef->parserState($parserState);
				} else {
					warn "Couldn't insert info into parse tree[4].\n";
				}

				print STDERR "parserState: Created parser state[3].\n" if ($parserStackDebug);
				$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
				$parserState->{skiptoken} = 1;
				$parserState->{lang} = $lang;
				$parserState->{inputCounter} = $inputCounter;
				$parserState->{initbsCount} = scalar(@braceStack);
				print STDERR "CURLINE CLEAR[PRS3]\n" if ($localDebug);
				$curline = "";
			}
	        }
		print STDERR "INMACRO: ".$parserState->{inMacro}."\n" if ($localDebug || $cppDebug || $cppDebug);
		# $parserState->{lastsymbol} ne "\\"
		print STDERR "IM: ".$parserState->{inMacro}." IQ: ".$parserState->isQuoted($lang, $sublang)."\n" if ($localDebug);
	        if (($parserState->{inMacro} == 3 && !$parserState->isQuoted($lang, $sublang)) || $parserState->{inMacro} == 4) {
		    print STDERR "CHECKPART \"$part\" AGAINST NEWLINE\n" if ($localDebug || $cppDebug);
		    if ($part =~ /[\n\r]/o && !$parserState->{inComment}) {
			print STDERR "MLS: $parserState->{lastsymbol}\n" if ($macroDebug);
			print STDERR "PARSER STACK CONTAINS ".scalar(@parserStack)." FRAMES\n" if ($cppDebug || $parserStackDebug);
			if (!scalar(@parserStack)) {
				$continue = 0;
				print STDERR "CONTINUE -> 0 [5]\n" if ($parseDebug || $cppDebug || $macroDebug || $localDebug || $continueDebug);
			} elsif (!$parserState->{onlyComments}) {
				# Process entry here
				print STDERR "NOT setting continue to 0 for macro: parser stack nonempty\n" if ($liteDebug);
				print STDERR "DONE WITH MACRO.  HANDLING.\n" if ($localDebug || $parseDebug);

				if ($parserState->{inMacro} == 3) {
					if (!$HeaderDoc::skipNextPDefine) {
						cpp_add($parserState->{hollow});
					} else {
						cpp_add($parserState->{hollow}, 1);
						$HeaderDoc::skipNextPDefine = 0;
					}
				}

				if ($parserState->{noInsert}) {
					print STDERR "parserState insertion skipped\n" if ($parserStackDebug);
				} elsif ($parserState->{hollow}) {
					my $treeRef = $parserState->{hollow};

					$parserState->{lastTreeNode} = $treeCur;
					$treeRef->addRawParsedParams(\@{$parserState->{parsedParamList}});
					$treeRef->parserState($parserState);
				} else {
					warn "Couldn't insert info into parse tree[5].\n";
				}

				print STDERR "parserState: Created parser state[4].\n" if ($parserStackDebug);
				$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
				$parserState->{skiptoken} = 1;
				$parserState->{lang} = $lang;
				$parserState->{inputCounter} = $inputCounter;
				$parserState->{initbsCount} = scalar(@braceStack);
				print STDERR "CURLINE CLEAR[PRS4]\n" if ($localDebug);
				$curline = "";
			}
		    }
	        } elsif ($parserState->{inMacro} == 2) {
		    my $linenum = $inputCounter + $fileoffset;
		    warn "$fullpath:$linenum: warning: Declaration starts with # but is not preprocessor macro\n";
		    warn "PART: $part\n";
	        } elsif ($parserState->{inMacro} == 3 && $parserState->isQuoted($lang, $sublang)) {
			# $parserState->{lastsymbol} eq "\\"
			print STDERR "TAIL BACKSLASH ($continue)\n" if ($localDebug || $macroDebug);
		}
	        if ($parserState->{valuepending} == 2) {
		    # skip the "=" part;
		    $parserState->{value} .= $part;
	        } elsif ($parserState->{valuepending}) {
		    $parserState->{valuepending} = 2;
		    print STDERR "valuepending -> 2\n" if ($valueDebug);
	        }
	    } # end if "we're not ignoring this token"


	    print STDERR "OOGABOOGA\n" if ($parserStackDebug);
	    if ($pushParserStateAfterToken == 1) {
			print STDERR "parserState pushed onto stack[token]\n" if ($parserStackDebug);
			$parserState->{lastTreeNode} = $treeCur;
			$curline = "";
			$parserState->{storeDec} = $declaration;
			$parserState->{freezereturn} = 1;
			$declaration = "";
			push(@parserStack, $parserState);
			$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
			$parserState->{skiptoken} = 1;
			$parserState->{lang} = $lang;
			$parserState->{inputCounter} = $inputCounter;
			$parserState->{initbsCount} = scalar(@braceStack);
			$pushParserStateAfterToken = 0;
			$pushParserStateAtBrace = 0;
	    } elsif ($pushParserStateAfterWordToken == 1) {
		if ($part =~ /\w/) {
			print STDERR "parserState pushed onto stack[word]\n" if ($parserStackDebug);
			$parserState->{lastTreeNode} = $treeCur;
			$curline = "";
			$parserState->{storeDec} = $declaration;
			$parserState->{freezereturn} = 1;
			$declaration = "";
			push(@parserStack, $parserState);
			$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
			$parserState->{skiptoken} = 1;
			$parserState->{lang} = $lang;
			$parserState->{inputCounter} = $inputCounter;
			$parserState->{initbsCount} = scalar(@braceStack);
			$pushParserStateAfterWordToken = 0;
		}
	    } elsif ($pushParserStateAfterWordToken) {
		print STDERR "PPSAFTERWT CHANGED $pushParserStateAfterWordToken -> " if ($parserStackDebug);
		$pushParserStateAfterWordToken--;
		print STDERR "$pushParserStateAfterWordToken\n" if ($parserStackDebug);
	    } elsif ($pushParserStateAtBrace) {
		print STDERR "PPSatBrace?\n" if ($parserStackDebug);
		if (casecmp($part, $lbrace, $case_sensitive)) {
			$parserState->{ISFORWARDDECLARATION} = 0;
			print STDERR "parserState pushed onto stack[brace]\n" if ($parserStackDebug);
			# if ($pushParserStateAtBrace == 2) {
				# print STDERR "NOINSERT parserState: $parserState\n" if ($parserStackDebug);
				# $parserState->{hollow} = undef;
				# $parserState->{noInsert} = 1;
			# }
			$parserState->{lastTreeNode} = $treeCur;
			$curline = "";
			$parserState->{storeDec} = $declaration;
			$parserState->{freezereturn} = 1;
			$declaration = "";
			push(@parserStack, $parserState);
			$parserState = HeaderDoc::ParserState->new( "FULLPATH" => $fullpath );
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
				print STDERR "inClass -> 0 [10]\n" if ($classDebug);
			}
			# if ($part =~ /\S/) { $pushParserStateAtBrace = 0; }
		}
		if (!$parserState->{hollow}) {
		    my $tok = $part; # $treeCur->token();
		    print STDERR "parserState: NOT HOLLOW [1]\n" if ($parserStackDebug);
		    print STDERR "IS: $parserState->{inString}\nICom: $parserState->{inComment}\nISLC: $parserState->{inInlineComment}\nIChar: $parserState->{inChar}\nSkipToken: $parserState->{skiptoken}\nHollowSkip: $hollowskip\n" if ($parserStackDebug);
		    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} || $parserState->{skiptoken} || $hollowskip)) {
			print STDERR "parserState: NOT STRING/CHAR/COMMENT\n" if ($parserStackDebug);
			if ($tok =~ /\S/) {
				print STDERR "parserState: PS IS $parserState\n" if ($parserStackDebug);
				print STDERR "parserState: NOT WHITESPACE : ".$parserState->{hollow}." -> $treeCur\n" if ($parserStackDebug);
				if (!casecmp($tok, $rbrace, $case_sensitive) && $part !~ /\)/) {
					# $parserState->{hollow} = $treeCur;
					setHollowWithLineNumbers(\$parserState, $treeCur, $fileoffset, $inputCounter);
					$HeaderDoc::curParserState = $parserState;
					print STDERR "parserState: WILL INSERT STATE $parserState (HOLLOW-AUTO-1) AT TOKEN \"$part\"/\"".$treeCur->token()."\"\n" if ($parserStackDebug);
				}
			}
		    }
		    $hollowskip = 0;
		    print STDERR "hollowskip -> 0 (NOTHOLLOW - 1)\n" if ($parserStateInsertDebug);
		    $parserState->{skiptoken} = 0;
		}
	    } else {
		if (!$parserState->{hollow}) {
		    my $tok = $part; # $treeCur->token();
		    print STDERR "parserState: NOT HOLLOW [2]\n" if ($parserStackDebug);
		    print STDERR "IS: $parserState->{inString}\nICom: $parserState->{inComment}\nISLC: $parserState->{inInlineComment}\nIChar: $parserState->{inChar}\nSkipToken: $parserState->{skiptoken}\nHollowSkip: $hollowskip\n" if ($parserStackDebug);
		    if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} || $parserState->{skiptoken} || $hollowskip)) {
			print STDERR "parserState: NOT STRING/CHAR/COMMENT\n" if ($parserStackDebug);
			if ($tok =~ /\S/) {
				print STDERR "parserState: PS IS $parserState\n" if ($parserStackDebug);
				print STDERR "parserState: NOT WHITESPACE : ".$parserState->{hollow}." -> $treeCur\n" if ($parserStackDebug);
				if (!casecmp($tok, $rbrace, $case_sensitive) && $part !~ /\)/) {
					setHollowWithLineNumbers(\$parserState, $treeCur, $fileoffset, $inputCounter);
					$HeaderDoc::curParserState = $parserState;
					print STDERR "parserState: WILL INSERT STATE $parserState (HOLLOW-AUTO-2) AT TOKEN \"$part\"/\"".$treeCur->token()."\"\n" if ($parserStackDebug);
				}
			}
		    }
		    $hollowskip = 0;
		    print STDERR "hollowskip -> 0 (NOTHOLLOW - 2)\n" if ($parserStateInsertDebug);
		    $parserState->{skiptoken} = 0;
		}
	    }

	    if ($part =~ /\w+/) {
		if (!($parserState->{inString} || $parserState->{inComment} || $parserState->{inInlineComment} || $parserState->{inChar} || $parserState->{skiptoken} || $hollowskip)) {
		    if ($parserState->{occparmlabelfound} == -2) {
			if (!($parserState->{initbsCount} - scalar(@braceStack) )) { # Skip types
				$parserState->{occparmlabelfound} = 0; # Next token is the label for the next parameter.
				if ($HeaderDoc::useParmNameForUnlabeledParms) {
					$parserState->{occmethodname} .= "$part:";
				} else {
					$parserState->{occmethodname} .= ":";
				}
				if ($occMethodNameDebug) {
					print STDERR "OCC parameter name substituted; OCC method name now ".$parserState->{occmethodname}." (lastsymbol was \"".$parserState->{lastsymbol}."\", part was \"".$part."\").\n";
				}
			}
		    } else {
			if (!($parserState->{initbsCount} - scalar(@braceStack) )) { # Skip types
				$parserState->{occparmlabelfound}++;
				if ($occMethodNameDebug && ($parserState->{occparmlabelfound} > 0)) {
					print STDERR "OCC possible label: \"$part\".\n";
				}
			}
		    }
		}
	    }


	    if (length($part) && $part =~ /\S/o) { $lastnspart = $part; }
	    if ($parserState->{seenTilde} && length($part) && $part !~ /\s/o) { $parserState->{seenTilde}--; }
	    $part = $nextpart;

	    if ($parserState->{leavingComment}) {
		$parserState->{leavingComment} = 0;
	    }

	    # Do this at the end so it does not affect continuation handling.
	    if ($parserState->{inMacro} > 1 && !$bshandled) {
		if ($part !~ /[ \t]/) {
			print STDERR "BS RESET\n" if ($parseDebug || $localDebug || $cppDebug || $macroDebug);
			$parserState->resetBackslash();
			print STDERR "ISQUOTED NOW: ".$parserState->isQuoted()."\n" if ($parseDebug || $localDebug || $cppDebug || $macroDebug);;
		}
		# Fall through.
	    };

	    if ($parserState->{rollbackPending}) {
			$curline = $parserState->{preExternCcurline};
			$declaration = $parserState->{preExternCdeclaration};
			$parserState->rollback();
	    }
	} # end foreach (parts of the current line)
    } # end while (continue && ...)

    if ($continue_no_return) {
	return blockParse($fullpath, $fileoffset, $inputLinesRef, $inputCounter, $argparse, $ignoreref,
		$perheaderignoreref, $perheaderignorefuncmacrosref, $keywordhashref, $case_sensitive);
    }

    print STDERR "RETURNING DECLARATION\n" if ($localDebug);

    # Format and insert curline into the declaration.  This handles the
    # trailing line.  (Deprecated.)

    if ($curline !~ /\n/) { $curline =~ s/^\s*//go; }
    if ($curline =~ /\S/o) {
	$scratch = nspaces($prespace);
	$declaration .= "$scratch$curline\n";
    }

    print STDERR "($parserState->{typestring}, $parserState->{basetype})\n" if ($localDebug || $listDebug);

    print STDERR "LS: $parserState->{lastsymbol}\n" if ($localDebug);

    $parserState->{lastTreeNode} = $treeCur;
    $parserState->{inputCounter} = $inputCounter;

print STDERR "PARSERSTATE: $parserState\n" if ($localDebug);

    if ($parserState->{inMacro} == 3) {
	if (!$HeaderDoc::skipNextPDefine) {
		cpp_add($treeTop);
	} else {
		cpp_add($treeTop, 1);
		$HeaderDoc::skipNextPDefine = 0;
	}
    }

print STDERR "LEFTBPMAIN\n" if ($localDebug || $hangDebug);

    if ($argparse && $apwarn) {
	print STDERR "end argparse\n";
    }

    # Return the top parser context even if we got interrupted.
    my $tempParserState = pop(@parserStack);
    $declaration = $parserState->{storeDec}.$declaration;
    while ($tempParserState) {
	$parserState = $tempParserState;
	$tempParserState = pop(@parserStack);
	$declaration = $parserState->{storeDec}.$declaration;
    }
    $HeaderDoc::module = $parserState->{MODULE};

    if ($localDebug || $apDebug || $liteDebug || $parseDebug) {
	print STDERR "LEAVING BLOCKPARSE\n";
    }

    if (0) {
	print STDERR "Returning the following parse tree:\n";
	$treeTop->dbprint();
	print STDERR "End of parse tree.\n";
    }

# print STDERR "FC: ".$parserState->{functionContents}."\n";
# print STDERR "DEFINENAME: \"$definename\"\n";

    return blockParseReturnState($parserState, $treeTop, $argparse, $declaration, $inPrivateParamTypes, $publicDeclaration, $lastACS, $retDebug, $fileoffset, 0, $definename, $inputCounter);
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
    my $subparse = shift;
    my $definename = shift;
    my $inputCounter = shift;

    my $fullpath = $parserState->{FULLPATH};

    my $nameObjDumpDebug = 0;

# $forcedebug = 1;
    $forcedebug = $forcedebug || $HeaderDoc::fileDebug;

    my $subparseDebug = 0;

    my $localDebug = 0 || $forcedebug;
    my $sodDebug   = 0 || $forcedebug;
    my $parseDebug = 0 || $forcedebug;
    my $listDebug  = 0 || $forcedebug;
    my $parmDebug  = 0 || $forcedebug;
    my $retDebug   = 0 || $forcedebug;

    if (!length($declaration)) {
	$declaration = $treeTop->textTree();
    }

    cluck("PARSERSTATE IS $parserState\n") if ($localDebug);

    if ($forcedebug) { $parserState->print(); }

if ($forcedebug) { print STDERR "FD\n"; }

    my $lang = $parserState->{lang};
    my $sublang = $parserState->{sublang};
    my $pascal = 0;
    if ($lang eq "pascal") { $pascal = 1; }
    my $perl_or_shell = 0;
    if ($lang eq "perl" || $lang eq "shell") {
	$perl_or_shell = 1;
    }

    if ($parserState->{seenElse} && $parserState->{functionContents} && !$parserState->{elseContents}) {
	$parserState->{elseContents} = $parserState->{functionContents};
	$parserState->{functionContents} = "";
    } elsif ($parserState->{seenIf} && $parserState->{functionContents} && !$parserState->{ifContents}) {
	$parserState->{ifContents} = $parserState->{functionContents};
	$parserState->{functionContents} = "";
    }

    my $returntype = $parserState->{returntype};
    my $sodtype = $parserState->{sodtype};
    my $sodname = $parserState->{sodname};
    my $basetype = $parserState->{basetype};
    my $callbackName = $parserState->{callbackName};
    my $psName = $parserState->{name};
    my $posstypes = $parserState->{posstypes};
    my $sodclass = $parserState->{sodclass};
    my $value = $parserState->{value};
    my $simpleTDcontents = $parserState->{simpleTDcontents};
    my $availability = $parserState->{availability};

    if ($pascal && $parserState->{nameList}) {
	$sodtype = $sodtype . $sodname;
	$sodname = "";
	# $sodname = $parserState->{nameList};
    }

    # if ($sodclass eq "variable") {
	# if ($parserState->{constKeywordFound}) {
		# $sodclass = "constant";
	# }
    # }

    if ($subparse) {
	$inputCounter = $parserState->{inputCounter};
    }

    my $extendsClass = cppsupers($parserState->{extendsClass}, $lang, $sublang);
    my $implementsClass = cppsupers($parserState->{implementsClass}, $lang, $sublang);

    print STDERR "PS: $parserState\n" if ($localDebug);

    my @parsedParamList = @{$parserState->{parsedParamList}};
    my @pplStack = @{$parserState->{pplStack}};
    my @freezeStack = @{$parserState->{freezeStack}};

    if ($parserState->{prekeywordsodname}) {
	print STDERR "RESTORING PRE-KEYWORD SODNAME: $sodname -> $parserState->{prekeywordsodname}\n" if ($localDebug);
	$sodname = $parserState->{prekeywordsodname};
	$sodtype = $parserState->{prekeywordsodtype};
    }
    if ($parserState->{stackFrozen}) {
	print STDERR "RESTORING SODNAME: $sodname -> $parserState->{frozensodname}\n" if ($localDebug);
	$sodname = $parserState->{frozensodname};
    }

    # if ($parserState->{simpleTypedef}) { $psName = ""; }

    my $conformsToList = $parserState->{conformsToList};

    # From here down is a bunch of code for determining which names
    # for a given type/function/whatever are legit and which aren't.
    # It is mostly a priority scheme.  The data type names are first,
    # and thus lowest priority.

    my @nameObjects = ();

    my $typelist = "";
    my $namelist = "";
    my $rawnamelist = $parserState->{lastsymbol};
    if ($pascal) {
	$rawnamelist = $parserState->{nameList};
    }

    # print STDERR "RNL: \"".$parserState->{nameList}."\"\n";

    my @names = split(/[,\s;]/, $rawnamelist);
    foreach my $insname (@names) {
	if ($insname =~ /\S/) {
		$insname =~ s/\s//so;
		$insname =~ s/^[*^]//sgo;
		my $newtype = $parserState->{typestring};
		if ($pascal && ($parserState->{waitingForTypeInformation})) {
			$newtype = $sodclass;
		}
		if (length($insname)) {
		    $typelist .= " $newtype";
		    $namelist .= ",$insname";
		}
		# print STDERR "INSNAME: $insname\n";
		my $nameobj = HeaderDoc::TypeHelper->new();
		$nameobj->{NAME} = $insname;
		$nameobj->{TYPE} = $newtype;
		$nameobj->{POSSTYPES} = $posstypes;
		$nameobj->{EXTENDSCLASS} = $extendsClass;
		$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
		$nameobj->{INSERTEDAT} = "names";
		push(@nameObjects, $nameobj);
	}
    }
    $typelist =~ s/^ //o;
    $namelist =~ s/^,//o;
# print STDERR "TLPOINT: \"$typelist\"\n";

    if ($pascal) {
	# Pascal only has one name for a type, and it follows the word "type"
	if (!length($typelist)) {
		$typelist .= "$parserState->{typestring}";
		$namelist .= "$psName";

		my $nameobj = HeaderDoc::TypeHelper->new();
		$nameobj->{NAME} = $psName;
		$nameobj->{TYPE} = $parserState->{typestring};
		$nameobj->{POSSTYPES} = $posstypes;
		$nameobj->{EXTENDSCLASS} = $extendsClass;
		$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
		$nameobj->{INSERTEDAT} = "Pascal type";
		push(@nameObjects, $nameobj);
	}
    }

print STDERR "TL (PRE): $typelist\n" if ($localDebug);

    if (!length($basetype)) { $basetype = $parserState->{typestring}; }
print STDERR "BT: $basetype\n" if ($localDebug);

print STDERR "NAME is $psName\n" if ($localDebug || $listDebug);

# print STDERR $HeaderDoc::outerNamesOnly . " or " . length($namelist) . ".\n";

    # If the name field contains a value, and if we've seen at least one brace or parenthesis
    # (to avoid "typedef struct foo bar;" giving us an empty declaration for struct foo), and
    # if either we want tag names (foo in "struct foo { blah } foo_t") or there is no name
    # other than a tag name (foo in "struct foo {blah}"), then we give the tag name.  Scary
    # little bit of logic.  Sorry for the migraine.

    # Note: at least for argparse == 2 (used when handling nested headerdoc
    # markup), we don't want to return more than one name/type EVER.

    # if (($psName && length($psName) && !$parserState->{simpleTypedef} && (!($HeaderDoc::outerNamesOnly || $argparse == 2) || !length($namelist))) || ($namelist !~ /\w/))

    if (($psName && length($psName) && !$parserState->{simpleTypedef} && (!($HeaderDoc::outerNamesOnly || $argparse == 2) || !scalar(@nameObjects))) || !scalar(@nameObjects)) { #  || ($namelist !~ /\w/))

	print STDERR "NAME BLOCK BEGIN\nNM: \"$psName\"\nSTD: $parserState->{simpleTypedef}\nONO: ".$HeaderDoc::outerNamesOnly."\nAP: $argparse\nLNL: \"".$namelist."\" (".length($namelist).")\nNAME BLOCK END\n" if ($localDebug);

	my $quotename = quote($psName);
	if ((!length($psName)) || $namelist !~ /$quotename/) {
		print STDERR "NAME AND TYPE APPENDED\n" if ($localDebug);
		if (length($namelist)) {
			$namelist .= ",";
			$typelist .= " ";
		}
		$namelist .= "$psName";
		$typelist .= "$basetype";

		my $nameobj = HeaderDoc::TypeHelper->new();
		$nameobj->{NAME} = $psName;
		$nameobj->{TYPE} = $basetype;

		if (!scalar(@nameObjects)) {
			# If this is the only name, set possible types.
			$nameobj->{POSSTYPES} = $posstypes;
		} else {
			# Otherwise, it's a tag name for a typedef struct,
			# eg. iname in "typedef struct iname {...} foo_t;"
			# and it is really just a struct, not a typedef.
			$nameobj->{POSSTYPES} = $basetype;
		}

		# @@@ FOR NOW
		$nameobj->{POSSTYPES} = $posstypes;

		$nameobj->{EXTENDSCLASS} = $extendsClass;
		$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
		$nameobj->{INSERTEDAT} = "Variable, enum, etc.";
		push(@nameObjects, $nameobj);
	}
    } else {
	# if we never found the name, it might be an anonymous enum,
	# struct, union, etc.
	print STDERR "Poss Anon Case\n" if ($localDebug);

	if (!scalar(@names)) {
		print STDERR "Empty output ($basetype, $parserState->{typestring}).\n" if ($localDebug || $listDebug);
		$namelist = " ";
		$typelist = "$basetype";

		my $nameobj = HeaderDoc::TypeHelper->new();
		$nameobj->{NAME} = "";
		$nameobj->{TYPE} = $basetype;
		$nameobj->{POSSTYPES} = $posstypes;
		$nameobj->{EXTENDSCLASS} = $extendsClass;
		$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
		$nameobj->{INSERTEDAT} = "Anonymous enum";
		push(@nameObjects, $nameobj);
	}

	print STDERR "NUMNAMES: ".scalar(@names)."\n" if ($localDebug || $listDebug);
    }

print STDERR "NL: \"$namelist\".\n" if ($localDebug || $listDebug);
print STDERR "TL: \"$typelist\".\n" if ($localDebug || $listDebug);
print STDERR "PT: \"$posstypes\"\n" if ($localDebug || $listDebug);
print STDERR "SN: \"$sodname\"\nST: \"$sodtype\"\nSC: \"$sodclass\"\n" if ($localDebug || $sodDebug);

    my $destructor = 0;
    if ($sodtype =~ s/\~$//s) {
	$sodname = "~" . $sodname;
	$destructor = 1;
    }

    # If it's a callback, the other names and types are bogus.  Throw them away.

    $callbackName =~ s/^.*:://o;
    $callbackName =~ s/^[*^]+//o;
    print STDERR "CBN: \"$callbackName\"\n" if ($localDebug || $listDebug);
    print STDERR "CBNP: \"$parserState->{callbackNamePending}\"\n" if ($localDebug || $listDebug);
    print STDERR "CBSN: \"$parserState->{cbsodname}\"\n" if ($localDebug || $listDebug);
    if (length($callbackName)) {
	if (length($parserState->{cbsodname})) {
		$callbackName = $parserState->{cbsodname};
	}
	$psName = $callbackName;
	print STDERR "DEC: \"$declaration\"\n" if ($localDebug || $listDebug);

	my $cbtype = "";
	$namelist = $psName;
	if ($parserState->{callbackIsTypedef}) {
		$cbtype = "typedef";
		$typelist = "typedef";
		$posstypes = "function";
	} else {
		$cbtype = "callback";
		$typelist = "callback";
		$posstypes = "function typedef";
	}

	@nameObjects = ();
	my $nameobj = HeaderDoc::TypeHelper->new();
	$nameobj->{NAME} = $psName;
	$nameobj->{TYPE} = $cbtype;
	$nameobj->{POSSTYPES} = $posstypes;
	$nameobj->{EXTENDSCLASS} = $extendsClass;
	$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
	$nameobj->{INSERTEDAT} = "callback";
	push(@nameObjects, $nameobj);

	print STDERR "NL: \"$namelist\".\n" if ($localDebug || $listDebug);
	print STDERR "TL: \"$typelist\".\n" if ($localDebug || $listDebug);
	print STDERR "PT: \"$posstypes\"\n" if ($localDebug || $listDebug);

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

    if (length($parserState->{preTemplateSymbol}) && ($sodclass eq "function")) {
	print STDERR "Template function detected.  Changing sodname from ".$sodname." to ".$parserState->{preTemplateSymbol}."\n" if ($localDebug || $sodDebug);
	$sodname = $parserState->{preTemplateSymbol};
	$sodclass = "ftmplt";
	print STDERR "sodclass -> ftmplt (return state)[13]\n" if ($sodDebug);
	$posstypes = "ftmplt function method"; # can it really be a method?
	changeAll(\@nameObjects, "POSSTYPES", "ftmplt function method", 0); # may be no-op.
    }

    # If it isn't a constant, the value is something else.  Otherwise,
    # the variable name is whatever came before the equals sign.

    print STDERR "TVALUE: $value\n" if ($localDebug);
    print STDERR "SC: \"".$sodclass."\"\n" if ($localDebug);

    my $holdtypelist = 0;

    if ($sodclass ne "variable") {
	$value = "";
    } elsif (length($value) || ($sodclass eq "variable")) {
	my $varDebug = 0;
	my $reset_name = 0;
	if (length($value)) {
		$reset_name = 1;
	}

	# If we have a variable whose type starts with "class" or similar, restore that token here.
	if ($parserState->{sodtypeclasstoken} && !$parserState->{ISFORWARDDECLARATION}) {
		$sodtype =~ s/^\s*//s;
		$sodtype = $parserState->{sodtypeclasstoken}." ".$sodtype;
	}

	# print STDERR "TYPELIST: $typelist\n";
	# print STDERR "PRE VALUE: ".$value."\n";
	# print STDERR "PT: ".$posstypes."\n";
	$value =~ s/^\s*//so;
	$value =~ s/\s*$//so;
        if ($parserState->{constKeywordFound}) {
		print STDERR "const keyword found\n" if ($localDebug || $sodDebug || $varDebug);
		$sodclass = "constant";
		print STDERR "sodclass -> constant (return state)[14]\n" if ($sodDebug);
		$typelist = "constant";
		$posstypes = "variable";
		changeAll(\@nameObjects, "TYPE", "constant", 0);
		changeAll(\@nameObjects, "POSSTYPES", "variable", 0);
	} else {
		print STDERR "const keyword NOT found\n" if ($localDebug || $sodDebug || $varDebug);
		$sodclass = "variable";
		print STDERR "sodclass -> variable (return state)[15]\n" if ($sodDebug);
		$typelist = "variable";
		$posstypes = "constant";
		changeAll(\@nameObjects, "TYPE", "variable", 0);
		changeAll(\@nameObjects, "POSSTYPES", "constant", 0);
	}
	$holdtypelist = 1;
	print STDERR "Variable detected.  Changing sodname from ".$sodname." to ".$parserState->{preEqualsSymbol}."\n" if ($localDebug || $sodDebug || $varDebug);
	print STDERR "Changing sodclass to ".$sodclass."\nChanging posstypes to ".$posstypes."\n" if ($localDebug || $sodDebug || $varDebug);
	print STDERR "TYPELIST IS ".$typelist."\n" if ($localDebug || $sodDebug || $varDebug);

	if ($reset_name) {
		$sodname = $parserState->{preEqualsSymbol};
	}
    }

    # We lock in the name prior to walking through parameter names for
    # K&R C-style declarations.  Restore that name first.
    if (length($parserState->{kr_c_name})) {
	print STDERR "K&R C declaration  detected.  Changing sodname from ".$sodname." to ".$parserState->{kr_c_name}."\n" if ($localDebug || $sodDebug);
	$sodname = $parserState->{kr_c_name};
	$sodclass = "function";
	print STDERR "sodclass -> function (return state)[16]\n" if ($sodDebug);
    }

    # Okay, so now if we're not an objective C method and the sod code decided
    # to specify a name for this function, it takes precendence over other naming.

    if (length($sodname) && !$parserState->{occmethod}) {
	if (!length($callbackName)) { # && $parserState->{callbackIsTypedef}
	    if ((!$perl_or_shell) || (!length($psName))) {
		$psName = $sodname;
		$namelist = $psName;

		@nameObjects = ();
		my $nameobj = HeaderDoc::TypeHelper->new();
		$nameobj->{NAME} = $psName;
		$nameobj->{TYPE} = $sodclass;
		$nameobj->{POSSTYPES} = $posstypes;
		$nameobj->{EXTENDSCLASS} = $extendsClass;
		$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
		$nameobj->{INSERTEDAT} = "sodname (functions, mostly)";
		push(@nameObjects, $nameobj);
	    } else {
		changeAll(\@nameObjects, "TYPE", $sodclass, 0);
	    }
	    $typelist = "$sodclass";
	    if (!length($parserState->{preTemplateSymbol}) && !$holdtypelist) {
	        $posstypes = "$sodclass";
		changeAll(\@nameObjects, "POSSTYPES", $sodclass, 0);
	    }
	    print STDERR "SETTING NAME/TYPE TO $sodname, $sodclass\n" if ($sodDebug);
	    if ($sodclass eq "function") {
		$posstypes .= " method";
		changeAll(\@nameObjects, "POSSTYPES", "method", 1);
	    }
	}
    }

    # If we're an objective C method, obliterate everything and just
    # shove in the right values.

    print STDERR "DEC: $declaration\n" if ($sodDebug || $localDebug);
    if ($parserState->{occmethod}) {
	$typelist = "method";
	changeAll(\@nameObjects, "TYPE", "method", 0);
	$posstypes = "method function";
	changeAll(\@nameObjects, "POSSTYPES", "method function", 0);
	if ($parserState->{occmethod} == 2) {
		$namelist = "$parserState->{occmethodname}";

		@nameObjects = ();
		my $nameobj = HeaderDoc::TypeHelper->new();
		$nameobj->{NAME} = $parserState->{occmethodname};
		$nameobj->{TYPE} = "method";
		$nameobj->{POSSTYPES} = "method function";
		$nameobj->{EXTENDSCLASS} = $extendsClass;
		$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
		$nameobj->{INSERTEDAT} = "occmethod";
		push(@nameObjects, $nameobj);
	}
    }

    # If we're a macro... well, this gets ugly.  We rebuild the parsed
    # parameter list from the declaration and otherwise use the name grabbed
    # by the sod code.
    if ($parserState->{inMacro} == 3) {
	$typelist = "#define";
	$posstypes = "function method";
	$namelist = $sodname;

	@nameObjects = ();
	my $nameobj = HeaderDoc::TypeHelper->new();
	$nameobj->{NAME} = $sodname;
	$nameobj->{TYPE} = "#define";
	$nameobj->{POSSTYPES} = "function* method*";
	$nameobj->{EXTENDSCLASS} = $extendsClass;
	$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
	$nameobj->{INSERTEDAT} = "CPP define";
	push(@nameObjects, $nameobj);

	$value = "";
	@parsedParamList = ();
	my $declaration = $treeTop->textTree();

	# cluck("DEFINENAME IS \"$definename\"\n");

	if ($definename && $declaration =~ /$definename\s+\w+\(/) {
		my $pplref = defParmParse($declaration, $inputCounter, $definename, $forcedebug, $fullpath);
		print STDERR "parsedParamList replaced\n" if ($parmDebug);
		@parsedParamList = @{$pplref};
	} else {
		# It can't be a function-like macro, but it could be
		# a constant.
		$posstypes = "constant";
		changeAll(\@nameObjects, "POSSTYPES", "constant", 0);
	}
    } elsif ($parserState->{inMacro} == 4) { 
	$typelist = "MACRO";
	$posstypes = "MACRO";
	$value = "";
	@parsedParamList = ();

	changeAll(\@nameObjects, "TYPE", "MACRO", 0);
	changeAll(\@nameObjects, "POSSTYPES", "MACRO", 0);
    }

    # If we're an operator, our type is 'operator', not 'function'.  Our fallback
    # name is 'function'.
    if ($parserState->{inOperator}) {
	$typelist = "operator";
	$posstypes = "function";

	changeAll(\@nameObjects, "TYPE", "operator", 0);
	changeAll(\@nameObjects, "POSSTYPES", "function", 0);
    }

    # if we saw private parameter types, restore the first declaration (the
    # public part) and store the rest for later.  Note that the parse tree
    # code makes this deprecated.

    my $privateDeclaration = "";
    if ($inPrivateParamTypes) {
	$privateDeclaration = $declaration;
	$declaration = $publicDeclaration;
    }

print STDERR "TYPELIST WAS \"$typelist\"\n" if ($localDebug);;
# warn("left blockParse (macro)\n");
# print STDERR "NumPPs: ".scalar(@parsedParamList)."\n";

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
		print STDERR "stack contained $stackitem\n";
	}
    }

    # If we have a C++ struct that looks like a class (struct foo : bar {...})
    # then we need to pick off the class name.
    if ($parserState->{structClassName}) {
	print STDERR "CHANGING NAME FROM $namelist to $parserState->{structClassName}.\n" if ($localDebug);
	$namelist = $parserState->{structClassName};

	@nameObjects = ();
	my $nameobj = HeaderDoc::TypeHelper->new();
	$nameobj->{NAME} = $parserState->{structClassName};
	$nameobj->{TYPE} = "struct"; # @@@ CHECK ME DAG
	$nameobj->{POSSTYPES} = "";
	$nameobj->{EXTENDSCLASS} = $extendsClass;
	$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
	$nameobj->{INSERTEDAT} = "Struct Class";
	push(@nameObjects, $nameobj);
    }

    # If we have a simple typedef, do some formatting on the contents.
    # This is used by the upper layers so that if you have
    # "typedef struct myStruct;", you can associate the fields from
    # "struct myStruct" with the typedef, thus allowing more
    # flexibility in tagged/parsed parameter comparison.
    # 
    $simpleTDcontents =~ s/^\s*//so;
    $simpleTDcontents =~ s/\s*;\s*$//so;
    if ($simpleTDcontents =~ s/\s*\w+$//so) {
	my $continue = 1;
	while ($simpleTDcontents =~ s/\s*,\s*$//so) {
		$simpleTDcontents =~ s/\s*\w+$//so;
	}
    }
    if (length($simpleTDcontents)) {
	my $psc = @pplStack;
	print STDERR "SIMPLETYPEDEF: $inputCounter, $declaration, $typelist, $namelist, $posstypes, $value, OMITTED $psc, $returntype, $privateDeclaration, $treeTop, $simpleTDcontents, $availability\n" if ($parseDebug || $sodDebug || $localDebug);
	$typelist = "typedef";
	if (length($sodname)) {
		# typedefs w/o struct/enum/union, e.g. "typedef uint32_t foo;"
		# don't have a sodname, so we'd better return something....
		$namelist = $sodname;

		@nameObjects = ();
		my $nameobj = HeaderDoc::TypeHelper->new();
		$nameobj->{NAME} = $sodname;
		$nameobj->{TYPE} = "typedef";
		$nameobj->{POSSTYPES} = "";
		$nameobj->{EXTENDSCLASS} = $extendsClass;
		$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
		$nameobj->{INSERTEDAT} = "Simple typedef";
		push(@nameObjects, $nameobj);
	} else {
		changeAll(\@nameObjects, "TYPE", "typedef", 0);
		changeAll(\@nameObjects, "POSSTYPES", "", 0);
	}

	$posstypes = "";
    }

    # If we have a class, do some funky stuff.
    if ($parserState->{inClass} || $parserState->{inProtocol}) {
	# if ($parserState->{inProtocol}) {
		# print STDERR "PROTOCOL\n";
		# $localDebug = 1;
	# }
	print STDERR "INCLASS!\n" if ($localDebug);
	print STDERR "PTS: $parserState->{preTemplateSymbol}\n" if ($localDebug);

	$declaration = $treeTop->textTree();

	$sodtype =~ s/^\s*//s;
	my @classparts = split(/\s/, $sodtype, 2);
	my $classname = "";
	my $superclass = "";
	# print STDERR "RETURNING CLASS: SODTYPE IS $sodtype\n";
	if (scalar(@classparts)) {
		print STDERR "CLASSPARTS FOUND.\n" if ($localDebug);
		$classname = $classparts[0];
		$superclass = $sodname;
		# $classparts[0]." : $sodname";
	} else {
		print STDERR "NO CLASSPARTS FOUND.\n" if ($localDebug);
		$classname = $sodname;
		$superclass = "";
	}
	if ($parserState->{inProtocol}) {
		# Get the class name a different way.
		$superclass = $parserState->{extendsProtocol};
		$superclass =~ s/\s+//sg;
		$superclass =~ s/,,/,/sg;
		$superclass =~ s/^,//sg;
		$superclass =~ s/,$//sg;
		$superclass =~ s/,/\cA/sg;
		print STDERR "PROTOCOLSUPER: $superclass\n" if ($localDebug);
	}
	$namelist = $classname;

	@nameObjects = ();
	my $nameobj = HeaderDoc::TypeHelper->new();
	$nameobj->{NAME} = $classname;
	$nameobj->{TYPE} =$parserState->{classtype};
        $nameobj->{POSSTYPES} = "$superclass"; # @@@ IS THIS RIGHT?
	$nameobj->{EXTENDSCLASS} = $extendsClass;
	$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
	$nameobj->{INSERTEDAT} = "class[1]";
	push(@nameObjects, $nameobj);

	# print STDERR "RETURNING CLASS: NAMELIST IS $classname\n";
	$typelist = "$parserState->{classtype}";
	$posstypes = "$superclass";
	changeAll(\@nameObjects, "TYPE", $parserState->{classtype}, 0);
	changeAll(\@nameObjects, "POSSTYPES", $superclass, 0);
	print STDERR "SUPER WAS \"$superclass\"\n" if ($localDebug);

	print STDERR "categoryClass is $parserState->{categoryClass}\n" if ($localDebug || $parseDebug);
	if (length($parserState->{categoryClass})) {
		$posstypes = $namelist;
		$posstypes =~ s/\s*//sg;
		$namelist .= $parserState->{categoryClass};
		$namelist =~ s/\s*//sg;
		print STDERR "NL: $namelist\n" if ($localDebug || $parseDebug);
		changeAll(\@nameObjects, "POSSTYPES", $posstypes, 0);

		$nameObjects[0]->{NAME} .= $parserState->{categoryClass};
		$nameObjects[0]->{NAME} =~ s/\s*//sg;
		$nameObjects[0]->{TYPE} = $parserState->{classtype};
        	$nameObjects[0]->{POSSTYPES} = $posstypes; # @@@ IS THIS RIGHT?
		$nameobj->{EXTENDSCLASS} = $extendsClass;
		$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
		$nameObjects[0]->{INSERTEDAT} = "class[2cat]";

		my $nameobj = $nameObjects[0];
		@nameObjects = ();
		push(@nameObjects, $nameobj);
	}
    }
    if ($parserState->{forceClassName}) { 
	$namelist = $parserState->{forceClassName};
	$posstypes = cppsupers($parserState->{forceClassSuper}, $lang, $sublang);
	print STDERR "CPPSUPERS: $posstypes\n" if ($localDebug);

	@nameObjects = ();
	my $nameobj = HeaderDoc::TypeHelper->new();
	$nameobj->{NAME} = $parserState->{forceClassName};
	$nameobj->{TYPE} = $parserState->{classtype};
       	$nameobj->{POSSTYPES} = cppsupers($parserState->{forceClassSuper}, $lang, $sublang); # @@@ IS THIS RIGHT?
	$nameobj->{EXTENDSCLASS} = $extendsClass;
	$nameobj->{IMPLEMENTSCLASS} = $implementsClass;
	$nameobj->{INSERTEDAT} = "class[3force]";
	push(@nameObjects, $nameobj);
    }
    if ($parserState->{occSuper}) {
	$posstypes = $parserState->{occSuper};
	changeAll(\@nameObjects, "POSSTYPES", $parserState->{occSuper}, 0);
    }

    $returntype = decomment($returntype);

    # print STDERR "Return type was: $returntype\n" if ($argparse || $sodclass eq "function" || $parserState->{occmethod});
    if (($lang eq "pascal" && length($sodtype)) ||
        ($lang ne "pascal" && (length($sodname) && !$parserState->{occmethod}))) {

# print "SODTYPE IS ".$sodtype."\n";

	$sodtype =~ s/\* \*/**/g;

	# If you consider trailing brackets to be part of the type, enable these lines:
	# if ($parserState->{sodbrackets}) {
		# $sodtype .= $parserState->{sodbrackets};
	# }

	# Note: the following code was added while tracking down a bug.  Do not reenable this.  This warning
	# will flag a lot of valid changes as warnings.
	# $b = $returntype;
	# $a = $sodtype;
	# $a =~ s/^\s*//sg; $a =~ s/\s*$//sg;
	# $b =~ s/^\s*//sg; $b =~ s/\s*$//sg;
	# if ($a ne $b) {
		# print STDERR "WARNING: CHANGED RETURN TYPE FROM ".$returntype." TO ".$sodtype."\n";
		# $treeTop->dbprint();
	# }

	if (!$pascal) {
		$returntype = $sodtype;
		print STDERR "REPLACING RETURNTYPE[4]: NOW \"$returntype\".\n" if ($retDebug);
	} elsif ($parserState->{waitingForTypeInformation}) {
		$returntype =~ s/^\s*:\s*//s;
		$returntype =~ s/\s*;\s*$//s;
	}
	# print STDERR "NEW RT: $returntype\n";
    }
    if (length($parserState->{occmethodreturntype})) {
	$returntype = decomment($parserState->{occmethodreturntype});
    }
    # print STDERR "Return type: $returntype\n" if ($argparse || $sodclass eq "function" || $parserState->{occmethod});
    # print STDERR "DEC: $declaration\n" if ($argparse || $sodclass eq "function" || $parserState->{occmethod});
    print STDERR "PTDEC: ".$treeTop->textTree()."\n" if ($localDebug || $retDebug);

    if ($parserState->{INMODULE}) {
	print STDERR "MODULE DETECTED\n" if ($localDebug);
	$sodclass = "module";
	print STDERR "sodclass -> module (return state)\n[17]" if ($sodDebug);
    }

    cluck("Backtrace\n") if ($localDebug);
    print STDERR "RETURNING NAMELIST: \"$namelist\"\nRETURNING TYPELIST: \"$typelist\"\n" if ($localDebug || $retDebug);

    if (length($lastACS)) {
	$HeaderDoc::AccessControlState = $lastACS;
    }

print STDERR "LEFTBP\n" if ($localDebug);
    if ($parserState->{isProperty}) {
		print STDERR "isProperty Set.\n" if ($localDebug || $listDebug);
		$typelist = "property";
		$posstypes="property";
		$sodclass="";
		print STDERR "sodclass -> \"\" (return state)[18]\n" if ($sodDebug);
		changeAll(\@nameObjects, "TYPE", "property", 0);
		changeAll(\@nameObjects, "POSSTYPES", "property variable* function* method*", 0);
    }

    if ($pascal && ($sodclass eq "enum")) {
	$posstypes = "constant variable";
	changeAll(\@nameObjects, "POSSTYPES", "constant variable", 0);
    }

print STDERR "UPDATED:\n" if ($localDebug || $listDebug);
print STDERR "NL: \"$namelist\".\n" if ($localDebug || $listDebug);
print STDERR "TL: \"$typelist\".\n" if ($localDebug || $listDebug);
print STDERR "PT: \"$posstypes\"\n" if ($localDebug || $listDebug);
print STDERR "SN: \"$sodname\"\nST: \"$sodtype\"\nSC: \"$sodclass\"\n" if ($localDebug || $sodDebug);

    $availability = mergeComplexAvailability($availability, $parserState->{availabilityNodesArray});


    print STDERR "SUBPARSE: $subparse NAME: $namelist TTIC: ".$treeTop->{INPUTCOUNTER}." TTBO: ".$treeTop->{BLOCKOFFSET}."\n" if ($subparseDebug);
    if ($subparse && defined($treeTop->{INPUTCOUNTER})) { $inputCounter = $treeTop->{INPUTCOUNTER}; } elsif ($subparse) { warn("Line numbers may be wrong.\n"); }
    if ($subparse && defined($treeTop->{BLOCKOFFSET})) { $fileoffset = $treeTop->{BLOCKOFFSET}; } elsif ($subparse) { warn("Line numbers may be wrong.\n"); }

    if ($parserState->{ISFORWARDDECLARATION}) {
	$typelist = "forwarddeclaration-$typelist";
	# print STDERR "FORWARD DECLARATION NAMELIST: $namelist\n";
    }

    # print STDERR "TREE TOP GOING OUT IS $treeTop\n" if ($localDebug);

    my $tempnl = "";
    my $temptl = "";
    my $temppt = "";

    # nameObjDump(\@nameObjects);
    foreach my $nameobj (@nameObjects) {
	$tempnl .= ",$nameobj->{NAME}";
	$temptl .= " $nameobj->{TYPE}";
	$temppt = "$nameobj->{POSSTYPES}";
    }
    $tempnl =~ s/^,//s;
    $temptl =~ s/^ //s;

    # This can be enabled for testing purposes, but may warn about legitimate differences on occasion.
    # if ($tempnl ne $namelist || $temptl ne $typelist || $temppt ne $posstypes) {
	# warn "NEW STYLE TYPES DO NOT MATCH.\n";
	# warn "NL:  $namelist\n";
	# warn "NLN: $tempnl\n";
	# warn "TL:  $typelist\n";
	# warn "TLN: $temptl\n";
	# warn "PT:  $posstypes\n";
	# warn "PTN: $temppt\n";
	# foreach my $nameobj (@nameObjects) {
		# warn "\tIA:  ".$nameobj->{INSERTEDAT}."\n";
	# }
    # }

    my $lastparm = pop(@pplStack);
    print STDERR "LP: $lastparm\n" if ($localDebug || $parmDebug);
    $lastparm =~ s/\s*;\s*$//s;
    if ($lastparm ne "") {
	push(@pplStack, $lastparm);
    }
    print STDERR "POSTLP: $lastparm\n" if ($localDebug || $parmDebug);

    # $returntype =~ s/^\s*//s;
    # $returntype =~ s/\s*$//s;

    if ($HeaderDoc::sublang eq "MIG" && $parserState->{simpleTypedef}) {
	# $parserState->dbprint();

	$returntype = $value;
	$value = "";
    }

    my $propertyAttributes = "";
    if ($parserState->{isProperty}) {
	# Objective-C properties in the form "@property(readOnly)" end up with
	# the @property and property type info in the returntype field.
	# Remove it and store it elsewhere (though this is somewhat superfluous
	# since it also shows up in the parsed parameters list).

	if ($returntype =~ s/^\s*\@property\s*\((.*?)\)\s*//s) {
		$propertyAttributes = $1;
	}
	
    }

    if ($localDebug || $nameObjDumpDebug) {
	print STDERR "DUMPING NAME OBJECTS ON RETURN:\n";
	nameObjDump(\@nameObjects);
	print STDERR "RETURN TYPE: $returntype\n";
    }

    # We're outta here.
    return ($inputCounter, $declaration, $typelist, $namelist, $posstypes, $value, \@pplStack, $returntype, $privateDeclaration, $treeTop, $simpleTDcontents, $availability, $fileoffset, $conformsToList, $parserState->{functionContents}, $parserState, \@nameObjects, $extendsClass, $implementsClass, $propertyAttributes);
}

sub nameObjDump($)
{
    my $arrayRef = shift;
    my @objarr = @{$arrayRef};

    print STDERR "DUMPING LIST\n";
    foreach my $obj (@objarr) {
	print STDERR "OBJECT $obj:\n";
	print STDERR "\tNAME:       ".$obj->{NAME}."\n";
	print STDERR "\tTYPE:       ".$obj->{TYPE}."\n";
	print STDERR "\tPOSSTYPES:  ".$obj->{POSSTYPES}."\n";
	print STDERR "\tINSERTEDAT: ".$obj->{INSERTEDAT}."\n";
    }
    print STDERR "END DUMP\n";
}

sub findMatch($$$)
{
    my $arrayRef = shift;
    my $element = shift;
    my $value = shift;
    my @array = @{$arrayRef};

    my $localDebug = 0;

    foreach my $item (@array) {
	print STDERR "CHECKING ELEMENT $element (".$item->{$element}.") against /$value/\n" if ($localDebug);
	if ($item->{$element} =~ /$value/) {
		print STDERR "MATCH\n" if ($localDebug);
		return $item;
	}
	print STDERR "NOT MATCH\n" if ($localDebug);
    }

    print STDERR "NO MATCHES FOUND\n" if ($localDebug);
    return undef;
}

sub changeAll($$$$)
{
    my $arrayRef = shift;
    my $element = shift;
    my $value = shift;
    my $append = shift;
    my @array = @{$arrayRef};

    my $localDebug = 0;

    foreach my $item (@array) {
	print STDERR "CHANGED $element from $item->{$element} to " if ($localDebug);
	if ($append) {
		$item->{$element} = $item->{$element}." ".$value;
	} else {
		$item->{$element} = $value;
	}
	print STDERR "$item->{$element}\n" if ($localDebug);
	print STDERR "APPEND: $append\n" if ($localDebug);
    }
}

sub mergeComplexAvailability($$)
{
    my $orig_avail = shift;
    my $nodearrayref = shift;

    if (!$nodearrayref) { return $orig_avail; }

    my @nodearray = @{$nodearrayref};

    foreach my $noderef (@nodearray) {
	my $node = $noderef;
	# print STDERR "NODE IS $node\n";

	my @availabilitylist = @{$node->parseComplexAvailability()};
	foreach my $entry (@availabilitylist) {
		if (index($orig_avail, $entry) == -1) {
			$orig_avail .= " ".$entry;
		}
	}
    }
    return $orig_avail;
}

sub spacefix
{
my $curline = shift;
my $part = shift;
my $lastchar = shift;
my $soc = shift;
my $eoc = shift;
my $ilc = shift;
my $ilc_b = shift;
my $localDebug = 0;

if ($HeaderDoc::use_styles) { return $curline; }

print STDERR "SF: \"$curline\" \"$part\" \"$lastchar\"\n" if ($localDebug);

	if (($part !~ /[;,]/o)
	  && length($curline)) {
		# space before most tokens, but not [;,]
		if ($part eq $ilc || $part eq $ilc_b) {
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
print STDERR "PAREN\n" if ($localDebug);
			if ($curline !~ /[\)\w\*]\s*$/o) {
				print STDERR "CASEA\n" if ($localDebug);
				if ($lastchar ne " ") {
					print STDERR "CASEB\n" if ($localDebug);
					$curline .= " ";
				}
			} else {
				print STDERR "CASEC\n" if ($localDebug);
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
	print STDERR "BS: ";
	foreach my $p (@braceStack) { print STDERR "$p "; }
	print STDERR "ENDBS\n";
    }
}

# parse #define arguments
sub defParmParse
{
    my $declaration = shift;
    my $inputCounter = shift;
    my $definename = shift;
    my $braceDebug = shift;
    my $fullpath = shift;

    my @myargs = ();
    my $localDebug = 0;
    my $curname = "";

# print STDERR "IN DEFPARMPARSE.  DECLARATION: $declaration\nDEFINENAME $definename\n";

    $declaration =~ s/.*?$definename\s+\w+\s*\(//so;

# print STDERR "IN DEFPARMPARSE.  PARM GUTS: $declaration\n";

    my @braceStack = ( "(" );

    my @tokens = split(/(\W)/, $declaration);
    foreach my $token (@tokens) {
	print STDERR "TOKEN: $token\n" if ($localDebug);
	if (!scalar(@braceStack)) { last; }
	if ($token =~ /[\(\[]/o) {
		print STDERR "open paren/bracket - $token\n" if ($localDebug);
		push(@braceStack, $token);
		print STDERR "PUSHED $token ONTO BRACESTACK\n" if ($braceDebug);
	} elsif ($token =~ /\)/o) {
		print STDERR "close paren\n" if ($localDebug);
		my $top = pop(@braceStack);
		print STDERR "POPPED $top FROM BRACESTACK\n" if ($braceDebug);
		if ($top !~ /\(/o) {
			warn("$fullpath:$inputCounter: warning: Parentheses do not match (macro).\nWe may have a problem.\n");
		}
	} elsif ($token =~ /\]/o) {
		print STDERR "close bracket\n" if ($localDebug);
		my $top = pop(@braceStack);
		print STDERR "POPPED $top FROM BRACESTACK\n" if ($braceDebug);
		if ($top !~ /\[/o) {
			warn("$fullpath:$inputCounter: warning: Braces do not match (macro).\nWe may have a problem.\n");
		}
	} elsif ($token =~ /,/o && (scalar(@braceStack) == 1)) {
		$curname =~ s/^\s*//sgo;
		$curname =~ s/\s*$//sgo;
		push(@myargs, $curname);
		print STDERR "pushed \"$curname\"\n" if ($localDebug);
		$curname = "";
	} else {
		$curname .= $token;
	}
    }
    $curname =~ s/^\s*//sgo;
    $curname =~ s/\s*$//sgo;
    if (length($curname)) {
	print STDERR "pushed \"$curname\"\n" if ($localDebug);
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

    print STDERR "CHECKING TOKEN: \"$part\"\n" if ($localDebug);

    my $def = $HeaderDoc::availability_defs{$part};
    if ($def && length($def)) {
	print STDERR "AVAILABILITY DEF FOUND: $def\n" if ($localDebug);
	if ($HeaderDoc::availability_has_args{$part}) {
		return 3;
	}
	return $def;
    }

    if ($ignorelist{$part}) {
	    print STDERR "IGNORING $part\n" if ($localDebug);
	    return 1;
    }
    if ($perheaderignorelist{$part}) {
	    print STDERR "IGNORING $part\n" if ($localDebug);
	    return 1;
    }
    print STDERR "NO MATCH FOUND\n" if ($localDebug);
    if ($localDebug) {
	print STDERR "Ignore list:\n";
	foreach my $key (keys %ignorelist) {
		print STDERR "    ".$key."   -> ".$ignorelist{$key}."\n";
	}
	print STDERR "\nPer-header ignore list:\n";
	foreach my $key (keys %perheaderignorelist) {
		print STDERR "    ".$key."   -> ".$perheaderignorelist{$key}."\n";
	}
	print STDERR "\nAvailability macros:\n";
	foreach my $key (keys %HeaderDoc::availability_defs) {
		print STDERR "    ".$key."   -> ".$HeaderDoc::availability_defs{$key}."\n";
	}
    }

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
    my $fullpath = shift;
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

# cluck("BPO: FP: $fullpath\n");

    my $filename = basename($fullpath);


# print STDERR "PREATPART: $preAtPart\n";

    my $localDebug = shift;
    my $hangDebug = shift;
    my $parmDebug = shift;
    my $blockDebug = shift;

    my $subparse = shift;
    my $subparseTree = shift;
    # my $outputdir = shift;
    my $nodec = shift;
    my $allow_multi = shift;

	print STDERR "ALLOW_MULTI: $allow_multi\n" if ($localDebug);

# $localDebug = 1; $blockDebug = 1;
    my $nameDebug = 0;
    my $nameObjDebug = 0;
    my $nestClassDebug = 0;

    my $missingParseTreeDebug = 0;

# cluck('BPO') if (!$subparse);
# cluck('BPO SUBPARSE') if ($subparse);

    if ($inClass && $nestClassDebug) {
	$localDebug = 1; $blockDebug = 1; $hangDebug = 1; # turn on the kitchen sink.
    }

    # The number of curly braces at the top level---aids in skipping
    # declarations at the outer levels
    my $numcurlybraces = 0;

    if ($localDebug) { if ($subparse) { print STDERR "SUBPARSE\n"; } else { print STDERR "PARSE\n"; } }

    my $lastParseTree = undef;
    my $slowokay = ($subparse == 2) ? 1 : 0;

    my @linkobjs = ();

    my $old_enable_cpp = $HeaderDoc::enable_cpp;
    if ($subparse) {
	# We don't want to remove #define macros just because they appeared
	# within a class declaration....  :-)
	$HeaderDoc::enable_cpp = -1;
    }

    print STDERR "IN BLOCKPARSEOUTSIDE, INPUTCOUNTER: $inputCounter BLOCKOFFSET: $blockOffset\n" if ($localDebug);

    # Args here

    my @classObjects = @{$classObjectsref};
    my @categoryObjects = ();
    if ($categoryObjectsref) {
	# print STDERR "GOT categoryObjectsRef!\n";
	@categoryObjects = @{$categoryObjectsref};
    }
    my @fields = @{$fieldsref};
    my @inputLines = @{$inputlinesref};
    my @parseTrees = ();
    my $methods_with_new_parser = 1;
    my $curObj = undef;
    my $classKeyword = "auto";
    my $functionContents = "";

    my $subparseInputCounter = undef;
    my $subparseBlockOffset = undef;

    if ($subparse && $subparseTree) {
	$subparseInputCounter = $subparseTree->{LINENUM};
	$subparseBlockOffset = 0;
    }

    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
	$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
	$typedefname, $varname, $constname, $structisbrace, $macronameref,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
	$requiredregexp, $propname, $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename) = parseTokens($lang, $HeaderDoc::sublang);

# print STDERR "BPO: FIELDS: \n";
# foreach my $field (@fields) {
	# print STDERR "FIELD: $field\n";
# }
# print STDERR "END FIELDS\n";

my $foundMatch = 0;

print STDERR "blockParseOutside: APIOWNER IS $apiOwner\n" if ($localDebug);

				if ($inUnknown || $inTypedef || $inStruct || $inEnum || $inUnion || $inConstant || $inVar || $inFunction || ($inMethod && $methods_with_new_parser) || $inPDefine || $inClass) {
# print STDERR "PROPNAME3: $propname\n";
				    my $varIsConstant = 0;
				    my $blockmode = 0;
				    my $blocklevel = 0;
				    my $curtype = "";
				    my $warntype = "";
				    my $blockDebug = 0 || $localDebug;
				    my $parmDebug = 0 || $localDebug;
				    # my $localDebug = 1;

				    if ($inPDefine == 2) {
					print STDERR "BLOCKMODE[1] -> 1\n" if ($localDebug || $blockDebug);
					$blockmode = 1;
				    }
				    if ($inFunction || $inMethod) {
					if ($localDebug) {
						if ($inMethod) {
							print STDERR "inMethod\n";
						} else {
							print STDERR "inFunction\n";
						}
					}
					my $method = 0;
					if ($classType eq "occ" ||
						$classType eq "intf" ||
						$classType eq "occCat") {
							if ($apiOwner !~ /^HeaderDoc::Header/) {
								$method = 1;
							}
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
                                        $curObj->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$curObj->linenuminblock($subparseInputCounter);
						$curObj->blockoffset($subparseBlockOffset);
					} else {
						$curObj->linenuminblock($inputCounter);
						$curObj->blockoffset($blockOffset);
					}
					# $curObj->linenum($inputCounter+$blockOffset);
					# print STDERR "LINE NUMBER IS $inputCounter + $blockOffset\n";
					if ($method) {
						$curObj->processComment(\@fields);
					} else {
						$curObj->processComment(\@fields);
					}
				    } elsif ($inPDefine) {
					print STDERR "inPDefine\n" if ($localDebug);
					$curtype = "#define";
					if ($blockmode) { $warntype = "defineblock"; }
					$curObj = HeaderDoc::PDefine->new;
					$curObj->apiOwner($apiOwner);
					$curObj->group($HeaderDoc::globalGroup);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
					$curObj->inDefineBlock($blockmode);
                                        $curObj->filename($filename);
                                        $curObj->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$curObj->linenuminblock($subparseInputCounter);
						$curObj->blockoffset($subparseBlockOffset);
					} else {
						$curObj->linenuminblock($inputCounter);
						$curObj->blockoffset($blockOffset);
					}
					# $curObj->linenum($inputCounter+$blockOffset);
					$curObj->processComment(\@fields);
				    } elsif ($inVar) {
# print STDERR "inVar!!\n";
					print STDERR "inVar\n" if ($localDebug);
					$curtype = "variable";
					$varIsConstant = 0;
					$curObj = HeaderDoc::Var->new;
					$curObj->apiOwner($apiOwner);
					$curObj->group($HeaderDoc::globalGroup);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
                                        $curObj->filename($filename);
                                        $curObj->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$curObj->linenuminblock($subparseInputCounter);
						$curObj->blockoffset($subparseBlockOffset);
					} else {
						$curObj->linenuminblock($inputCounter);
						$curObj->blockoffset($blockOffset);
					}
					# $curObj->linenum($inputCounter+$blockOffset);
					$curObj->processComment(\@fields);
				    } elsif ($inConstant) {
					print STDERR "inConstant\n" if ($localDebug);
					$curtype = "constant";
					$varIsConstant = 1;
					$curObj = HeaderDoc::Constant->new;
					$curObj->apiOwner($apiOwner);
					$curObj->group($HeaderDoc::globalGroup);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
                                        $curObj->filename($filename);
                                        $curObj->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$curObj->linenuminblock($subparseInputCounter);
						$curObj->blockoffset($subparseBlockOffset);
					} else {
						$curObj->linenuminblock($inputCounter);
						$curObj->blockoffset($blockOffset);
					}
					# $curObj->linenum($inputCounter+$blockOffset);
					$curObj->processComment(\@fields);
				    } elsif ($inUnknown || $inClass) {
					if ($localDebug) {
						if ($inUnknown) {
							print STDERR "inUnknown\n";
						} else {
							print STDERR "inClass\n";
						}
					}
					$curtype = "UNKNOWN";
					if ($inUnknown) {
						$curObj = HeaderDoc::HeaderElement->new;
						$curObj->apiOwner($apiOwner);
						$classKeyword = "auto";
					} else {
						$curObj = HeaderDoc::APIOwner->new;
						$curObj->apiOwner($apiOwner);
						$classKeyword = $fields[0];
						$classKeyword =~ s/^\s*\/\*\!\s*//s;
						if (!length($classKeyword)) {
							$classKeyword = $fields[1];
						}
					}
                                        $curObj->filename($filename);
                                        $curObj->fullpath($fullpath);
					$curObj->apiOwner($apiOwner);
					$curObj->group($HeaderDoc::globalGroup);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
					warnHDComment(\@inputLines, $inputCounter, $blockOffset, "unknown", "11");
				    } elsif ($inTypedef) {
# print STDERR "inTypedef\n"; $localDebug = 1;
					print STDERR "inTypedef\n" if ($localDebug);
					$curtype = $typedefname;
					# if ($lang eq "pascal") {
						# $curtype = "type";
					# } else {
						# $curtype = "typedef";
					# }
					$curObj = HeaderDoc::Typedef->new;
					$curObj->apiOwner($apiOwner);
					$curObj->group($HeaderDoc::globalGroup);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
                                        $curObj->filename($filename);
                                        $curObj->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$curObj->linenuminblock($subparseInputCounter);
						$curObj->blockoffset($subparseBlockOffset);
					} else {
						$curObj->linenuminblock($inputCounter);
						$curObj->blockoffset($blockOffset);
					}
					# $curObj->linenum($inputCounter+$blockOffset);
					$curObj->processComment(\@fields);
					$curObj->masterEnum(0);
					
					warnHDComment(\@inputLines, $inputCounter, $blockOffset, "enum", "11a");
					# if a struct declaration precedes the typedef, suck it up
				} elsif ($inStruct || $inUnion) {
					if ($localDebug) {
						if ($inUnion) {
							print STDERR "inUnion\n";
						} else {
							print STDERR "inStruct\n";
						}
					}
					if ($inUnion) {
						$curtype = "union";
					} else {
						$curtype = "struct";
					}
                                        $curObj = HeaderDoc::Struct->new;
                                        $curObj->apiOwner($apiOwner);
					$curObj->group($HeaderDoc::globalGroup);
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
                                        $curObj->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$curObj->linenuminblock($subparseInputCounter);
						$curObj->blockoffset($subparseBlockOffset);
					} else {
						$curObj->linenuminblock($inputCounter);
						$curObj->blockoffset($blockOffset);
					}
					# $curObj->linenum($inputCounter+$blockOffset);
                                        $curObj->processComment(\@fields);
					warnHDComment(\@inputLines, $inputCounter, $blockOffset, "$curtype", "11b");
				} elsif ($inEnum) {
					print STDERR "inEnum\n" if ($localDebug);
					$curtype = "enum";
                                        $curObj = HeaderDoc::Enum->new;
                                        $curObj->apiOwner($apiOwner);
					$curObj->masterEnum(1);
					$curObj->group($HeaderDoc::globalGroup);
                                        if ($xml_output) {
                                            $curObj->outputformat("hdxml");
                                        } else {
                                            $curObj->outputformat("html");
                                        }
                                        $curObj->filename($filename);
                                        $curObj->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$curObj->linenuminblock($subparseInputCounter);
						$curObj->blockoffset($subparseBlockOffset);
					} else {
						$curObj->linenuminblock($inputCounter);
						$curObj->blockoffset($blockOffset);
					}
					# $curObj->linenum($inputCounter+$blockOffset);
                                        $curObj->processComment(\@fields);
					warnHDComment(\@inputLines, $inputCounter, $blockOffset, "$curtype", "11c");
				}

				my $origcurtype = $curtype;
				my $firstBlockObjType = "";
				my $firstBlockCurType = "";

				# if ($nameDebug) { $curObj->dbprint(); }

				if (!length($warntype)) { $warntype = $curtype; }
                                while (($inputLines[$inputCounter] !~ /\S/o)  && ($inputCounter <= $nlines)){
                                	# print STDERR "BLANKLINE IS $inputLines[$inputCounter]\n";
                                	$inputCounter++;
					print STDERR "INCREMENTED INPUTCOUNTER [3]\n" if ($HeaderDoc::inputCounterDebug);
# print STDERR "warntype is $warntype\n";
                                	warnHDComment(\@inputLines, $inputCounter, $blockOffset, "$warntype", "12");
                                	print STDERR "Input line number[7]: $inputCounter\n" if ($localDebug);
                                };
                                # my  $declaration = $inputLines[$inputCounter];

print STDERR "NEXT LINE is ".$inputLines[$inputCounter].".\n" if ($localDebug);

	my $outertype = ""; my $newcount = 0; my $declaration = ""; my $namelist = "";
	my $extendsClass = "";
	my $implementsClass = "";
	my $returnedParserState = undef;
	my $nameObjectsRef = undef;

	my ($case_sensitive, $keywordhashref) = $curObj->keywords();
	my $typelist = ""; my $innertype = ""; my $posstypes = "";
	print STDERR "PTCT: $posstypes =? $curtype\n" if ($localDebug || $blockDebug);
	my $blockDec = "";
	$localDebug = 0 || $nestClassDebug;
	my $hangDebug = 0 || $localDebug;
	my $subparseDebug = 0 || $localDebug;

	print STDERR "ENTERING IC: $inputCounter\n" if ($localDebug);

	my $lastParserState = undef;
	print STDERR "LOOPTEST: (($blockmode || ($outertype ne $curtype && $innertype ne $curtype && $posstypes !~ /$curtype/ && !($inTypedef && $outertype =~ /^(class|\@class|\@interface|\@implementation|\@protocol)/))) && ($inputCounter <= $nlines))\n" if ($localDebug);
	my $previousInputCounter = $inputCounter;
	my $bail = 0;
	my $checkLineNumbers = 0;
	my $blockCurNameAutoGenerated = 0;
	my $firstDeclaration = 1;
	my $leading_linenum = $inputCounter+$blockOffset;
	while (($blockmode || ($outertype ne $curtype && $innertype ne $curtype && $posstypes !~ /$curtype/ && !($inTypedef && $outertype =~ /^(class|\@class|\@interface|\@implementation|\@protocol)/))) && (($inputCounter <= $nlines) || ($subparse && !$checkLineNumbers))) { # ($typestring !~ /$typedefname/)

		print STDERR "TOP OF LOOP: BLOCKMODE IS $blockmode\n" if ($blockDebug || $localDebug);

		# print STDERR "DEC: $declaration\n";

		if ($firstDeclaration) {
			$firstDeclaration = 0;
		} else {
			if ($HeaderDoc::enableParanoidWarnings && $posstypes !~ "MACRO") {
				my ($tagname, $tag_re, $superclassFieldName) = $curObj->tagNameRegexpAndSuperclassFieldNameForType();
				warn("$fullpath:$leading_linenum: ".$tagname." ".$curObj->name()." declaration not\nfound immediately after HeaderDoc comment.  Declarations after this will\nbe treated as part of the same declaration.\n");
				# warn("Last declaration was $declaration\n");
				# warn("CURTYPE $curtype INNERTYPE $innertype OUTERTYPE $outertype POSSTYPES $posstypes\n");
			}
		}


		if ($hangDebug) { print STDERR "In Block Loop\n"; }

		# while ($inputLines[$inputCounter] !~ /\S/o && ($inputCounter <= $nlines)) { $inputCounter++; }
		# if (warnHDComment(\@inputLines, $inputCounter, 0, "blockParse:$outertype", "18b")) {
			# last;
		# } else { print STDERR "OK\n"; }
		print STDERR "DOING SOMETHING\n" if ($localDebug);

		print STDERR "CURRENT LINE: ".$inputLines[$inputCounter]."\n" if ($localDebug);
		# $HeaderDoc::ignore_apiuid_errors = 1;
		# my $oldisdoc = $curObj->appleRefIsDoc();
		# $curObj->appleRefIsDoc(1);
		# my $junk = $curObj->apirefSetup();
		# $curObj->appleRefIsDoc($oldisdoc);
		# $HeaderDoc::ignore_apiuid_errors = 0;
		# the value of a constant
		my $value = "";
		my $pplref = undef;
		my $returntype = undef;
		my $propertyAttributes = undef;
		my $pridec = "";
		my $parseTree = undef;
		my $simpleTDcontents = "";
		my $bpavail = "";
		my $conformsToList; # Suddenly feeling nonconformist.
		print STDERR "Entering blockParse\n" if ($hangDebug);
		print STDERR "ENTERING BP WITH IC: $inputCounter\n" if ($HeaderDoc::inputCounterDebug);
		if ($subparse) {
			print STDERR "subparse\n" if ($localDebug || $blockDebug || $hangDebug);
			if ($lastParseTree) {
				# second parse tree encountered (i.e. last one didn't match)
				if ($subparseDebug) {
					print STDERR "GOING FOR ANOTHER PARSE TREE.\n";
					$lastParseTree->printTree();
				}
				if ($lastParserState) {
					my $lastend = $lastParserState->{lastTreeNode};
					$parseTree = $lastend->nextTokenNoComments($soc, $ilc, $ilc_b);
				} else {
					$parseTree = $lastParseTree->nextTokenNoComments($soc, $ilc, $ilc_b);
				}
				if ($subparseDebug) {
					print STDERR "FOUND:\n";
					$parseTree->printTree();
				}
				if (!$parseTree) {
					my $curname = $curObj->name();
					if (!empty_comment(@fields)) {
						# Don't warn if there wasn't
						# really a HeaderDoc comment
						# involved.
						warn("End of parse tree reached while searching for matching declaration[1].\n");
						warn "No matching declaration found.  Last name was $curname\n";
						warn buildCommentFromFields(@fields, $preAtPart, "The HeaderDoc comment that caused this was:\n")."\n";
						warn "$outertype ne $curtype && $innertype ne $curtype && $posstypes !~ $curtype\n";
					}
					$HeaderDoc::enable_cpp = $old_enable_cpp;
					objlink(\@linkobjs);

					my $classobjref = undef; my $catobjref = undef;
					$classType = undef; $blockOffset = 0;
					$numcurlybraces = 0; $foundMatch = 0;
					return ($inputCounter, $cppAccessControlState, $classType,
						$classobjref, $catobjref, $blockOffset, $numcurlybraces,
						$foundMatch);
				}
				$lastParseTree = $parseTree;
			} else {
				# first parse tree encountered
				print STDERR "FIRST PARSE TREE.\n" if ($subparseDebug);

				$parseTree = $subparseTree;;

				if ($subparseDebug) {
					print STDERR "PARSETREE FOLLOWS:\n";
					$parseTree->printTree();
				}

				print STDERR "HERE\n" if ($localDebug);
				$lastParseTree = $parseTree;
				if (!$parseTree) {
					my $curname = $curObj->name();
					if (!empty_comment(@fields)) {
						# Don't warn if there wasn't
						# really a HeaderDoc comment
						# involved.
						warn("End of parse tree reached while searching for matching declaration[2].\n");
						warn "No matching declaration found.  Last name was $curname\n";
						warn buildCommentFromFields(@fields, $preAtPart, "The HeaderDoc comment that caused this was:\n")."\n";
						warn "$outertype ne $curtype && $innertype ne $curtype && $posstypes !~ $curtype\n";
					}
					$HeaderDoc::enable_cpp = $old_enable_cpp;
					objlink(\@linkobjs);
					my $classobjref = undef; my $catobjref = undef;
					$classType = undef; $blockOffset = 0;
					$numcurlybraces = 0; $foundMatch = 0;
					return ($inputCounter, $cppAccessControlState, $classType,
						$classobjref, $catobjref, $blockOffset, $numcurlybraces,
						$foundMatch);
				}
			}
			# From here down happens for every parse tree.
			print STDERR "THERE\n" if ($localDebug);

			# print STDERR "Dumping parse tree (debug)\n";
			# print STDERR $parseTree->textTree();
			# print STDERR "Done dumping parse tree (debug)\n";
			my @ppl = ();
			@ppl = $parseTree->parsedParams();
			$pplref = \@ppl;
			print STDERR "PPLREF is $pplref\n" if ($localDebug);
			my $treestring = $parseTree->textTree();
			if ($treestring !~ /\w/) {
				my $curname = $curObj->name();
				if (!empty_comment(@fields)) {
					# Don't warn if there wasn't
					# really a HeaderDoc comment
					# involved.
					warn("End of parse tree reached while searching for matching declaration[3].\n");
					warn "No matching declaration found.  Last name was $curname\n";
					warn buildCommentFromFields(@fields, $preAtPart, "The HeaderDoc comment that caused this was:\n")."\n";
					warn "$outertype ne $curtype && $innertype ne $curtype && $posstypes !~ $curtype\n";
				}
				$HeaderDoc::enable_cpp = $old_enable_cpp;
				objlink(\@linkobjs);
				my $classobjref = undef; my $catobjref = undef;
				$classType = undef; $blockOffset = 0;
				$numcurlybraces = 0; $foundMatch = 0;
				return ($inputCounter, $cppAccessControlState, $classType,
					$classobjref, $catobjref, $blockOffset, $numcurlybraces,
					$foundMatch);
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
				print STDERR "POS IS $findstate\n" if ($localDebug);
				print STDERR "POSTOKE IS ".$findstate->token()."\n" if ($localDebug);
				$parserState = $findstate->parserState;
				$findstate = $findstate->next();
			}
			if (!$parserState || $inClass) {
				# Failure case.  Something is broken in the block parser.
				print STDERR "NOPARSERSTATE\n" if ($localDebug);
				if (!$slowokay && !$inClass) {
					$checkLineNumbers = 1;
					warn("Couldn't find parser state.  Using slow method.\n");
					warn buildCommentFromFields(@fields, $preAtPart, "The HeaderDoc comment that caused this was:\n")."\n";
					$localDebug = 1;
					$hangDebug = 1;

					if (0) {
						warn "APIO: PT: ".$apiOwner->parseTree()."\n";
						my $tree = ${$apiOwner->parseTree()};
						bless($tree, "HeaderDoc::ParseTree");
						print STDERR "TREE is $tree\n";
						$tree->dbprint();
					}
				}
				($newcount, $declaration, $typelist, $namelist, $posstypes, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $blockOffset, $conformsToList, $functionContents, $returnedParserState, $nameObjectsRef, $extendsClass, $implementsClass, $propertyAttributes) = &blockParse($fullpath, $blockOffset, \@inputLines, $inputCounter, 0, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);

				# @@@ CHECKME DAG
				if ($curObj->isAPIOwner) {
					$parseTree->apiOwner($curObj);
				} else {
					$parseTree->addAPIOwner($curObj);
				}
				print STDERR "API OWNER FOR $parseTree is ".$parseTree->apiOwner()."\n" if ($localDebug);

print STDERR "NC: $newcount\n" if ($localDebug);
                                $numcurlybraces += $parseTree->curlycount($lbrace);
				if ($inClass && $nestClassDebug) {
					print STDERR "NESTCLASS TEST:\n";
					$parseTree->dbprint();
					print STDERR "END NESTCLASS TEST:\n";
					print STDERR "(\$newcount, \$declaration, \$typelist, \$namelist, \$posstypes, \$value, \$pplref, \$returntype, \$pridec, \$parseTree, \$simpleTDcontents, \$bpavail, \$blockOffset) = ";
					print STDERR "($newcount, DECLARATION OMITTED, $typelist, $namelist, $posstypes, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $blockOffset)\n";
				}
				print STDERR "newcount IS $newcount\n" if ($localDebug);
				print STDERR "dec is $declaration\n" if ($localDebug);
				print STDERR "TYPELIST IS \"$typelist\"\n" if ($localDebug);
				print STDERR "NAMELIST IS \"$namelist\"\n" if ($localDebug);
				print STDERR "POSSTYPES IS \"$posstypes\"\n" if ($localDebug);
				print STDERR "PPLREF IS $pplref\n" if ($localDebug);
				print STDERR "RETURNTYPE IS \"$returntype\"\n" if ($localDebug);
				print STDERR "parseTree IS \"$parseTree\"\n" if ($localDebug);
				$lastParserState = undef; # prevent infinite loop.

				if ($typelist eq "MACRO" && ($returntype !~ /\#define/)) {
					if ($blockmode != 2) {
						if ($returntype =~ /#if/ && $allow_multi) {
							$blockmode = 3;
							$blocklevel++;
							$curObj->isBlock(1);
							print STDERR "BLOCKMODE[2] -> $blockmode\n" if ($localDebug || $blockDebug);
							$curObj->addParseTree(\$parseTree);
						} elsif ($returntype =~ /#endif/ && $allow_multi) {
							$blocklevel--;
							if (!$blocklevel) { $blockmode = 0; }
							print STDERR "BLOCKMODE[3] -> $blockmode\n" if ($localDebug || $blockDebug);
							$curObj->addParseTree(\$parseTree);
							last;
						} elsif ($returntype =~ /#elif/ && $allow_multi) {
							$curObj->addParseTree(\$parseTree);
						} elsif ($returntype =~ /#else/ && $allow_multi) {
							$curObj->addParseTree(\$parseTree);
						} elsif ($returntype !~ /#(if|endif|else)/ || $allow_multi) {
							if (@fields != ()) {
								warn("$fullpath:".($inputCounter + $blockOffset).": warning: Macros found between comments and declaration.  Ignoring.\n") if (!$HeaderDoc::test_mode);
								print STDERR "DEC: $declaration\n" if (!$HeaderDoc::test_mode);
							}
						}
					}
					$inputCounter = $newcount;
					next;
				}
			} else {
				$parserState->{APIODONE} = 1;
				# Normal case.
				my $subParseDebugPoint = (0 || $localDebug || $subparseDebug);
				my $newblockoffset;
				($newcount, $declaration, $typelist, $namelist, $posstypes, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $newblockoffset, $conformsToList, $functionContents, $returnedParserState, $nameObjectsRef, $extendsClass, $implementsClass, $propertyAttributes) = &blockParseReturnState($parserState, $parseTree, 0, "", 0, "", "", ($localDebug || $blockDebug), 0, $subparse, $definename);
				# $newcount = 1;

				# @@@ CHECKME DAG
				if ($curObj->isAPIOwner) {
					$parseTree->apiOwner($curObj);
				} else {
					$parseTree->addAPIOwner($curObj);
				}
				print STDERR "API OWNER FOR $parseTree is ".$parseTree->apiOwner()."\n" if ($localDebug);

                                $numcurlybraces += $parseTree->curlycount($lbrace, $parserState->{lastTreeNode});

				print STDERR "newcount IS $newcount\n" if ($subParseDebugPoint);
				print STDERR "dec is $declaration\n" if ($subParseDebugPoint);
				print STDERR "TYPELIST IS \"$typelist\"\n" if ($subParseDebugPoint);
				print STDERR "NAMELIST IS \"$namelist\"\n" if ($subParseDebugPoint);
				print STDERR "POSSTYPES IS \"$posstypes\"\n" if ($subParseDebugPoint);
				print STDERR "PPLREF IS $pplref\n" if ($subParseDebugPoint);
				print STDERR "RETURNTYPE IS \"$returntype\"\n" if ($subParseDebugPoint);
				print STDERR "parseTree IS \"$parseTree\"\n" if ($subParseDebugPoint);
				print STDERR "allow_multi IS $allow_multi\n" if ($subParseDebugPoint);
				$lastParserState = $parserState;

				# Do this up front because blockParseReturnState is pulling more precise line numbers that we tuck away in the parse tree.
				$blockOffset = $newblockoffset;
				$inputCounter = $newcount;

				if ($typelist eq "MACRO" && ($returntype !~ /\#define/)) {
					if ($blockmode != 2) {
						if ($returntype =~ /#if/ && $allow_multi) {
							$blockmode = 3; 
							$curObj->isBlock(1);
							print STDERR "BLOCKMODE[4] -> $blockmode\n" if ($localDebug || $blockDebug);
							# print STDERR "RETURN TYPE IS $returntype\n";
							# print STDERR "DECLARATION IS $declaration\n";
							$curObj->addParseTree(\$parseTree);
						} elsif ($returntype =~ /#endif/ && $allow_multi) {
							$blockmode = 0; 
							print STDERR "BLOCKMODE[5] -> $blockmode\n" if ($localDebug || $blockDebug);
							$curObj->addParseTree(\$parseTree);
							print STDERR "CUROBJ: $curObj\n" if ($missingParseTreeDebug);
							my $temp = $curObj->parseTreeList();
							last;
						} elsif ($returntype =~ /#else/ && $allow_multi) {
							$curObj->addParseTree(\$parseTree);
						} elsif ($returntype !~ /#(if|endif|else)/ || $allow_multi) {
							if (@fields != ()) {
								warn("$fullpath:".($inputCounter + $blockOffset).": warning: Macros found between comments and declaration.  Ignoring.\n") if (!$HeaderDoc::test_mode); 
								print STDERR "DEC: $declaration\n" if (!$HeaderDoc::test_mode);
							}
						}
					}
					$subparseInputCounter = $parseTree->{LINENUM};
					$subparseBlockOffset = 0;
					$curObj->linenuminblock($subparseInputCounter);
					$curObj->blockoffset($subparseBlockOffset);
					next;
				}
				$subparseInputCounter = $parseTree->{LINENUM};
				$subparseBlockOffset = 0;
				$curObj->linenuminblock($subparseInputCounter);
				$curObj->blockoffset($subparseBlockOffset);
				print STDERR "NEW INFO: LNIB: $inputCounter BO: $blockOffset\n" if ($subParseDebugPoint);
			}
		} else {
			# NOT subparse
			print STDERR "Parsing\n" if ($localDebug);
			if ($nodec) {
				print STDERR "NODEC\n" if ($localDebug);
				$declaration = "";
				$parseTree = HeaderDoc::ParseTree->new();
				my @ppl = ();
				$pplref = \@ppl;
				$newcount = $inputCounter;
			} else {
					print STDERR "PREIC: $inputCounter\n" if ($localDebug);
				($newcount, $declaration, $typelist, $namelist, $posstypes, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $blockOffset, $conformsToList, $functionContents, $returnedParserState, $nameObjectsRef, $extendsClass, $implementsClass, $propertyAttributes) = &blockParse($fullpath, $blockOffset, \@inputLines, $inputCounter, 0, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);

				# @@@ CHECKME DAG
				if ($curObj->isAPIOwner) {
					$parseTree->apiOwner($curObj);
				} else {
					$parseTree->addAPIOwner($curObj);
				}
				print STDERR "API OWNER FOR $parseTree is ".$parseTree->apiOwner()."\n" if ($localDebug);
				# print STDERR "DECLARATION:\n"; $parseTree->printTree(); print STDERR "END DECLARATION\n";

				 if ($localDebug) {
					print STDERR "IL0: ".$inputLines[0]."\n";
					print STDERR "STARTDEC\n$declaration\nENDDEC\n";
					print STDERR "STARTTREE\n";
					$parseTree->printTree();
					print STDERR "ENDTREE\n";
				}
				if ($inClass) {
					# If they explicitly tagged it, it must be important.
					if ($typelist =~ "forwarddeclaration-") {
						$typelist =~ s/^forwarddeclaration-//;
					}
					if ($typelist eq "MACRO" && ($returntype !~ /\#define/)) {
						# print STDERR "INCLASS: returned $typelist\n";
						if ($returntype =~ /#if/ && $allow_multi) {
							$blockmode = 3; 
							$curObj->isBlock(1);
							print STDERR "BLOCKMODE[6] -> $blockmode\n" if ($localDebug || $blockDebug);
							$curObj->addParseTree(\$parseTree);
						} elsif ($returntype =~ /#endif/ && $allow_multi) {
							$blockmode = 0; 
							print STDERR "BLOCKMODE[7] -> $blockmode\n" if ($localDebug || $blockDebug);
							$curObj->addParseTree(\$parseTree);
							last;
						} elsif ($returntype =~ /#else/ && $allow_multi) {
							$curObj->addParseTree(\$parseTree);
						} elsif ($returntype !~ /#(if|endif|else)/ || $allow_multi) {
							if (@fields != ()) {
								warn("$fullpath:".($inputCounter + $blockOffset).": warning: Macros found between class comments and class.  Ignoring.\n") if (!$HeaderDoc::test_mode);
								print STDERR "DEC: $declaration\n" if (!$HeaderDoc::test_mode);
							}
						}
						$inputCounter = $newcount;
						next;
					}
				} else {
					if ($typelist eq "MACRO" && ($returntype !~ /\#define/)) {
						if ($returntype =~ /#if/ && $allow_multi) {
							$blockmode = 3; 
							$curObj->isBlock(1);
							print STDERR "BLOCKMODE[8] -> $blockmode\n" if ($localDebug || $blockDebug);
							$curObj->addParseTree(\$parseTree);
						} elsif ($returntype =~ /#endif/ && $allow_multi) {
							$blockmode = 0; 
							print STDERR "BLOCKMODE[9] -> $blockmode\n" if ($localDebug || $blockDebug);
							$curObj->addParseTree(\$parseTree);
							last;
						} elsif ($returntype =~ /#else/ && $allow_multi) {
							$curObj->addParseTree(\$parseTree);
						} elsif ($returntype !~ /#(if|endif|else)/ || $allow_multi) {
							if (@fields != ()) {
								warn("$fullpath:".($inputCounter + $blockOffset).": warning: Macros found between comments and declaration.  Ignoring.\n") if (!$HeaderDoc::test_mode);
								print STDERR "DEC: $declaration\n" if (!$HeaderDoc::test_mode);
							}
						}
						$inputCounter = $newcount;
						next;
					}
				}
			}
		}

		print STDERR "HAVE DECLARATION NOW\n" if ($nameObjDebug);

		# print STDERR "TYPELIST: $typelist\n";
		# print STDERR "POSSTYPES: $posstypes\n";
		# print STDERR "NAMELIST: $namelist\n";

		if ($declaration !~ /\S/) { last; }
		$foundMatch = 1;

		if ($hangDebug) {
			print STDERR "DUMPING PARSE TREE:\n";$parseTree->dbprint();print STDERR "PARSETREEDONE\n";
		}

		if ($bpavail && length($bpavail)) { $curObj->availability($bpavail); }
		# print STDERR "BPAVAIL ($namelist): $bpavail\n";

		print STDERR "Left blockParse\n" if ($hangDebug);

		$curObj->privateDeclaration($pridec);
		$curObj->parseTree(\$parseTree);
		# print STDERR "PT: $parseTree\n";
		# print STDERR "PT IS A $parseTree\n";
		# $parseTree->htmlTree();
		# $parseTree->processEmbeddedTags();

		my @parsedParamList = @{$pplref};
		print STDERR "VALUE IS $value\n" if ($localDebug);
		# warn("nc: $newcount.  ts: $typestring.  nl: $namelist\nBEGIN:\n$declaration\nEND.\n");

		$declaration =~ s/^\s*//so;
		# if (!length($declaration)) { next; }

	print STDERR "obtained declaration\n" if ($localDebug);
		if ($localDebug || $nameDebug) {
			print STDERR "IC: $inputCounter\n";
			print STDERR "DC: \"$declaration\"\n"; # if ($localDebug);
			print STDERR "TL: \"$typelist\"\n";
			print STDERR "NL: \"$namelist\"\n";
			print STDERR "PT: \"$posstypes\"\n";
		}

		$inputCounter = $newcount;

		# FIX UP TYPES HERE
		my @oldnames = split(/[,;]/, $namelist);
		my @oldtypes = split(/ /, $typelist);
		my @nameObjects = @{$nameObjectsRef};

		my $i=0;

		if ($nameObjDebug) {
			print STDERR "\n";
			while ($i < scalar(@oldnames)) {
				print STDERR "EXPECTED NAME: ".$oldnames[$i]."\n";
				print STDERR "EXPECTED TYPE: ".$oldtypes[$i]."\n\n";
				$i++;
			}
			foreach my $tempobj (@nameObjects) {
				print STDERR "GOT NAME: ".$tempobj->{NAME}."\n";
			}
			foreach my $tempobj (@nameObjects) {
				print STDERR "GOT TYPE: ".$tempobj->{TYPE}."\n";
			}
		}

		my $curname = $curObj->rawname();
		my $curname_extended = $curObj->rawname_extended();
		# print "CN1: $curname CN2: ".$curObj->name()."\n";
		# my $curname = $curObj->name();

		# print STDERR "NN: $#oldnames CN: \"$curname\"\n";

		# $outertype = $oldtypes[0];
		# my $outername = $oldnames[0];

		$outertype = $nameObjects[0]->{TYPE};
		my $outername = $nameObjects[0]->{NAME};

		# print STDERR "ON: \"$outername\"\n";
		if ($outertype eq "") {
			$outertype = $curtype;
			my $linenum = $inputCounter + $blockOffset;
			if ((!$nodec) && (!$HeaderDoc::test_mode)) {
				warn("$fullpath:$linenum: WARNING: anonymous type.\n");
				warn("IC: $inputCounter\n");
				warn("DC: \"$declaration\"\n");
				warn("TL: \"$typelist\"\n");
				warn("NL: \"$namelist\"\n");
				warn("PT: \"$posstypes\"\n");
			}
		}

		if ($outername eq "") {
			if ($HeaderDoc::enableParanoidWarnings || $curname eq "") {
				my $linenum = $inputCounter + $blockOffset;
				my $withno = "";
				if ($curname eq "") {
					$withno = " with no name provided in comment";
				}
				if (!$HeaderDoc::test_mode) {
					warn("$fullpath:$linenum: WARNING: anonymous type$withno.\n");
					warn("IC: $inputCounter\n");
					warn("DC: \"$declaration\"\n");
					warn("TL: \"$typelist\"\n");
					warn("NL: \"$namelist\"\n");
					warn("PT: \"$posstypes\"\n");
				}

				if ($curname eq "") {
					# Try first parsed parameter name.
					my $ppstring = $parsedParamList[0];
					{
						$ppstring .= ";";
						my @array = ( $ppstring );

						my $parseTree = undef;
						my $simpleTDcontents = "";
						my $bpavail = "";
						my $bogusblockoffset;
						my $conformsToList; # throw this away here.
						my ($foo, $dec, $type, $name, $pt, $value, $pplref, $returntype, $pridec, $tempParseTree, $tempSimpleTDcontents, $tempbpavail, $tempbogusblockoffset, $tempConformsToList, $tempfunctionContents, $ec, $ic, $propertyAttributes) = &blockParse($fullpath, ($inputCounter + $blockOffset), \@array, 0, 1, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);
						$curname = $name;
					}

					# Try first tagged parameter name.
					if ($curname eq "") { $curname = $curObj->firstconstname(); }
					if ($curname ne "" && (!$HeaderDoc::test_mode)) {
						warn("Using first constant name: $curname\n");
					}
					$curname_extended = $curname;
				}
			}
		}

		$nodec = 0;
		# $innertype = $oldtypes[scalar(@oldtypes)-1];
		$innertype = $nameObjects[scalar(@nameObjects)-1]->{TYPE};

		# print STDERR "IT: $innertype\nEXPECTED: ". $oldtypes[scalar(@oldtypes)-1]."\n";

		if ($localDebug) {
			foreach my $obj (@nameObjects) {
				my $ot = $obj->{TYPE};
				print STDERR "TYPE: \"$ot\"\n";
			}
		}

		my $explicit_name_differs = 1;
		my $explicit_name_canonical = 0;
		$curname =~ s/^\s*//o;
		$curname =~ s/\s*$//o;
		$curname_extended =~ s/^\s*//o;
		$curname_extended =~ s/\s*$//o;
		# my @names = ( ); #$curname
		# my @types = ( ); #$outertype

		# my $nameDebug = 1; # Uncomment to enable debugging.
		my $dropped = 0;
		print STDERR "names:\n" if ($nameDebug);

		# Fix up user-specified names that should end in a colon but don't.
		if ($curname !~ /:$/o) {
		    # foreach my $name (@oldnames) {
		    foreach my $obj (@nameObjects) {
			my $name = $obj->{NAME};
			my $NCname = $name;
			my $NCcurname = $curname;
			$NCname =~ s/:$//so;
			$NCcurname =~ s/:$//so;
			print STDERR "NM \"$name\" CN: \"$curname\" [1]\n" if ($nameDebug);
			if ($NCname eq $NCcurname && $name ne $curname) {
			    $curname .= ":";
			    $curObj->name($curname);
			    $curObj->rawname($curname);
			# } elsif ($name eq $curname) {
			    # $dropped = 1;
			    # print STDERR "dropped ($name = $curname)\n" if ($nameDebug);
			}
		    }
		}

		# Fix up user-specified names that should not end in a colon but do.
		# Also check for a type conversion request.  Just a bit of explanation.
		# A conversion request occurs when the main object is not of the correct
		# type to accept the declaration that follows it, where the following
		# declaration is of a type that ostensibly should be allowed to silently
		# against that type, but where the declaration cannot be safely assigned
		# to a curObj of that type for technical reasons (missing functions in
		# the object, for example).

		my $conversion_requested = 0;
		my $newcurtype = "UNKNOWN";

		print STDERR "Initial name object count: ".scalar(@nameObjects)."\n" if ($nameDebug);
		# foreach my $name (@oldnames)
		foreach my $obj (@nameObjects) {
			my $pt = $obj->{POSSTYPES};

			if ($curname =~ /:$/o) {
				my $name = $obj->{NAME};
				my $NCname = $name;
				my $NCcurname = $curname;
				$NCname =~ s/:$//so;
				$NCcurname =~ s/:$//so;
				print STDERR "NM \"$name\" CN: \"$curname\" [2]\n" if ($nameDebug);
				if ($NCname eq $NCcurname && $name ne $curname) {
				    $curname = $NCcurname;
				    $curObj->name($curname);
				    $curObj->rawname($curname);
				# } elsif ($name eq $curname) {
				    # $dropped = 1;
				    # print STDERR "dropped ($name = $curname)\n" if ($nameDebug);
				    $curname_extended = $curObj->rawname_extended();
				}
			}

			print STDERR "\nLOOKING FOR $curtype*\n" if ($nameDebug);
			print STDERR "POSS $pt\n" if ($nameDebug);
			print STDERR "NAME OBJ TYPE IS ".$obj->{TYPE}."\n" if ($nameDebug);
			print STDERR "CUROBJ IS $curObj\n" if ($nameDebug);
			if ((!$conversion_requested) && ($pt =~ /\Q$curtype\E\*/) && (!$curObj->{INSERTED})) {
				print STDERR "MATCH: CONVERSION REQUESTED => 1\n" if ($nameDebug);
				$conversion_requested = 1;
				$newcurtype = $obj->{TYPE};
			} elsif ($pt =~ /\Q$curtype\E/) {
				print STDERR "EXACT MATCH: CONVERSION REQUESTED => -1\n" if ($nameDebug);
				$conversion_requested = -1;
				$newcurtype = $obj->{TYPE};
			} else {
				print STDERR "NO CONVERSION MATCH.\nPT: $pt\nCT: $curtype\nOT: $obj->{TYPE}\n" if ($nameDebug);
			}
		}

		print STDERR "CR is $conversion_requested\n" if ($nameDebug);

		$curname =~ s/^\s*//o;
		$curname =~ s/\s*$//o;
		$curname_extended =~ s/^\s*//o;
		$curname_extended =~ s/\s*$//o;

		print STDERR "endnames\n" if ($nameDebug);
	# print STDERR "DROPPED: $dropped\n";

		if ((!length($curname) && $blockmode) || ($conversion_requested == 1)) {
			print STDERR "CONVERSION PATH\n" if ($nameDebug);
			my $allmatch = 1;
			if ($conversion_requested == 1) {
				# $nameDebug = 1;
				print STDERR "CONVERSION REQUESTED\n" if ($nameDebug);
				print STDERR "NAMES:\n" if ($nameDebug);
			} else {
				print STDERR "HAS NO CURNAME IN BLOCK MODE\n" if ($nameDebug);
				print STDERR "NAMES:\n" if ($nameDebug);
			}
			
			my $count = 0;
			print STDERR "CURTYPE: $curtype\n" if ($nameDebug);
			# foreach my $name (@oldnames) {
			foreach my $obj (@nameObjects) {
				my $name = $obj->{NAME};
				if (!length($curname)) {
					$curname = $name;
					print STDERR "CHANGING CURTYPE FROM $curtype to ".$obj->{TYPE}."\n" if ($nameDebug);
					print STDERR "CURTYPE CHANGE[1]\n" if ($nameDebug);
					$curtype = $obj->{TYPE}; # $oldtypes[$count];
					if (($curtype ne $origcurtype) && ($origcurtype ne "UNKNOWN")) {
						my $possfound = 0;
						my $pt = $obj->{POSSTYPES}; # $pt;
						my @ptlist = split(/\s/, $posstypes);
						foreach my $pt (@ptlist) {
							my $pttrim = $pt;
							$pttrim =~ s/\W//sg;
							if ($pttrim eq $curtype) { $possfound = 1; }
							if ($pttrim eq $curtype."*") { $possfound = 2; }
							print STDERR "CMP_P $pttrim $curtype\n" if ($nameDebug);
						}
						if (!$possfound) {
							warn "WARNING: Not all types in block match ($curtype != $origcurtype).\n";
						}
					}
				} elsif ($name ne $curname) {
					print STDERR "NAMEMATCH: $name NE $curname\n" if ($nameDebug);
					$allmatch = 0;
				}
				print STDERR "NAME: $name\n" if ($nameDebug);
				$count++;
			}
			$curname =~ s/^\s*//o;
			$curname =~ s/\s*$//o;

			if ($conversion_requested == 1) {
				print STDERR "CURTYPE CHANGE[2]\n" if ($nameDebug);
				$curtype = $newcurtype;
			}
			print STDERR "CURTYPE NOW: $curtype\n" if ($nameDebug);
			print STDERR "ENDNAMES:\n" if ($nameDebug);
			if (!$allmatch && !$conversion_requested) {
				warn "WARNING: No name found in block mode and names do not match.  Using first.\n" if (!$HeaderDoc::test_mode);
				# $curname =~ s/^_//;
				# $curname .= "_multideclaration_block";
			}
			# push(@names, $curname);
			# push(@types, $outertype);
			# $curname_inserted = 1;
			my $newcurObj;
			my $typestring = $curtype;
			($newcurObj, $classType, $varIsConstant) = objForType( $curObj, $typedefname, $typestring,
					$posstypes, $outertype, $curtype, $classType, $classKeyword, $declaration,
					\@fields, $functionGroup, $varIsConstant, $blockmode, $inClass, $inInterface,
					$inTypedef, $inStruct, $fullpath, $inputCounter, $blockOffset, $lang, 0, $functionContents,
					$apiOwner, $subparseInputCounter, $subparseBlockOffset, $extendsClass, $implementsClass);

			print STDERR "CUROBJ ON RETURN: $curObj\n" if ($nameDebug);
			print STDERR "NEWCUROBJ ON RETURN: $newcurObj\n" if ($nameDebug);
			
			my @keys = keys %{$curObj};
			print STDERR "Cloning object $curObj to $newcurObj...\n" if ($nameDebug);
			foreach my $key (@keys) {
				# print STDERR "$key => ".$curObj->{$key}."\n";
				if ($key ne "CLASS") {
					$newcurObj->{$key} = $curObj->{$key};
				}
			}
			print STDERR "NEWCUROBJ AFTER CLONE: $newcurObj\n" if ($nameDebug);
			print STDERR "NEWCUROBJ CLASS AFTER CLONE: ".$newcurObj->{CLASS}."\n" if ($nameDebug);

			# if ($curObj->can("masterEnum") && $curObj->masterEnum())
				# $newcurObj->masterEnum($curObj->masterEnum());
			# }
			my $group = $curObj->group();
			$newcurObj->group($group);
			$curObj->apiOwner()->removeFromGroup($group, $curObj);
			$parseTree->apiOwnerSub($curObj, $newcurObj); # @@@
			$curObj = $newcurObj;
			$curObj->apiOwner($apiOwner);
			$blockCurNameAutoGenerated = 1;
		} elsif ($blockCurNameAutoGenerated == 1) {
			my $count = 0;
			print STDERR "CURNAME AUTO PATH (BLOCKMODE $blockmode)\n" if ($nameDebug);
			my $allmatch = 1;
			my $typematch = 1;
			# foreach my $name (@oldnames) {
			foreach my $obj (@nameObjects) {
				my $name = $obj->{NAME};
				if (!length($curname)) {
					$curname = $name;
					if (!length($curtype)) {
						print STDERR "CURTYPE CHANGE[3]\n" if ($nameDebug);
						$curtype = $obj->{TYPE}; # $oldtypes[$count];
					} elsif ($curtype ne $obj->{TYPE}) {
						$typematch = 0;
					}
				} elsif ($name ne $curname) {
					$allmatch = 0;
				}
				print STDERR "NAME: $name\n" if ($nameDebug);
				$count++;
			}
			if (!$allmatch || !$typematch) {
				if (!$allmatch) {
					warn "WARNING: No name found in block mode and names do not match.  Using first.\n" if (!$HeaderDoc::test_mode);
				}
				if (!$typematch) {
					warn "WARNING: No name found in block mode and types do not match.  Using first.\n" if (!$HeaderDoc::test_mode);
				}
				# $curname =~ s/^_//;
				$curname .= "_multideclaration_block";
				$blockCurNameAutoGenerated = 2;
			}
		}

		if (!$firstBlockObjType) {
			$firstBlockObjType = $newcurtype;
			$firstBlockCurType = $curtype;
		}

		print STDERR "ORIG CURTYPE $origcurtype FIRST CURTYPE WAS $firstBlockCurType\n".
		             "CURTYPE $curtype INFUNCTION: $inFunction\n" if ($nameObjDebug || $nameDebug);

		# Normally, initialize to 0, but initialize to 1 if
		# we're in an @defineblock block.  The parsed type
		# of a block define doesn't match the type generated
		# by the presence of an @defineblock directive
		# (intentionally to avoid an early exit).  Thus,
		# the type comparison code fails for that case.
		# Fortunately, in that case, we always want to add
		# the name, so we just initialize this to the value
		# stored in $blockmode....  :-)

		# The curname (user-specified name) should be inserted
		# only if:
		# 	1.  The type matches.
		#	2.  The name does not match.

		# See if the type matches.
		my $docname = "";
		my $found = $blockmode;

		print STDERR "Current APIOwner is ".$apiOwner->name()."\n" if ($localDebug || $nameDebug);
		# foreach my $ot (@oldtypes) {
		foreach my $obj (@nameObjects) {
			my $ottrim = $obj->{TYPE};
			$ottrim =~ s/\W//sg;

			# DAG - Not sure why, but in some cases, the types
			# from $typelist are simply listed as 'define'
			# instead of '#define'.  Fix those up.
			if ($ottrim eq "define") { $ottrim = "#define"; }

			# Now do the comparison....
			if ($ottrim eq $curtype) { $found = 1; }
			print STDERR "CMP_O $ottrim $curtype\n" if ($nameDebug);
			# print "CUROBJ IS $curObj\n" if ($nameDebug);
		}
		my @ptlist = split(/\s/, $posstypes);
		# foreach my $pt (@ptlist) {
		foreach my $obj (@nameObjects) {
			my $pt = $obj->{POSSTYPES}; # $pt;
			my @ptlist = split(/\s/, $posstypes);
			foreach my $pt (@ptlist) {
				my $pttrim = $pt;
				$pttrim =~ s/\W//sg;
				if ($pttrim eq $curtype) { $found = 1; }
				if ($pttrim eq $curtype."*") { $found = 2; }
				print STDERR "CMP_P $pttrim $curtype\n" if ($nameDebug);
			}
		}

		my $curname_inserted = 0;
		my $curObjIsTainted = 0;
		# Insert curname into list if it matches the current type
		# or if there is no other name.
	# print STDERR "TEST POINT\n";

		if (length($curname) && length($curtype)) {
			print STDERR "HAS CURNAME\n" if ($nameDebug);
			my $foundName = findMatch(\@nameObjects, "NAME", '\S');
			if ($found || !$foundName) {
				if (!$foundName) { 
					print STDERR "Explicit name is canonical name\n" if ($nameDebug);
					$explicit_name_canonical = 1;
				} else {
					print STDERR "Found matching name\n" if ($nameDebug);
				}

				my $nameobj = HeaderDoc::TypeHelper->new();
				$nameobj->{NAME} = $curname;
				$nameobj->{TYPE} = $outertype;
       				$nameobj->{POSSTYPES} = $nameObjects[0]->{POSSTYPES};
				$nameobj->{INSERTEDAT} = "OUTER 1";
				$nameobj->{ACTIVE} = 1;
				print STDERR "CREATING AS ACTIVE: $curname\n" if ($nameObjDebug);

				if (($curtype ne $outertype) && !$explicit_name_canonical && !$blockmode) {
					print STDERR "TYPE MISMATCH for $nameobj" if ($nameDebug);
					$curObj->unregister();
					$curObjIsTainted = $nameobj;
				} else {
					@nameObjects = ( ($nameobj), (@nameObjects));

					# push(@names, $curname);
					# push(@types, $outertype);

					# (( $obj->{TYPE} eq $curtype && ($cmpname eq $cmpcurname || !length($curname))) && ((!$curObj->{INSERTED}) || $typestring eq $firstBlockCurType))
					# $obj->{TYPE};

					$curname_inserted = 1;
				}
			} else {
				print STDERR "NOT FOUND AND NAME COUNT NONZERO\n" if ($nameDebug);
			}
		} else {
			print STDERR "HAS NO CURNAME NOT IN BLOCK MODE\n" if ($nameDebug);
		}

		my $on = scalar(@nameObjects);
		print STDERR "NUMNAMES: ".scalar(@nameObjects).", FOUND: $found CURNAME: $curname, ".$curObj->name."\n" if ($nameDebug);
		if ($localDebug || $nameDebug) {
			foreach my $nameObj (@nameObjects) {
				my $name = $nameObj->{NAME};
				my $type = $nameObj->{TYPE};
				print STDERR "NAME $name TYPE $type\n";
			}
		}
		# print STDERR "COAPIO: ".$curObj->apiOwner()." APIO: $apiOwner\n";
		my $count = 0;
		my $operator = 0;
		# if ($typelist eq "operator") {
			# $operator = 1;
		# }
		# foreach my $name (@oldnames) {

		# We don't want to insert an object that could potentially cause a tainted
		# curObj to appear in the output, so if its name matches the name and
		# type of an actual object (e.g. "@struct _foo" for
		# "typedef struct _foo { ...} foo") because that would cause a crash.
		# However, if there is no name/type match, we need to insert the object or
		# the alternate "doc" naming won't work.  The following code checks for this
		# condition and inserts the object if needed.
		if ($curObjIsTainted) {
			my $foundMatchWithTaintedObject = 0;
			foreach my $obj (@nameObjects) {
				if ($obj == $curObjIsTainted) {
					next;
				}
				if ($obj->{NAME} eq $curObjIsTainted->{NAME}) {
					$foundMatchWithTaintedObject = 1;
				}
			}
			if (!$foundMatchWithTaintedObject) {
				@nameObjects = ( ($curObjIsTainted), (@nameObjects));
				$curname_inserted = 1;
			}
		}
		foreach my $obj (@nameObjects) {
			if ($obj->{ACTIVE}) {
				# Skip the main name that we just added.
				next;
			}
			# my $name = $obj->{NAME};
			# my $objtype = $obj->{TYPE};

			my $operator = 0;
			if ($obj->{TYPE} eq "operator") {
				$operator = 1;
			}
			print STDERR "OPERATOR: $operator\n" if ($nameDebug || $nameObjDebug);

			if (!length($obj->{NAME})) {
				# Anonymous enum
				next;
			}

			if ($operator) {
				$obj->{NAME} =~ s/^\s*operator\s*//so;
				$obj->{NAME} = "operator ".$obj->{NAME};
				$curname =~ s/^operator(\s*)(\S+)/operator $2/so;
			}

			print STDERR "NAME \"$obj->{NAME}\"\nCURNAME \"$curname\"\nOUTERTYPE \"$outertype\"\nOBJTYPE \"".$obj->{TYPE}."\"\n" if ($nameDebug);

			# If we have a valid tag name, we normally don't insert it if it is the same as the
			# parsed name to avoid duplication.  If we didn't insert it before, though, we had
			# better insert it here.  It will be inserted either above or here, depending on
			# whether we have a type match.  If we have an anonymous type, it will always be
			# inserted above.
			# if ((($obj->{NAME} eq $curname) && ($oldtypes[$count] eq $outertype)) && $curname_inserted) 
			if ((($obj->{NAME} eq $curname) && ($obj->{TYPE} eq $outertype)) && $curname_inserted)  {
				print STDERR "Explicit name matches parsed.\n" if ($nameDebug);
				# We've already inserted this name.
				$explicit_name_differs = 0;
				$count++;
			} else {
				# print STDERR "Explicit name ($curname) does NOT match parsed ($obj->{NAME}) or type ($outertype) does not match (".$oldtypes[$count].") or not curname_inserted ($curname_inserted).  Adding to list.\n" if ($nameDebug);
				print STDERR "Explicit name ($curname) does NOT match parsed ($obj->{NAME}) or type ($outertype) does not match (".$obj->{TYPE}.") or not curname_inserted ($curname_inserted).  Adding to list.\n" if ($nameDebug);
				if (($obj->{NAME} eq $curname)) { $curname_inserted = 1; }
				print STDERR "DOCNAME: $curname\n" if ($nameDebug);
				$docname = $curname;
				# push(@names, $obj->{NAME});
				# push(@types, $oldtypes[$count++]);

				print STDERR "ACTIVATING: $obj->{NAME}\n" if ($nameObjDebug);
				$obj->{ACTIVE} = 1;
			}
		}
		# if (!$curname_inserted && $found) {
			# push(@names, $curname);
			# push(@types, $outertype);
		# }
		# foreach my $xname (@names) { print STDERR "XNAME: $xname\n"; }
		if ($hangDebug) { print STDERR "Point A\n"; }
		# $explicit_name_differs = 0;

		print STDERR "END: $explicit_name_differs; ENC: $explicit_name_canonical\n" if ($nameDebug);

		# If we are the only name for an object, the user-entered (tagged/explicit) name is
		# treated as canonical for apple_ref purposes.  Otherwise, the user-
		# entered name is just a special doc name.
		if ($explicit_name_differs && !$explicit_name_canonical && ($curname_inserted || $curname =~ /\W/) && $found) {
			# The tagged name is different and is not the only name for this object

			print STDERR "Setting APPLEREFISDOC for $curObj : \"".$curObj->name()."\" [1]\n" if ($nameDebug);
			$curObj->appleRefIsDoc(1);
			my $prevignore = $HeaderDoc::ignore_apiuid_errors;
			$HeaderDoc::ignore_apiuid_errors = 1;
			my $junk = $curObj->apirefSetup(1);
			print STDERR "APIREF: ".$curObj->apiref()."\n" if ($nameDebug);
			$HeaderDoc::ignore_apiuid_errors = $prevignore; 
		} else {
			# Either the tagged name is different or there is no parsed name.

			if ($curname_extended =~ /[^\w\:]/) {
				# The tagged name contains illegal characters.  This cannot be treated as a code symbol name.

				print STDERR "Setting APPLEREFISDOC for $curObj : \"".$curObj->name()."\" [2]\n" if ($nameDebug);

				if ($explicit_name_canonical) {
					# If there is no parsed namem, embedded constants and similar should be treated
					# as code symbols because this is the only place they are documented.
					$curObj->appleRefIsDoc(2);
				} else {
					$curObj->appleRefIsDoc(1);
				}
				if ($curObj->{INSERTED}) {
					my $prevignore = $HeaderDoc::ignore_apiuid_errors;
					$HeaderDoc::ignore_apiuid_errors = 1;
					my $junk = $curObj->apirefSetup(1);
					print STDERR "APIREF: ".$curObj->apiref()."\n" if ($nameDebug);
					$HeaderDoc::ignore_apiuid_errors = $prevignore; 
				}
			} else {
				print STDERR "Clearing APPLEREFISDOC for $curObj : ".$curObj->name()."\n" if ($nameDebug);
				$curObj->appleRefIsDoc(0);

				# print "INSERTED: ".$curObj->{INSERTED}."\n";
				if ($curObj->{INSERTED}) {
					my $prevignore = $HeaderDoc::ignore_apiuid_errors;
					$HeaderDoc::ignore_apiuid_errors = 1;

					# $curObj->dbprint();
					my $junk = $curObj->apirefSetup(1);
					print STDERR "APIREF: ".$curObj->apiref()."\n" if ($nameDebug);
					$HeaderDoc::ignore_apiuid_errors = $prevignore; 
				}
			}
		}
		print STDERR "CUROBJ IS $curObj\n" if ($nameDebug);
		my $matching_declaration = 0;
		if ($outertype eq $curtype || $innertype eq $curtype || findMatch(\@nameObjects, "POSSTYPES", $curtype)) {
			if (findMatch(\@nameObjects, "POSSTYPES", $curtype.'\*')) {
				$matching_declaration = 2;
			} else {
				$matching_declaration = 1;
			}
			# Make sure we have the right UID for methods
			$curObj->declaration($declaration);
			# $HeaderDoc::ignore_apiuid_errors = 1;

			# my $oldisdoc = $curObj->appleRefIsDoc();
			# $curObj->appleRefIsDoc(1);
			# my $junk = $curObj->apirefSetup();
			# $curObj->appleRefIsDoc($oldisdoc);
			# $HeaderDoc::ignore_apiuid_errors = 0;
		} else {
			print STDERR "NOMATCH: OUTER: $outertype CUR: $curtype INNER: $innertype\n" if ($nameDebug);
		}

		$count = 0;
		# foreach my $name (@names) {
		print STDERR "ENTERING NAMEOBJECT LOOP.  CURTYPE IS $curtype CUROBJ is $curObj\n" if ($nameObjDebug || $nameDebug);
		foreach my $obj (@nameObjects) {
		    print STDERR "IN NAMEOBJECT LOOP.  CURTYPE IS $curtype CUROBJ is $curObj\n" if ($nameObjDebug || $nameDebug);
		    # print STDERR "NAMEOBJECT: NAME: ".$obj->{NAME}." TYPE: ".$obj->{TYPE}."\n";
		    if (!$obj->{ACTIVE}) {
			print STDERR "SKIPPING OBJECT $obj (NAME ".$obj->{NAME}.") because it is inactive\n" if ($nameDebug || $nameObjDebug);
			# This one wasn't marked for inclusion.
			next;
		    } else {
			print STDERR "ADDING OBJECT $obj (NAME ".$obj->{NAME}.") because it is active\n" if ($nameDebug || $nameObjDebug);
		    }
		    my $name = $obj->{NAME};
		    my $typestring = $obj->{TYPE};
		    my $posstypes = $obj->{POSSTYPES};

		    my $outerLocalDebug = $localDebug;
		    my $localDebug = 0 || $nameDebug || $nestClassDebug;
		    # my $typestring = $types[$count++];
		    my $rawname = $name;

		    print STDERR "NAME IS \"$name\"\n" if ($localDebug);
		    print STDERR "CURNAME IS \"$curname\"\n" if ($localDebug);
		    print STDERR "TYPESTRING IS $typestring\n" if ($localDebug);
		    print STDERR "CURTYPE IS $curtype\n" if ($localDebug);
			print STDERR "MATCH: $name IS A $typestring.\n" if ($localDebug);

print STDERR "DEC ($name / $typestring): $declaration\n" if ($localDebug && $outerLocalDebug);

		    $name =~ s/\s*$//go;
		    $name =~ s/^\s*//go;
		    my $cmpname = $name;
		    my $cmpcurname = $curname;
		    $cmpname =~ s/:$//so;
		    $cmpcurname =~ s/:$//so;
		    if (!length($name)) { next; }
		    my $extra_needs_setup = 0;

			print STDERR "Got $name ($curname)\n" if ($localDebug);
			print STDERR "POSSTYPES: $posstypes\n" if ($nameDebug || $nameObjDebug);

		print "CUROBJ CHECKPOINT: $curObj\n" if ($nameObjDebug);

			my $extra = undef;

			if ($blockmode && ($typestring ne $firstBlockObjType) && ($typestring ne $firstBlockCurType)) {
				warn "WARNING: Block declaration contains multiple types\n    ($typestring != $firstBlockObjType)\n";
			}
			# print STDERR "$typestring eq $curtype && ($cmpname eq $cmpcurname || !length($curname))\n";

			print "CUROBJ: $curObj INSERTED: $curObj->{INSERTED}\n" if ($nameDebug);
			# print STDERR "(($typestring eq $curtype && ($cmpname eq $cmpcurname || !length($curname))) && ((!$curObj->{INSERTED}) || $typestring eq $firstBlockCurType))" if ($nameDebug);
			if ((!$curObjIsTainted) && (($typestring eq $curtype && ($cmpname eq $cmpcurname || !length($curname))) && ((!$curObj->{INSERTED}) || $typestring eq $firstBlockCurType))) {
				print STDERR "$curtype = $typestring\n" if ($localDebug);
				# $HeaderDoc::ignore_apiuid_errors = 1;
				# my $junk = $curObj->apirefSetup();
				# $HeaderDoc::ignore_apiuid_errors = 0;

				$extra = $curObj;

				print STDERR "EXTRA IS CUROBJ ($extra) FROM NAME OBJECT $obj\n" if ($nameObjDebug);

				if ($curObj->{INSERTED}) {
					$curObj->{INSERTED} = 2;
				} else {
					$curObj->{INSERTED} = 1;
				}
# print STDERR "E=C\n$extra\n$curObj\n";

				push(@linkobjs, \$extra);
				if ($blockmode) {
					$blockDec .= $declaration;
					print STDERR "SPDF[1]\n" if ($hangDebug);
					$curObj->isBlock(1);
					# $curObj->setDeclaration($blockDec);
					print STDERR "END SPDF[1]\n" if ($hangDebug);
					# $declaration = $curObj->declaration() . $declaration;
				}
			} else {
			    $extra_needs_setup = 1;
				print STDERR "NAME IS $name\n" if ($localDebug);
			    if ($curtype eq "function" && $posstypes =~ /function/o) {
				$curtype = "UNKNOWN";
print STDERR "setting curtype to UNKNOWN\n" if ($localDebug);
			    }
			    if ($typestring =~ /forwarddeclaration-/) {
				$bail = 1;
				## $inputCounter--; ## @@@ DAG CHECKME @@@
				## print STDERR "DECREMENTED INPUTCOUNTER [4]\n" if ($HeaderDoc::inputCounterDebug);
				last;
			    }
			    ($extra, $classType, $varIsConstant) = objForType( $curObj, $typedefname, $typestring,
					$posstypes, $outertype, $curtype, $classType, $classKeyword, $declaration,
					\@fields, $functionGroup, $varIsConstant, $blockmode, $inClass, $inInterface,
					$inTypedef, $inStruct, $fullpath, $inputCounter, $blockOffset, $lang, $outerLocalDebug,
					$functionContents, $apiOwner, $subparseInputCounter, $subparseBlockOffset,
					$extendsClass, $implementsClass);

			print "EXTRA ON RETURN IS $extra\n" if ($nameDebug || $nameObjDebug);
			}
			my $class = ref($extra) || $extra;
			if ($class =~ /HeaderDoc::HeaderElement/) {
				next;
			}

			# print STDERR "CHECKING TYPE: ";
			if ($extra) {
				if ($matching_declaration) {
					# print STDERR "$name: YES ($extra)\n";
					$extra->suppressChildren(0);
				} else {
					# print STDERR "$name: NO ($extra)\n";
					$extra->suppressChildren(1);
				}
			}

			# print STDERR "CHECKING NAME: ";
			# if ($extra && ($rawname eq $docname) && (!$explicit_name_canonical || !$matching_declaration )) {
				# print STDERR "NAME: $rawname DOCNAME: $docname EXPLICIT: $explicit_name_canonical CURTYPE: $curtype TYPESTRING: $typestring MATCHING: $matching_declaration\n";
				# # $extra->appleRefIsDoc(1);
				# print STDERR "YES\n";
			# } else {
				# if ($extra) {
					# # $extra->appleRefIsDoc(0);
					# # $extra->wipeUIDCache();
					# # $extra->apirefSetup(1);
					# print STDERR "NO: $docname != $rawname \n";
				# }
			# }

			# print STDERR "HERE\n";
			if ($hangDebug) { print STDERR "Point NEWB\n"; }
			if ($curtype eq "UNKNOWN" && $extra && !$blockmode) {
				my $orig_parsetree_ref = $curObj->parseTree();
				bless($orig_parsetree_ref, "HeaderDoc::ParseTree");
				my $pt = ${$orig_parsetree_ref};
				$curObj = $extra;
				$curObj->apiOwner($apiOwner);

				my $prevignore = $HeaderDoc::ignore_apiuid_errors;
				$HeaderDoc::ignore_apiuid_errors = 2;

				# my $oldisdoc = $curObj->appleRefIsDoc();
				# $curObj->appleRefIsDoc(1);
				# my $junk = $curObj->apirefSetup();
				# $curObj->appleRefIsDoc($oldisdoc);

				$HeaderDoc::ignore_apiuid_errors = $prevignore;

				# @@@ CHECKME DAG
				if ($extra->isAPIOwner) {
					$pt->apiOwner($extra);
				} else {
					$pt->addAPIOwner($extra);
				}
				$curObj->parseTree($orig_parsetree_ref);
			} else {
				print STDERR "NOT SETTING PARSETREE.  Current type is $curtype\n"."\n" if ($missingParseTreeDebug);
				print STDERR "CUROBJ IS $curObj\n" if ($missingParseTreeDebug);
				print STDERR "CUROBJ PT IS".$curObj->parseTree()."\n" if ($missingParseTreeDebug);
			}
			if ($extra) {
				print STDERR "Processing \"extra\" ($extra).\n" if ($localDebug);
				print STDERR "EXTRANAME: \"".$name."\"\n" if ($localDebug);
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

				my $discussion_set = $curObj->discussion_set();
				my $discussion = $curObj->raw_discussion();
				my $override_discussion = undef;
				my $nameline_discussion = $curObj->raw_nameline_discussion();
				print STDERR "OLD DISCUSSION: $discussion\n" if ($nameDebug);
				print STDERR "OLD NAMELINE DISCUSSION: $nameline_discussion\n" if ($nameDebug);
				my $pridec = $curObj->privateDeclaration();
				$extra->privateDeclaration($pridec);

				if ($curObj != $extra) {
					my $orig_parsetree_ref = $curObj->parseTree();
					print STDERR "CO != EX.  PTREF is $orig_parsetree_ref\n"."\n" if ($missingParseTreeDebug);
					# my $orig_parsetree = ${$orig_parsetree_ref};
					bless($orig_parsetree_ref, "HeaderDoc::ParseTree");

					# @@@ CHECKME DAG
					if ($extra->isAPIOwner) {
						$$orig_parsetree_ref->apiOwner($extra);
					} else {
						$$orig_parsetree_ref->addAPIOwner($extra);
					}
					$extra->parseTree($orig_parsetree_ref); # ->clone());
					# my $new_parsetree = $extra->parseTree();
					# bless($new_parsetree, "HeaderDoc::ParseTree");
					# $new_parsetree->addAPIOwner($extra);
					# $new_parsetree->processEmbeddedTags();
				} else {
					print STDERR "CO == EX.\n"."\n" if ($missingParseTreeDebug);
				}
				# print STDERR "PROCESSING CO $curObj EX $extra\n";
				# print STDERR "PT: ".$curObj->parseTree()."\n";

				if ($blockmode) {
					my $parmDescDebug = $parmDebug || 0;
					# my $altDiscussionRef = $curObj->checkAttributeLists("Included Defines");
					print STDERR "Looking for discussion for \"$name\"\n" if ($parmDescDebug);
					my $discussionParam = $curObj->taggedParamMatching($name);
					print STDERR "GOT OBJECT $discussionParam\n" if ($parmDescDebug);
	print STDERR "SELF: $curObj DP: $discussionParam\n" if ($parmDescDebug); #  ADP: $altDiscussionRef
					if ($discussionParam) {
						my $altdiscussion = $discussionParam->halfbaked_discussion();
						print "AD IS: $altdiscussion\n" if ($parmDescDebug);
						if ($altdiscussion =~ /\S/) { $override_discussion = $altdiscussion; }
						$discussionParam->{MAINOBJECT} = \$extra;
						# print "MAINOBJECT: ".$discussionParam->{MAINOBJECT}."\n";
						# print "EXTRA: $extra\n";
						# print STDERR "SET DISCUSSION TO $override_discussion\n" if ($parmDescDebug);
					# } elsif ($altDiscussionRef) {
						# my @altDiscEntries = @{$altDiscussionRef};
						# foreach my $field (@altDiscEntries) {
						    # my ($dname, $ddisc, $is_on_nameline) = &getAPINameAndDisc($field);
						    # if ($name eq $dname) {
						    	# if (!$is_on_nameline) {
								# $discussion = $ddisc;
								# $discussion_set = 1;
								# $nameline_discussion = "";
							# } else {
								# $nameline_discussion = $ddisc;
								# $discussion = "";
							# }
						    # }
						# }
					}
					if ($curObj != $extra) {
						# we use the parsed parms to
						# hold subdefines.
						$curObj->addParsedParameter($extra);
					}
				}

				print STDERR "Point B1\n" if ($hangDebug);
				if ($extraclass ne "HeaderDoc::Method" && !$extra->isAPIOwner()) {
					print STDERR "Point B2\n" if ($hangDebug);
					my $paramName = "";
					my $position = 0;
					my $type = "";
					if ($extraclass eq "HeaderDoc::Function") {
						$extra->returntype($returntype);
					}
					my @tempPPL = @parsedParamList;
					foreach my $parsedParam (@tempPPL) {
						# the real code
						my $ppDebug = 0 || $parmDebug;

						print STDERR "PARSED PARAM: \"$parsedParam\"\n" if ($ppDebug);

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
						my @nameObjects = ();
						if ($ppstring eq "...") {
							# $name = $ppstring;
							# $type = "";
							# $pt = "";

							my $nameobj = HeaderDoc::TypeHelper->new();
							$nameobj->{NAME} = $ppstring;
							$nameobj->{TYPE} = "";
							$nameobj->{POSSTYPES} = "";
							$nameobj->{INSERTEDAT} = "varargs";
							push(@nameObjects, $nameobj);
						} else {
							$ppstring .= ";";
							my @array = ( $ppstring );

							my $parseTree = undef;
							my $simpleTDcontents = "";
							my $bpavail = "";
							my $bogusblockoffset;
							my $conformsToList; # throw this away here.
							my $ec = ""; my $ic = "";
							($foo, $dec, $type, $name, $pt, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $bogusblockoffset, $conformsToList, $functionContents, $returnedParserState, $nameObjectsRef, $ec, $ic, $propertyAttributes) = &blockParse($fullpath, $extra->linenum(), \@array, 0, 1, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);
							@nameObjects = @{$nameObjectsRef};
						}

						if ($ppDebug) {
							print STDERR "NAME: $name\n";
							print STDERR "TYPE: $type\n";
							print STDERR "PT:   $pt\n";
							print STDERR "RT:   $returntype\n";
						}

						foreach my $obj (@nameObjects) {
							my $name = $obj->{NAME};

							print STDERR "NAME: $name TYPE: $returntype\n" if ($ppDebug);
							my $param = HeaderDoc::MinorAPIElement->new();
							$param->apiOwner($apiOwner);
							if (defined($subparseInputCounter)) {
								$param->linenuminblock($subparseInputCounter);
								$param->blockoffset($subparseBlockOffset);
							} else {
								$param->linenuminblock($inputCounter);
								$param->blockoffset($blockOffset);
							}
							# $param->linenum($inputCounter+$blockOffset);
							$param->outputformat($extra->outputformat);
							$returntype =~ s/^\s*//s;
							$returntype =~ s/\s*$//s;

							if ($obj->{NAME} eq "") {
								warn("Anonymous data structure inside struct or union.\n");
								warn("LISTING:\n$parsedParam\nEND LISTING\n");
								warn("OWNER IS: $rawname\n");
								next;
							}
							print STDERR "BLOCKPARSE PARAMETER PARSE RETURNED NAME $obj->{NAME}\n" if ($ppDebug);
							print STDERR "RETURNTYPE IS $returntype\n" if ($ppDebug);

							    # ($returntype =~ /(^|\s)(struct|union|enum|record|typedef)$/)
# print STDERR "SL: $HeaderDoc::sublang\n";
							if ($extraclass =~ /HeaderDoc::Function/ && $HeaderDoc::sublang ne "php") {
								# Handle cases where a function is given with no actual
								# parameter name.
								if ($lang ne "pascal" && $HeaderDoc::sublang ne "MIG" && 
								    (($structname && $returntype =~ /(^|\s)\Q$structname\E$/) ||
								     ($unionname && $returntype =~ /(^|\s)\Q$unionname\E$/) ||
								     ($enumname && $returntype =~ /(^|\s)\Q$enumname\E$/) ||
								     ($typedefname && $returntype =~ /(^|\s)\Q$typedefname\E$/))) {
									print STDERR "OOPS\n";
									$returntype .= " ".$obj->{NAME};
									$name = "";
								} elsif (!length($returntype)) {
									$returntype .= " $name";
									if ($name !~ /\.\.\./) {
										$name = ""; # WAS anonymous$name, which broke parameter checking
									}
								}
							}
							print STDERR "NM: $name RT $returntype\n" if ($ppDebug);
							$param->name($name);
							$param->position($position++);
							$param->type($returntype);
							$extra->addParsedParameter($param);
						}
					}
				} elsif ($extraclass eq "HeaderDoc::Method") {
					# we're a method
					$extra->returntype($returntype);
					my @newpps = $parseTree->objCparsedParams();
					# print STDERR "PPLIST for $name:\n";
					foreach my $newpp (@newpps) {
						# print STDERR "Parsed param: ".$newpp->{NAME}."\n";
						$extra->addParsedParameter($newpp);
					}
					# print STDERR "END PPLIST:\n";
					# $extra->dbprint();
				}
				if ($extra->{CLASS} ne "HeaderDoc::PDefine" && $blockmode == 2) {
					# Bail out of block mode if something other than a #define appears.
					my $linenum = $inputCounter + $blockOffset;

					warn("$fullpath:$linenum: warning: Unterminated \@defineblock detected.\n");

					$blockmode = 0;

					print STDERR "\$extra->{CLASS}=\"".$extra->{CLASS}."\"\n" if ($localDebug);

					# Now try to back off a bit so that we can get the previous HeaderDoc
					# block, if any.  If we're just reprocessing existing parse trees,
					# this isn't needed.
					if (!$subparse) {
						my $poplines = 0;
						print STDERR "Previous line number was ".($previousInputCounter + $blockOffset).".\n" if ($localDebug);
						$inputCounter = $previousInputCounter;
						while ($inputCounter >= 1) {
							my $checkline = $inputLines[$inputCounter];
							print STDERR "IC: $inputCounter CL: $checkline\n" if ($localDebug);

							if ($checkline =~ /\/\*\!/) { last; }
							if ($checkline =~ /\#\s*define/) {
								while ($checkline =~ /\\\s*$/) {
									$inputCounter++;
									print STDERR "INCREMENTED INPUTCOUNTER [5]\n" if ($HeaderDoc::inputCounterDebug);
									$checkline = $inputLines[$inputCounter];
								}
								# pointing at the last line of the declaration.  Bump it
								# forward one more to undo the inputCounter-- after the loop.
								$inputCounter++;
								print STDERR "INCREMENTED INPUTCOUNTER [6]\n" if ($HeaderDoc::inputCounterDebug);
								last;
							}
							$inputCounter--;
							print STDERR "DECREMENTED INPUTCOUNTER [7]\n" if ($HeaderDoc::inputCounterDebug);
							$poplines++;
						}
						# point to the line before the comment marker.
						$inputCounter--;
						print STDERR "DECREMENTED INPUTCOUNTER [8]\n" if ($HeaderDoc::inputCounterDebug);
						$curObj->dropParsedParameter();
						# $curObj->dbprint();

						$bail = 1;
						last;
					}
				}
				if (length($simpleTDcontents)) {
					$extra->typedefContents($simpleTDcontents);
				}
				print STDERR "Point B3\n" if ($hangDebug || $nameDebug);

				print STDERR "AT BOTTOM: DISCUSSION IS: ".($override_discussion ? $override_discussion : $discussion)."\n" if ($parmDebug);

				if ($preAtPart =~ /\S/) {
					print STDERR "preAtPart: $preAtPart\n" if ($localDebug);
					if (($blockmode == 2) && $extra->can("blockDiscussion")) {
						$extra->blockDiscussion($preAtPart);
						if ($override_discussion) {
							$extra->discussion($override_discussion);
						}
					} else {
						$extra->discussion(($override_discussion ? $override_discussion : $preAtPart));
					}
				} elsif ($extra != $curObj) {
					print STDERR "SETTING DISCUSSION: extra != CurObj\n" if ($parmDebug);
					# Otherwise this would be bad....
					if ($blockmode == 2) {
						if (!$discussion) {
							# Only do this if no discussion.  Otherwise, this is
							# part of the block name.
							print STDERR "SETTING DISCUSSION: blockmode == 2 and NO discussion\n" if ($parmDebug);
							$extra->blockDiscussion($nameline_discussion);
						} else {
							print STDERR "SETTING DISCUSSION: blockmode == 2 and discussion\n" if ($parmDebug);
							$extra->blockDiscussion($discussion);
						}
						if ($override_discussion) {
							$extra->discussion($override_discussion);
						}
					} else {
						if ($discussion_set || $override_discussion) {
							print STDERR "SETTING DISCUSSION: blockmode NOT 2 and discussion_set\n" if ($parmDebug);
							$extra->discussion(($override_discussion ? $override_discussion : $discussion));
							print STDERR "NEW DISC IS $discussion\n" if ($parmDebug);
						} else {
							print STDERR "SETTING DISCUSSION: blockmode NOT 2 and NOT discussion_set\n" if ($parmDebug);
							$extra->nameline_discussion($nameline_discussion);
						}
					}

				}
				print STDERR "Point B4\n" if ($hangDebug || $nameDebug);
				$extra->abstract($abstract);
				if (length($value)) { $extra->value($value); }
				if ($extra != $curObj || !length($curObj->name())) {
					$name =~ s/^(\s|\*)*//sgo;
				}
				print STDERR "NAME IS \"$name\"\n" if ($localDebug || $nameDebug);
				$extra->rawname($name);
				print STDERR "RN: \"".$extra->rawname()."\"\n" if ($localDebug || $nameDebug);
				print STDERR "NM: \"".$extra->name()."\"\n" if ($localDebug || $nameDebug);
				# my $namestring = $curObj->name();
				# if ($explicit_name_differs && 0) {
					# $extra->name("$name ($namestring)");
				# } else {
					# $extra->name($name);
				# }
				print STDERR "Point B5\n" if ($hangDebug);
				# $HeaderDoc::ignore_apiuid_errors = 1;
				$extra->name($name);
				# my $junk = $extra->apirefSetup();
				# $HeaderDoc::ignore_apiuid_errors = 0;

				# Set up Objective-C "Conforming To" list.
				if ($extra =~ /HeaderDoc::ObjC/) {
					$extra->conformsToList($conformsToList)
				}

				# print STDERR "NAMES: \"".$curObj->name()."\" & \"".$extra->name()."\"\n";
				# print STDERR "ADDYS: ".$curObj." & ".$extra."\n";

				if ($extra != $curObj) {
				    my @params = $curObj->taggedParameters();
				    foreach my $param (@params) {
					print STDERR "CONSTANT $param\n" if ($parmDebug);
					if (!$param->{ISDEFINE}) {
						$extra->addTaggedParameter($param->clone());
					} elsif ($param->name() eq $extra->rawname() || $param->name() eq $extra->name()) {
						my @subparams = $param->userDictArray();
						print "SP: ".@subparams."\n" if ($parmDebug);
						foreach my $hashRef (@subparams) {
							while (my ($param, $disc) = each %{$hashRef}) {
								print STDERR "PARAM IS $param\n" if ($parmDebug);

								my $paramobj = HeaderDoc::MinorAPIElement->new();
								$paramobj->name($param);
								$paramobj->discussion($disc);
								$extra->addTaggedParameter($paramobj);
							}
						}
					} else {
						print STDERR "NOMATCH: PARAM NAME IS ".$param->name()." EXTRA IS ".$extra->rawname()." OR ".$extra->name()."\n" if ($parmDebug);
					}
				    }
				    my @constants = $curObj->constants();
				    foreach my $constant (@constants) {
					# print STDERR "CONSTANT $constant\n";
					if ($extra->can("addToConstants")) {
					    $extra->addToConstants($constant->clone());
					    # print STDERR "ATC\n";
					} elsif ($extra->can("addConstant")) {
					    $extra->addConstant($constant->clone());
					    # print STDERR "AC\n";
					}
				    }

				    print STDERR "Point B6\n" if ($hangDebug);
				    if (length($curObj->name())) {
	# my $a = $extra->rawname(); my $b = $curObj->rawname(); my $c = $curObj->name();
	# print STDERR "EXTRA RAWNAME: $a\nCUROBJ RAWNAME: $b\nCUROBJ NAME: $c\n";
					push(@linkobjs, \$extra);
					# $curObj->attributelist("See Also", $ern." ".$extra->apiuid());
					# $extra->attributelist("See Also", $crn." ".$curObj->apiuid());
				    }
				}
				print STDERR "Point B7 TS = $typestring\n" if ($hangDebug);
				if (ref($apiOwner) ne "HeaderDoc::Header") {
				    if (!$apiOwner->isCOMInterface()) {
					$extra->accessControl($cppAccessControlState); # @@@ FIXME DAG CHECK FOR OBJC
				    }
				}
				if ($extra != $curObj && $curtype ne "UNKNOWN" && $curObj->can("fields") && $extra->can("fields")) {
					my @fields = $curObj->fields();
					print STDERR "B7COPY\n" if ($localDebug);

					foreach my $field (@fields) {
						bless($field, "HeaderDoc::MinorAPIElement");
						my $newfield = $field->clone();
						$extra->addToFields($newfield);
						# print STDERR "Added field ".$newfield->name()." to $extra ".$extra->name()."\n";
					}
				}
				$extra->apiOwner($apiOwner);
				if ($xml_output) {
				    $extra->outputformat("hdxml");
				} else { 
				    $extra->outputformat("html");
				}
				$extra->filename($filename);
				$extra->fullpath($fullpath);

		# warn("Added ".$extra->name()." ".$extra->apiuid().".\n");

				# print STDERR "ITD: $inTypedef\n";
				print STDERR "B8X blockmode=$blockmode ts=$typestring\n" if ($localDebug || $hangDebug);

				if (($typestring =~ /^(class|\@class|\@interface|\@implementation|\@protocol|interface|module|namespace)/ || $inClass) && !$inTypedef) {
					print STDERR "ITSACLASS! ($extra->name)\n" if ($localDebug);
					$extra->declaration($declaration);
					$extra->declarationInHTML($declaration);

					# if (!$subparse || ($extra != $curObj)) {
					    # my $localDebug = 1;
						print STDERR "ADDING \"".$extra->name."\" TO CLASSES/PROTOCOLS/*\n" if ($localDebug);
					print STDERR "RAWDEC: $declaration\n" if ($localDebug);
						$classType = classTypeFromFieldAndBPinfo($classKeyword, $typestring." ".$posstypes, $declaration, $fullpath, $inputCounter+$blockOffset, $HeaderDoc::sublang);
						# print STDERR "CT: $classType\n";
						if ($classType eq "intf") {
							push (@classObjects, $extra);
							print STDERR "intf\n" if ($localDebug);
							$apiOwner->addToProtocols($extra) if ($extra->{INSERTED} != 2);
						} elsif ($classType eq "occCat") {
							push (@categoryObjects, $extra);
							print STDERR "occCat\n" if ($localDebug);
							$apiOwner->addToCategories($extra) if ($extra->{INSERTED} != 2);
						} elsif ($classType eq "occ") {
							push (@classObjects, $extra);
							print STDERR "occ\n" if ($localDebug);
							$apiOwner->addToClasses($extra) if ($extra->{INSERTED} != 2);
						} elsif ($classType eq "C" || $classType eq "cpp" || $extra->isCOMInterface() || $classType eq $lang || $classType eq $HeaderDoc::sublang) {
							# print STDERR "CT: $classType\n";
							# class or typedef struct
							# (java, C, cpp, etc.)
							push (@classObjects, $extra);
							print STDERR "other ($classType)\n" if ($localDebug);
							$apiOwner->addToClasses($extra) if ($extra->{INSERTED} != 2);
							# print STDERR "ADDING CLASS\n";
						} else {
							print STDERR "Unknown class type $classType\n";
						}
					# }
				} elsif (($typestring =~ /$typedefname/ && length($typedefname)) || ($typestring =~ /^(class|\@class|\@interface|\@implementation|\@protocol)/)) {
	                		if (length($declaration)) {
                        			$extra->setDeclaration($declaration);
					}
					if (length($extra->name())) {
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToTypedefs($extra) if ($extra->{INSERTED} != 2); }
				    	}
				} elsif ($typestring =~ /MACRO/o) {
					# throw these away.
					# $extra->setDeclaration($declaration);
					# $apiOwner->addToPDefines($extra);
				} elsif ($typestring =~ /#define/o) {
					print STDERR "SPDF[2]\n" if ($hangDebug);
					$extra->setDeclaration($declaration);
# print STDERR "DEC:$declaration\n" if ($hangDebug);
					print STDERR "END SPDF[2]\n" if ($hangDebug);
					print STDERR "EXTRA IS $extra\n" if ($nameDebug);
					print STDERR "PDEF DECL: $declaration\n" if ($nameDebug);
					if ($extra !~ /HeaderDoc::PDefine/) { die("Unexpected type $extra"); }
					if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToPDefines($extra) if ($extra->{INSERTED} != 2); }
					if ($extra->can('isAvailabilityMacro') && $extra->isAvailabilityMacro()) {
						addAvailabilityMacro($extra->name, $extra->discussion);
					}
				} elsif ($typestring =~ /struct/o || $typestring =~ /union/o || ($lang eq "pascal" && $typestring =~ /record/o)) {
					if ($typestring =~ /union/o) {
						$extra->isUnion(1);
					} else {
						$extra->isUnion(0);
					}
					# $extra->declaration($declaration);
# print STDERR "PRE (DEC IS $declaration)\n";
					$extra->setDeclaration($declaration);
# print STDERR "POST\n";
					if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToStructs($extra) if ($extra->{INSERTED} != 2); }
				} elsif ($typestring =~ /enum/o) {
					# print STDERR "TYPESTRING MATCH: \"$typestring\" = \"enum\"\n";
					$extra->declaration($declaration);
					$extra->declarationInHTML($extra->getEnumDeclaration($declaration));
print STDERR "B8ENUM\n" if ($localDebug || $hangDebug);
					if (($blockmode != 2) || ($extra != $curObj)) {
print STDERR "B8ENUMINSERT apio=$apiOwner\n" if ($localDebug || $hangDebug);
 $apiOwner->addToEnums($extra) if ($extra->{INSERTED} != 2); }
				} elsif ($typestring =~ /\#define/o) {
					print STDERR "SPDF[3]\n" if ($hangDebug);
					$extra->setDeclaration($declaration);
					print STDERR "END SPDF[3]\n" if ($hangDebug);
					if (($blockmode != 2) || ($extra != $curObj)) { $headerObject->addToPDefines($extra); }
					if ($extra->can('isAvailabilityMacro') && $extra->isAvailabilityMacro()) {
						addAvailabilityMacro($extra->name, $extra->discussion);
					}
				} elsif ($typestring =~ /(function|method|operator|ftmplt|callback)/o) {
					if ($typestring =~ /method/) {
						$extra->setDeclaration($declaration);
						# $HeaderDoc::ignore_apiuid_errors = 1;
						# my $junk = $extra->apirefSetup(1);
						# $HeaderDoc::ignore_apiuid_errors = 0;
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToMethods($extra) if ($extra->{INSERTED} != 2); }
					} else {
						print STDERR "SFD\n" if ($hangDebug);
						# print STDERR "EXTRA IS $extra\n";
						if (ref($extra) ne "HeaderDoc::HeaderElement") {
							$extra->setDeclaration($declaration);
						}
						print STDERR "END SFD\n" if ($hangDebug);
						# $HeaderDoc::ignore_apiuid_errors = 1;
						# my $junk = $extra->apirefSetup(1);
						# $HeaderDoc::ignore_apiuid_errors = 0;
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToFunctions($extra) if ($extra->{INSERTED} != 2); }
					}
					if ($typestring eq "callback") {
						# For future expansion
						$extra->isCallback(1);
					}
					if ($typestring eq "ftmplt") {
						$extra->isTemplate(1);
					}
				} elsif ($typestring =~ /^property/o) {
					$varIsConstant = 0;
					$extra->isProperty(1);
					$extra->declaration($declaration);
					$extra->setDeclaration($declaration);
					$returntype =~ s/^\s*//s;
					$returntype =~ s/\s*$//s;
					$extra->returntype($returntype); # ("\@property $returntype");
					if (ref($apiOwner) ne "HeaderDoc::Header") {
						$extra->accessControl($cppAccessControlState);
					}
					if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToProps($extra) if ($extra->{INSERTED} != 2); }
				} elsif ($typestring =~ /constant/o) {
					# print STDERR "TS CONSTANT (RET: $returntype)\n";
					$extra->declaration($declaration);
					$extra->setDeclaration($declaration);
					$returntype =~ s/^\s*//s;
					$returntype =~ s/\s*$//s;
					$extra->returntype($returntype);
                                        if (length($extra->name())) {
                                                if (ref($apiOwner) ne "HeaderDoc::Header") {
                                                        $extra->accessControl($cppAccessControlState);
                                                        if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToVars($extra) if ($extra->{INSERTED} != 2); }
                                                } else { # headers group by type
                                                            if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToConstants($extra) if ($extra->{INSERTED} != 2); }
                                                }
                                        }
				} elsif ($typestring =~ /variable/o) {
					$extra->declaration($declaration);
					$extra->setDeclaration($declaration);
					$returntype =~ s/^\s*//s;
					$returntype =~ s/\s*$//s;
					$extra->returntype($returntype);
                                        if (ref($apiOwner) ne "HeaderDoc::Header") {
                                            $extra->accessControl($cppAccessControlState);
					}
					if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToVars($extra) if ($extra->{INSERTED} != 2); }
				} else {
					my $linenum = $inputCounter + $blockOffset;
					warn("$fullpath:$linenum: warning: Unknown typestring $typestring returned by blockParse\n");
				}
				print STDERR "B9 blockmode=$blockmode ts=$typestring\n" if ($localDebug || $hangDebug);
		    print STDERR "DOC REF: ".$extra->appleRefIsDoc()."\n" if ($nameDebug);
				$extra->checkDeclaration();
				my $prevignore = $HeaderDoc::ignore_apiuid_errors;
				$HeaderDoc::ignore_apiuid_errors = 1;
				my $junk = $extra->apirefSetup(1);
				print STDERR "APIREF (".$extra->name()."): ".$extra->apiref()."\n" if ($nameDebug);
				$HeaderDoc::ignore_apiuid_errors = $prevignore;

				print "ADDED $extra to APIOWNER $apiOwner (curObj is $curObj)\n" if ($nameDebug);
			}
		}
		if ($hangDebug) {
			print STDERR "Point C\n";
			print STDERR "inputCounter is $inputCounter, #inputLines is $nlines\n";
		}

		print STDERR "READ DECLARATION \"$declaration\"\n" if ($HeaderDoc::inputCounterDebug);

		if (!$bail) {
			while ($inputLines[$inputCounter] !~ /\S/o && ($inputCounter <= $nlines)) {
				$inputCounter++;
				print STDERR "INCREMENTED INPUTCOUNTER [9]\n" if ($HeaderDoc::inputCounterDebug);
			}
			# $inputCounter--;
			# print STDERR "DECREMENTED INPUTCOUNTER [10]: LINE ".$inputLines[$inputCounter]."\n" if ($HeaderDoc::inputCounterDebug);
			if ($hangDebug) { print STDERR "Point D\n"; }
			if ($curtype eq "UNKNOWN") { $curtype = $outertype; }
			if ((($outertype ne $curtype && $innertype ne $curtype && $posstypes !~ /$curtype/ && !($inTypedef && $outertype =~ /^(class|\@class|\@interface|\@implementation|\@protocol)/))) && (($inputCounter > $nlines) || warnHDComment(\@inputLines, $inputCounter, $blockOffset, "blockParse:$outertype", "18a"))) {
				if (!$HeaderDoc::test_mode) {
					warn "No matching declaration found.  Last name was $curname\n";
					warn buildCommentFromFields(@fields, $preAtPart, "The HeaderDoc comment that caused this was:\n")."\n";
					warn "$outertype ne $curtype && $innertype ne $curtype && $posstypes !~ $curtype\n";
				}
				$foundMatch = 0;
				last;
			}
			if ($hangDebug) { print STDERR "Point E\n"; }
			if ($blockmode == 1) {
				warn "next line: ".$inputLines[$inputCounter]."\n" if ($hangDebug);
				print STDERR "BLOCKMODE[10] -> 2\n" if ($localDebug || $blockDebug);
				$blockmode = 2;
			}
			if ($blockmode == 2) {
				my $prevignore = $HeaderDoc::ignore_apiuid_errors;
				$HeaderDoc::ignore_apiuid_errors = 1;
				if (warnHDComment(\@inputLines, $inputCounter, $blockOffset, "blockMode:$outertype", "18a") == 1) {
					# print STDERR "OT: $outertype\n";
					$blockmode = 0;
					print STDERR "BLOCKMODE[11] -> 0\n" if ($localDebug || $blockDebug);
					warn "Block Mode Ending\n" if ($hangDebug);
				}
				$HeaderDoc::ignore_apiuid_errors = $prevignore;
			}
			print STDERR "PTCT: $posstypes =? $curtype\n" if ($localDebug || $blockDebug);
			$previousInputCounter = $inputCounter;
		} else {
			last;
		}
	}

	print STDERR "OUT OF LOOP.  Exited with blockmode = $blockmode\n" if ($localDebug || $blockDebug);
	if (length($blockDec)) {
		$curObj->declaration($blockDec);
		$curObj->declarationInHTML($blockDec);
	}
	if ($hangDebug) { print STDERR "Point F\n"; }
	if ($curObj->can('isAvailabilityMacro') && $curObj->isAvailabilityMacro()) {
		addAvailabilityMacro($curObj->name, $curObj->discussion);
	}
	print STDERR "Out of Block\n" if ($localDebug || $blockDebug);
	# the end of this block assumes that inputCounter points
	# to the last line grabbed, but right now it points to the
	# next line available.  Back it up by one.
	$inputCounter--;
	print STDERR "DECREMENTED INPUTCOUNTER [11]: LINE ".$inputLines[$inputCounter]."\n" if ($HeaderDoc::inputCounterDebug);
	# warn("NEWDEC:\n$declaration\nEND NEWDEC\n");

				}  ## end blockParse handler
	if ($subparse) {
		$HeaderDoc::enable_cpp = $old_enable_cpp;
	}
	objlink(\@linkobjs);
	return ($inputCounter, $cppAccessControlState, $classType, \@classObjects, \@categoryObjects, $blockOffset, $numcurlybraces, $foundMatch);
}

sub objForType
{
	my $curObj = shift;               # IN
	my $typedefname = shift;          # IN
	my $typestring = shift;           # IN
	my $posstypes = shift;            # IN
	my $outertype = shift;            # IN
	my $curtype = shift;              # IN
	my $classType = shift;            # INOUT
	my $classKeyword = shift;         # INOUT
	my $declaration = shift;          # IN
	my $fieldref = shift;             # IN
	my $functionGroup = shift;        # IN
	my $varIsConstant = shift;        # INOUT
	my $blockmode = shift;            # IN
	my $inClass = shift;              # IN
	my $inInterface = shift;          # IN
	my $inTypedef = shift;            # IN
	my $inStruct = shift;             # IN
	my $fullpath = shift;             # IN
	my $inputCounter = shift;         # IN
	my $blockOffset = shift;          # IN
	my $lang = shift;                 # IN
	my $outerLocalDebug = shift;      # IN
	my $functionContents = shift;     # IN
	my $apiOwner = shift;             # IN
	my $subparseInputCounter = shift; # IN
	my $subparseBlockOffset  = shift; # IN
	my $extendsClass = shift;         # IN
	my $implementsClass = shift;      # IN

	my $extra = undef;
	my $localDebug = 0;

	my $filename = basename($fullpath);

	print STDERR "FOR DECLARATION $declaration\n" if ($localDebug);

			    if ($typestring eq $outertype || !$HeaderDoc::outerNamesOnly) {
				if (($typestring =~ /^(class|\@class|\@interface|\@implementation|\@protocol|interface|module|namespace)/ || $inClass) && !$inTypedef && !$inStruct) {
                                        print STDERR "blockParse returned class\n" if ($localDebug);
					print STDERR "RAWDEC: $declaration\n" if ($localDebug && $outerLocalDebug);
					$classType = classTypeFromFieldAndBPinfo($classKeyword, $typestring." ".$posstypes, $declaration, $fullpath, $inputCounter+$blockOffset, $HeaderDoc::sublang);
					print STDERR "classtype: $classType\n" if ($localDebug);
					if ($classType eq "intf") {
						$extra = HeaderDoc::ObjCProtocol->new();
                                        	$extra->apiOwner($apiOwner);
					} elsif ($classType eq "occCat") {
						$extra = HeaderDoc::ObjCCategory->new();
                                        	$extra->apiOwner($apiOwner);
					} elsif ($classType eq "occ") {
						$extra = HeaderDoc::ObjCClass->new();
                                        	$extra->apiOwner($apiOwner);
					} else {
						$extra = HeaderDoc::CPPClass->new();
                                        	$extra->apiOwner($apiOwner);
						if ($inInterface || $typestring =~ /typedef/ || $typestring =~ /struct/) {
							# cluck "Setting isCOMInterface -> 1 ($inInterface, $typestring)\n";
							$inInterface = 0;
							$extra->isCOMInterface(1);
							$extra->tocTitlePrefix('COM&nbsp;Interface:');
						}
						if ($classType eq "IDL") {
							if ($typestring =~ /(module|namespace)/) {
								$extra->isModule(1);
							}
						}
					}
					if ($typestring =~ /typedef/ || $outertype =~ /typedef/) {
						$extra->CClass(1);
					}

					print STDERR "TYPESTRING IS $typestring.  CCLASS IS ".$extra->CClass."\n" if ($localDebug);

					$extra->group($HeaderDoc::globalGroup);
					if ($HeaderDoc::module) {
						$extra->indexgroup($HeaderDoc::module);
						print STDERR "MODULE: ".$HeaderDoc::module."\n"; # @@@
					}
                                        $extra->filename($filename);
                                        $extra->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$extra->linenuminblock($subparseInputCounter);
						$extra->blockoffset($subparseBlockOffset);
					} else {
						$extra->linenuminblock($inputCounter);
						$extra->blockoffset($blockOffset);
					}
					# $extra->linenum($inputCounter+$blockOffset);
					$extra->headerObject($HeaderDoc::headerObject);
					# my $superclass = &get_super($classType, $declaration);
					my $class = ref($extra) || $extra;
					my $superclass = $posstypes;
					my $superclassfieldname = "Superclass";
					if ($extra->CClass()) {
						$superclassfieldname = "";
					} elsif ($class =~ /HeaderDoc::ObjCCategory/) {
						$superclassfieldname = "Extends&nbsp;Class";
					} elsif ($class =~ /HeaderDoc::ObjCProtocol/) {
						$superclassfieldname = "Extends&nbsp;Protocol";
					}
					if (length($superclass) && length($superclassfieldname) && (!($extra->checkShortLongAttributes($superclassfieldname))) && !$extra->CClass()) {
						$extra->attribute($superclassfieldname, $superclass, 0, 1);
					}


					$superclassfieldname = "Extends&nbsp;Class";
					if (length($extendsClass) && length($superclassfieldname) && (!($extra->checkShortLongAttributes($superclassfieldname))) && !$extra->CClass()) {
						$extra->attribute($superclassfieldname, $extendsClass, 0, 1);
					}
					$superclassfieldname = "Implements&nbsp;Class";
					if (length($implementsClass) && length($superclassfieldname) && (!($extra->checkShortLongAttributes($superclassfieldname))) && !$extra->CClass()) {
						$extra->attribute($superclassfieldname, $implementsClass, 0, 1);
					}


					# $extra->declaration($declaration);
					# $extra->declarationInHTML($declaration);
					# if ($curtype eq "UNKNOWN") {
						$extra->processComment($fieldref);
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

				} elsif (($typestring =~ /^$typedefname/ && length($typedefname)) || $typestring =~ /^(class|\@class|\@interface|\@implementation|\@protocol)/) {
					print STDERR "blockParse returned $typedefname\n" if ($localDebug);
					if ($localDebug) {
						foreach my $field (@{$fieldref}) {
							print STDERR "FIELD $field\n";
						}
					}
					$extra = HeaderDoc::Typedef->new;
                                        $extra->apiOwner($apiOwner);
					$extra->group($HeaderDoc::globalGroup);
                                        $extra->filename($filename);
                                        $extra->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$extra->linenuminblock($subparseInputCounter);
						$extra->blockoffset($subparseBlockOffset);
					} else {
						$extra->linenuminblock($inputCounter);
						$extra->blockoffset($blockOffset);
					}
					# $extra->linenum($inputCounter+$blockOffset);
					$curObj->masterEnum(1);
					if ($curtype eq "UNKNOWN") {
						$extra->processComment($fieldref);
					}
				} elsif ($typestring =~ /^struct/o || $typestring =~ /^union/o || ($lang eq "pascal" && $typestring =~ /^record/o)) {
					print STDERR "blockParse returned struct or union ($typestring)\n" if ($localDebug);
					$extra = HeaderDoc::Struct->new;
                                        $extra->apiOwner($apiOwner);
					$extra->group($HeaderDoc::globalGroup);
                                        $extra->filename($filename);
                                        $extra->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$extra->linenuminblock($subparseInputCounter);
						$extra->blockoffset($subparseBlockOffset);
					} else {
						$extra->linenuminblock($inputCounter);
						$extra->blockoffset($blockOffset);
					}
					# $extra->linenum($inputCounter+$blockOffset);
					if ($typestring =~ /union/o) {
						$extra->isUnion(1);
					}
					if ($curtype eq "UNKNOWN") {
						$extra->processComment($fieldref);
					}
				} elsif ($typestring =~ /^enum/o) {
					print STDERR "blockParse returned enum\n" if ($localDebug);
					$extra = HeaderDoc::Enum->new;
                                        $extra->apiOwner($apiOwner);
					$extra->group($HeaderDoc::globalGroup);
                                        $extra->filename($filename);
                                        $extra->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$extra->linenuminblock($subparseInputCounter);
						$extra->blockoffset($subparseBlockOffset);
					} else {
						$extra->linenuminblock($inputCounter);
						$extra->blockoffset($blockOffset);
					}
					# $extra->linenum($inputCounter+$blockOffset);
					if ($curtype eq "UNKNOWN") {
						$extra->processComment($fieldref);
					}
					if ($curtype eq "enum" || $curtype eq "typedef") {
						$extra->masterEnum(0);
					} else {
						$extra->masterEnum(1);
					}
				} elsif ($typestring =~ /^MACRO/o) {
					print STDERR "blockParse returned MACRO\n" if ($localDebug);
					# silently ignore this noise.
				} elsif ($typestring =~ /^\#define/o) {
					print STDERR "blockParse returned #define\n" if ($localDebug);
					$extra = HeaderDoc::PDefine->new;
                                        $extra->apiOwner($apiOwner);
					$extra->inDefineBlock($blockmode);
					$extra->group($HeaderDoc::globalGroup);
                                        $extra->filename($filename);
                                        $extra->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$extra->linenuminblock($subparseInputCounter);
						$extra->blockoffset($subparseBlockOffset);
					} else {
						$extra->linenuminblock($inputCounter);
						$extra->blockoffset($blockOffset);
					}
					# $extra->linenum($inputCounter+$blockOffset);
					if ($curtype eq "UNKNOWN") {
						$extra->processComment($fieldref);
					}
				} elsif ($typestring =~ /^property/o) {
					$varIsConstant = 0;
					print STDERR "blockParse returned property\n" if ($localDebug);
					$extra = HeaderDoc::Var->new;
                                        $extra->apiOwner($apiOwner);
					$extra->group($HeaderDoc::globalGroup);
                                       	$extra->filename($filename);
                                        $extra->fullpath($fullpath);
					if (defined($subparseInputCounter)) {
						$extra->linenuminblock($subparseInputCounter);
						$extra->blockoffset($subparseBlockOffset);
					} else {
						$extra->linenuminblock($inputCounter);
						$extra->blockoffset($blockOffset);
					}
					# $extra->linenum($inputCounter+$blockOffset);
					$extra->isProperty(1);
					if ($curtype eq "UNKNOWN") {
						$extra->processComment($fieldref);
					}
				} elsif ($typestring =~ /^constant/o) {
					# if ($declaration =~ /\s+const\s+/o) {
						$varIsConstant = 1;
						print STDERR "blockParse returned constant\n" if ($localDebug);
						$extra = HeaderDoc::Constant->new;
                                        	$extra->apiOwner($apiOwner);
						$extra->group($HeaderDoc::globalGroup);
                                        	$extra->filename($filename);
                                        	$extra->fullpath($fullpath);
						if (defined($subparseInputCounter)) {
							$extra->linenuminblock($subparseInputCounter);
							$extra->blockoffset($subparseBlockOffset);
						} else {
							$extra->linenuminblock($inputCounter);
							$extra->blockoffset($blockOffset);
						}
						# $extra->linenum($inputCounter+$blockOffset);
						if ($curtype eq "UNKNOWN") {
							$extra->processComment($fieldref);
						}
				} elsif ($typestring =~ /^variable/o) {
						$varIsConstant = 0;
						print STDERR "blockParse returned variable\n" if ($localDebug);
						$extra = HeaderDoc::Var->new;
                                        	$extra->apiOwner($apiOwner);
						$extra->group($HeaderDoc::globalGroup);
                                        	$extra->filename($filename);
                                        	$extra->fullpath($fullpath);
						if (defined($subparseInputCounter)) {
							$extra->linenuminblock($subparseInputCounter);
							$extra->blockoffset($subparseBlockOffset);
						} else {
							$extra->linenuminblock($inputCounter);
							$extra->blockoffset($blockOffset);
						}
						# $extra->linenum($inputCounter+$blockOffset);
						if ($curtype eq "UNKNOWN") {
							$extra->processComment($fieldref);
						}
				} elsif ($typestring =~ /^(function|method|operator|ftmplt|callback)/o) {
					print STDERR "blockParse returned function or method\n" if ($localDebug);
					if ($typestring =~ /method/) {
						$extra = HeaderDoc::Method->new;
                                        	$extra->apiOwner($apiOwner);
						$extra->functionContents($functionContents);
						if (length($functionGroup)) {
							$extra->group($functionGroup);
						} else {
							$extra->group($HeaderDoc::globalGroup);
						}
                                        	$extra->filename($filename);
                                        	$extra->fullpath($fullpath);
						if (defined($subparseInputCounter)) {
							$extra->linenuminblock($subparseInputCounter);
							$extra->blockoffset($subparseBlockOffset);
						} else {
							$extra->linenuminblock($inputCounter);
							$extra->blockoffset($blockOffset);
						}
						# $extra->linenum($inputCounter+$blockOffset);
						if ($curtype eq "UNKNOWN") {
							$extra->processComment($fieldref);
						}
					} else {
						$extra = HeaderDoc::Function->new;
                                        	$extra->apiOwner($apiOwner);
						$extra->functionContents($functionContents);
						if (length($functionGroup)) {
							$extra->group($functionGroup);
						} else {
							$extra->group($HeaderDoc::globalGroup);
						}
                                        	$extra->filename($filename);
                                        	$extra->fullpath($fullpath);
						if (defined($subparseInputCounter)) {
							$extra->linenuminblock($subparseInputCounter);
							$extra->blockoffset($subparseBlockOffset);
						} else {
							$extra->linenuminblock($inputCounter);
							$extra->blockoffset($blockOffset);
						}
						# $extra->linenum($inputCounter+$blockOffset);
						if ($curtype eq "UNKNOWN") {
							$extra->processComment($fieldref);
						}
					}
					if ($typestring eq "callback") {
						$extra->isCallback(1);
					}
					if ($typestring eq "ftmplt") {
						$extra->isTemplate(1);
					}
				} else {
					my $linenum = $inputCounter + $blockOffset;
					warn("$fullpath:$linenum: warning: Unknown keyword $typestring in block-parsed declaration.\n".
					     "This usually means that your code requires C preprocessing in order to be\n".
					     "valid C syntax and either C preprocessing is not enabled (-p) or the required\n".
					     "macros lack HeaderDoc comments.  Use of the \@parseOnly tag is recommended\n".
					     "for these special symbols.\n");
				}
			    } else {
				print STDERR "Dropping alternate name\n" if ($localDebug);
			    }

	return ($extra, $classType, $varIsConstant);
}


sub objlink
{
    my $listref = shift;
    my @list = @{$listref};
    my @reflist = ();
    my @masterreflist = ();
    my $localDebug = 0;
    my $masterobj = undef;

    foreach my $objref (@list) {
	my $obj = ${$objref};
	bless($obj, "HeaderDoc::HeaderElement");
	bless($obj, $obj->{CLASS});

	if ($obj =~ /HeaderDoc::PDefine/) {
		if ($obj->isBlock) {
			$masterobj = $obj;
		}
	}
	print STDERR "Obj: $obj\n" if ($localDebug);
    }
    print STDERR "MO: $masterobj\n" if ($localDebug);

    foreach my $objref (@list) {
	my $obj = ${$objref};
	# change whitespace to ctrl-d to
	# allow multi-word names.
	my $ern = $obj->rawname();
	if ($ern =~ /\s/o && $localDebug) {
		print STDERR "changed space to ctrl-d\n";
		print STDERR "ref is ".$obj->apiuid()."\n";
	}
	$ern =~ s/\s/\cD/sgo;
	if ($obj != $masterobj) {
		push(@reflist, $ern." ".$obj->apiuid());
	}
    }
    if ($masterobj) {
	my $ern = $masterobj->rawname();
	if ($ern =~ /\s/o && $localDebug) {
		print STDERR "changed space to ctrl-d\n";
		print STDERR "ref is ".$masterobj->apiuid()."\n";
	}
	$ern =~ s/\s/\cD/sgo;
	@masterreflist = @reflist;
	@reflist = ();
	push(@reflist, $ern." ".$masterobj->apiuid());
	foreach my $apiref (@masterreflist) {
		# Don't try this.
		# $masterobj->see("seealso\n$apiref");
		if (!$masterobj->seeDupCheck($apiref) && !$masterobj->{HIDESINGLETONS}) {
			$masterobj->attributelist("See Also", $apiref);
			$masterobj->seeDupCheck($apiref, 1);
		}
	}
    }
    foreach my $objref (@list) {
	my $obj = ${$objref};
	if ($obj != $masterobj) {
	    my $uid = $obj->apiuid();
	    if ($masterobj->{HIDESINGLETONS}) {
		$obj->{HIDEDOC} = 1;
	    }
	    foreach my $apiref (@reflist) {
		my $rawref = $apiref;
		$rawref =~ s/.* //s;
		if ($rawref ne $uid) {
			# Don't try this.
			# $obj->see("seealso\n$apiref");
			if (!$obj->seeDupCheck($apiref)) {
				$obj->attributelist("See Also", $apiref);
				$obj->seeDupCheck($apiref, 1);
			}
		}
	    }
	}
    }
}

# /*! Used by availability macros so the C preprocessor doesn't strip the
# token out before the parser sees it. */
sub cpp_remove($)
{
    my $name = shift;
    my $localDebug = 0;

    print STDERR "Removing token \"$name\" from C preprocessor macros list.\n" if ($localDebug);

    $CPP_HASH{$name} = undef;
    $CPP_ARG_HASH{$name} = undef;
}

sub cpp_add($$)
{
    my $parseTree = shift;
    my $string = $parseTree->textTreeNC($HeaderDoc::lang, $HeaderDoc::sublang);
    my $dropdeclaration = shift;

    return cpp_add_string($string, $dropdeclaration);
}

sub cpp_add_string($$)
{
    my $string = shift;
    my $dropdeclaration = shift;
    my $localDebug = 0;

    $string =~ s/\n$//s;
    my $slstring = $string;
    $slstring =~ s/\\\n/ /sg;

    print STDERR "cpp_add: STRING WAS $string\n" if ($cppDebug || $localDebug || $cppDebugFromToken);
    print STDERR "SLSTRING: $slstring\n" if ($localDebug || $cppDebug);

    # if ($slstring =~ s/^\s*#define\s+(\w+)\(//s) {
    if ($dropdeclaration && $slstring =~ s/^\s*#define\s+(\w+)(\s|$)//s) {
	my $name = $1;

	print STDERR "Dropping declaration \"$name\"." if ($localDebug || $cppDebug);

	# Deleting token by force.
	if ($CPP_HASH{$name}) {
		warn "Multiple definitions for $name.  Using first.\n" if ($cppDebug || $warnAllMultipleDefinitions);
	} else {
		$CPP_HASH{$name} = "";
	}
    } elsif ($slstring =~ s/^(?:\/\*.*?\*\/)?\s*#define\s+((?:\w|::|->)+)(\s|\(|\{)//s) {
	my $name = $1;
	my $namequot = quote($name);
	# my $firsttoken = $2;

	print STDERR "CPP ADDING FUNC-LIKE MACRO\n" if ($localDebug || $cppDebug);

	print STDERR "GOT NAME $name\n" if ($localDebug || $cppDebug);

	$string =~ s/^.*?$namequot//s;

	print STDERR "POST-STRIP: STRING IS \"$string\"\n" if ($localDebug || $cppDebug);

	my @tokens = split(/(\/\/|\/\*|\*\/|\W)/, $string);

	my $firstpart = "";
	my $lastpart = "";
	my $fpdone = 0;
	my $lasttoken = "";
	my $inChar = 0; my $inString = 0; my $inComment = 0; my $inSLC = 0;
	my $inParen = 0;
	foreach my $token (@tokens) {
	    if (!$token) { next; };
	    print STDERR "TOK: $token LAS: $lasttoken ICH: $inChar ICO: $inComment ISL: $inSLC IST: $inString\n" if ($localDebug || $cppDebug);
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
		print STDERR "TAILTOKEN: \"$token\"\n" if ($cppDebug);
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

	print STDERR "FP: \"$firstpart\"\nLP: \"$lastpart\"\nFPLPEND\n" if ($cppDebug || $localDebug);

	if ($lastpart) {
		my @lines = split(/[\r\n]/, $lastpart);
		my $lastline = pop(@lines);
		my $definition = "";

		foreach my $line (@lines) {
			if ($line) {
				$line =~ s/\\\s*$//s;
				$line .= "\n";
				$definition .= $line;
			}
		}
		# $lastline .= "\n";
		print STDERR "LL: \"$lastline\"\n" if ($cppDebug);

		push(@lines, $lastline);
		$definition .= "$lastline";

		print STDERR "ADDING NAME=\"$name\" ARGS=\"$firstpart\" DEFINITION=\"$definition\"\n" if ($cppDebug);

		if (defined $CPP_HASH{$name}) {
			warn "Multiple definitions for $name.  Using first.\n" if (($cppDebug || $warnAllMultipleDefinitions) && $HeaderDoc::enable_cpp);
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
			warn "Multiple definitions for $name.  Using first.\n" if ($cppDebug || $warnAllMultipleDefinitions);
		} else {
			$CPP_HASH{$name} = "";
			if (length($firstpart)) {
				$CPP_ARG_HASH{$name} = $firstpart;
			}
		}
	}
    } elsif ($slstring =~ s/^\s*#define\s+(\w+)\s*$//s) {
	my $name = $1;
	# This is defining a single token to delete.
	if ($CPP_HASH{$name}) {
		warn "Multiple definitions for $name.  Using first.\n" if ($cppDebug || $warnAllMultipleDefinitions);
	} else {
		$CPP_HASH{$name} = "";
	}
    } else {
	warn "CAN'T HANDLE \"$string\".\n" if ($HeaderDoc::enable_cpp);
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
# print STDERR "CPP ENABLE\n";
      foreach my $hashhashref (@HeaderDoc::cppHashList) {
# print STDERR "HASHREFCHECK\n";
	my $hashref = $hashhashref->{HASHREF};
	print STDERR "HASHREF: $hashref\n" if ($cppDebug);
	if (!$hashref) {
		warn "Empty hashref object!\n";
		next;
	}
	my %hash = %{$hashhashref->{HASHREF}};
	if ($linenum <= $hashhashref->{LINENUM}) {
		print STDERR "Skiping hash $hashhashref->{FILENAME}.  Line not reached.\n" if ($cppDebug);
		next;
	}
	print STDERR "COUNT: $count\nNARGHASHES: ".scalar(@HeaderDoc::cppArgHashList)."\n" if ($cppDebug);
	print STDERR "NHASHES: ".scalar(@HeaderDoc::cppHashList)."\n" if ($cppDebug);
	my $arghashref = $HeaderDoc::cppArgHashList[$count++];
	my %arghash = %{$arghashref};

	my $altpart = $hash{$part};
	my $exists = defined $hash{$part};
	if ($exists) {
		print STDERR "EXTHASH FOUND NAME=\"$part\" REPLACEMENT=\"$altpart\"\n" if ($cppDebug);
		if ($arghash{$part}) { $hasargs = 1; }
		print STDERR "HASARGS: $hasargs\n" if ($cppDebug);
		return ($altpart, $hasargs, $arghash{$part});
	}
      }

      my $altpart = $CPP_HASH{$part};
      my $exists = defined $CPP_HASH{$part};
      if ($exists) {
	print STDERR "FOUND NAME=\"$part\" REPLACEMENT=\"$altpart\"\n" if ($cppDebug);
	if (defined($CPP_ARG_HASH{$part})) { $hasargs = 1; }
	print STDERR "HASARGS: $hasargs\n" if ($cppDebug);
	return ($altpart, $hasargs, $CPP_ARG_HASH{$part});
      }
    }

    # If we got here, either CPP is disabled or we didn't find anything.
    if ($HeaderDoc::enable_cpp != -1) {
	print STDERR "Checking token \"$part\" for ignored macros\n" if ($cppDebug);
	my $altpart = $HeaderDoc::perHeaderIgnoreFuncMacros{$part};
	if ($altpart && length($altpart)) {
		print STDERR "Found token \"$part\" among ignored macros\n" if ($cppDebug);
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
	print STDERR "CPP_ARGPARSE: NM $name ARGS:\n";
	foreach my $arg (@arglist) {
		print STDERR "$arg\n";
		$arg->printTree();
	}
	print STDERR "ENDARGS\n";
    }

    print STDERR "SEARCHING FOR NAME \"$name\"\n" if ($cppDebug);
    my ($newtoken, $has_args, $pattern) = cpp_preprocess($name, $linenum); # , $cpphashref, $cpparghashref);

    print STDERR "PATTERN WAS \"$pattern\"\n" if ($cppDebug);

    my @parts = split(/,/, $pattern);
    my $count = 0;

    while ($count < scalar(@parts)) {
	my $part = $parts[$count];
	print STDERR "ORIGPART WAS $part\n" if ($cppDebug);
	$part =~ s/\s//sg;
	if (!$arglist[$count]) {
		warn "Not enough arguments to macro $name\n";
	} else {
		print STDERR "CALLING ON ".$arglist[$count]."\n" if ($cppDebug);
		$arghash{$part} = cpp_subparse($arglist[$count]); #, $cpphashref, $cpparghashref);
		print STDERR "PART \"$part\" VALUE: \"".$arghash{$part}."\"\n" if ($cppDebug)
	}
	print STDERR "POINTS TO $arghash{$part}\n" if ($cppDebug);
	$count++;
    }

    my $retstring = "";
    my @ntparts = split(/(\W)/, $newtoken);
    my $lastpart = "";
    my $poundcount = 0;
    foreach my $part (@ntparts) {
	if (length($part)) {
		print STDERR "PART WAS $part\n" if ($cppDebug);
		if ($part eq "#") {
			$poundcount++;
		} else {
			my $curpart = "";
			if (defined($arghash{$part})) {
				# 1 pound means stringify the next argument
				print STDERR "Inserting argument (".$arghash{$part}.") for $part\n" if ($cppDebug);
				$curpart = (($poundcount == 1) ? "\"" : "").$arghash{$part}.(($poundcount == 1) ? "\"" : "");
			} else {
				# 1 pound means stringify the next argument
				print STDERR "No arguments found for $part\n" if ($cppDebug);
				$curpart = (($poundcount == 1) ? "\"" : "").$part.(($poundcount == 1) ? "\"" : "");
			}
			if ($poundcount < 2) {
				# poundcount of 1 means just stringify, 0 means just insert the token.
				# Either way, commit the previous token and set the new last token value
				# to the current token.
				$retstring .= $lastpart;
				$lastpart = $curpart;
			} else {
				# 2 means concatenate with the last token
				# Concatenate this token onto the last token for further processing.
				$lastpart .= $curpart;
			}
			$poundcount = 0;
		}
	}
    }
    $retstring .= $lastpart;
    return $retstring;
}

sub cpp_subparse($)
{
    my $tree = shift;
    # my $hashlistref = shift;
    # my $arghashlistref = shift;

    my ($newtoken, $has_args) = cpp_preprocess($tree->token(), $tree->linenum()); #, $hashlistref, $arghashlistref);

    if ($cppDebug) {
	print STDERR "SUBPARSE: ARGS: $has_args\n";
	$tree->dbprint();
	print STDERR "END SUBPARSE DUMP\n";
    }
    if ($has_args) {
	my $name = $tree->token();
	my $paren = $tree->next();
	if (!$paren) { return; }
	if ($paren->token() =~ /\(/) {
		# Recurse.
		my @parts = ();
		my $fc = $paren->firstchild();
		my $subparsetop = HeaderDoc::ParseTree->new();
		my $subparsecur = $subparsetop;
		while ($fc) { # drop closing ')'
			my $fct = $fc->token();
			print STDERR "FCT: $fct\n" if ($cppDebug);
			if ($fct eq ',') {
				print STDERR "PUSH\n" if ($cppDebug);
				push(@parts, $subparsetop);
				$subparsetop = $subparsecur = HeaderDoc::ParseTree->new();
			} else {
				print STDERR "NEXT\n" if ($cppDebug);
				$subparsecur = $subparsecur->next(HeaderDoc::ParseTree->new());
				$subparsecur->token($fct);
			}
			$fc = $fc->next();
		}
		push(@parts, $subparsetop);

		print STDERR "CALLING ARGPARSE FROM cpp_subparse().\n" if ($cppDebug);
		my $ap = cpp_argparse($name, $tree->linenum(), \@parts); #, $hashlistref, $arghashlistref);
		print STDERR "Changed token from $tree (\"".$tree->token."\") to \"$ap\"\n" if ($cppDebug);
		$tree->token($ap);
		$paren->token("");
		$paren->firstchild(undef);
		if ($paren->next->token ne ")") {
			warn("Tree structure problem (cpp_subparse point 1).  Please file a bug.\n");
		} else {
			$paren->next->token("");
		}
	} else {
		warn("Tree structure problem (cpp_subparse point 2).  Please file a bug.\n");
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
    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
	$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
	$typedefname, $varname, $constname, $structisbrace, $macronameref,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
	$requiredregexp, $propname, $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename) = parseTokens($lang, $sublang);
# print STDERR "PROPNAME4: $propname\n";

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

	print STDERR "SUPERLIST IS $superlist\n" if ($localDebug);

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

sub buildCommentFromFields
{
    my @fields = shift;
    my $preAtPart = shift;
    my $message = shift;
    my $string = "";
    my $first = 1;
    my $at = "";

    foreach my $field (@fields) {
	# print STDERR "FIELD: $field\n";
	$string .= $at.$field."\n";
	if ($first && length($preAtPart)) {
		$string .= "    ".$preAtPart."\n";
		$first = 0;
	}
	$at = "\@";
    }

    if ($string =~ /\S/) {
	if ($string =~ /^\s*\/\*/s) {
		$string .= " */\n";
	}
	$string = "$message\n".$string;
    }

    return $string;
}

sub setHollowWithLineNumbers
{
	my $parserStateRef = shift;
	my $treeCur = shift;
	my $blockOffset = shift;
	my $inputCounter = shift;

	$treeCur->{BLOCKOFFSET} = $blockOffset;
	$treeCur->{INPUTCOUNTER} = $inputCounter;

	my $parserState = ${$parserStateRef};
	$parserState->{hollow} = $treeCur;
}

sub macroRegexpFromList
{
	my $nameref = shift;
	my $onlywithpound = shift;
	my %names = %{$nameref};
	my $regexpstring = "";
	my $pipe = "";
	my $lparen = "(";

	foreach my $name (keys %names) {
		# print "MACRONAME $name\n";
		if (!$onlywithpound) {
			$regexpstring .= $lparen.$pipe.$name;
			$pipe = "|"; $lparen = "";
		} elsif ($onlywithpound == 1) {
			# Stuff starting with "#"
			if ($name =~ /^#/) {
				my $temp = $name;
				$temp =~ s/^#//;
				$regexpstring .= $lparen.$pipe.$temp;
				$pipe = "|"; $lparen = "";
			}
		} elsif ($onlywithpound == 2) {
			# Stuff that does NOT start with "#"
			if ($name !~ /^#/) {
				$regexpstring .= $lparen.$pipe.$name;
				$pipe = "|"; $lparen = "";
			}
		}
	}
	if ($regexpstring ne "") { $regexpstring .= ")"; }
	return $regexpstring;
}

sub empty_comment
{
	my @fields = shift;
	if (!scalar(@fields)) { return 1; }
	if (scalar(@fields) == 1 && $fields[0] =~ /^\s*\/\*\!\s*$/) { return 1; }
	return 0;
	

}

1;

