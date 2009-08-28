#! /usr/bin/perl -w
#
# Class name: 	ParseTree
# Synopsis: 	Used by headerdoc2html.pl to hold parse trees
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
package HeaderDoc::ParseTree;

use strict;
use vars qw($VERSION @ISA);
use HeaderDoc::Utilities qw(isKeyword parseTokens quote stringToFields casecmp emptyHDok complexAvailabilityToArray);
use HeaderDoc::BlockParse qw(blockParse nspaces);
use Carp qw(cluck);

# use WeakRef;


$HeaderDoc::ParseTree::VERSION = '$Revision: 1.24 $';
################ General Constants ###################################
my $debugging = 0;

my $apioDebug = 0;

my $treeDebug = 0;
my %defaults = (
	# TOKEN => undef,
	# NEXT => undef,
	# FIRSTCHILD => undef,
	# APIOWNER => undef,
	# PARSEDPARAMS => undef,
	PETDONE => 0,
	REFCOUNT => 0,
	# XMLTREE => undef,
	# HTMLTREE => undef,
	# CPNC => undef,
	# NTNC => undef,
	# RAWPARSEDPARAMETERS => undef,
	# PARSERSTATE => undef,
	# ACCESSCONTROLSTATE => undef,
	# FILENAME => undef,
	# LINENUM => undef,
	# HIDDEN => undef
    );

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my %selfhash = %defaults;
    my $self = \%selfhash;

    # cluck("Parse tree $self created\n");
    
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

    $self->{ACCESSCONTROLSTATE} = $HeaderDoc::AccessControlState;
    $self->{FILENAME} = $HeaderDoc::headerObject->filename();
    $self->{FULLPATH} = $HeaderDoc::headerObject->fullpath();
    $self->{LINENUM} = $HeaderDoc::CurLine;
    $self->{HIDDEN} = $HeaderDoc::hidetokens;
    $self->{APIOWNER} = ();
    $self->{PARSEDPARAMS} = ();
    $self->{RAWPARSEDPARAMETERS} = ();

    return;

    my($self) = shift;
    # $self->{TOKEN} = undef;
    # $self->{NEXT} = undef;
    # $self->{FIRSTCHILD} = undef;
    $self->{APIOWNER} = ();
    $self->{ACCESSCONTROLSTATE} = $HeaderDoc::AccessControlState;
    $self->{PARSEDPARAMS} = ();
    $self->{FILENAME} = $HeaderDoc::headerObject->filename();
    $self->{FULLPATH} = $HeaderDoc::headerObject->fullpath();
    $self->{LINENUM} = $HeaderDoc::CurLine; # $HeaderDoc::headerObject->linenum();
    # $self->{PETDONE} = 0;
    $self->{HIDDEN} = $HeaderDoc::hidetokens;
    $self->{REFCOUNT} = 0;
    # $self->{XMLTREE} = undef;
    # $self->{HTMLTREE} = undef;
    # $self->{CPNC} = undef;
    # # $self->{NPCACHE} = undef;
    # $self->{NTNC} = undef;
    # # $self->{CTSUB} = undef;
    # # $self->{CTSTRING} = undef;
    $self->{RAWPARSEDPARAMETERS} = ();
    $self->{PARSERSTATE} = undef; # $HeaderDoc::curParserState;
}

my $colorDebug = 0;

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
        $clone = shift;
    } else {
        $clone = HeaderDoc::ParseTree->new(); 
    }

    # $self->SUPER::clone($clone);

    # now clone stuff specific to ParseTree

    $clone->{TOKEN} = $self->{TOKEN};

    # Note: apiOwner is no longer recursive, so there is no need
    # to recursively clone parse trees.  Only the top node will
    # ever be modified legitimately (except when pruning headerdoc
    # comments, but that needs to occur for all instances).

    $clone->{FIRSTCHILD} = $self->{FIRSTCHILD};
    $clone->{NEXT} = $self->{NEXT};

    # $clone->{FIRSTCHILD} = undef;
    # if ($self->{FIRSTCHILD}) {
	# my $firstchild = $self->{FIRSTCHILD};
	# $clone->{FIRSTCHILD} = $firstchild->clone();
    # }
    # $clone->{NEXT} = undef;
    # if ($self->{NEXT}) {
	# my $next = $self->{NEXT};
	# $clone->{NEXT} = $next->clone();
    # }

    $clone->{APIOWNER} = $self->{APIOWNER};
    $clone->{PARSEDPARAMS} = $self->{PARSEDPARAMS};
    $clone->{PETDONE} = 0;

    return $clone;
}

sub addSibling
{
    my $self = shift;
    my $name = shift;
    my $hide = shift;
    my $newnode = HeaderDoc::ParseTree->new();
    my $localDebug = 0;

# print STDERR "addSibling $self\n";
    print STDERR "addSibling $self \"$name\" HIDDEN: $hide\n" if ($treeDebug || $localDebug);

    # Always hide siblings of descendants of elements that the parser
    # tells us to hide.  Also spaces if they are the immediate successor
    # to the element right after a hidden element, regardless.
    if ($self->hidden() == 2) {
	$hide = 2;
    } elsif ($name =~ /^\s+$/ && $self->hidden()) {
	$hide = 1;
    }

    print STDERR "HIDE NOW $hide\n" if ($treeDebug || $localDebug);

    my $parent = $self->parent;

    my $pos = $self;
    # if ($parent) {
	# $pos = $parent->lastchild();
	# bless($pos, "HeaderDoc::ParseTree");
    # } else {
	# warn "NOPARENTA!\nNOPARENTB!\nNOPARENTC!\n";
	bless($pos, "HeaderDoc::ParseTree");

	# print STDERR "POS: $pos\n";
	# while ($pos && $pos->next()) {
		# $pos = $pos->next();
		# bless($pos, "HeaderDoc::ParseTree");
		# # print STDERR "POS: $pos: ".$pos->token()."\n";
	# }
    	# bless($pos, "HeaderDoc::ParseTree");
    # }
    $newnode->token($name);
    if ($hide) { $newnode->hidden($hide); }
    $newnode->parent($parent);
    # if ($parent) { $parent->lastchild($newnode); }

    my $noderef = $newnode;

    return $pos->next($noderef);
	# print STDERR "$self (".$self->token().") RET $ret (".$ret->token().")\n";
    # return $ret;
}

sub addChild
{
    my $self = shift;
    my $name = shift;
    my $hide = shift;

# print STDERR "addChild\n";
    print STDERR "addChild $self \"$name\"\n" if ($treeDebug);

    # If the parser wants a node hidden, any children of such a node
    # should be hidden, as should any siblings of those children or their
    # descendants.  Siblings of the original node should not, however.
    #
    # The block parser passes in 3 for "$hide" to hide the node and its
    # descendants.  Its descendants have their hidden value set to 2 so
    # that their siblings will also be hidden, but the top level node
    # still retains the original hidden value of 3.
    if ($self->hidden() == 2 || $self->hidden() == 3) { $hide = 2; }

    if (!$self->firstchild()) {
	my $newnode = HeaderDoc::ParseTree->new();
	if ($hide) { $newnode->hidden($hide); }
	$newnode->token($name);
	my $noderef = $newnode;
	$newnode->parent($self);
	# $self->lastchild($noderef);
	return $self->firstchild($noderef);
    } else {
	warn "addChild called when firstchild exists.  Dropping.\n";
	# my $node = $self->firstchild();
	# bless($node, "HeaderDoc::ParseTree");
	# return $node->addSibling($name, $hide);
    }
}

# /*! Check to see if this node is at the same level of the parse tree
#     and occurs after the node specified (or is the node specified). */
sub isAfter
{
	my $self = shift;
	my $node = shift;

	my $ptr = $node;
	while ($ptr) {
		if ($ptr == $self) {
			return 1;
		}
		$ptr = $ptr->next();
	}
	return 0;
}

# /*! Add an additional apiOwner for a tree.
#  */
sub addAPIOwner {
    my $self = shift;
    my $newapio = shift;

    print STDERR "addAPIOwner: SELF WAS $self\n" if ($apioDebug);
    print STDERR "addAPIOwner: APIO WAS $newapio\n" if ($apioDebug);
    if (!$newapio) {
	warn("apiOwner called with empty APIO!\n");
	return undef;
    } else {
	$self->{REFCOUNT}++;
	# weaken($newapio);
	push(@{$self->{APIOWNER}}, $newapio);
    }

    return $newapio;
}

# /*! Set the apiOwner for the tree.
#  */
sub apiOwnerSub
 {
    my $self = shift;
    my $old = shift;
    my $new = shift;

    my $localDebug = 0;

    print STDERR "apiOwnerSub called with SELF=$self OLD=$old NEW=$new\n" if ($localDebug);;

    my @arr = ();

    my $found = 0;
    foreach my $possowner (@{$self->{APIOWNER}}) {
	if ($possowner != $old) {
		push(@arr, $possowner);
	} else {
		$found = 1;
	}
    }
    if (!$found) {
	warn("OLD API OWNER NOT FOUND IN apiOwnerSub().  Please file a bug.\n");
    }
    push(@arr, $new);

    $self->{APIOWNER} = \@arr; 
}

# /*! Set the apiOwner for the tree.
#  */
sub apiOwner {
    my $self = shift;

    if (@_) {
	my $newapio = shift;

	if (!$newapio) {
		warn("apiOwner called with empty APIO!\n");
	}

	print STDERR "apiOwner: SETTING TO $newapio (".$newapio->rawname().")\n" if ($apioDebug);

	$self->{APIOWNER} = ();
        push(@{$self->{APIOWNER}}, $newapio);
	$self->{REFCOUNT} = 1;
    }

    my $apio = undef;
    foreach my $possowner (@{$self->{APIOWNER}}) {
	# print STDERR "TESTING $possowner\n";
	if ($possowner !~ /HeaderDoc::HeaderElement/) {
		if ($possowner !~ /HeaderDoc::APIOwner/) {
			if ($possowner) {
				$apio = $possowner;
				# print STDERR "CHOSE $apio\n";
			}
		}
	}
    }
    if (!$apio) {
	$apio = pop(@{$self->{APIOWNER}});
	push(@{$self->{APIOWNER}}, $apio);
	# print STDERR "GUESSING $apio\n";
    }

    # Try this if you run into trouble, but....
    # if (!$apio && $self->{PARENT}) {
	# return $self->{PARENT}->apiOwner();
    # }

    return $apio;
}

sub apiOwners
{
    my $self = shift;

    # foreach my $apio (@{$self->{APIOWNER}} ) {
	# print STDERR "APIOWNER LIST INCLUDES $apio\n";
    # }

    return $self->{APIOWNER};
}

sub lastSibling {
    my $self = shift;

    while ($self && $self->next()) { $self = $self->next(); }

    return $self;
}

sub acs {
    my $self = shift;

    if (@_) {
        $self->{ACCESSCONTROLSTATE} = shift;
    }
    return $self->{ACCESSCONTROLSTATE};
}

sub token {
    my $self = shift;

    if (@_) {
        $self->{TOKEN} = shift;
    }
    return $self->{TOKEN};
}

sub hidden {
    my $self = shift;

    if (@_) {
	my $value = shift;
        $self->{HIDDEN} = $value;
	my $fc = $self->firstchild();
	if ($fc) { $fc->hiddenrec($value); }
    }
    return $self->{HIDDEN};
}

sub hiddenrec
{
    my $self = shift;
    my $value = shift;

    # print STDERR "SETTING HIDDEN VALUE OF TOKEN ".$self->token()." to $value\n";
    # $self->hidden($value);
    $self->{HIDDEN} = $value;

    my $fc = $self->firstchild();
    if ($fc) { $fc->hiddenrec($value); }
    my $nx = $self->next();
    if ($nx) { $nx->hiddenrec($value); }
}

sub objCparsedParams()
{
    my $self = shift;
    my @parsedParams = ();
    my $objCParmDebug = 0;

    my $inType = 0;
    my $inName = 0;
    my $position = 0;
    my $curType = "";
    my $curName = "";
    my $cur = $self;
    my @stack = ();

    my $eoDec = 0;
    my $lastTag = "";
    my $tagName = "";

    my $noParse = 1;
    my $noTag = 1;
    while ($cur || scalar(@stack)) {
	while (!$cur && !$eoDec) {
	    if (!($cur = pop(@stack))) {
		$eoDec = 1;
	    } else {
		$cur = $cur->next();
	    }
	}

	if ($eoDec) { last; }

	# process this element
	my $token = $cur->token();
	if ($token eq ":") {
	    if (!$noTag) {
		$tagName = $lastTag;
	    }
	    $noParse = 0;
	} elsif ($noParse) {
	    # drop token on the floor.  It's part of the name.
	} elsif ($token eq "(") {
	    $inType++;
	    $curType .= $token;
	} elsif ($token eq ")") {
	    if (!(--$inType)) {
		$inName = 1;
		$noTag = 0;
	    }
	    $curType .= $token;
	} elsif ($token =~ /^[\s\W]/o && !$inType) {
	    # drop white space and symbols on the floor (except
	    # for pointer types)

	    if ($inName && ($curName ne "")) {
		$inName = 0;
		my $param = HeaderDoc::MinorAPIElement->new();
		$param->linenuminblock($self->apiOwner()->linenuminblock());
		$param->blockoffset($self->apiOwner()->blockoffset());
		# $param->linenum($self->apiOwner()->linenum());
		$param->outputformat($self->apiOwner()->outputformat());
		$param->tagname($tagName);
		$param->name($curName);
		$param->type($curType);
		$param->position($position++);
		print STDERR "ADDED $curType $curName [$tagName]\n" if ($objCParmDebug);
		$curName = "";
		$curType = "";
		push(@parsedParams, $param);
		$noParse = 1;
	    }

	} elsif ($inType) {
	    $curType .= $token;
	} elsif ($inName) {
	    $curName .= $token;
	}

	my $fc = $cur->firstchild();
	if ($fc) {
	    push(@stack, $cur);
	    $cur = $fc;
	} else { 
	    $cur = $cur->next();
	}
	if ($token =~ /\w/) {
		$lastTag = $token;
	}
    }

    if ($objCParmDebug) {
	foreach my $parm (@parsedParams) {
	    print STDERR "OCCPARSEDPARM: ".$parm->type()." ".$parm->name()."\n";
	}
    }

    return @parsedParams;
}

# /*! This subroutine is for future transition.  The end goal is to
#     move the parsed parameter support from the HeaderElement level
#     entirely into the parse tree. */
sub parsedParams($)
{
    my $self = shift;
    my @array = ();

# print STDERR "parsedParams called\n";

    if (@_) {
	if ($self->apiOwner() eq "HeaderDoc::Method") {
	    @{$self->{PARSEDPARAMS}} = $self->objCparsedParams();
	} else {
	    my $pplref = shift;
	    # @{$self->{PARSEDPARAMS}} = @_;
	    @{$self->{PARSEDPARAMS}} = @{$pplref};
	    # for my $parm (@{$pplref}) {
		# print STDERR "ADDING PARM $parm\n";
	    # }
	}
    }

    if (!($self->{PARSEDPARAMS})) {
	# print STDERR "PARSEDPARAMS PROBLEM: TOKEN WAS ".$self->token()."\n";
	# print STDERR "PRINTTREE:\n";
	# $self->printTree();
	# print STDERR "ENDOFTREE\n";
	my $next = $self->next();
	if ($next) { return $next->parsedParams(); }
	else { return undef; }
	# else { die("Can't find parsed params\n"); }
    }
# print STDERR "HERE: $self : ". $self->token." : ".$self->{PARSEDPARAMS}."\n";
    # foreach my $parm (@{$self->{PARSEDPARAMS}}) {
	# print STDERR "FOUND PARM $parm\n";
    # }
    return @{$self->{PARSEDPARAMS}};
}

sub slowprev()
{
    my $self = shift;

    my $parent = $self->parent;
    my $fc = $parent->firstchild;
    while ($fc && $fc->next && ($fc->next != $self)) { $fc = $fc->next; }
    return $fc;
}

sub parsedParamCopy()
{
    my $self = shift;
    my $pplref = shift;
    my $localDebug = 0;

    my @parms = @{$pplref};
    my @newparms = ();

    foreach my $parm (@parms) {
	push(@newparms, $parm);
    }

    $self->parsedParams(\@newparms);
print STDERR "PARSEDPARAMCOPY -> $self\n" if ($localDebug);
print STDERR "TOKEN WAS ".$self->token()."\n" if ($localDebug);
}

# /*! This subroutine handles embedded HeaderDoc markup, returning a list
#     of parameters, constants, etc.
#  */
sub processEmbeddedTags
{
    my $self = shift;
    my $xmlmode = shift;
    my $apiOwner = shift; # Always subclass of APIOwner, used as "true" apio value for calling APIOprocessEmbeddedTagsRec

    my $apiolist = $self->apiOwners();
    my $apio = $self->apiOwner();

    # $self->printTree();
    # $self->dbprint();
    my $localDebug = 0;
    # if ($apio->isAPIOwner()) { $localDebug = 1; }

    print STDERR "PET: $apio\n" if ($localDebug);
    print STDERR $apio->name()."\n" if ($localDebug);
    print STDERR "APIOLIST IS $apiolist\n" if ($localDebug);;
    # for my $tempapio (@{$apiolist}) {
	# print STDERR "RETURNED APIOLIST INCLUDES $tempapio\n";
    # }

    if ($self->{PETDONE}) {
	print STDERR "SHORTCUT\n" if ($localDebug);
	return;
    }
    $self->{PETDONE} = 1;

    if (!$apio) { return; }

    my $apioclass = ref($apio) || $apio;

    my $old_enable_cpp = $HeaderDoc::enable_cpp;
    if ($apioclass =~ /HeaderDoc::PDefine/ && $apio->parseOnly()) {
	if ($HeaderDoc::enable_cpp) {
		print STDERR "CPP Enabled.  Not processing comments embedded in #define macros marked as 'parse only'.\n" if ($localDebug);
		return;
	}
    } elsif ($apioclass =~ /HeaderDoc::PDefine/) {
	if ($HeaderDoc::enable_cpp) {
		print STDERR "Temporarily disabling CPP.\n" if ($localDebug);
		$HeaderDoc::enable_cpp = 0;
	}
    }

    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macronamesref) = parseTokens($apio->lang(), $apio->sublang());
		# @_;

    my $eoDeclaration = 1;
    my $lastDeclaration = "";
    my $curDeclaration = "";
    my $sodec = $self;
    my $pendingHDcomment = "";

    my ($case_sensitive, $keywordhashref) = $apio->keywords();

	my $eocquot = quote($eoc);

    my $lastnode = undef;
    my $parserState = $self->parserState();
    if ($parserState) {
	print STDERR "PARSERSTATE\n" if ($localDebug);
	$lastnode = $parserState->{lastTreeNode};
	print STDERR "LASTNODE: $lastnode\n" if ($localDebug);
	if ($lastnode && $localDebug) { print STDERR "LASTNODE TEXT: \"".$lastnode->token()."\"\n"; }
    }

    my $enable_javadoc_comments = $HeaderDoc::parse_javadoc || ($apio->lang() eq "java");
    # print STDERR "EJC: $enable_javadoc_comments\n";

    if ($apio->isAPIOwner()) {
	print STDERR "Owner is APIOwner.  Using APIOprocessEmbeddedTagsRec for parse tree $self.\n" if ($localDebug);

	$self->APIOprocessEmbeddedTagsRec($apiOwner, $soc, $eoc, $ilc, $ilc_b, $lbrace, $case_sensitive, $lastnode, 0, 1, $enable_javadoc_comments);
    } else {
	print STDERR "calling processEmbeddedTagsRec for $apio (".$apio->name().") for parse tree $self.\n" if ($localDebug);
	$self->processEmbeddedTagsRec($xmlmode, $eoDeclaration, $soc, $eoc, $eocquot, $ilc, $ilc_b, $lbrace, $rbrace, $typedefname,
		$case_sensitive, $keywordhashref, $lastDeclaration, $curDeclaration, $pendingHDcomment,
		$apio, $apiolist, $sodec, $lastnode, $enable_javadoc_comments);
    }

print STDERR "PETDONE\n" if ($localDebug);

    $HeaderDoc::enable_cpp = $old_enable_cpp;

    return;
}

# /*! This subroutine helps the parse tree code by simplifying the
#     work needed to use the block parser.
#  */
sub getNameAndFieldTypeFromDeclaration
{
    my $self = shift;
    my $string = shift;
    my $apio = shift;
    my $typedefname = shift;
    my $case_sensitive = shift;
    my $keywordhashref = shift;

    my $localDebug = 0;
    my $inputCounter = 0;

    my $fullpath = $apio->fullpath();
    my $linenum = $apio->linenum();
    my $lang = $apio->lang();
    my $sublang = $apio->sublang();

    my $blockoffset = $linenum;
    my $argparse = 2;

    # This never hurts just to make sure the parse terminates.
    # Be sure to add a newline before the semicolon in case
    # there's an inline comment at the end.
    $string .= "\n;\n";

    print STDERR "STRING WAS $string\n" if ($localDebug);

    cluck("getNameAndFieldTypeFromDeclaration backtrace\n") if ($localDebug);

    my @lines = split(/\n/, $string);

    # my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
        # $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	# $enumname,
        # $typedefname, $varname, $constname, $structisbrace, $macronameref)
		# = parseTokens($lang, $sublang);

    # my @newlines = ();
    foreach my $line (@lines) {
	$line .= "\n";
	# push(@newlines, $line);
        # print STDERR "LINE: $line\n" if ($localDebug);
    }
    # @lines = @newlines;

    # my ($case_sensitive, $keywordhashref) = $apio->keywords();

    my $lastlang = $HeaderDoc::lang;
    my $lastsublang = $HeaderDoc::sublang;
    $HeaderDoc::lang = $apio->lang;
    $HeaderDoc::sublang = $apio->sublang;
    my ($inputCounter, $declaration, $typelist, $namelist, $posstypes, $value, $pplStackRef, $returntype, $privateDeclaration, $treeTop, $simpleTDcontents, $availability, $fileoffset, $conformsToList) = blockParse($fullpath, $blockoffset, \@lines, $inputCounter, $argparse, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);
    $HeaderDoc::lang = $lastlang;
    $HeaderDoc::sublang = $lastsublang;

    # @@@ FIX UP TYPES HERE

    print STDERR "IC:$inputCounter DEC:$declaration TL:$typelist NL:$namelist PT:$posstypes VAL:$value PSR:$pplStackRef RT:$returntype PD:$privateDeclaration TT:$treeTop STC:$simpleTDcontents AV:$availability\n" if ($localDebug);

    $self->parsedParamCopy($pplStackRef);

    my $name = $namelist;
    $name =~ s/^\s*//so; # ditch leading spaces
    $name =~ s/\s.*$//so; # ditch any additional names. (There shouldn't be any)
	# print STDERR "NAME WAS $name\n";
    my $typestring = $typelist . $posstypes;

print STDERR "TS: $typestring\n" if ($localDebug);

    my $type = "\@constant";
    if ($typestring =~ /^(function|method|ftmplt|operator|callback)/o) {
	$type = "\@$1";
	if ($typestring =~ /(ftmplt|operator)/) { $type = "\@function"; }
	# $type = "\@callback";
    } elsif ($typestring =~ /^(class|interface|module|category|protocol)/o) {
	$typestring =~ s/^ *//;
	$type = "\@$typestring";
	$type =~ s/ .*$//;
    } elsif ($typestring =~ /^(struct|union|record|enum|typedef)/o || (($typedefname ne "") && $typestring =~ /^$typedefname/)) {
	$type = "\@field";
    } elsif ($typestring =~ /(MACRO|#define)/o) {
	$type = "\@field";
	if ($apio eq "HeaderDoc::PDefine") {
		# The @defineblock case
		$type = "\@define";
	}
    } elsif ($typestring =~ /(constant)/o) {
	$type = "\@constant";
	print STDERR "VALUE: \"$value\"\n" if ($localDebug);
	if (($value eq "")) {
		# It's just a variable.
		$type = "\@field";
	}
    } else {
	warn "getNameAndFieldTypeFromDeclaration: UNKNOWN TYPE ($typestring) RETURNED BY BLOCKPARSE\n";
	print STDERR "STRING WAS $string\n" if ($localDebug);
    }

    if (!$name || ($name eq "")) {
	warn "COULD NOT GET NAME FROM DECLARATION.  DECLARATION WAS:\n$string\n";
	return ("", "");
    }
    print STDERR "TYPE $type, NAME $name\n" if ($localDebug);

    return ($name, $type);
}

# /*! This subroutine tells whether to process comments nested as children of
#     a given node in the parse tree.  We explicitly avoid things like strings
#     and regular expressions without the need to search for them by
#     disallowing children of any non-word, non-whitespace characters
#     other than parentheses, curly braces, and colons.
#  */
sub commentsNestedIn
{
    my $token = shift;
    my $soc = shift;
    my $eoc = shift;
    my $ilc = shift;
    my $ilc_b = shift;
    my $lbrace = shift;
    my $case_sensitive = shift;

    # if ($token eq $soc || $token eq $eoc || $token eq $ilc || $token eq $ilc_b) { return 1; }
    if ($token =~ /\W/o) {
	if ($token =~ /[{(}):]/o) { return 1; }
	if ($token =~ /^#/o) { return 2; }
	if (casecmp($token, $lbrace, $case_sensitive)) { return 1; }
	if ($token =~ /\s/o) { return 1; }
	return 0;
    }
    # if (casecmp($token, $lbrace, $case_sensitive)) { return 1; }
    return 1;
}

# /*! This is the variant of processEmbeddedTagsRec used for API owners.
#     it does significantly less work because the code in APIOwner.pm
#     handles most of the effort.
#  */
sub APIOprocessEmbeddedTagsRec
{
    my $self = shift;
    my $apiOwner = shift; # my $apiolist = shift;
    my $soc = shift;
    my $eoc = shift;
    my $ilc = shift;
    my $ilc_b = shift;
    my $lbrace = shift;
    my $case_sensitive = shift;
    my $lastTreeNode = shift;
    my $skipchildren = shift;
    my $isroot = shift; # Don't add parse tree to the parent object twice.
    my $enable_javadoc_comments = shift;

    # print STDERR "EJC: $enable_javadoc_comments\n";

    my $localDebug = 0;

    if (1 && $localDebug) {
	my $apio = $self->apiOwner();
	if ($apio) {
		# if ($apio->name() eq "OSObject") {
		print STDERR "DUMPING TREE.\n"; $self->dbprint();
		# }
	}
    }

    my $continue = 1;
    if ($self == $lastTreeNode) {
	$continue = 0;
	print STDERR "CONTINUE -> 0\n" if ($localDebug);
	# return $continue; # Added this to try to fix a bug, but wasn't needed.
    }

    my $token = $self->token();
    my $firstchild = $self->firstchild();
    my $next = $self->next();

    print STDERR "APIOprocessEmbeddedTagsRec: TOKEN IS \"".$self->token()."\"\n" if ($localDebug);
    my $handled = 0;

    print STDERR "SOC: \"$soc\" ILC: \"$ilc\" ILC_B: \"$ilc_b\"\n" if ($localDebug);

    if ((length($soc) && ($token eq $soc)) ||
	(length($ilc) && ($token eq $ilc)) ||
	(length($ilc_b) && ($token eq $ilc_b))) {

	if ((($token eq $soc) || ($token eq $ilc)) && $firstchild) {
		print STDERR "COMMENT CHECK\n" if ($localDebug);
		my $ntoken = $firstchild->token();
		my $nntoken = "";
		if ($firstchild->next()) {
			$nntoken = $firstchild->next()->token();
		}
		if ($ntoken eq "" && $firstchild->next()) {
			$ntoken = $firstchild->next()->token();
			if ($firstchild->next()->next()) {
				$nntoken = $firstchild->next()->next()->token();
			} else {
				$nntoken = "";
			}
		}
		print STDERR "NTOKEN: $ntoken\n" if ($localDebug);
		print STDERR "NNTOKEN: $nntoken\n" if ($localDebug);
		if (($ntoken eq "!" || ($enable_javadoc_comments && $ntoken eq "*" && $nntoken !~ /^\*/)) && !$self->hidden()) {
			print STDERR "NODECHECK: $self\n" if ($localDebug);
			# HDCOMMENT
			my $string = $token.$firstchild->textTree();
			if ($ntoken eq "*") { $string =~ s/^\s*\/\*\*/\/\*\!/s; }
			print STDERR "FOUND HDCOMMENT:\n$string\nEND HDCOMMENT\n" if ($localDebug);
			# $string =~ s/^\/[\*\/]\!//s;
			# $string =~ s/^\s*//s;
			if ($token eq $soc) {
				$string =~ s/\Q$eoc\E\s*$//s;
			}

			# @@@ DAG CHECK ME
			# $string =~ s/^\s*\*\s*//mg;
			my $fieldref = stringToFields($string, $self->fullpath, $self->linenum);
			# print STDERR "APIOLIST AT INSERT IS $apiolist\n" if ($localDebug);
			# foreach my $owner (@{$apiolist}) {
			    # print STDERR "X POSSOWNER of $self: $owner\n" if ($localDebug);
			# }
			# foreach my $owner (@{$apiolist}) {
			    print STDERR "POSSOWNER of $self: $apiOwner\n" if ($localDebug);
			    if ($apiOwner && $apiOwner->isAPIOwner()) {
				print STDERR "ADDING[1] TO $apiOwner.\n" if ($localDebug);
		    		my $ncurly = $apiOwner->processComment($fieldref, 1, $self->nextTokenNoComments($soc, $ilc, $ilc_b, 1, $enable_javadoc_comments), $soc, $ilc, $ilc_b);

				# We have found the correct level.  Anything
				# nested deeper than this is bogus (unless we hit a curly brace).
				print STDERR "skipochildren -> 1 [1]" if ($localDebug);
				$skipchildren = 1;
				$next = $next->skipcurly($lbrace, $ncurly); # nextTokenNoComments($soc, $ilc, $ilc_b, 0, $enable_javadoc_comments);
				if ($localDebug) {
					print STDERR "NEXT IS $next (";
					if ($next) {print STDERR $next->token(); }
					print STDERR ")\n";
				}
			    }
			# }
			$handled = 1;
		}
	} elsif ($firstchild && $firstchild->next() && $firstchild->next()->next()) {
			my $pos = $firstchild->next();
			my $fcntoken = $pos->token();

			while ($fcntoken =~ /\s/ && $pos) {
				$pos = $pos->next;
				$fcntoken = $pos->token();
			}
			if (($fcntoken eq $soc) || ($fcntoken eq $ilc)) {
				my $fcnntoken = $firstchild->next()->next()->token();
				my $fcnnntoken = "";
				if ($firstchild->next()->next()->next()) {
					$fcnnntoken = $firstchild->next()->next()->next()->token();
				}
				if ($fcnntoken eq "!" || ($enable_javadoc_comments && $fcnntoken eq "*" && $fcnnntoken !~ /^\*/)) {
					# HDCOMMENT
					my $string = $fcntoken.$firstchild->textTree();
					if ($fcnntoken eq "*") { $string =~ s/^\s*\/\*\*/\/\*\!/s; }
					print STDERR "FOUND HDCOMMENT:\n$string\nEND HDCOMMENT\n" if ($localDebug);
					# my $quotetoken = quote($fcntoken);
					# $string =~ s/^$quotetoken//s;
					# $string =~ s/^\s*\/[\*\/]\!//s;
					# $string =~ s/^\s*//s;
					if ($fcntoken eq $soc) {
						$string =~ s/\Q$eoc\E\s*$//s;
					}
					# @@@ DAG CHECKME LEADING STARS
					# $string =~ s/^\s*\*\s*//mg;
					my $fieldref = stringToFields($string, $self->fullpath, $self->linenum);
					# foreach my $owner (@{$apiolist}) {
					    print STDERR "POSSOWNER of $self: $apiOwner\n" if ($localDebug);
					    if ($apiOwner && $apiOwner->isAPIOwner()) {
						print STDERR "ADDING[2] TO $apiOwner.\n" if ($localDebug);
				    		my $ncurly = $apiOwner->processComment($fieldref, 1, $self->nextTokenNoComments($soc, $ilc, $ilc_b, 1, $enable_javadoc_comments), $soc, $ilc, $ilc_b);
						print STDERR "skipochildren -> 1 [2]" if ($localDebug);
						$skipchildren = 1;
						# skip the current declaration before
						# processing anything else to avoid
						# bogus warnings from nested
						# HeaderDoc comments.
						$next = $next->skipcurly($lbrace, $ncurly); # nextTokenNoComments($soc, $ilc, $ilc_b, 0, $enable_javadoc_comments);
						if ($localDebug) {
							print STDERR "NEXT IS $next (";
							if ($next) {print STDERR $next->token(); }
							print STDERR ")\n";
						}
					    }
					# }
					$handled = 1;
				}
			}
	}
    }
    if (!$handled && $self->parserState() && !$self->parserState()->{APIODONE} && $HeaderDoc::process_everything && !$isroot) {
	print STDERR "Declaration without markup\n" if ($localDebug);
	# foreach my $owner (@{$apiolist}) {
	    # print STDERR "X POSSOWNER of $self: $apiOwner\n" if ($localDebug);
	# }
	# foreach my $apiOwner (@{$apiolist}) {
	    print STDERR "POSSOWNER of $self: $apiOwner\n" if ($localDebug);
	    # Found an embedded declaration that is not tagged.
	    my @fields = ();
	    if ($apiOwner && $apiOwner->isAPIOwner()) {
		$apiOwner->processComment(\@fields, 1, $self, $soc, $ilc, $ilc_b);
		$handled = 1;
	    } else {
		warn "$apiOwner is not API Owner\n";
	    }
	# }
    }
    if ($handled && $localDebug) {
	print STDERR "ADDED TREE TO $apiOwner (".$apiOwner->name().")\n";
	$self->dbprint();
	print STDERR "DUMPING API OWNER:\n";
	$apiOwner->dbprint();
	print STDERR "END DUMP.\n";
    }
    # print STDERR "PS: ".$self->parserState()."\n";
    # If we get here, we weren't a skipped brace, so we can start nesting again.
    if (length($lbrace) && $token eq $lbrace) {
	print STDERR "skipochildren -> 0 [3]" if ($localDebug);
	$skipchildren = 0;
    }

    if ($firstchild && !$skipchildren) {
	print STDERR "APIOprocessEmbeddedTagsRec: MAYBE GOING TO CHILDREN\n" if ($localDebug);
	my $nestallowed = commentsNestedIn($token, $soc, $eoc, $ilc, $ilc_b, $lbrace, $case_sensitive);

        if ($nestallowed) {
		print STDERR "APIOprocessEmbeddedTagsRec: YUP.  CHILDREN.\n" if ($localDebug);
		my $newcontinue = $firstchild->APIOprocessEmbeddedTagsRec($apiOwner, $soc, $eoc, $ilc, $ilc_b, $lbrace, $case_sensitive, $lastTreeNode, $skipchildren, 0, $enable_javadoc_comments);
		if ($continue) { $continue = $newcontinue; }
		print STDERR "Back from Child\n" if ($localDebug);
		print STDERR "skipochildren -> $skipchildren [RECURSEOUT]" if ($localDebug);
	}
    }
    if ($next && $continue) {
	print STDERR "APIOprocessEmbeddedTagsRec: GOING TO NEXT\n" if ($localDebug);
	$continue = $next->APIOprocessEmbeddedTagsRec($apiOwner, $soc, $eoc, $ilc, $ilc_b, $lbrace, $case_sensitive, $lastTreeNode, $skipchildren, 0, $enable_javadoc_comments);
	print STDERR "Back from Next\n" if ($localDebug);
    }
print STDERR "SN: ".$self->next()." (".($self->next() ? $self->next()->token() : "").")\n" if ($localDebug);
print STDERR "RECURSEOUT (CONTINUE is $continue)\n" if ($localDebug);
    return $continue;
}

# /*! This subroutine processes the parse tree recursively looking for
#     (and subsequently processing) embedded headerdoc markup.  This does
#     the actual work for processEmbeddedTags.
#  */
sub processEmbeddedTagsRec
{
    my $self = shift;
    my $xmlmode = shift;
    my $eoDeclaration = shift;
    my $soc = shift;
    my $eoc = shift;
    my $eocquot = shift;
    my $ilc = shift;
    my $ilc_b = shift;
    my $lbrace = shift;
    my $rbrace = shift;
    my $typedefname = shift;
    my $case_sensitive = shift;
    my $keywordhashref = shift;
    my $lastDeclaration = shift;
    my $curDeclaration = shift;
    my $pendingHDcomment = shift;
    my $apio = shift;
    my $apiolist = shift;
    my $sodec = shift;
    my $lastTreeNode = shift;
    my $enable_javadoc_comments = shift;

    my $localDebug = 0;
    my $oldCurDeclaration = $curDeclaration;
    my $oldsodec = $sodec;
    my $ntoken = $self->nextpeeknc($soc, $ilc, $ilc_b);
    my $skipchildren = 0;
    my $oldPHD = $pendingHDcomment;
    my $do_process = 0;

    my $continue = 1;
    my $inBlockDefine = 0;
    my $dropinvalid = 0;
    my $lastsodec = $sodec;
    my $nextsodec = $sodec;

print STDERR "PETREC\n" if ($localDebug);

    if (!$self) { return ($eoDeclaration, $pendingHDcomment, $continue); }

    my $apioclass = ref($apio) || $apio;
    if ($apioclass =~ /HeaderDoc::PDefine/) {
	# print STDERR "HDPDEF: ISBLOCK: ".$apio->isBlock()." inDefineBlock: ".$apio->inDefineBlock().".\n";
        if ($apio->inDefineBlock() || $apio->isBlock()) {
	    $inBlockDefine = 1;
	    my $x = pop(@{$apiolist});
	    $do_process = $x;
	    if ($x) {
		push(@{$apiolist}, $x);
	    }
	}
    }

    if ($self == $lastTreeNode) {
	print STDERR "CONTINUE -> 0\n" if ($localDebug);
	$continue = 0;
    }

    # print STDERR "lastDec: $lastDeclaration\ncurDec: $curDeclaration\neoDec: $eoDeclaration\n" if ($localDebug);

    # Walk the tree.
    my $token = $self->token();
    $curDeclaration .= $token;

    print STDERR "TOKEN: $token\n" if ($localDebug);

# if ($token !~ /\s/o) { print STDERR "TOKEN: \"$token\" SOC: \"$soc\" ILC: \"$ilc\" ILC_B: \"$ilc_b\".\n"; }

    if ($token eq $soc || $token eq $ilc || $token eq $ilc_b) {
	my $firstchild = $self->firstchild();

	if ($firstchild) {
	print STDERR "FCT: ".$firstchild->token()."\n" if ($localDebug);
	  my $nextchild = $firstchild->next();
	  my $nntoken = "";
	  if ($nextchild && $nextchild->next()) { $nntoken = $nextchild->next()->token(); }

	  if ($nextchild && ($nextchild->token eq "!" || ($enable_javadoc_comments && $nextchild->token eq "*" && $nntoken !~ /^\*/))) {
	      print STDERR "Found embedded HeaderDoc markup\n" if ($localDebug);
	      print STDERR "NCT: ".$nextchild->token()."\n" if ($localDebug);
		# print STDERR "NCT TREE:\n"; $self->printTree(); print STDERR "NCT ENDTREE\n";

		print STDERR "WILL SET SODEC.  SEARCHING IN:\n" if ($localDebug);
		$self->dbprint() if ($localDebug);
	      $sodec = $self->nextTokenNoComments($soc, $ilc, $ilc_b, 0, $enable_javadoc_comments);
		# print STDERR "NCT SODECTREE:\n"; $sodec->printTree(); print STDERR "NCT ENDTREE\n";
		print STDERR "SODEC SET TO $sodec\n" if ($localDebug);
		if ($sodec) {
			$sodec->dbprint() if ($localDebug);
		}

	      my $string = $firstchild->textTree();
	      my $fullpath = $apio->fullpath();
	      my $linenum = $apio->linenum();
	      if ($token eq $soc) {
		$string =~ s/$eocquot\s*$//s;
	      }
	      if ($string =~ /^\s*\!/o) {
		      $string =~ s/^\s*\!//so;

		      print STDERR "EOD $eoDeclaration NT $ntoken STR $string\n" if ($localDebug);;

		      if ((($eoDeclaration && $lastDeclaration =~ /[a-zA-Z]/) || !$ntoken ||
			   $ntoken =~ /[)}]/o || casecmp($ntoken, $rbrace, $case_sensitive)) &&
			  $string !~ /^\s*\@/o) {
			# If we're at the end of a declaration (prior to the
			# following newline) and the declaration starts with
			# a string of text (JavaDoc-style markup), we need to
			# figure out the name of the previous declaration and
			# insert it.

			print STDERR "Using previous declaration because:\n" if ($localDebug);
			print STDERR "EODEC: $eoDeclaration NTOKEN: \"$ntoken\"\n" if ($localDebug);
			print STDERR "RBRACE: \"$rbrace\" STRING: \"$string\"\n" if ($localDebug);

			if (!$eoDeclaration) {
				print STDERR "LASTDITCH PROCESSING\n" if ($localDebug);
			} else {
				print STDERR "EOD PROCESSING\n" if ($localDebug);
			}

			# Roll back to the previous start of declaration.
			# This comment is at the end of a line or whatever.
			$nextsodec = $sodec;
			$sodec = $lastsodec;

			$string =~ s/^\s*//so;
	
			print STDERR "COMMENTSTRING WAS: $string\n" if ($localDebug);
			print STDERR "PRE1\n" if ($localDebug);

			print STDERR "LAST DECLARATION: $lastDeclaration\n" if ($localDebug);

			print STDERR "calling getNameAndFieldTypeFromDeclaration\n" if ($localDebug);
			my ($name, $type) = $self->getNameAndFieldTypeFromDeclaration($lastDeclaration, $apio, $typedefname, $case_sensitive, $keywordhashref);
	
			$string = "$type $name\n$string";
			print STDERR "COMMENTSTRING NOW: $string\n" if ($localDebug);
		      } elsif (!$eoDeclaration && (!$ntoken ||
                           $ntoken =~ /[)]/o || casecmp($ntoken, $rbrace, $case_sensitive)) &&
                           $string =~ /^\s*\@/o) {
                           # We have found an embedded headerdoc comment embedded that is
			   # right before a close parenthesis, but which starts with an @ sign.
				my $nlstring = $string;
				$nlstring =~ s/[\n\r]/ /sg;
				if (!emptyHDok($nlstring)) {
					warn "$fullpath:$linenum: warning: Found invalid headerdoc markup: $nlstring\n";
					$dropinvalid = 1;
				} else {
					warn "$fullpath:$linenum: warning Found headerdoc markup where none expected: $nlstring\n";
				}
		      }
		      $string =~ s/^\s*//so;
		      if ($string =~ /^\s*\@/o) {
			print STDERR "COMMENTSTRING: $string\n" if ($localDebug);
	
			my $fieldref = stringToFields($string, $fullpath, $linenum);
		# print STDERR "APIO: $apio\n";
			foreach my $owner (@{$apiolist}) {
			    my $copy = $fieldref;
			print STDERR "OWNER[1]: $owner\n" if ($localDebug);
			    if ($owner) {
				if (!$inBlockDefine || $do_process == $owner) {
					my @copyarray = @{$copy};
					# print STDERR "COPY[1]: ".$copyarray[1]."\n";
					if ($inBlockDefine && !length($copyarray[0])) { $copyarray[1] =~ s/^field .*?\n/discussion /s; $copy = \@copyarray; }
					# print STDERR "COPY[1]: ".$copyarray[1]."\n";
			    		$owner->processComment($copy, 1, $sodec, $soc, $ilc, $ilc_b);
				}
			    }
			}
# print STDERR "APIO: $apio\n";
			$apio->{APIREFSETUPDONE} = 0;
		      } else {
			if (!$dropinvalid) {
				$pendingHDcomment = $string;
			}
		      }
		      if (!$HeaderDoc::dumb_as_dirt) {
			# Drop this comment from the output.
			if ($xmlmode) {
				# We were doing this for HTML when we needed to
				# be able to reparse the tree after copying
				# it to a cloned data type.  This is no longer
				# needed, and the old method (above) is slightly
				# faster.
				$self->hidden(1); $skipchildren = 1;
			} else {
				$self->{TOKEN} = "";
				$self->{FIRSTCHILD} = undef;
				print STDERR "HIDING $self\n" if ($localDebug);
			}
			print STDERR "DROP\n" if ($localDebug);
			$curDeclaration = $oldCurDeclaration;
		      }
	      }
	   }
	}
    } elsif ($token =~ /[;,}]/o) {
	print STDERR "SETTING LASTDEC TO $curDeclaration\n" if ($localDebug);
	$lastDeclaration = "$curDeclaration\n";
	if ($pendingHDcomment) {
                # If we're at the end of a declaration (prior to the
                # following newline) and the declaration starts with
                # a string of text (JavaDoc-style markup), we need to
                # figure out the name of the previous declaration and
                # insert it.

			print STDERR "PRE2\n" if ($localDebug);
		print STDERR "calling getNameAndFieldTypeFromDeclaration\n" if ($localDebug);
                my ($name, $type) = $self->getNameAndFieldTypeFromDeclaration($lastDeclaration, $apio, $typedefname, $case_sensitive, $keywordhashref);
                my $string = "$type $name\n$pendingHDcomment";
		my $fullpath = $apio->fullpath();
		my $linenum = $apio->linenum();

                my $fieldref = stringToFields($string, $fullpath, $linenum);
		print STDERR "COMMENTSTRING: $string\n" if ($localDebug);
		foreach my $owner (@{$apiolist}) {
			my $copy = $fieldref;
			print STDERR "OWNER[2]: $owner\n" if ($localDebug);
			if ($owner) {
			    if (!$inBlockDefine || $do_process == $owner) {
				my @copyarray = @{$copy};
				# print STDERR "COPY[1]: ".$copyarray[1]."\n";
				if ($inBlockDefine && !length($copyarray[0])) { $copyarray[1] =~ s/^field .*?\n/discussion /s; $copy = \@copyarray; }
				# print STDERR "COPY[1]: ".$copyarray[1]."\n";
				$owner->processComment($copy, 1, $sodec, $soc, $ilc, $ilc_b);
			    }
			}
		}
# print STDERR "APIO: $apio\n";
		$apio->{APIREFSETUPDONE} = 0;

		$pendingHDcomment = "";
	} else {
		$eoDeclaration = 1;
	}
	$curDeclaration = "";
    } elsif ($token !~ /\s/o) {
	$eoDeclaration = 0;
    }

    $sodec = $nextsodec;

    my $firstchild = $self->firstchild();
    my $next = $self->next();

    if ($firstchild && !$skipchildren) {
	my $nestallowed = commentsNestedIn($token, $soc, $eoc, $ilc, $ilc_b, $lbrace, $case_sensitive);
	if ($nestallowed == 1) {
		my $newcontinue;
		($eoDeclaration, $pendingHDcomment, $newcontinue) = $firstchild->processEmbeddedTagsRec($xmlmode, $eoDeclaration, $soc, $eoc, $eocquot, $ilc, $ilc_b, $lbrace, $rbrace, $typedefname, $case_sensitive, $keywordhashref, "", "", "", $apio, $apiolist, $sodec, $lastTreeNode, $enable_javadoc_comments);
		if ($continue) { $continue = $newcontinue; }
	} else {
		my $newcontinue;
		($eoDeclaration, $pendingHDcomment, $newcontinue) = $firstchild->processEmbeddedTagsRec($xmlmode, $eoDeclaration, $soc, $eoc, $eocquot, $ilc, $ilc_b, $lbrace, $rbrace, $typedefname, $case_sensitive, $keywordhashref, "", "$curDeclaration", "", $apio, $apiolist, $sodec, $lastTreeNode, $enable_javadoc_comments);
		if ($continue) { $continue = $newcontinue; }
	}
	$curDeclaration .= textTree($firstchild);
    } elsif ($firstchild && !$skipchildren) {
	$curDeclaration .= textTree($firstchild);
    }

    if ($ntoken) {
	print STDERR "NTOKEN: $ntoken\n" if ($localDebug);
    } else {
	print STDERR "NTOKEN: (null)\n" if ($localDebug);
    }

    if (!$ntoken || $ntoken =~ /[)]/o || casecmp($ntoken, $rbrace, $case_sensitive)) {
	# Last-ditch chance to process pending comment.
	# This takes care of the edge case where some languages
	# do not require the last item in a struct/record to be
	# terminated by a semicolon or comma.

	if ($ntoken =~ /[)}]/o || casecmp($ntoken, $rbrace, $case_sensitive)) {
		if ($oldCurDeclaration =~ /\S/) {
			print STDERR "CLOSEBRACE LASTDITCH: SETTING LASTDEC TO $curDeclaration\n" if ($localDebug);
			$lastDeclaration = $oldCurDeclaration;
		}
	} else {
		if ($oldCurDeclaration =~ /\S/) {
			print STDERR "NONCLOSEBRACE LASTDITCH: SETTING LASTDEC TO $curDeclaration\n" if ($localDebug);
			$lastDeclaration = $curDeclaration;
		}
	}
	if ($pendingHDcomment) {
		print STDERR "LASTDITCH\n" if ($localDebug);

                # If we're at the end of a declaration (prior to the
                # following newline) and the declaration starts with
                # a string of text (JavaDoc-style markup), we need to
                # figure out the name of the previous declaration and
                # insert it.

			print STDERR "PRE3\n" if ($localDebug);
		print STDERR "calling getNameAndFieldTypeFromDeclaration\n" if ($localDebug);
                my ($name, $type) = $self->getNameAndFieldTypeFromDeclaration($lastDeclaration, $apio, $typedefname, $case_sensitive, $keywordhashref);
                my $string = "$type $name\n$pendingHDcomment";
		my $fullpath = $apio->fullpath();
		my $linenum = $apio->linenum();

                my $fieldref = stringToFields($string, $fullpath, $linenum);
		print STDERR "COMMENTSTRING: $string\n" if ($localDebug);
		foreach my $owner (@{$apiolist}) {
			my $copy = $fieldref;
			print STDERR "OWNER[3]: $owner\n" if ($localDebug);
			if ($owner) {
			    if (!$inBlockDefine || $do_process == $owner) {
				my @copyarray = @{$copy};
				# print STDERR "COPY[1]: ".$copyarray[1]."\n";
				if ($inBlockDefine && !length($copyarray[0])) { $copyarray[1] =~ s/^field .*?\n/discussion /s; $copy = \@copyarray; }
				# print STDERR "COPY[1]: ".$copyarray[1]."\n";
				$owner->processComment($copy, 1, $sodec, $soc, $ilc, $ilc_b);
			    }
			}
		}
# print STDERR "APIO: $apio\n";
		$apio->{APIREFSETUPDONE} = 0;

		$pendingHDcomment = "";
	}
    }
			# $sodec = $oldsodec;
    if ($next && $continue) {
	($eoDeclaration, $pendingHDcomment, $continue) = $next->processEmbeddedTagsRec($xmlmode, $eoDeclaration, $soc, $eoc, $eocquot, $ilc, $ilc_b, $lbrace, $rbrace, $typedefname, $case_sensitive, $keywordhashref, $lastDeclaration, $curDeclaration, $pendingHDcomment, $apio, $apiolist, $sodec, $lastTreeNode, $enable_javadoc_comments);
    }

    return ($eoDeclaration, $pendingHDcomment, $continue);
}

# THIS CODE USED TO PROCESS COMMENTS WHENEVER IT IS TIME.
	      # my $fieldref = stringToFields($string, $fullpath, $linenum);
	      # $apio->processComment($fieldref, 1, $self, $soc, $ilc, $ilc_b);
		# $apio->{APIREFSETUPDONE} = 0;

sub next {
    my $self = shift;

    if (@_) {
	my $node = shift;
        $self->{NEXT} = $node;
    }
    return $self->{NEXT};
}

sub firstchild {
    my $self = shift;

    if (@_) {
	my $node = shift;
        $self->{FIRSTCHILD} = $node;
    }
    return $self->{FIRSTCHILD};
}


# sub lastchild {
    # my $self = shift;
# 
    # if (@_) {
	# my $node = shift;
        # $self->{LASTCHILD} = $node;
    # }
    # return $self->{LASTCHILD};
# }


sub parent {
    my $self = shift;

    if (@_) {
	my $node = shift;
        $self->{PARENT} = $node;
	# weaken($self->{PARENT});
    }
    return $self->{PARENT};
}


sub printTree {
    my $self = shift;

    print STDERR "BEGINPRINTTREE\n";
    print STDERR $self->textTree();
    print STDERR "ENDPRINTTREE\n";
}

sub textTree {
    my $self = shift;
    my $parserState = $self->parserState();
    my $lastnode = undef;

    if ($parserState) {
	$lastnode = $parserState->{lastTreeNode};
    }
    # print STDERR "TEXTTREE: LASTNODE: $lastnode\n";
    # if ($lastnode) { print STDERR "LASTNODE TEXT: \"".$lastnode->token()."\"\n"; }

    my ($string, $continue) = $self->textTreeSub(0, "", "", $lastnode);
    return $string;
}

sub textTreeNC {
    my $self = shift;
    my $lang = shift;
    my $sublang = shift;
    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macronamesref,
        $classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
	$requiredregexp, $propname, $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename) = parseTokens($lang, $sublang);

    my ($string, $continue) = $self->textTreeSub(1, $soc, $ilc, $ilc_b);
    return $string;
}

sub textTreeSub
{
    my $self = shift;
    my $nc = shift;
    my $soc = shift;
    my $ilc = shift;
    my $ilc_b = shift;
    my $lastnode = shift;

    my $localDebug = 0;
    my $continue = 1;

    # print STDERR "TTSUB: LN: $lastnode SELF: $self\n";

    if ($lastnode == $self) {
	# print STDERR "TTSUB: CONTINUE -> 0\n";
	$continue = 0;
    }

    my $string = "";
    my $skip = 0;
    my $token = $self->token();
    if ($nc) {
	if ($localDebug) {
		print STDERR "NC\n";
		print STDERR "SOC: $soc\n";
		print STDERR "ILC: $ilc\n";
		print STDERR "ILC_B: $ilc_b\n";
		print STDERR "TOK: $token\n";
	}
	if (($token eq "$soc") || ($token eq "$ilc") || ($token eq "$ilc_b")) {
		$skip = 1;
	}
    }

    if (!$skip) {
	$string .= $token;
	# if (!$continue) {
		# return ($string, $continue);
	# }
	if ($self->{FIRSTCHILD}) {
		my $node = $self->{FIRSTCHILD};
		bless($node, "HeaderDoc::ParseTree");
		my ($newstring, $newcontinue) = $node->textTreeSub($nc, $soc, $ilc, $ilc_b, $lastnode);
		if ($continue) { $continue = $newcontinue; }
		$string .= $newstring;
	}
    }
    if (!$continue) {
	return ($string, $continue);
    }
    if ($self->{NEXT}) {
	my $node = $self->{NEXT};
	bless($node, "HeaderDoc::ParseTree");
	my ($newstring, $newcontinue) = $node->textTreeSub($nc, $soc, $ilc, $ilc_b, $lastnode);
	$continue = $newcontinue;
	$string .= $newstring;
    }

    return ($string, $continue);
}


sub xmlTree {
    my $self = shift;
    my $keep_whitespace = shift;
    my $drop_pdefine_contents = shift;
    # my $apiOwner = shift;
    # my $apio = $self->apiOwner();

    if ($self->{XMLTREE}) { return $self->{XMLTREE}; }

    # $self->printTree();
    my $apiOwner = undef;
    my $lang = undef;
    my $sublang = undef;
    my $occmethod = 0;
    my $localDebug = 0;

    my $debugName = ""; # "TypedefdStructWithCallbacksAndStructs";

    if ($self->apiOwner()) {
	$apiOwner = $self->apiOwner();
	bless($apiOwner, "HeaderDoc::HeaderElement");
	bless($apiOwner, $apiOwner->class());
	$lang = $apiOwner->lang();
	$sublang = $apiOwner->sublang();

	if (($debugName ne "") && ($apiOwner->name() eq $debugName)) {
		$colorDebug = 1;
	} else {
		$colorDebug = 0;
		print STDERR $apiOwner->name()."\n" if ($localDebug);
	}

	if ($apiOwner->class() eq "HeaderDoc::Method") {
		$occmethod = 1;
	} else {
		$occmethod = 0;
	}

	# print STDERR "APIOWNER was type $apiOwner\n";
    } else {
	$apiOwner = HeaderDoc::HeaderElement->new();
	$lang = $HeaderDoc::lang;
	$sublang = $HeaderDoc::sublang;
	$apiOwner->lang($lang);
	$apiOwner->sublang($sublang);
	$occmethod = 0; # guess
    }
    # colorizer goes here


    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macroListRef,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
        $requiredregexp, $propname, $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename) = parseTokens($apiOwner->lang(), $apiOwner->sublang());

    $self->processEmbeddedTags(1, $apiOwner);
	# , $sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
        # $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	# $enumname,
        # $typedefname, $varname, $constname, $structisbrace, $macroListRef);

    my $lastnode = undef;
    my $parserState = $self->parserState();
    if ($parserState) {
	$lastnode = $parserState->{lastTreeNode};
	if ($parserState->{lastDisplayNode}) {
		$lastnode = $parserState->{lastDisplayNode};
	}
    }

    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macroListRef,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
        $requiredregexp, $propname, $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename) = parseTokens($lang, $sublang);

    my ($retvalref, $junka, $junkb, $junkc, $junkd, $junke, $lastTokenType, $spaceSinceLastToken) = $self->colorTreeSub($keep_whitespace, $apiOwner, "", 0, 0, 0, $occmethod, "", $sotemplate, $soc, $eoc, $ilc, $ilc_b, $lbrace, $rbrace, $sofunction, $soprocedure, $varname, $constname, $unionname, $structname, $enumname, $typedefname, $structisbrace, $macroListRef, "", $lang, $sublang, 1, 0, 0, 0, 0, 0, "", "", $lastnode, "", 0, 0, 0, 0, $drop_pdefine_contents);
    my $retval = ${$retvalref};

    # my $retval = "";
    # $retval = $self->textTree();
    # $self->printTree();

    $self->{XMLTREE} = $retval;

    return $retval;
}

sub htmlTree {
    my $self = shift;
    my $keep_whitespace = shift;
    my $drop_pdefine_contents = shift;
    # my $apiOwner = shift;

    # print STDERR "TREE\n";
    # $self->printTree();
    # $self->dbprint();
    # print STDERR "ENDTREE\n";
    my $apiOwner = undef;
    my $lang = undef;
    my $sublang = undef;
    my $occmethod = 0;
    my $localDebug = 0;

    my $debugName = ""; # "TypedefdStructWithCallbacksAndStructs";

    # $self->dbprint();

    if ($self->{HTMLTREE}) {
	 print STDERR "SHORTCUT\n" if ($localDebug);
	 return $self->{HTMLTREE};
    }

    if ($self->apiOwner()) {
	$apiOwner = $self->apiOwner();
	bless($apiOwner, "HeaderDoc::HeaderElement");
	bless($apiOwner, $apiOwner->class());
	$lang = $apiOwner->lang();
	$sublang = $apiOwner->sublang();

	if (($debugName ne "") && ($apiOwner->name() eq $debugName)) {
		$colorDebug = 1;
	} else {
		$colorDebug = 0;
		print STDERR $apiOwner->name()."\n" if ($localDebug);
	}

	if ($apiOwner->class() eq "HeaderDoc::Method") {
		$occmethod = 1;
	} else {
		$occmethod = 0;
	}

	print STDERR "APIOWNER was type $apiOwner\n" if ($localDebug);
    } else {
	$apiOwner = HeaderDoc::HeaderElement->new();
	warn("Parse tree $self has no APIOWNER!\n");
	$apiOwner->apiOwner($HeaderDoc::headerObject);
	$lang = $HeaderDoc::lang;
	$sublang = $HeaderDoc::sublang;
	$apiOwner->lang($lang);
	$apiOwner->sublang($sublang);
	$occmethod = 0; # guess
    }
    # colorizer goes here

    my $lastnode = undef;
    my $parserState = $self->parserState();
    if ($parserState) {
	$lastnode = $parserState->{lastTreeNode};
	if ($parserState->{lastDisplayNode}) {
		$lastnode = $parserState->{lastDisplayNode};
	}
    }

    # if ($lang eq "shell") {
	# $keep_whitespace = 1;
    # }

    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macroListRef,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
        $requiredregexp, $propname, $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename) = parseTokens($lang, $sublang);

    $self->processEmbeddedTags(0, $apiOwner); 
	# , $sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
        # $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	# $enumname,
        # $typedefname, $varname, $constname, $structisbrace, $macroListRef);

    my ($retvalref, $junka, $junkb, $junkc, $junkd, $junke, $lastTokenType, $spaceSinceLastToken) = $self->colorTreeSub($keep_whitespace, $apiOwner, "", 0, 0, 0, $occmethod, "", $sotemplate, $soc, $eoc, $ilc, $ilc_b, $lbrace, $rbrace, $sofunction, $soprocedure, $varname, $constname, $unionname, $structname, $enumname, $typedefname, $structisbrace, $macroListRef, "", $lang, $sublang, 0, 0, 0, 0, 0, 0, "", "", $lastnode, "", 0, 0, 0, 0, $drop_pdefine_contents);
    my $retval = ${$retvalref};

    # my $retval = "";
    # $retval = $self->textTree();
    # $self->printTree();

    if ($HeaderDoc::align_columns) {
	my @retarr = split(/(\n)/s, $retval);
	my $newret = "";
	foreach my $line (@retarr) {
		my $first = "";
		# print STDERR "LINE: $line\n";
		if ($line =~ s/^<tr><td nowrap=\"nowrap\"><nowrap>//s) {
			$first = "<tr><td nowrap=\"nowrap\"><nowrap>";
			# print STDERR "FIRST (line = \"$line\")\n";
		}
		if ($line =~ s/^( +)//) {
			my $spaces = $1;
			my $count = ($spaces =~ tr/^ //);
			while ($count--) { $line = "&nbsp;$line"; }
			$newret .= "$first$line";
		} else {
			$newret .= "$first$line";
		}
	}
	$retval = $newret;
	$retval = "<table><tr><td nowrap=\"nowrap\"><nowrap>$retval</nowrap></td></tr></table>";
    }

    $self->{HTMLTREE} = $retval;

    return $retval;
}

sub childpeeknc
{
    my $self = shift;
    my $soc = shift;
    my $ilc = shift;
    my $ilc_b = shift;
    my $cache = $self->{CPNC};
    if ($cache) { return $cache; }

    my $node = $self->{FIRSTCHILD};

    if (!$node) { return ""; }

    bless($node, "HeaderDoc::ParseTree");

    if (!$node->token()) { return $node->childpeeknc($soc, $ilc, $ilc_b) || return $node->nextpeeknc($soc, $ilc, $ilc_b); }
    if ($node->token() =~ /\s/o) { return $node->childpeeknc($soc, $ilc, $ilc_b) || return $node->nextpeeknc($soc, $ilc, $ilc_b); }
    if ($node->token() eq $soc) { return $node->childpeeknc($soc, $ilc, $ilc_b) || return $node->nextpeeknc($soc, $ilc, $ilc_b); }
    if ($node->token() eq $ilc || $node->token() eq $ilc_b) { return $node->childpeeknc($soc, $ilc, $ilc_b) || return $node->nextpeeknc($soc, $ilc, $ilc_b); }

    $cache = $node->token();
    $self->{CPNC} = $cache;

    return $cache;
}

sub nextpeek
{
    my $self = shift;
    my $soc = shift;
    my $ilc = shift;
    my $ilc_b = shift;

    # This cache appears to be slowing things down.
    # if ($self->{NPCACHE}) { return $self->{NPCACHE}; }

    my $node = undef;
    if ($self->firstchild()) {
	$node = $self->firstchild();
	$node = $node->next;
    } else {
	$node = $self->next();
    }

    if (!$node) {
	# $self->{NPCACHE} = "";
	return "";
    }

    my $token = $node->token();
    if ($token =~ /\s/o && $token !~ /[\r\n]/o) {
	my $ret = $node->nextpeek($soc, $ilc, $ilc_b);
	# $self->{NPCACHE} = $ret;
	return $ret;
    }
    if ($node->hidden()) {
	my $ret = $node->nextpeek($soc, $ilc, $ilc_b);
	# $self->{NPCACHE} = $ret;
	return $ret;
    }

    # $self->{NPCACHE} = $node->token();
    return $node->token();

}

sub nextpeeknc
{
    my $self = shift;
    my $soc = shift;
    my $ilc = shift;
    my $ilc_b = shift;

    my $node = $self->nextTokenNoComments($soc, $ilc, $ilc_b, 0, 0);
    if (!$node) { return ""; }

    return $node->token();

}

sub nextnextpeeknc
{
    my $self = shift;
    my $soc = shift;
    my $ilc = shift;
    my $ilc_b = shift;

    my $node = $self->nextTokenNoComments($soc, $ilc, $ilc_b, 0, 0);
    if (!$node) { return ""; }

    my $nodeafter = $node->nextTokenNoComments($soc, $ilc, $ilc_b, 0, 0);
    if (!$nodeafter) { return ""; }

    return $nodeafter->token();

}

sub nextTokenNoComments($$$$$)
{
    my $self = shift;
    my $soc = shift;
    my $ilc = shift;
    my $ilc_b = shift;
    my $failOnHDComments = shift;
    my $enable_javadoc_comments = shift;

    my $localDebug = 0;

    my $cache = $self->{NTNC};
    if ($cache) { return $cache; }

    my $node = $self->{NEXT};

    if (!$node) { return undef }

    bless($node, "HeaderDoc::ParseTree");
# print STDERR "SOC: $soc ILC: $ilc ILC_B: $ilc_b\n" if ($colorDebug);

    # print STDERR "MAYBE ".$node->token()."\n";

    if ($failOnHDComments) {
	# print STDERR "FOHDC\n";
	# print STDERR "FC: ".$node->firstchild()."\n";
	if ($node->firstchild() && $node->firstchild()->next()) {
	    # print STDERR "POINT 1\n";
	    # first child always empty.
	    my $testnode = $node->firstchild()->next();
	    if (($node->token() eq $ilc) || ($node->token() eq $ilc_b)) {
	    # print STDERR "ILC\n";
		if ($node->token() eq $ilc) {
			my $ntoken = "";
			if ($testnode->next()) { $ntoken = $testnode->next()->token(); }
			if ($testnode->token() eq "!" || ($enable_javadoc_comments && $testnode->token eq "*" && $ntoken !~ /^\*/)) {
				print STDERR "Unexpected HD Comment\n" if ($localDebug);
				return undef;
			}
		} else {
			if ($testnode->token() eq $ilc) {
				my $nntoken = "";
				if ($testnode->next() && $testnode->next()->next()) { $nntoken = $testnode->next()->next()->token(); }
				if ($testnode->next() && (($testnode->next()->token() eq "!") || ($enable_javadoc_comments && $testnode->next()->token eq "*" && $nntoken !~ /^\*/))) {
					print STDERR "Unexpected HD Comment\n" if ($localDebug);
					return undef;
				}
			}
		}
	    } elsif ($node->token() eq $soc) {
	    # print STDERR "SOC\n";
		my $ntoken = "";
		if ($testnode->next()) { $ntoken = $testnode->next()->token(); }
		if (($testnode->token() eq "!") || ($enable_javadoc_comments && $testnode->token eq "*" && $ntoken !~ /^\*/)) {
			print STDERR "Unexpected HD Comment\n" if ($localDebug);
			return undef;
		}
	    # } else {
		# print STDERR "TOKEN: ".$node->token()."\n";
	    }
	}
    }

    if (!$node->token()) { return $self->{NTNC} = $node->nextTokenNoComments($soc, $ilc, $ilc_b, $failOnHDComments, $enable_javadoc_comments); }
    if ($node->token() =~ /\s/o) { return $self->{NTNC} = $node->nextTokenNoComments($soc, $ilc, $ilc_b, $failOnHDComments, $enable_javadoc_comments); }
    if ($node->token() eq $soc) { return $self->{NTNC} = $node->nextTokenNoComments($soc, $ilc, $ilc_b, $failOnHDComments, $enable_javadoc_comments); }
    if ($node->token() eq $ilc || $node->token() eq $ilc_b) { return $self->{NTNC} = $node->nextTokenNoComments($soc, $ilc, $ilc_b, $failOnHDComments, $enable_javadoc_comments); }

    $self->{NTNC} = $node;
    # weaken($self->{NTNC});
    return $node;
}

sub isMacro
{
    my $self = shift;
    my $token = shift;
    my $lang = shift;
    my $sublang = shift;

    if ($lang ne "C") { return 0; }

    if ($token =~ /^\#\w+/o) { return 1; }

    return 0;
}

sub colorTreeSub
{
    my $self = shift;
    my $keep_whitespace = shift;
    my $apio = shift;
    my $type = shift;
    my $depth = shift;
    my $inComment = shift;
    my $inQuote = shift;
    my $inObjCMethod = shift;
    my $lastBrace = shift;
    my $sotemplate = shift;
    my $soc = shift;
    my $eoc = shift;
    my $ilc = shift;
    my $ilc_b = shift;
    my $lbrace = shift;
    my $rbrace = shift;
    my $sofunction = shift;
    my $soprocedure = shift;
    my $varname = shift;
    my $constname = shift;
    my $unionname = shift;
    my $structname = shift;
    my $enumname = shift;
    my $typedefname = shift;
    my $structisbrace = shift;
    my $macroListRef = shift;
    my $prespace = shift;
    my $lang = shift;
    my $sublang = shift;
    my $xmlmode = shift;
    my $newlen = shift;
    my $breakable = shift;
    my $inMacro = shift;
    my $inEnum = shift;
    my $seenEquals = shift;
    my $lastKeyword = shift;
    my $lastnstoken = shift;
    my $lastTreeNode = shift;
    my $lastTokenType = shift;
    my $spaceSinceLastToken = shift;
    my $inAttribute = shift;
    my $inRaises = shift;
    my $inTypeOf = shift;
    my $drop_pdefine_contents = shift;
    my $continue = 1;

    if ($self == $lastTreeNode) { $continue = 0; }

    my %macroList = %{$macroListRef};
    my $oldLastBrace = $lastBrace;
    my $oldDepth = $depth;
    my $oldInMacro = $inMacro;
    my $oldInQuote = $inQuote;
    my $oldLastKeyword = $lastKeyword;
    my $oldInComment = $inComment;
    my $oldInAttribute = $inAttribute;
    my $oldInRaises = $inRaises;
    my $oldInTypeOf = $inTypeOf;
    my $dropFP = 0;

    # This cache slows things down now that it works....
    # if ($self->{CTSUB}) { return (\$self->{CTSTRING}, $self->{CTSUB}); }
    my $localDebug = 0;
    my $psDebug = 0;
    my $treeDebug = 0;
    my $dropDebug = 0;
    my $tokenDebug = 0;
    my $codePathDebug = 0;

    my $keep_all_newlines = 0;

    print STDERR "DPC: $drop_pdefine_contents\n" if ($localDebug);

    if ($lang eq "shell" || $sublang eq "javascript") {
	$keep_all_newlines = 1;
    }

    if ($xmlmode && $localDebug) {
	print STDERR "XMLMODE.\n";
    }

    # foreach my $listitem (keys %macroList) { print STDERR "ML: $listitem\n"; }

    print STDERR "IM: $inMacro\n" if ($localDebug);
    print STDERR "IAtt: $inAttribute\n" if ($localDebug);
    print STDERR "IRai: $inRaises\n" if ($localDebug);

    my $mustbreak = 0;
    my $nextprespace = "";
    my $string = "";
    my $tailstring = "";
    my $token = $self->{TOKEN};
    my $escapetoken = "";
    my ($case_sensitive, $keywordhashref) = $apio->keywords();
    my $tokennl = 0;
    if ($token =~ /^[\r\n]/o) { $tokennl = 1; }
    if (($token eq "\t" || $token =~ /^ +$/) && (!$keep_whitespace)) { $token = " "; }
    if ($keep_whitespace) {
	$prespace = "";
	$nextprespace = "";
    }
    my $tokenIsKeyword = isKeyword($token, $keywordhashref, $case_sensitive);

    # my $ctoken = $self->childpeek($soc, $ilc, $ilc_b);
    # print STDERR "TK $token\n" if ($colorDebug);
    my $ctoken = $self->childpeeknc($soc, $ilc, $ilc_b);
    my $ntoken = $self->nextpeek($soc, $ilc, $ilc_b);
    my $ntokennc = $self->nextpeeknc($soc, $ilc, $ilc_b);
    my $nntokennc = $self->nextnextpeeknc($soc, $ilc, $ilc_b);
    my $tokenType = undef;
    my $drop = 0;
    my $firstCommentToken = 0;
    my $leavingComment = 0;
    my $hidden = ($self->hidden() && !$xmlmode);

    my $isTypeStar = 0;

    my $begintr = "";
    my $endtr = "";
    my $newtd = "";
    if (!$xmlmode && $HeaderDoc::align_columns) {
	$begintr = "<tr><td nowrap=\"nowrap\"><nowrap>";
	$endtr = "</nowrap></td></tr>";
	$newtd = "</nowrap></td><td nowrap=\"nowrap\"><nowrap>";
    }

    if ($treeDebug || $localDebug || $codePathDebug) {
    	my $tokenname = $token;
    	if ($token eq "\n") { $tokenname = "[NEWLINE]"; }
    	elsif ($token eq "\r") { $tokenname = "[CARRIAGE RETURN]"; }

    	my $ntokenname = $ntoken;
    	if ($ntoken eq "\n") { $ntokenname = "[NEWLINE]"; }
    	elsif ($ntoken eq "\r") { $ntokenname = "[CARRIAGE RETURN]"; }

    	my $lastnstokenname = $lastnstoken;
    	if ($lastnstoken eq "\n") { $lastnstokenname = "[NEWLINE]"; }
    	elsif ($lastnstoken eq "\r") { $lastnstokenname = "[CARRIAGE RETURN]"; }

	print STDERR "TOKEN: $tokenname NTOKEN: $ntokenname LASTNSTOKEN: $lastnstokenname IC: $inComment\n" if ($treeDebug || $localDebug || $codePathDebug);
    }
    print STDERR "OCC: $inObjCMethod\n" if ($colorDebug || $localDebug);
    print STDERR "HIDDEN: $hidden\n" if ($localDebug);

    # last one in each chain prior to a "," or at end of chain is "var"
    # or "parm" (functions)
    print STDERR "TK $token NT $ntoken NTNC $ntokennc NNTNC $nntokennc LB: $lastBrace PS: ".length($prespace)."\n" if ($colorDebug);

    my $nospaceafter = 0;
    my $nextbreakable = 0;
    if ($breakable == 2) {
	$breakable = 0;
	$nextbreakable = 1;
    } elsif ($breakable == 3) {
	$mustbreak = 1;
	$breakable = 1;
	$nextbreakable = 0;
    }

    print STDERR "POST_CHECK MUSTBREAK: $mustbreak BREAKABLE: $breakable NEXTBREAKABLE: $nextbreakable\n" if ($localDebug);

    if ($lang eq "C" && $token eq $enumname) {
	my $curname = $apio->name();
	print STDERR "NAME: $curname\n" if ($localDebug);
	print STDERR "NOW ENUM\n" if ($localDebug);
	$inEnum = 1;
    }

    if ($inObjCMethod && $token =~ /^[+-]/o && ($lastBrace eq "")) {
	$lastBrace = $token;
    }

    my $MIG = 0;
    if ($lang eq "C" && $sublang eq "MIG") { $MIG = 1; }

    my $splitchar = "";
    if ($type =~ /^(typedef|struct|record|union)/o) {
		$splitchar = ";";
    } elsif ($type =~ /^(enum|funcptr)/o) {
		$splitchar = ",";
    } elsif ($lastBrace eq "(") {
		$splitchar = ",";
		if ($MIG) { $splitchar = ";"; }
    } elsif ($lastBrace eq $lbrace) {
		if ($inEnum) {
			$splitchar = ",";
		} else {
			$splitchar = ";";
		}
    } elsif (($lastBrace eq $structname) && $structisbrace) {
		$splitchar = ";";
    }
print STDERR "SPLITCHAR IS $splitchar\n" if ($localDebug);
    if ($splitchar && ($token eq $splitchar)) { # && ($ntoken !~ /^[\r\n]/o)) {
	print STDERR "WILL SPLIT AFTER \"$token\" AND BEFORE \"$ntoken\".\n" if ($localDebug);
	$nextbreakable = 3;
    }

print STDERR "SOC: \"$soc\"\nEOC: \"$eoc\"\nILC: \"$ilc\"\nILC_B: \"$ilc_b\"\nLBRACE: \"$lbrace\"\nRBRACE: \"$rbrace\"\nSOPROC: \"$soprocedure\"\nSOFUNC: \"$sofunction\"\nVAR: \"$varname\"\nSTRUCTNAME: \"$structname\"\nTYPEDEFNAME: \"$typedefname\"\n" if ($tokenDebug);

print STDERR "inQuote: $inQuote\noldInQuote: $oldInQuote\ninComment: $inComment\ninMacro: $inMacro\ninEnum: $inEnum\n" if ($localDebug);
print STDERR "oldInMacro: $oldInMacro\noldInComment: $oldInComment\n" if ($localDebug);

    # print STDERR "TOKEN: $token\n" if ($localDebug);

    if ($inEnum) {
	# If we see this, anything nested below here is clearly not a union.
	if (casecmp($token, $unionname, $case_sensitive)) { $inEnum = 0; };
	if (casecmp($token, $structname, $case_sensitive)) { $inEnum = 0; };
	if (casecmp($token, $typedefname, $case_sensitive)) { $inEnum = 0; };
    }

    my $nonword = 0; my $pascal = 0;
    if ($token =~ /\W/) { $nonword = 1; }
    if ($lang eq "pascal") { $pascal = 1; }

    if ($lang eq "C" || $lang eq "java" || $pascal || $sublang eq "javascript" ||
		$lang eq "php" || $lang eq "perl" ||
		$lang eq "Csource" || $lang eq "shell") {
	if ($inQuote == 1) {
		print STDERR "    STRING\n" if ($localDebug || $codePathDebug);
		$tokenType = "string";
	} elsif ($inQuote == 2) {
		print STDERR "    CHAR\n" if ($localDebug || $codePathDebug);
		$tokenType = "char";
	} elsif ($nonword && $token eq $soc && $soc ne "") {
	    if (!$hidden) {
		$tokenType = "comment";
		print STDERR "    COMMENT [1]\n" if ($localDebug || $codePathDebug);
		if (!$inComment) {
			$inComment = 1;
			$firstCommentToken = 1;
			if ($xmlmode) {
				$string .= "<declaration_comment>";
			} else {
				$string .= "<span class=\"comment\">";
			}
		} else {
			print STDERR "    nested comment\n" if ($localDebug || $codePathDebug);
		}
	    } else {
		print STDERR "    COMMENT [1a]: HIDDEN\n" if ($localDebug || $codePathDebug);
	    }
	} elsif ($nonword && ((($token eq $ilc) && ($ilc ne "")) || (($token eq $ilc_b) && ($ilc_b ne "")))) {
	    if (!$hidden) {
		print STDERR "    ILCOMMENT [1]\n" if ($localDebug || $codePathDebug);
		$tokenType = "comment";
		if (!$inComment) {
			print STDERR "    REALILCOMMENT\n" if ($localDebug || $codePathDebug);
			$inComment = 2;
			$firstCommentToken = 1;
			if ($xmlmode) {
				$string .= "<declaration_comment>";
			} else {
				$string .= "<span class=\"comment\">";
			}
		} else {
			print STDERR "    nested comment\n" if ($localDebug || $codePathDebug);
		}
	    } else {
		print STDERR "    ILCOMMENT [1a]: HIDDEN\n" if ($localDebug || $codePathDebug);
	    }
	} elsif ($nonword && $token eq $eoc && $eoc ne "") {
		print STDERR "    EOCOMMENT [1]\n" if ($localDebug || $codePathDebug);
		$tokenType = "comment";
		if ($xmlmode) {
			$tailstring .= "</declaration_comment>";
		} else {
			$tailstring = "</span>";
		}
		$leavingComment = 1;
		$inComment = 0;
	} elsif ($tokennl && ($ntoken !~ /^[\r\n]/o || $keep_whitespace || $keep_all_newlines)) {
		if ($inComment == 2) {
			print STDERR "    EOL INCOMMENT: END ILCOMMENT [1]\n" if ($localDebug || $codePathDebug);
			$tokenType = "comment";
			if ($xmlmode) {
				$string .= "</declaration_comment>";
			} else {
				$string .= "</span>";
			}
			$inComment = 0;
			$newlen = 0;
			$mustbreak = 1;
			# $token = "";
			$drop = 1;
		} elsif ($inMacro || $keep_all_newlines) {
			print STDERR "    EOL INMACRO\n" if ($localDebug || $codePathDebug);
			$mustbreak = 1;
			$newlen = 0;
		} elsif ($inComment) {
			print STDERR "    EOL INCOMMENT\n" if ($localDebug || $codePathDebug);
			$mustbreak = 1;
			$newlen = 0;
			# $token = "";
			$drop = 1;
		}
		$breakable = 0;
		$nextbreakable = 0;
		# $nextprespace = nspaces(4 * $depth);
		$newlen = 0;
	# } elsif ($ntoken =~ /^[\r\n]/o) {
		# print STDERR "    NEXT TOKEN IS NLCR\n" if ($localDebug || $codePathDebug);
		# $breakable = 0;
		# $nextbreakable = 0;
	} elsif ($inComment) {
		print STDERR "    COMMENT [2:$inComment]\n" if ($localDebug || $codePathDebug);
		$tokenType = "comment";
		if ($inComment == 1) {
			if (($token =~ /^\s/o && !$tokennl && $ntoken !~ /^\s/o) && (!$keep_whitespace)) {
				# Only allow wrapping of multi-line comments.
				# Don't blow in extra newlines at existing ones.
				$breakable = 1;
			}
		}
	} elsif ($inMacro) {
		print STDERR "    MACRO [IN]\n" if ($localDebug || $codePathDebug);
		$tokenType = "preprocessor";
	} elsif ($token eq "=") {
		print STDERR "    EQUALS\n" if ($localDebug || $codePathDebug);
		$nextbreakable = 1;
		if ($type eq "pastd") {
			$type = "";
			print STDERR "    END OF VAR\n" if ($localDebug || $codePathDebug);
		}
		if ($pascal) { $seenEquals = 1; }
	} elsif ($token eq "-") {
		print STDERR "    MINUS\n" if ($localDebug || $codePathDebug);
		if ($ntoken =~ /^\d/o) {
			$tokenType = "number";
			print STDERR "    NUMBER [1]\n" if ($localDebug || $codePathDebug);
		} else {
			print STDERR "    TEXT [1]\n" if ($localDebug || $codePathDebug);
			$tokenType = "";
		}
	} elsif ($token =~ /^\d+$/o || $token =~ /^0x[\dabcdef]+$/io) {
		$tokenType = "number";
		$type = "hexnumber";
		print STDERR "    \nNUMBER [2]: $token\n" if ($localDebug || $codePathDebug);
	} elsif (!$nonword && casecmp($token, $sofunction, $case_sensitive) || casecmp($token, $soprocedure, $case_sensitive)) {
		$tokenType = "keyword";
		$lastKeyword = $token;
		print STDERR "    SOFUNC/SOPROC\n" if ($localDebug || $codePathDebug);
		$type = "funcproc";
		$lastBrace = "(";
		$oldLastBrace = "(";
	} elsif (!$nonword && $type eq "funcproc") {
		if ($token =~ /^\;/o) {
			$type = "";
			$nextbreakable = 3;
		}
		print STDERR "    FUNC/PROC NAME\n" if ($localDebug || $codePathDebug);
		$tokenType = "function";
	} elsif (!$nonword && casecmp($token, $constname, $case_sensitive)) {
		$tokenType = "keyword";
		print STDERR "    VAR\n" if ($localDebug || $codePathDebug);
		$type = "pasvar";
	} elsif (!$nonword && casecmp($token, $varname, $case_sensitive)) {
		$tokenType = "keyword";
		print STDERR "    VAR\n" if ($localDebug || $codePathDebug);
		$type = "pasvar";
	} elsif ($nonword && ($type eq "pasvar" || $type eq "pastd") &&
		 ($token =~ /^[\;\:\=]/o)) {
			# NOTE: '=' is handled elsewhere,
			# but it is included above for clarity.
			$type = "";
			print STDERR "    END OF VAR\n" if ($localDebug || $codePathDebug);
	} elsif ($type eq "pasvar" || $type eq "pastd") {
		print STDERR "    VAR NAME\n" if ($localDebug || $codePathDebug);
		$tokenType = "var";
	} elsif (!$nonword && ($pascal) && casecmp($token, $typedefname, $case_sensitive)) {
		# TYPE: This is the start of a pascal type
		print STDERR "    TYPE\n" if ($localDebug || $codePathDebug);
		$tokenType = "keyword";
		$type = "pastd";
	} elsif (!$nonword && ($pascal) && casecmp($token, $structname, $case_sensitive)) {
		# RECORD: This is the start of a pascal record
		print STDERR "    RECORD/STRUCT\n" if ($localDebug || $codePathDebug);
		$lastBrace = $token;
		$tokenType = "keyword";
		$type = "pasrec";
	} elsif (!$nonword && $tokenIsKeyword) {
		# This is a keyword in any language.
		$tokenType = "keyword";

		if ($tokenIsKeyword == 2) {
			# This formatting change applies only to children of this node.
			$inAttribute = 1;
		} elsif ($tokenIsKeyword == 5) {
			# This formatting change applies only to children of this node.
			$inRaises = 1;
		} elsif ($tokenIsKeyword == 6) {
			$inTypeOf = 1;
		}


		# NOTE: If anybody ever wants "class" to show up colored
		# as a keyword within a template, the next block should be
		# made conditional on a command-line option.  Personally,
		# I find it distracting, hence the addition of these lines.

		if ($lastBrace eq $sotemplate && $sotemplate ne "") {
			$tokenType = "template";
		}

		print STDERR "    KEYWORD\n" if ($localDebug || $codePathDebug);
		# $inMacro = $self->isMacro($token, $lang, $sublang);
		# We could have keywords in a macro, so don't set this
		# to zero.  It will get zeroed when we pop a level
		# anyway.  Just set it to 1 if needed.
		if ($case_sensitive) {
			if ($macroList{$token}) {
				print STDERR "    IN MACRO\n" if ($localDebug || $codePathDebug);
				$inMacro = 1;
			}
		} else {
		    foreach my $cmpToken (keys %macroList) {
			if (casecmp($token, $cmpToken, $case_sensitive)) {
				$inMacro = 1;
			}
		    }
		}
		print STDERR "    TOKEN IS $token, IM is now $inMacro\n" if ($localDebug || $codePathDebug);
		if (casecmp($token, $rbrace, $case_sensitive)) {
			print STDERR "PS: ".length($prespace)." -> " if ($psDebug);
			# $prespace = nspaces(4 * ($depth-1));
			$mustbreak = 2;
			print STDERR length($prespace)."\n" if ($psDebug);
		}
	} elsif (!$inQuote && !$inComment && isKnownMacroToken($token, \%macroList, $case_sensitive)) {
		# This is a preprocessor directive
				print STDERR "    IN MACRO\n" if ($localDebug || $codePathDebug);
				$inMacro = 1;
	} elsif (($token eq "*") && ($depth == 1) && ($lastTokenType eq "type" || $lastTokenType eq "star")) {
		print STDERR "    ASTERISK\n" if ($localDebug || $codePathDebug);
		# spacing fix for '*' characters
		if (!$spaceSinceLastToken && (!$keep_whitespace) && $lastTokenType ne "star") {
			if ($prespace == "") { $prespace = " "; }
		}
		$tokenType = "type";
		$isTypeStar = 1;
		$nospaceafter = 1;
	} elsif ($ntokennc eq ":" && $inObjCMethod) {
		# Detecting of objective-C method separators
		print STDERR "    COLON (FUNCTION [1])\n" if ($localDebug || $codePathDebug);
		$tokenType = "function";
	} elsif ($token eq ":" && $inObjCMethod) {
		# Detecting of objective-C method separators
		print STDERR "    COLON (FUNCTION [2])\n" if ($localDebug || $codePathDebug);
		$tokenType = "function";
	} elsif ($token eq ":" && $ctoken) {
		# Don't indent Objective-C method parts so far.
		$depth = $depth - 1; # We'll change it back before the next token.
	} elsif ($ntokennc eq "(" && !$seenEquals && !$inAttribute && !$inRaises && !$inTypeOf) {
		# Upcoming parenthesis handling
		$tokenType = "function";
		print STDERR "    LPAREN (FUNCTION [3])\n" if ($localDebug || $codePathDebug);
		if ($nntokennc eq "(") {
			$tokenType = "type";
			$type = "funcptr";
		}
		if ($inObjCMethod) {
			$tokenType = ""; # shouldn't happen 
		}
		if ($token eq "(") { $dropFP = 1; }
	} elsif ($ntokennc eq $lbrace && $lbrace ne "") {
		# Upcoming brace handling
		$tokenType = "type";
		print STDERR "    LBRACE (TYPE [1])\n" if ($localDebug || $codePathDebug);
	} elsif (($inAttribute || $inRaises || $inTypeOf) && $token eq "(") {
		print STDERR "    LPAREN (ATTRIBUTE)\n" if ($localDebug || $codePathDebug);
		# Parenthesis handling for attributes
		$nextbreakable = 0;
	} elsif ($token eq "(") {
		print STDERR "    LPAREN (GENERAL)\n" if ($localDebug || $codePathDebug);
		# Parenthesis handling
		if ($inObjCMethod && $lastBrace =~ /^[+-]/o) {
			$nextbreakable = 0;
			$oldLastBrace = "";
		} elsif ($ctoken ne ")") {
			$nextbreakable = 3;
		}
		$lastBrace = $token;
		#if (!$depth) {
			#$nospaceafter = 2;
		#}
	} elsif ($token eq $sotemplate && $sotemplate ne "") {
		print STDERR "    TEMPLATE\n" if ($localDebug || $codePathDebug);
		# This is the word "template" or similar.
		$lastBrace = $token;
		$nextbreakable = 0;
		$breakable = 0;
	} elsif (casecmp($token, $lbrace, $case_sensitive)) {
		print STDERR "    LBRACE (GENERAL)\n" if ($localDebug || $codePathDebug);
		# Brace handling.
		$lastBrace = $token;
		$nextbreakable = 3;
		if (!casecmp($ctoken, $rbrace, $case_sensitive)) {
			$nextbreakable = 3;
		}
	} elsif ($token =~ /^\"/o) {
		print STDERR "    DQUOTE\n" if ($localDebug || $codePathDebug);
		# Double quote handling
		$inQuote = 1;
	} elsif ($token =~ /^\'/o) {
		print STDERR "    SQUOTE\n" if ($localDebug || $codePathDebug);
		# Single quote handling
		$inQuote = 2;
	} elsif ($ntokennc =~ /^(\)|\,|\;)/o || casecmp($ntokennc, $rbrace, $case_sensitive)) {
		# Detection of the last token before the end of a part of a declaration.
		# last token
		print STDERR "    LASTTOKEN\n" if ($localDebug || $codePathDebug);
		if ($nextbreakable != 3) {
			$nextbreakable = 2;
		}
		if ($lastBrace eq $sotemplate && $sotemplate ne "") {
			$nextbreakable = 0;
		}
		if ($lastBrace eq "(") {
			if ($MIG || $pascal) {
				$tokenType = "type";
				print STDERR "    TYPE [2]\n" if ($localDebug || $codePathDebug);
			} else {
				$tokenType = "param";
				print STDERR "    PARAM [1]\n" if ($localDebug || $codePathDebug);
			}
		} elsif ($lastBrace eq $sotemplate && $sotemplate ne "") {
			print STDERR "    TEMPLATE[1]\n" if ($localDebug || $codePathDebug);
			$tokenType = "template";
		} elsif ($type eq "funcptr") {
			$tokenType = "function";
			print STDERR "    FUNCTION [1]\n" if ($localDebug || $codePathDebug);
			$breakable = 0;
			$nextbreakable = 0;
		} else {
			if ($MIG || $pascal) {
				$tokenType = "type";
				print STDERR "    TYPE [2a]\n" if ($localDebug || $codePathDebug);
			} else {
				$tokenType = "var";
				print STDERR "    VAR [1] (LB: $lastBrace)\n" if ($localDebug || $codePathDebug);
			}
		}
		if (casecmp($ntokennc, $rbrace, $case_sensitive) && $type eq "pasrec") {
			$type = "";
		}
		if ($ntokennc eq ")") {
			$nextbreakable = 0;
			if ($inObjCMethod || ($token eq "*")) {
				print STDERR "    TYPE [3]\n" if ($localDebug || $codePathDebug);
				$tokenType = "type";
			}
		}
	} elsif ($prespace ne "" && ($token =~ /^\)/o || casecmp($token, $rbrace, $case_sensitive))) {
		print STDERR "PS: ".length($prespace)." -> " if ($psDebug);
		if (!$keep_whitespace) { $prespace = nspaces(4 * ($depth-1)); }
		print STDERR length($prespace)."\n" if ($psDebug);
		$mustbreak = 2;
	} elsif (casecmp($token, $rbrace, $case_sensitive)) {
		if (!$keep_whitespace) { $prespace = nspaces(4 * ($depth-1)); }
		print STDERR length($prespace)."\n" if ($psDebug);
		$mustbreak = 2;
	} else {
		if ($inObjCMethod) {
			if ($lastBrace eq "(") {
				print STDERR "    TYPE [4]\n" if ($localDebug || $codePathDebug);
				$tokenType = "type";
			} else { 
				print STDERR "    PARAM [2]\n" if ($localDebug || $codePathDebug);
				$tokenType = "param";
			}
		} elsif ($MIG || $pascal) {
			if ($lastBrace eq "(") {
				print STDERR "    PARAM [3]\n" if ($localDebug || $codePathDebug);
				$tokenType = "param";
			}
		} else {
			if ($lastBrace eq $sotemplate && ($sotemplate ne "")) {
				print STDERR "    TEMPLATE [5]\n" if ($localDebug || $codePathDebug);
				$tokenType = "template";
			} elsif ($inEnum) {
				# Constants are a special case of variable
				print STDERR "    TYPE [5]\n" if ($localDebug || $codePathDebug);
				$tokenType = "var";
			} else {
				print STDERR "    TYPE [5]\n" if ($localDebug || $codePathDebug);
				$tokenType = "type";
			}
		}
	}
    } else {
	my $fullpath = $apio->fullpath;
	my $linenum = $apio->linenum;
	warn "$fullpath:$linenum: warning: Unknown language $lang. Not coloring. Please file a bug.\n";
    }
    if ($inRaises && $tokenType && ($tokenType ne "keyword")) {
	$tokenType = "type";
    }
    if ($inTypeOf && $tokenType && ($tokenType ne "keyword")) {
	$tokenType = "param";
    }
# print STDERR "TOKEN: $token TYPE: $tokenType\n";

    if ($hidden) {
	$tokenType = "ignore";
	if ($mustbreak) {
		$nextbreakable = 3;
	} else {
		$nextbreakable = 0;
	}
	$mustbreak = 0;
	$breakable = 0;
    }
    if (($ntoken =~ /[,;]/) && ($token =~ /[ \t]/) && !$inComment && !$inMacro && !$inQuote) {
	# print STDERR "DROP\n";
	$hidden = 1;
	$tokenType = "ignore";
	$nextbreakable = 0;
	$mustbreak = 0;
	$breakable = 0;
    }
    if ($MIG || $pascal) {
	if ($lastnstoken =~ /:/ && $lastTokenType eq "var") {
		$string .= $newtd;
	}
    } else {
	if (($lastTokenType eq "type") && !$hidden && ($token =~ /[\w\*]/) &&
		($tokenType eq "var" || $tokenType eq "param" ||
		 $tokenType eq "function" || $token eq "*") && ($lastnstoken =~ /\w/)) {
			$string .= $newtd;
	}
    }

    if (((($ilc ne "") && ($ntoken eq $ilc)) || (($ilc_b ne "") && ($ntoken eq $ilc_b))) && !$inComment) {
	$breakable = 0; $nextbreakable = 0;
    } elsif (($soc ne "") && $ntoken eq $soc && !$inComment) {
	$breakable = 0; $nextbreakable = 0;
    }
print STDERR "NB: $nextbreakable\n" if ($localDebug);

    if ($inObjCMethod) {
	$nextbreakable = 0;
	$breakable = 0;
	$mustbreak = 0;
	if ($ntoken eq ":" && $tokenType eq "function") {
		$breakable = 1;
	}
    }

    if ($type eq "pasrec" && $tokenType eq "") { $tokenType = "var"; }
    else { print STDERR "TYPE: $type TT: $tokenType\n" if ($localDebug); }
    print STDERR "IM: $inMacro\n" if ($localDebug);

    if (!$inComment && $token =~ /^\s/o && !$tokennl && ($mustbreak || !$newlen) && (!$keep_whitespace)) {
	print STDERR "CASEA\n" if ($localDebug);
	print STDERR "NL: $newlen TOK: \"$token\" PS: \"$prespace\" NPS: \"$nextprespace\"\n" if ($localDebug);
	print STDERR "dropping leading white space\n" if ($localDebug);
	$drop = 1;
    } elsif (!$inComment && $tokennl && (!$keep_whitespace)) {
	print STDERR "CASEB\n" if ($localDebug);
	if ($lastnstoken ne $eoc) {
		# Insert a space instead.

		print STDERR "dropping newline\n" if ($localDebug);
		$drop = 1;
		$string .= " ";
	} else {
		$mustbreak = 1;
	}
    } elsif ($inComment || $token =~ /^\s/o || ($token =~ /^\W/o && $token ne "*") || !$tokenType) {
	print STDERR "CASEC\n" if ($localDebug);
	my $macroTail = "";
	$escapetoken = $apio->textToXML($token);
	print STDERR "OLDPS: \"$prespace\" ET=\"$escapetoken\" DROP=$drop\n" if ($localDebug);
	if ($inComment && $prespace ne "" && !$hidden) {
		if ($xmlmode) {
			$string .= "</declaration_comment>\n$prespace<declaration_comment>";
		} else {
			$string .= "</span>\n$endtr$prespace$begintr<span class=\"comment\">";
		}
	} elsif ($inMacro) {
		# Could be the initial keyword, which contains a '#'
		if ($xmlmode) {
			$string .= "$prespace<declaration_$tokenType>";
			$macroTail = "</declaration_$tokenType>";
		} else {
			$string .= "$prespace<span class=\"$tokenType\">";
			$macroTail = "</span>";
		}
	} elsif (!$hidden) {
		$string .= $prespace;
	}
	if ($drop) { $escapetoken = ""; }
	if ($tokenType eq "ignore") {
	    if (!$HeaderDoc::dumb_as_dirt) {
		# Drop token.
		print STDERR "HD: DROPPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
		$escapetoken = "";
	    } else {
		print STDERR "HD BASIC: KEEPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
	    }
	}

	$string .= "$escapetoken$macroTail";
	print STDERR "comment: $token\n" if ($localDebug);
    } else {
	print STDERR "CASED\n" if ($localDebug);

	# my $add_link_requests = $HeaderDoc::add_link_requests;
	$escapetoken = $apio->textToXML($token);

	if (($tokenType ne "") && ($token ne "") && token !~ /^\s/o) {
		my $fontToken = "";
		if ($xmlmode) {
			$fontToken = "<declaration_$tokenType>$escapetoken</declaration_$tokenType>";
		} else {
		    if ($tokenType ne "ignore") {
			$fontToken = "<span class=\"$tokenType\">$escapetoken</span>";
		    } elsif (!$HeaderDoc::dumb_as_dirt) {
			# Drop token.
			print STDERR "HD: DROPPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
			$fontToken = "";
		    } else {
			print STDERR "HD BASIC: KEEPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
			$fontToken = $escapetoken;
		    }
		}
		my $linkTokenType = $tokenType;
		if ($inTypeOf && ($tokenType ne "keyword")) {
			$linkTokenType = "";
		}
		my $refToken = $apio->genRef($lastKeyword, $escapetoken, $fontToken, $linkTokenType);

		# Don't add noisy link requests in XML.
		if ($HeaderDoc::add_link_requests && ($tokenType =~ /^(function|var|type|preprocessor)/o || $inTypeOf) && !$xmlmode) {
			$string .= "$prespace$refToken";
		} else {
			$string .= "$prespace$fontToken";
		}
	} else {
		$escapetoken = $apio->textToXML($token);
		if ($tokenType eq "ignore") {
		    if (!$HeaderDoc::dumb_as_dirt) {
			# Drop token.
			print STDERR "HD: DROPPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
			$escapetoken = "";
		    } else {
			print STDERR "HD BASIC: KEEPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
		    }
		}
		$string .= "$prespace$escapetoken";
	}
	print STDERR "$tokenType: $token\n" if ($localDebug);
    }
    $prespace = $nextprespace;

    if (!$drop) {
	$newlen += length($token);
    }

    print STDERR "NL $newlen MDL $HeaderDoc::maxDecLen BK $breakable IM $inMacro\n" if ($localDebug);
    if ((!$keep_whitespace) && ($mustbreak ||
		(($newlen > $HeaderDoc::maxDecLen) &&
		    $breakable && !$inMacro && !$hidden))) {
	print STDERR "MUSTBREAK CASE\n" if ($localDebug);
	if (($token =~ /^\s/o || $token eq "") && (!$keep_whitespace)) {
		$nextprespace = nspaces(4 * ($depth+(1-$mustbreak)));
		print STDERR "PS WILL BE \"$nextprespace\"\n" if ($localDebug);
		$nextbreakable = 3;
	} else {
		print STDERR "NEWLEN: $newlen\n" if ($localDebug);
		$newlen = length($token);
		print STDERR "NEWLEN [2]: $newlen\n" if ($localDebug);
		print STDERR "MB: $mustbreak, DP: $depth\n" if ($localDebug);
		my $ps = nspaces(4 * ($depth+(1-$mustbreak)));
		if (($inComment == 1 && !$firstCommentToken) || $leavingComment) {
		    if ($xmlmode) {
			$string = "</declaration_comment>\n$ps<declaration_comment>$string";
		    } else {
			$string = "</span>$endtr\n$begintr$ps<span class=\"comment\">$string";
		    }
		} else {
			$string = "$endtr\n$begintr$ps$string";
		}
		print STDERR "PS WAS \"$ps\"\n" if ($localDebug);
	}
    }

    if ($token !~ /^\s/o) { $lastnstoken = $token; }

    if ($token !~ /\s/) {
	if ($isTypeStar) {
		$lastTokenType = "star";
	} else {
    		$lastTokenType = $tokenType;
	}
	$spaceSinceLastToken = 0;
    } else {
	$spaceSinceLastToken = 1;
    }

    my $newstring = "";
    my $node = $self->{FIRSTCHILD};
    my $newstringref = undef;
    if ($node && $continue) {
	if ($nospaceafter == 1) { $nospaceafter = 0; }
	print STDERR "BEGIN CHILDREN\n" if ($localDebug || $colorDebug || $treeDebug);
	bless($node, "HeaderDoc::ParseTree");
	($newstringref, $newlen, $nextbreakable, $prespace, $lastnstoken, $continue, $lastTokenType, $spaceSinceLastToken) = $node->colorTreeSub($keep_whitespace, $apio, $type, $depth + 1, $inComment, $inQuote, $inObjCMethod, $lastBrace, $sotemplate, $soc, $eoc, $ilc, $ilc_b, $lbrace, $rbrace, $sofunction, $soprocedure, $varname, $constname, $unionname, $structname, $enumname, $typedefname, $structisbrace, $macroListRef, $prespace, $lang, $sublang, $xmlmode, $newlen, $nextbreakable, $inMacro, $inEnum, $seenEquals, $lastKeyword, $lastnstoken, $lastTreeNode, $lastTokenType, $spaceSinceLastToken, $inAttribute, $inRaises, $inTypeOf, $drop_pdefine_contents);
	$newstring = ${$newstringref};
	print STDERR "END CHILDREN\n" if ($localDebug || $colorDebug || $treeDebug);
    }
    $string .= $newstring; $newstring = "";
    print STDERR "SET STRING TO $string\n" if ($localDebug);

    if (($prespace ne "")) {
	# if we inherit a need for prespace from a descendant, it means
	# that the descendant ended with a newline.  We don't want to
	# propagate the extra indentation to the next node, though, so
	# we'll regenerate the value of prespace here.
	$prespace = nspaces(4 * $depth);
    }
    print STDERR "HMLT: ".$self->{HIDEMACROLASTTOKEN}."\n" if ($localDebug);

    if ($self->{HIDEMACROLASTTOKEN} && $drop_pdefine_contents) {
	$continue = 0;
    }

    $string .= $tailstring;
    $tailstring = "";
    print STDERR "LB $lastBrace -> $oldLastBrace\n" if ($colorDebug || $localDebug);
    $lastBrace = $oldLastBrace;
    $depth = $oldDepth;

    print STDERR "Resetting inMacro ($inMacro) to previous value ($oldInMacro).\n" if ($localDebug);
    $inMacro = $oldInMacro;
    $lastKeyword = $oldLastKeyword;
    $inComment = $oldInComment;
    $inQuote = $oldInQuote;
    $inAttribute = $oldInAttribute;
    $inRaises = $oldInRaises;
    $inTypeOf = $oldInTypeOf;
    # if ($inComment && !$oldInComment) {
	# $inComment = $oldInComment;
	# if ($xmlmode) {
		# $string .= "</declaration_comment>";
	# } else {
		# $string .= "</span>";
	# }
    # }

    if ($dropFP) { $type = $apio->class(); }

    $node = $self->{NEXT};
    if ($node && $continue) {
	bless($node, "HeaderDoc::ParseTree");

	if ($nospaceafter) {
		while ($node && ($node->token =~ /[ \t]/)) {
			print STDERR "SKIPPED NODE (\"".$node->token."\").\n" if ($localDebug);
			$node = $node->next;
			bless($node, "HeaderDoc::ParseTree");
		}
		print STDERR "STOPPED SKIPPING AT NODE \"".$node->token."\".\n" if ($localDebug);
	}
	print STDERR "CONTINUING TO NODE \"".$node->token."\".\n" if ($localDebug);
	if ($node) {
		($newstringref, $newlen, $nextbreakable, $prespace, $lastnstoken, $continue, $lastTokenType, $spaceSinceLastToken) = $node->colorTreeSub($keep_whitespace, $apio, $type, $depth, $inComment, $inQuote, $inObjCMethod, $lastBrace, $sotemplate, $soc, $eoc, $ilc, $ilc_b, $lbrace, $rbrace, $sofunction, $soprocedure, $varname, $constname, $unionname, $structname, $enumname, $typedefname, $structisbrace, $macroListRef, $prespace, $lang, $sublang, $xmlmode, $newlen, $nextbreakable, $inMacro, $inEnum, $seenEquals, $lastKeyword, $lastnstoken, $lastTreeNode, $lastTokenType, $spaceSinceLastToken, $inAttribute, $inRaises, $inTypeOf, $drop_pdefine_contents);
		$newstring = ${$newstringref};
	}
    }
    $string .= $newstring;
    print STDERR "SET STRING TO $string\n" if ($localDebug);

    # $self->{CTSTRING} = $string;
    # $self->{CTSUB} = ($newlen, $nextbreakable, $prespace, $lastnstoken);
    return (\$string, $newlen, $nextbreakable, $prespace, $lastnstoken, $continue, $lastTokenType, $spaceSinceLastToken);
}

sub test_output_dump_rec
{
    my $self = shift;
    my $depth = shift;
    my $lastnode = shift;
    my $ret = "";

    my $parserState = $self->parserState();
    if ($parserState && !$lastnode) {
        $lastnode = $parserState->{lastTreeNode};
    }

    if ($self->token ne "") {
      my $i = $depth-1;
      while ($i > 0) {
	$ret .= "|   ";
	$i--;
      }
      my $HYPHEN = "-";
      my $psString = "";
      if ($self->parserState()) {
	$HYPHEN = "*";
	$psString = " (HAS STATE)";
      }
      if ($depth) {
	$ret .= "+-$HYPHEN-";
      }
      if ($self->token =~ /\n$/) {
	$ret .= "[ NEWLINE ]$psString\n";
      } else {
	$ret .= $self->token()."$psString\n";
	# if ($self->token !~ /\n$/) { $ret .= "\n"; }
      }
    }

    if ($self == $lastnode) {
	$ret .= "-=-=-=-=-=-=- EODEC -=-=-=-=-=-=-\n";
    }

    if ($self->firstchild()) {
	$ret .= $self->firstchild()->test_output_dump_rec($depth+1, $lastnode);
    }
    if ($self->next()) {
	$ret .= $self->next()->test_output_dump_rec($depth, $lastnode);
    }

    return $ret;
}

sub dbprintrec
{
    my $self = shift;
    my $depth = shift;
    my $lastnode = shift;

    my $parserState = $self->parserState();
    if ($parserState && !$lastnode) {
        $lastnode = $parserState->{lastTreeNode};
    }

    if ($self->token ne "") {
      my $i = $depth-1;
      while ($i > 0) {
	print STDERR "|   ";
	$i--;
      }
      my $HYPHEN = "-";
      my $psString = "";
      if ($self->parserState()) {
	$HYPHEN = "*";
	$psString = " (TOKENID: ".$self.", PSID: ".$self->parserState().")";
      }
      if ($depth) {
	print STDERR "+-$HYPHEN-";
      }
      if ($self->token =~ /\n$/) {
	print STDERR "[ NEWLINE ]$psString\n";
      } else {
	print STDERR $self->token()."$psString\n";
	# if ($self->token !~ /\n$/) { print STDERR "\n"; }
      }
    }

    if ($self == $lastnode) {
	print STDERR "-=-=-=-=-=-=- EODEC -=-=-=-=-=-=-\n";
    }

    if ($self->firstchild()) {
	$self->firstchild()->dbprintrec($depth+1, $lastnode);
    }
    if ($self->next()) {
	$self->next()->dbprintrec($depth, $lastnode);
    }
}

sub test_output_dump
{
    my $self = shift;

    return $self->test_output_dump_rec(1);
}

sub dbprint
{
    my $self = shift;
    $self->dbprintrec(1);
}

sub filename
{
    my $self = shift;

    if (@_) {
	$self->{FILENAME} = shift;
    }

    return $self->{FILENAME};
}

sub fullpath
{
    my $self = shift;

    if (@_) {
	$self->{FULLPATH} = shift;
    }

    return $self->{FULLPATH};
}

sub linenum
{
    my $self = shift;

    if (@_) {
	$self->{LINENUM} = shift;
    }
    return $self->{LINENUM};
}


sub printObject {
    my $self = shift;
 
    print STDERR "----- ParseTree Object ------\n";
    print STDERR "token: $self->{TOKEN}\n";
    print STDERR "next: $self->{NEXT}\n";
    print STDERR "firstchild: $self->{FIRSTCHILD}\n";
    print STDERR "\n";
}


sub addRawParsedParams
{
    my $self = shift;
    my $pplref = shift;

    my @array = @{$pplref};

    foreach my $param (@array) {
	push(@{$self->{RAWPARSEDPARAMETERS}}, $pplref);
    }

    return $self->{RAWPARSEDPARAMETERS};
}


sub rawParsedParams
{
    my $self = shift;

    return $self->{RAWPARSEDPARAMETERS};
}

sub parserState
{
    my $self = shift;
    my $localDebug = 0;

    if (@_) {
	my $state = shift;
	print STDERR "Setting parser state for $self\n" if ($localDebug);
	print STDERR "Last token (raw) is $state->{lastTreeNode}\n" if ($localDebug);
	print STDERR "Last token (text) is ".$state->{lastTreeNode}->token()."\n" if ($localDebug);
	$self->{PARSERSTATE} = $state;
    }

    return $self->{PARSERSTATE};
}

sub trygcc
{
    my $self = shift;
    my $rawvalue = shift;
    my $success = 0;
    my $value = 0;
    my $timestamp = time();
    my $localDebug = 0;

    # print STDERR "RV: $rawvalue\n";

    if (open(GCCFILE, ">/tmp/headerdoc-gcctemp-$timestamp.c")) {
	print GCCFILE "#include <inttypes.h>\nmain(){printf(\"%d\\n\", $rawvalue);}\n";
	close(GCCFILE);

	if (open(GCCPIPE, $HeaderDoc::c_compiler." /tmp/headerdoc-gcctemp-$timestamp.c -o /tmp/headerdoc-gcctemp-$timestamp > /dev/null 2> /dev/null |")) {
		my $junkstring = <GCCPIPE>;
		close(GCCPIPE);
		if ($?) {
			$success = 0;
		} else {
			$success = 1;
		}

		if ($success) {
			if (open(EXECPIPE, "/tmp/headerdoc-gcctemp-$timestamp |")) {
				my $retstring = <EXECPIPE>;
				$value = $retstring;
				$value =~ s/\n//sg;
				print STDERR "VALUE: $value\nSUCCESS: $success\n" if ($localDebug);
			} else {
				$success = 0;
			}
		}
	}
	unlink("/tmp/headerdoc-gcctemp-$timestamp.c");
	unlink("/tmp/headerdoc-gcctemp-$timestamp");
    }

    print STDERR "RET $success, $value\n" if ($localDebug);
    return ($success, $value);
}

sub getPTvalue
{
    my $self = shift;
    my $success = 0;
    my $value = 0;
    my $localDebug = 0;

    my $pos = $self;

    while ($pos && ($pos->token() ne "#define")) {
	$pos = $pos->next();
    }
    if (!$pos) {
	return($success, $value);
    }
    $pos = $pos->firstchild();

    while ($pos && ($pos->hidden != 3) && ($pos->{HIDEMACROLASTTOKEN} != 2)) {
	$pos = $pos->next();
    }

    if ($pos) {
	my $rawvalue = $pos->textTree();
	print STDERR "getPTvalue: WE HAVE A WINNER.\n" if ($localDebug);
	print STDERR "RAWVALUE IS: $rawvalue\n" if ($localDebug);

	($success, $value) = $self->trygcc($rawvalue);
    }

    return($success, $value);
}

sub dispose
{
    my $self = shift;
    my $localDebug = 0;

    # Decrement the reference count.
    if (--$self->{REFCOUNT}) { return; }

    print STDERR "Disposing of tree\n" if ($localDebug);

    $self->{PARENT} = undef;
    if ($self->{FIRSTCHILD}) {
	$self->{FIRSTCHILD}->dispose();
	$self->{FIRSTCHILD} = undef;
    }
    if ($self->{NEXT}) {
	$self->{NEXT}->dispose();
	$self->{NEXT} = undef;
    }
    $self->{NTNC} = undef;
    if ($self->{APIOWNER}) { $self->{APIOWNER} = undef; }
    if ($self->{PARSERSTATE}) { $self->{PARSERSTATE} = undef; }
    $self->{PARSEDPARAMS} = ();
    $self->{RAWPARSEDPARAMETERS} = ();
    $self->{TOKEN} = undef;

}

sub nextAtLevelOf
{
    my $self = shift;
    my $matchingnode = shift;
    my $fullpath = shift;
    my $nextnode = $self;

    while ($nextnode && !$nextnode->isAfter($matchingnode)) {
	$nextnode = $nextnode->parent();
    }
    if ($nextnode) {
	$nextnode = $nextnode->next();
    } else {
	my $tt = $self;
	while ($tt->parent()) { $tt = $tt->parent(); }
	my $apio = $tt->apiOwner();
	cluck("TT: $tt\n");
	# my $fullpath = $apio->fullpath();
	warn("$fullpath:0:Ran off top of stack looking for next node.\n");
	# $nextnode = $matchingnode->next();
	$nextnode = undef;
    }
    return $nextnode;
}

sub DESTROY
{
    my $self = shift;
    my $localDebug = 0;

    print STDERR "Destroying $self\n" if ($localDebug);
}

# Count the number of curly braces at this level in the parse tree
sub curlycount
{
    my $self = shift;
    my $lbrace = shift;
    my $last = shift;
    my $pos = $self;
    my $count = 0;

    while ($last && !$last->isAfter($self)) {
	$last = $last->parent();
    }
    if ($last) { $last = $last->next(); } # first node after this declaration

    while ($pos && $pos != $last) {
	if ($pos->token eq "$lbrace") {
		$count++;
	}
	$pos = $pos->next();
    }
    return $count;
}

sub skipcurly
{
    my $self = shift;
    my $lbrace = shift;
    my $count = shift;

    my $localDebug = 0;
    my $pos = $self;

    print STDERR "SKIPPING $count curly braces (lbrace = '$lbrace') at POS=$pos\n" if ($localDebug);
    if (!$count) { return $self; }

    while ($pos) {
	my $tok = $pos->token();
	print STDERR "TOKEN: '$tok'\n" if ($localDebug);
	if ($tok eq "$lbrace") {
		print STDERR "MATCH\n" if ($localDebug);
		if (!--$count) {
			my $next = $pos->next;
			if ($localDebug) {
				print STDERR "FOUND ONE.  Next tree is:\n";
				if ($next) {
					$next->dbprint();
				} else {
					print STDERR "UNDEFINED!\n";
				}
			}
			return $next;
		}
	}
	$pos = $pos->next();
    }
    warn "Yikes!  Ran out of open braces!\n";
    return $pos;
}

sub isKnownMacroToken
{
    my $token = shift;
    my $macroListRef = shift;
    my $case_sensitive = shift;

    my %macroList = %{$macroListRef};
    if ($case_sensitive) {
	if ($macroList{$token}) { return 1; }
	return 0;
    }

    foreach my $cmpToken (keys %macroList) {
	if (casecmp($token, $cmpToken, $case_sensitive)) {
		return 1;
	}
    }
    return 0;
}

sub parseComplexAvailability
{
    my $self = shift;
    my $localDebug = 0;

    print STDERR "parseComplexAvailability: dumping tree for $self.\n" if ($localDebug);

    $self->dbprint() if ($localDebug);

    my $token = $self->token();
    my $availstring = "";

    my $pos = $self->firstchild();
    while ($pos && ($pos->token() ne "(")) {
	$pos = $pos->next();
    }
    if (!$pos) { my @arr = (); return \@arr; }

    $pos = $pos->next();
    while ($pos && ($pos->token() ne ")")) {
	$availstring .= $pos->token();
	$pos = $pos->next();
    }

    print STDERR "TOKEN: $token\nSTRING: $availstring\n" if ($localDebug);
    return complexAvailabilityToArray($token, $availstring);
}

1;
