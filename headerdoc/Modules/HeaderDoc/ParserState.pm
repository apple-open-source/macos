#! /usr/bin/perl -w
#
# Class name: 	ParserState
# Synopsis: 	Used by gatherHeaderDoc.pl to hold parser state
# Author: David Gatwood(dgatwood@apple.com)
# Last Updated: $Date: 2004/10/07 17:25:56 $
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
use HeaderDoc::Utilities qw(isKeyword parseTokens quote stringToFields);
use HeaderDoc::BlockParse qw(blockParse nspaces);

$VERSION = '$Revision: 1.1.2.1 $';
################ General Constants ###################################
my $debugging = 0;

my $treeDebug = 0;

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};

# print "CREATING NEW PARSER STATE!\n";
    
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
    # $self->{parsedParam} = "";         # The current parameter being parsed.
    # $self->{postPossNL} = 0;           # Used to force certain newlines to be added
                                  # to the parse tree (to end macros, etc.)
    $self->{categoryClass} = "";
    $self->{classtype} = "";
    $self->{inClass} = 0;

    $self->{seenTilde} = 0;          # set to 1 for C++ destructor.

    my @emptylist = ();
    $self->{parsedParamList} = \@emptylist; # currently active parsed parameter list.
    my @emptylistb = ();
    $self->{pplStack} = \@emptylistb; # stack of parsed parameter lists.  Used to handle
                       # fields and parameters in nested callbacks/structs.
    my @emptylistc = ();
    $self->{freezeStack} = \@emptylistc; # copy of pplStack when frozen.

    $self->{initbsCount} = 0;
    $self->{hollow} = undef;      # a spot in the tree to put stuff.
    $self->{noInsert} = 0;

    return $self;
}

sub print
{
    my $self = shift;
    my %selfhash = %{$self};

    print "BEGIN PARSER STATE:\n";
    foreach my $key (keys(%selfhash)) {
	print "$key => $self->{$key}\n";
    }
    print "END PARSER STATE\n";
}

1;
