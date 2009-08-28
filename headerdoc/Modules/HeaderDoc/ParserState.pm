#! /usr/bin/perl -w
#
# Class name: 	ParserState
# Synopsis: 	Used by gatherHeaderDoc.pl to hold parser state
# Last Updated: $Date: 2009/03/30 19:38:51 $
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
package HeaderDoc::ParserState;

use strict;
use vars qw($VERSION @ISA);
use HeaderDoc::Utilities qw(isKeyword quote stringToFields);

$HeaderDoc::ParserState::VERSION = '$Revision: 1.8 $';
################ General Constants ###################################
my $debugging = 0;

my $treeDebug = 0;

my $backslashDebug = 0;

my %defaults = (
	frozensodname => "",
	stackFrozen => 0, # set to prevent fake parsed params with inline funcs
	returntype => "",
	freezereturn => 0,       # set to prevent fake return types with inline funcs
	availability => "",      # holds availability string if we find an av macro.
	lang => "C",

	inComment => 0,
	inInlineComment => 0,
	inString => 0,
	inChar => 0,
	inTemplate => 0,
	inOperator => 0,
	inPrivateParamTypes => 0,  # after a colon in a C++ function declaration.
	onlyComments => 1,         # set to 0 to avoid switching to macro parse.
                                  # mode after we have seen a code token.
	inMacro => 0,
	inMacroLine => 0,          # for handling macros in middle of data types.
	seenMacroPart => 0,        # used to control dropping of macro body.
	macroNoTrunc => 1,         # used to avoid truncating body of macros
	inBrackets => 0,           # square brackets ([]).
    # $self->{inPType} = 0;              # in pascal types.
    # $self->{inRegexp} = 0;             # in perl regexp.
    # $self->{regexpNoInterpolate} = 0;  # Don't interpolate (e.g. tr)
    # $self->{inRegexpTrailer} = 0;      # in the cruft at the end of a regexp.
    # $self->{ppSkipOneToken} = 0;       # Comments are always dropped from parsed
                                  # parameter lists.  However, inComment goes
                                  # to 0 on the end-of-comment character.
                                  # This prevents the end-of-comment character
                                  # itself from being added....

    # $self->{lastsymbol} = "";          # Name of the last token, wiped by braces,
                                  # parens, etc.  This is not what you are
                                  # looking for.  It is used mostly for
                                  # handling names of typedefs.
	name => "",                # Name of a basic data type.
	callbackNamePending => 0,  # 1 if callback name could be here.  This is
                                  # only used for typedef'ed callbacks.  All
                                  # other callbacks get handled by the parameter
                                  # parsing code.  (If we get a second set of
                                  # parsed parameters for a function, the first
                                  # one becomes the callback name.)
	callbackName => "",        # Name of this callback.
	callbackIsTypedef => 0,    # 1 if the callback is wrapped in a typedef---
                                  # sets priority order of type matching (up
                                  # one level in headerdoc2HTML.pl).

	namePending => 0,          # 1 if name of func/variable is coming up.
	basetype => "",            # The main name for this data type.
	posstypes => "",           # List of type names for this data type.
	posstypesPending => 1,     # If this token could be one of the
                                  # type names of a typedef/struct/union/*
                                  # declaration, this should be 1.
	sodtype => "",             # 'start of declaration' type.
	sodname => "",             # 'start of declaration' name.
	sodclass => "",            # 'start of declaration' "class".  These
                                  # bits allow us keep track of functions and
                                  # callbacks, mostly, but not the name of a
                                  # callback.

	simpleTypedef => 0,        # High if it's a typedef w/o braces.
	simpleTDcontents => "",    # Guts of a one-line typedef.  Don't ask.
	seenBraces => 0,           # Goes high after initial brace for inline
                                  # functions and macros -only-.  We
                                  # essentially stop parsing at this point.
	kr_c_function => 0,        # Goes high if we see a K&R C declaration.
	kr_c_name => "",           # The name of a K&R function (which would
                                  # otherwise get lost).

    # $self->{lastchar} = "";            # Ends with the last token, but may be longer.
    # $self->{lastnspart} = "";          # The last non-whitespace token.
    # $self->{lasttoken} = "";           # The last token seen (though [\n\r] may be
                                  # replaced by a space in some cases).
	startOfDec => 1,           # Are we at the start of a declaration?
    # $self->{prespace} = 0;             # Used for indentation (deprecated).
    # $self->{prespaceadjust} = 0;       # Indentation is now handled by the parse
                                  # tree (colorizer) code.
    # $self->{scratch} = "";             # Scratch space.
    # $self->{curline} = "";             # The current line.  This is pushed onto
                                  # the declaration at a newline and when we
                                  # enter/leave certain constructs.  This is
                                  # deprecated in favor of the parse tree.
    # $self->{curstring} = "";           # The string we're currently processing.
    # $self->{continuation} = 0;         # An obscure spacing workaround.  Deprecated.
    # $self->{forcenobreak} = 0;         # An obscure spacing workaround.  Deprecated.
	occmethod => 0,            # 1 if we're in an ObjC method.
    # $self->{occspace} = 0;             # An obscure spacing workaround.  Deprecated.
	occmethodname => "",       # The name of an objective C method (which
                                  # gets augmented to be this:that:theother).
	preTemplateSymbol => "",   # The last symbol prior to the start of a
                                  # C++ template.  Used to determine whether
                                  # the type returned should be a function or
                                  # a function template.
	preEqualsSymbol => "",     # Used to get the name of a variable that
                                  # is followed by an equals sign.
	valuepending => 0,         # True if a value is pending, used to
                                  # return the right value.
	value => "",               # The current value.
	parsedParamParse => 0,
    # $self->{parsedParam} = "";         # The current parameter being parsed.
    # $self->{postPossNL} = 0;           # Used to force certain newlines to be added
                                  # to the parse tree (to end macros, etc.)
	categoryClass => "",
	classtype => "",
	inClass => 0,

	seenTilde => 0,          # set to 1 for C++ destructor.

	# parsedParamList => undef, # currently active parsed parameter list.
	# pplStack => undef, # stack of parsed parameter lists.  Used to handle
                       # fields and parameters in nested callbacks/structs.
	# freezeStack => undef, # copy of pplStack when frozen.

	initbsCount => 0,
	# hollow => undef,      # a spot in the tree to put stuff.
	noInsert => 0,
	bracePending => 0,	# set to 1 if lack of a brace would change
				# from being a struct/enum/union/typedef
				# to a variable.
	backslashcount => 0,

	functionReturnsCallback => 0

);

# print STDERR "DEFAULTS: startOfDec: ".$defaults{startOfDec}."\n";
# print STDERR "DEFAULTS: inClass: ".$defaults{inClass}."\n";

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my %selfhash = %defaults;
    my $self = \%selfhash;

    # print STDERR "startOfDec: ".$self->{startOfDec}."\n";
    # print STDERR "startOfDecX: ".$defaults{startOfDec}."\n";

# print STDERR "CREATING NEW PARSER STATE!\n";
    
    bless($self, $class);
    $self->_initialize();
    # Now grab any key => value pairs passed in
    my (%attributeHash) = @_;
    foreach my $key (keys(%attributeHash)) {
        my $ucKey = uc($key);
        $self->{$ucKey} = $attributeHash{$key};
    }  
    return ($self);
}

sub _initialize {
    my($self) = shift;
    my @arr1 = ();
    my @arr2 = ();
    my @arr3 = ();

    $self->{parsedParamList} = \@arr1; # currently active parsed parameter list.
    $self->{pplStack} = \@arr2; # stack of parsed parameter lists.  Used to handle
                       # fields and parameters in nested callbacks/structs.
    $self->{freezeStack} = \@arr3; # copy of pplStack when frozen.

    my %orighash = %{$self};

    return;

    # my($self) = shift;

    $self->{frozensodname} = "";
    $self->{stackFrozen} = 0; # set to prevent fake parsed params with inline funcs
    $self->{returntype} = "";
    $self->{freezereturn} = 0;       # set to prevent fake return types with inline funcs
    $self->{availability} = "";      # holds availability string if we find an av macro.
    $self->{lang} = "C";

    $self->{inComment} = 0;
    $self->{inInlineComment} = 0;
    $self->{inString} = 0;
    $self->{inChar} = 0;
    $self->{inTemplate} = 0;
    $self->{inOperator} = 0;
    $self->{inPrivateParamTypes} = 0;  # after a colon in a C++ function declaration.
    $self->{onlyComments} = 1;         # set to 0 to avoid switching to macro parse.
                                  # mode after we have seen a code token.
    $self->{inMacro} = 0;
    $self->{inMacroLine} = 0;          # for handling macros in middle of data types.
    $self->{seenMacroPart} = 0;        # used to control dropping of macro body.
    $self->{macroNoTrunc} = 1;         # used to avoid truncating body of macros
    $self->{inBrackets} = 0;           # square brackets ([]).
    # $self->{inPType} = 0;              # in pascal types.
    # $self->{inRegexp} = 0;             # in perl regexp.
    # $self->{regexpNoInterpolate} = 0;  # Don't interpolate (e.g. tr)
    # $self->{inRegexpTrailer} = 0;      # in the cruft at the end of a regexp.
    # $self->{ppSkipOneToken} = 0;       # Comments are always dropped from parsed
                                  # parameter lists.  However, inComment goes
                                  # to 0 on the end-of-comment character.
                                  # This prevents the end-of-comment character
                                  # itself from being added....

    # $self->{lastsymbol} = "";          # Name of the last token, wiped by braces,
                                  # parens, etc.  This is not what you are
                                  # looking for.  It is used mostly for
                                  # handling names of typedefs.
    $self->{name} = "";                # Name of a basic data type.
    $self->{callbackNamePending} = 0;  # 1 if callback name could be here.  This is
                                  # only used for typedef'ed callbacks.  All
                                  # other callbacks get handled by the parameter
                                  # parsing code.  (If we get a second set of
                                  # parsed parameters for a function, the first
                                  # one becomes the callback name.)
    $self->{callbackName} = "";        # Name of this callback.
    $self->{callbackIsTypedef} = 0;    # 1 if the callback is wrapped in a typedef---
                                  # sets priority order of type matching (up
                                  # one level in headerdoc2HTML.pl).

    $self->{namePending} = 0;          # 1 if name of func/variable is coming up.
    $self->{basetype} = "";            # The main name for this data type.
    $self->{posstypes} = "";           # List of type names for this data type.
    $self->{posstypesPending} = 1;     # If this token could be one of the
                                  # type names of a typedef/struct/union/*
                                  # declaration, this should be 1.
    $self->{sodtype} = "";             # 'start of declaration' type.
    $self->{sodname} = "";             # 'start of declaration' name.
    $self->{sodclass} = "";            # 'start of declaration' "class".  These
                                  # bits allow us keep track of functions and
                                  # callbacks, mostly, but not the name of a
                                  # callback.

    $self->{simpleTypedef} = 0;        # High if it's a typedef w/o braces.
    $self->{simpleTDcontents} = "";    # Guts of a one-line typedef.  Don't ask.
    $self->{seenBraces} = 0;           # Goes high after initial brace for inline
                                  # functions and macros -only-.  We
                                  # essentially stop parsing at this point.
    $self->{kr_c_function} = 0;        # Goes high if we see a K&R C declaration.
    $self->{kr_c_name} = "";           # The name of a K&R function (which would
                                  # otherwise get lost).

    # $self->{lastchar} = "";            # Ends with the last token, but may be longer.
    # $self->{lastnspart} = "";          # The last non-whitespace token.
    # $self->{lasttoken} = "";           # The last token seen (though [\n\r] may be
                                  # replaced by a space in some cases.
    $self->{startOfDec} = 1;           # Are we at the start of a declaration?
    # $self->{prespace} = 0;             # Used for indentation (deprecated).
    # $self->{prespaceadjust} = 0;       # Indentation is now handled by the parse
                                  # tree (colorizer) code.
    # $self->{scratch} = "";             # Scratch space.
    # $self->{curline} = "";             # The current line.  This is pushed onto
                                  # the declaration at a newline and when we
                                  # enter/leave certain constructs.  This is
                                  # deprecated in favor of the parse tree.
    # $self->{curstring} = "";           # The string we're currently processing.
    # $self->{continuation} = 0;         # An obscure spacing workaround.  Deprecated.
    # $self->{forcenobreak} = 0;         # An obscure spacing workaround.  Deprecated.
    $self->{occmethod} = 0;            # 1 if we're in an ObjC method.
    # $self->{occspace} = 0;             # An obscure spacing workaround.  Deprecated.
    $self->{occmethodname} = "";       # The name of an objective C method (which
                                  # gets augmented to be this:that:theother).
    $self->{preTemplateSymbol} = "";   # The last symbol prior to the start of a
                                  # C++ template.  Used to determine whether
                                  # the type returned should be a function or
                                  # a function template.
    $self->{preEqualsSymbol} = "";     # Used to get the name of a variable that
                                  # is followed by an equals sign.
    $self->{valuepending} = 0;         # True if a value is pending, used to
                                  # return the right value.
    $self->{value} = "";               # The current value.
    $self->{parsedParamParse} => 0,
    # $self->{parsedParam} = "";         # The current parameter being parsed.
    # $self->{postPossNL} = 0;           # Used to force certain newlines to be added
                                  # to the parse tree (to end macros, etc.)
    $self->{categoryClass} = "";
    $self->{classtype} = "";
    $self->{inClass} = 0;

    $self->{seenTilde} = 0;          # set to 1 for C++ destructor.

    #my @emptylist = ();
    #$self->{parsedParamList} = \@emptylist; # currently active parsed parameter list.
    #my @emptylistb = ();
    #$self->{pplStack} = \@emptylistb; # stack of parsed parameter lists.  Used to handle
                       # fields and parameters in nested callbacks/structs.
    #my @emptylistc = ();
    #$self->{freezeStack} = \@emptylistc; # copy of pplStack when frozen.

    $self->{initbsCount} = 0;
    $self->{hollow} = undef;      # a spot in the tree to put stuff.
    $self->{noInsert} = 0;
    $self->{bracePending} = 0;	# set to 1 if lack of a brace would change
				# from being a struct/enum/union/typedef
				# to a variable.
    $self->{backslashcount} = 0;

    # foreach my $key (keys %{$self}) {
	# if ($self->{$key} != $orighash{$key}) {
		# print STDERR "HASH DIFFERS FOR KEY $key (".$self->{$key}." != ".$orighash{$key}.")\n";
	# } else {
		# print STDERR "Hash keys same for key $key\n";
	# }
    # }

    return $self;
}

# For consistency.
sub dbprint
{
    my $self = shift;
    return $self->print();
}

sub rollback
{
    my $self = shift;

    my $localDebug = 0;

    my $cloneref = $self->{rollbackState};
    my $clone = ${$cloneref};
    my %selfhash = %{$self};
    my %clonehash = %{$clone};

    if ($localDebug) {
	print STDERR "BEGIN PARSER STATE:\n";
	foreach my $key (keys(%clonehash)) {
		if ($self->{$key} ne $clone->{$key}) {
			print STDERR "$key: ".$self->{$key}." != ".$clone->{$key}."\n";
		}
	}
	print STDERR "END PARSER STATE\n";
    }
    foreach my $key (keys(%selfhash)) {
	# print STDERR "$key => $self->{$key}\n";
	$self->{$key} = undef;
    }
    foreach my $key (keys(%clonehash)) {
	$self->{$key} = $clone->{$key};
    }
    $self->{rollbackState} = undef;
}

sub rollbackSet
{
    my $self = shift;

    my $clone = HeaderDoc::ParserState->new();
    my %selfhash = %{$self};

    # print STDERR "BEGIN PARSER STATE:\n";
    foreach my $key (keys(%selfhash)) {
	# print STDERR "$key => $self->{$key}\n";
	$clone->{$key} = $self->{$key};
    }
    $self->{rollbackState} = \$clone;
    # print STDERR "END PARSER STATE\n";
}

sub print
{
    my $self = shift;
    my %selfhash = %{$self};

    print STDERR "BEGIN PARSER STATE:\n";
    foreach my $key (keys(%selfhash)) {
	print STDERR "$key => $self->{$key}\n";
    }
    print STDERR "END PARSER STATE\n";
}

sub resetBackslash
{
    my $self = shift;
    $self->{backslashcount}=0;

    print STDERR "RESET BACKSLASH. COUNT NOW ".$self->{backslashcount}."\n" if ($backslashDebug);
}

sub addBackslash
{
    my $self = shift;

    $self->{backslashcount}++;

    print STDERR "ADD BACKSLASH. COUNT NOW ".$self->{backslashcount}."\n" if ($backslashDebug);

}

sub isQuoted
{
    my $self = shift;
    my $lang = shift;
    my $sublang = shift;

    my $inSingle = $self->{inChar};
    my $inString = $self->{inString};
    my $count = $self->{backslashcount};

	print STDERR "LANG: $lang INSINGLE: $inSingle INSTRING: $inString\n" if ($backslashDebug);

    # Shell scripts treat single quotes as raw data.  Backslashes
    # inside are not treated as quote characters, so to put a single
    # quote, you have to put it inside a double quote contest, e.g.
    # "It's" or 'It'"'"'s'
    if ($inSingle && $lang eq "shell") {
	print STDERR "isQuoted: Shell script single quote backslash: not quoted.  Returning 0 (count is $count).\n" if ($backslashDebug);
	return 0;
    }

    # C shell scripts don't interpret \ within a string.
    if ($inString && $lang eq "shell" && $sublang eq "csh") {
	print STDERR "isQuoted: C Shell script backslash in double quotes: not quoted.  Returning 0 (count is $count).\n" if ($backslashDebug);
	return 0;
    }


    if ($count % 2) {
	print STDERR "isQuoted: Returning 1 (count is $count).\n" if ($backslashDebug);
	return 1;
    }
    print STDERR "isQuoted: Returning 0 (count is $count).\n" if ($backslashDebug);
    return 0;
}

1;
