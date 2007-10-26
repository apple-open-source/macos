#! /usr/bin/perl -w
#
# Class name: 	ParseTree
# Synopsis: 	Used by headerdoc2html.pl to hold parse trees
# Author: David Gatwood(dgatwood@apple.com)
# Last Updated: $Date: 2007/08/20 19:54:10 $
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
use HeaderDoc::Utilities qw(isKeyword parseTokens quote stringToFields casecmp);
use HeaderDoc::BlockParse qw(blockParse nspaces);
use Carp qw(cluck);

# use WeakRef;


$VERSION = '$Revision: 1.1.2.79 $';
################ General Constants ###################################
my $debugging = 0;

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

# print "addSibling $self\n";
    print "addSibling $self \"$name\"\n" if ($treeDebug);

    my $parent = $self->parent;

    my $pos = $self;
    # if ($parent) {
	# $pos = $parent->lastchild();
	# bless($pos, "HeaderDoc::ParseTree");
    # } else {
	# warn "NOPARENTA!\nNOPARENTB!\nNOPARENTC!\n";
	bless($pos, "HeaderDoc::ParseTree");

	# print "POS: $pos\n";
	# while ($pos && $pos->next()) {
		# $pos = $pos->next();
		# bless($pos, "HeaderDoc::ParseTree");
		# # print "POS: $pos: ".$pos->token()."\n";
	# }
    	# bless($pos, "HeaderDoc::ParseTree");
    # }
    $newnode->token($name);
    if ($hide) { $newnode->hidden($hide); }
    $newnode->parent($parent);
    # if ($parent) { $parent->lastchild($newnode); }

    my $noderef = $newnode;

    return $pos->next($noderef);
	# print "$self (".$self->token().") RET $ret (".$ret->token().")\n";
    # return $ret;
}

sub addChild
{
    my $self = shift;
    my $name = shift;
    my $hide = shift;

# print "addChild\n";
    print "addChild $self \"$name\"\n" if ($treeDebug);

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

    # print "addAPIOwner: SELF WAS $self\n";
    # print "addAPIOwner: APIO WAS $newapio\n";
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
sub apiOwner {
    my $self = shift;

    if (@_) {
	my $newapio = shift;

	if (!$newapio) {
		warn("apiOwner called with empty APIO!\n");
	}

	# print "apiOwner: SETTING TO $newapio\n";

	$self->{APIOWNER} = ();
        push(@{$self->{APIOWNER}}, $newapio);
	$self->{REFCOUNT} = 1;
    }

    my $apio = undef;
    foreach my $possowner (@{$self->{APIOWNER}}) {
	# print "TESTING $possowner\n";
	if ($possowner !~ /HeaderDoc::HeaderElement/) {
		if ($possowner !~ /HeaderDoc::APIOwner/) {
			if ($possowner) {
				$apio = $possowner;
				# print "CHOSE $apio\n";
			}
		}
	}
    }
    if (!$apio) {
	$apio = pop(@{$self->{APIOWNER}});
	push(@{$self->{APIOWNER}}, $apio);
	# print "GUESSING $apio\n";
    }

    return $apio;
}

sub apiOwners
{
    my $self = shift;

    # foreach my $apio (@{$self->{APIOWNER}} ) {
	# print "APIOWNER LIST INCLUDES $apio\n";
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

    # print "SETTING HIDDEN VALUE OF TOKEN ".$self->token()." to $value\n";
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

    my $noParse = 1;
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
	    $noParse = 0;
	} elsif ($noParse) {
	    # drop token on the floor.  It's part of the name.
	} elsif ($token eq "(") {
	    $inType++;
	    $curType .= $token;
	} elsif ($token eq ")") {
	    if (!(--$inType)) {
		$inName = 1;
	    }
	    $curType .= $token;
	} elsif ($token =~ /^[\s\W]/o && !$inType) {
	    # drop white space and symbols on the floor (except
	    # for pointer types)

	    if ($inName && ($curName ne "")) {
		$inName = 0;
		my $param = HeaderDoc::MinorAPIElement->new();
		$param->linenum($self->apiOwner()->linenum());
		$param->outputformat($self->apiOwner()->outputformat());
		$param->name($curName);
		$param->type($curType);
		$param->position($position++);
		print "ADDED $curType $curName\n" if ($objCParmDebug);
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
    }

    if ($objCParmDebug) {
	foreach my $parm (@parsedParams) {
	    print "OCCPARSEDPARM: ".$parm->type()." ".$parm->name()."\n";
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

# print "parsedParams called\n";

    if (@_) {
	if ($self->apiOwner() eq "HeaderDoc::Method") {
	    @{$self->{PARSEDPARAMS}} = $self->objCparsedParams();
	} else {
	    my $pplref = shift;
	    # @{$self->{PARSEDPARAMS}} = @_;
	    @{$self->{PARSEDPARAMS}} = @{$pplref};
	    # for my $parm (@{$pplref}) {
		# print "ADDING PARM $parm\n";
	    # }
	}
    }

    if (!($self->{PARSEDPARAMS})) {
	# print "PARSEDPARAMS PROBLEM: TOKEN WAS ".$self->token()."\n";
	# print "PRINTTREE:\n";
	# $self->printTree();
	# print "ENDOFTREE\n";
	my $next = $self->next();
	if ($next) { return $next->parsedParams(); }
	else { return undef; }
	# else { die("Can't find parsed params\n"); }
    }
# print "HERE: $self : ". $self->token." : ".$self->{PARSEDPARAMS}."\n";
    # foreach my $parm (@{$self->{PARSEDPARAMS}}) {
	# print "FOUND PARM $parm\n";
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
print "PARSEDPARAMCOPY -> $self\n" if ($localDebug);
print "TOKEN WAS ".$self->token()."\n" if ($localDebug);
}

# /*! This subroutine handles embedded HeaderDoc markup, returning a list
#     of parameters, constants, etc.
#  */
sub processEmbeddedTags
{
    my $self = shift;
    my $xmlmode = shift;
    my $apiolist = $self->apiOwners();
    my $apio = $self->apiOwner();
    # $self->printTree();
    # $self->dbprint();
    my $localDebug = 0;
    # if ($apio->isAPIOwner()) { $localDebug = 1; }

    print "PET: $apio\n" if ($localDebug);
    print $apio->name()."\n" if ($localDebug);
    print "APIOLIST IS $apiolist\n" if ($localDebug);;
    # for my $tempapio (@{$apiolist}) {
	# print "RETURNED APIOLIST INCLUDES $tempapio\n";
    # }

    if ($self->{PETDONE}) {
	print "SHORTCUT\n" if ($localDebug);
	return;
    }
    $self->{PETDONE} = 1;

    if (!$apio) { return; }

    my $apioclass = ref($apio) || $apio;

    my $old_enable_cpp = $HeaderDoc::enable_cpp;
    if ($apioclass =~ /HeaderDoc::PDefine/ && $apio->parseOnly()) {
	if ($HeaderDoc::enable_cpp) {
		print "CPP Enabled.  Not processing comments embedded in #define macros marked as 'parse only'.\n" if ($localDebug);
		return;
	}
    } elsif ($apioclass =~ /HeaderDoc::PDefine/) {
	if ($HeaderDoc::enable_cpp) {
		print "Temporarily disabling CPP.\n" if ($localDebug);
		$HeaderDoc::enable_cpp = 0;
	}
    }

    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macronamesref) =
		@_;
		# parseTokens($apio->lang(), $apio->sublang());

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
	print "PARSERSTATE\n" if ($localDebug);
	$lastnode = $parserState->{lastTreeNode};
	print "LASTNODE: $lastnode\n" if ($localDebug);
	if ($lastnode && $localDebug) { print "LASTNODE TEXT: \"".$lastnode->token()."\"\n"; }
    }

    if ($apio->isAPIOwner()) {
	print "Owner is APIOwner.  Using APIOprocessEmbeddedTagsRec for parse tree $self.\n" if ($localDebug);
	$self->APIOprocessEmbeddedTagsRec($apiolist, $soc, $eoc, $ilc, $lbrace, $case_sensitive, $lastnode, 0);
    } else {
	print "calling processEmbeddedTagsRec for $apio (".$apio->name().") for parse tree $self.\n" if ($localDebug);
	$self->processEmbeddedTagsRec($xmlmode, $eoDeclaration, $soc, $eoc, $eocquot, $ilc, $lbrace, $rbrace, $typedefname,
		$case_sensitive, $keywordhashref, $lastDeclaration, $curDeclaration, $pendingHDcomment,
		$apio, $apiolist, $sodec, $lastnode);
    }

print "PETDONE\n" if ($localDebug);

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

    my $filename = $apio->filename();
    my $linenum = $apio->linenum();
    my $lang = $apio->lang();
    my $sublang = $apio->sublang();

    my $blockoffset = $linenum;
    my $argparse = 2;

    # This never hurts just to make sure the parse terminates.
    # Be sure to add a newline before the semicolon in case
    # there's an inline comment at the end.
    $string .= "\n;\n";

    print "STRING WAS $string\n" if ($localDebug);

    my @lines = split(/\n/, $string);

    # my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
        # $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	# $enumname,
        # $typedefname, $varname, $constname, $structisbrace, $macronameref)
		# = parseTokens($lang, $sublang);

    # my @newlines = ();
    foreach my $line (@lines) {
	$line .= "\n";
	# push(@newlines, $line);
        # print "LINE: $line\n" if ($localDebug);
    }
    # @lines = @newlines;

    # my ($case_sensitive, $keywordhashref) = $apio->keywords();

    my $lastlang = $HeaderDoc::lang;
    my $lastsublang = $HeaderDoc::sublang;
    $HeaderDoc::lang = $apio->lang;
    $HeaderDoc::sublang = $apio->sublang;
    my ($inputCounter, $declaration, $typelist, $namelist, $posstypes, $value, $pplStackRef, $returntype, $privateDeclaration, $treeTop, $simpleTDcontents, $availability, $fileoffset, $conformsToList) = blockParse($filename, $blockoffset, \@lines, $inputCounter, $argparse, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);
    $HeaderDoc::lang = $lastlang;
    $HeaderDoc::sublang = $lastsublang;

    print "IC:$inputCounter DEC:$declaration TL:$typelist NL:$namelist PT:$posstypes VAL:$value PSR:$pplStackRef RT:$returntype PD:$privateDeclaration TT:$treeTop STC:$simpleTDcontents AV:$availability\n" if ($localDebug);

    $self->parsedParamCopy($pplStackRef);

    my $name = $namelist;
    $name =~ s/^\s*//so; # ditch leading spaces
    $name =~ s/\s.*$//so; # ditch any additional names. (There shouldn't be any)
	# print "NAME WAS $name\n";
    my $typestring = $typelist . $posstypes;

print "TS: $typestring\n" if ($localDebug);

    my $type = "\@constant";
    if ($typestring =~ /^(function|method|ftmplt|operator|callback)/o) {
	$type = "\@$1";
	if ($typestring =~ /(ftmplt|operator)/) { $type = "\@function"; }
	# $type = "\@callback";
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
	print "VALUE: \"$value\"\n" if ($localDebug);
	if (($value eq "")) {
		# It's just a variable.
		$type = "\@field";
	}
    } else {
	warn "getNameAndFieldTypeFromDeclaration: UNKNOWN TYPE ($typestring) RETURNED BY BLOCKPARSE\n";
	print "STRING WAS $string\n" if ($localDebug);
    }

    if (!$name || ($name eq "")) {
	warn "COULD NOT GET NAME FROM DECLARATION.  DECLARATION WAS:\n$string\n";
	return ("", "");
    }
    print "TYPE $type, NAME $name\n" if ($localDebug);

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
    my $lbrace = shift;
    my $case_sensitive = shift;

    # if ($token eq $soc || $token eq $eoc || $token eq $ilc) { return 1; }
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
    my $apiolist = shift;
    my $soc = shift;
    my $eoc = shift;
    my $ilc = shift;
    my $lbrace = shift;
    my $case_sensitive = shift;
    my $lastTreeNode = shift;

    my $skipchildren = shift;
    my $localDebug = 0;

    if (1 && $localDebug) {
	my $apio = $self->apiOwner();
	if ($apio) {
		# if ($apio->name() eq "OSObject") {
		print "DUMPING TREE.\n"; $self->dbprint();
		# }
	}
    }

    my $continue = 1;
    if ($self == $lastTreeNode) {
	$continue = 0;
	print "CONTINUE -> 0\n" if ($localDebug);
    }

    my $token = $self->token();
    my $firstchild = $self->firstchild();
    my $next = $self->next();

    print "APIOprocessEmbeddedTagsRec: TOKEN IS ".$self->token()."\n" if ($localDebug);

    if ((length($soc) && ($token eq $soc)) ||
	(length($ilc) && ($token eq $ilc))) {

	if (($token eq "/*") || ($token eq "//")) {
		print "COMMENT CHECK\n" if ($localDebug);
		if ($firstchild && $firstchild->next()) {
			print "HASNEXT : '".$firstchild->next()->token()."'\n" if ($localDebug);
			if ($firstchild->next()->token() eq "!" && !$self->hidden()) {
				print "NODECHECK: $self\n" if ($localDebug);
				# HDCOMMENT
				my $string = $token.$firstchild->textTree();
				print "FOUND HDCOMMENT:\n$string\nEND HDCOMMENT\n" if ($localDebug);
				# $string =~ s/^\/[\*\/]\!//s;
				# $string =~ s/^\s*//s;
				if ($token eq "/*") {
					$string =~ s/\*\/\s*$//s;
				}
				$string =~ s/^\s*\*\s*//mg;
				my $fieldref = stringToFields($string, $self->filename, $self->linenum);
				print "APIOLIST AT INSERT IS $apiolist\n" if ($localDebug);
				foreach my $owner (@{$apiolist}) {
				    print "X POSSOWNER: $owner\n" if ($localDebug);
				}
				foreach my $owner (@{$apiolist}) {
				    print "POSSOWNER: $owner\n" if ($localDebug);
				    if ($owner && $owner->isAPIOwner()) {
					print "ADDING[1] TO $owner.\n" if ($localDebug);
			    		my $ncurly = $owner->processComment($fieldref, 1, $self->nextTokenNoComments($soc, $ilc, 1), $soc, $ilc);

					# We have found the correct level.  Anything
					# nested deeper than this is bogus (unless we hit a curly brace).
					print "skipochildren -> 1 [1]" if ($localDebug);
					$skipchildren = 1;
					$next = $next->skipcurly($lbrace, $ncurly); # nextTokenNoComments($soc, $ilc, 0);
					if ($localDebug) {
						print "NEXT IS $next (";
						if ($next) {print $next->token(); }
						print ")\n";
					}
				    }
				}
			}
		}
	} else {
		if ($firstchild && $firstchild->next() && $firstchild->next()->next()) {
			my $pos = $firstchild->next();
			my $fcntoken = $pos->token();

			while ($fcntoken =~ /\s/ && $pos) {
				$pos = $pos->next;
				$fcntoken = $pos->token();
			}
			if (($fcntoken eq "/*") || ($fcntoken eq "//")) {
				my $fcnntoken = $firstchild->next()->next()->token();
				if ($fcnntoken eq "!") {
					# HDCOMMENT
					my $string = $fcntoken.$firstchild->textTree();
					print "FOUND HDCOMMENT:\n$string\nEND HDCOMMENT\n" if ($localDebug);
					# my $quotetoken = quote($fcntoken);
					# $string =~ s/^$quotetoken//s;
					# $string =~ s/^\s*\/[\*\/]\!//s;
					# $string =~ s/^\s*//s;
					if ($fcntoken eq "/*") {
						$string =~ s/\*\/\s*$//s;
					}
					$string =~ s/^\s*\*\s*//mg;
					my $fieldref = stringToFields($string, $self->filename, $self->linenum);
					foreach my $owner (@{$apiolist}) {
					    print "POSSOWNER: $owner\n" if ($localDebug);
					    if ($owner && $owner->isAPIOwner()) {
						print "ADDING[2] TO $owner.\n" if ($localDebug);
				    		my $ncurly = $owner->processComment($fieldref, 1, $self->nextTokenNoComments($soc, $ilc, 1), $soc, $ilc);
						print "skipochildren -> 1 [2]" if ($localDebug);
						$skipchildren = 1;
						# skip the current declaration before
						# processing anything else to avoid
						# bogus warnings from nested
						# HeaderDoc comments.
						$next = $next->skipcurly($lbrace, $ncurly); # nextTokenNoComments($soc, $ilc, 0);
						if ($localDebug) {
							print "NEXT IS $next (";
							if ($next) {print $next->token(); }
							print ")\n";
						}
					    }
					}
				}
			}
		}
	}
    }
    # If we get here, we weren't a skipped brace, so we can start nesting again.
    if (length($lbrace) && $token eq $lbrace) {
	print "skipochildren -> 0 [3]" if ($localDebug);
	$skipchildren = 0;
    }

    if ($firstchild && !$skipchildren) {
	print "APIOprocessEmbeddedTagsRec: MAYBE GOING TO CHILDREN\n" if ($localDebug);
	my $nestallowed = commentsNestedIn($token, $soc, $eoc, $ilc, $lbrace, $case_sensitive);

        if ($nestallowed) {
		print "APIOprocessEmbeddedTagsRec: YUP.  CHILDREN.\n" if ($localDebug);
		my $newcontinue = $firstchild->APIOprocessEmbeddedTagsRec($apiolist, $soc, $eoc, $ilc, $lbrace, $case_sensitive, $lastTreeNode, $skipchildren);
		if ($continue) { $continue = $newcontinue; }
		print "Back from Child\n" if ($localDebug);
		print "skipochildren -> $skipchildren [RECURSEOUT]" if ($localDebug);
	}
    }
    if ($next && $continue) {
	print "APIOprocessEmbeddedTagsRec: GOING TO NEXT\n" if ($localDebug);
	$continue = $next->APIOprocessEmbeddedTagsRec($apiolist, $soc, $eoc, $ilc, $lbrace, $case_sensitive, $lastTreeNode, $skipchildren);
	print "Back from Next\n" if ($localDebug);
    }
print "SN: ".$self->next()." (".($self->next() ? $self->next()->token() : "").")\n" if ($localDebug);
print "RECURSEOUT (CONTINUE is $continue)\n" if ($localDebug);
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

    my $localDebug = 0;
    my $oldCurDeclaration = $curDeclaration;
    my $oldsodec = $sodec;
    my $ntoken = $self->nextpeeknc($soc, $ilc);
    my $skipchildren = 0;
    my $oldPHD = $pendingHDcomment;
    my $do_process = 0;

    my $continue = 1;
    my $inBlockDefine = 0;
    my $dropinvalid = 0;
    my $lastsodec = $sodec;
    my $nextsodec = $sodec;

print "PETREC\n" if ($localDebug);

    if (!$self) { return ($eoDeclaration, $pendingHDcomment, $continue); }

    my $apioclass = ref($apio) || $apio;
    if ($apioclass =~ /HeaderDoc::PDefine/) {
	# print "HDPDEF: ISBLOCK: ".$apio->isBlock()." inDefineBlock: ".$apio->inDefineBlock().".\n";
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
	print "CONTINUE -> 0\n" if ($localDebug);
	$continue = 0;
    }

    # print "lastDec: $lastDeclaration\ncurDec: $curDeclaration\neoDec: $eoDeclaration\n" if ($localDebug);

    # Walk the tree.
    my $token = $self->token();
    $curDeclaration .= $token;

    print "TOKEN: $token\n" if ($localDebug);

# if ($token !~ /\s/o) { print "TOKEN: \"$token\" SOC: \"$soc\" ILC: \"$ilc\".\n"; }

    if ($token eq $soc || $token eq $ilc) {
	my $firstchild = $self->firstchild();

	if ($firstchild) {
	print "FCT: ".$firstchild->token()."\n" if ($localDebug);
	  my $nextchild = $firstchild->next();
	  if ($nextchild && $nextchild->token eq "!") {
	      print "Found embedded HeaderDoc markup\n" if ($localDebug);
	      print "NCT: ".$nextchild->token()."\n" if ($localDebug);
		# print "NCT TREE:\n"; $self->printTree(); print "NCT ENDTREE\n";

		print "WILL SET SODEC.  SEARCHING IN:\n" if ($localDebug);
		$self->dbprint() if ($localDebug);
	      $sodec = $self->nextTokenNoComments($soc, $ilc);
		# print "NCT SODECTREE:\n"; $sodec->printTree(); print "NCT ENDTREE\n";
		print "SODEC SET TO $sodec\n" if ($localDebug);
		if ($sodec) {
			$sodec->dbprint() if ($localDebug);
		}

	      my $string = $firstchild->textTree();
	      my $filename = $apio->filename();
	      my $linenum = $apio->linenum();
	      if ($token eq $soc) {
		$string =~ s/$eocquot\s*$//s;
	      }
	      if ($string =~ /^\s*\!/o) {
		      $string =~ s/^\s*\!//so;

		      print "EOD $eoDeclaration NT $ntoken STR $string\n" if ($localDebug);;

		      if (($eoDeclaration || !$ntoken ||
			   $ntoken =~ /[)}]/o || casecmp($ntoken, $rbrace, $case_sensitive)) &&
			  $string !~ /^\s*\@/o) {
			# If we're at the end of a declaration (prior to the
			# following newline) and the declaration starts with
			# a string of text (JavaDoc-style markup), we need to
			# figure out the name of the previous declaration and
			# insert it.

			if (!$eoDeclaration) {
				print "LASTDITCH PROCESSING\n" if ($localDebug);
			} else {
				print "EOD PROCESSING\n" if ($localDebug);
			}

			# Roll back to the previous start of declaration.
			# This comment is at the end of a line or whatever.
			$nextsodec = $sodec;
			$sodec = $lastsodec;

			$string =~ s/^\s*//so;
	
			print "COMMENTSTRING WAS: $string\n" if ($localDebug);
			print "PRE1\n" if ($localDebug);

			print "LAST DECLARATION: $lastDeclaration\n" if ($localDebug);

			print "calling getNameAndFieldTypeFromDeclaration\n" if ($localDebug);
			my ($name, $type) = $self->getNameAndFieldTypeFromDeclaration($lastDeclaration, $apio, $typedefname, $case_sensitive, $keywordhashref);
	
			$string = "$type $name\n$string";
			print "COMMENTSTRING NOW: $string\n" if ($localDebug);
		      } elsif (!$eoDeclaration && (!$ntoken ||
                           $ntoken =~ /[)]/o || casecmp($ntoken, $rbrace, $case_sensitive)) &&
                           $string =~ /^\s*\@/o) {
                           # We have found an embedded headerdoc comment embedded that is
			   # right before a close parenthesis, but which starts with an @ sign.
				my $nlstring = $string;
				$nlstring =~ s/[\n\r]/ /sg;
				warn "$filename:$linenum: warning: Found invalid headerdoc markup: $nlstring\n";
				$dropinvalid = 1;
		      }
		      $string =~ s/^\s*//so;
		      if ($string =~ /^\s*\@/o) {
			print "COMMENTSTRING: $string\n" if ($localDebug);
	
			my $fieldref = stringToFields($string, $filename, $linenum);
		# print "APIO: $apio\n";
			foreach my $owner (@{$apiolist}) {
			    my $copy = $fieldref;
			print "OWNER[1]: $owner\n" if ($localDebug);
			    if ($owner) {
				if (!$inBlockDefine || $do_process == $owner) {
					my @copyarray = @{$copy};
					# print "COPY[1]: ".$copyarray[1]."\n";
					if ($inBlockDefine && !length($copyarray[0])) { $copyarray[1] =~ s/^field .*?\n/discussion /s; $copy = \@copyarray; }
					# print "COPY[1]: ".$copyarray[1]."\n";
			    		$owner->processComment($copy, 1, $sodec, $soc, $ilc);
				}
			    }
			}
# print "APIO: $apio\n";
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
				print "HIDING $self\n" if ($localDebug);
			}
			print "DROP\n" if ($localDebug);
			$curDeclaration = $oldCurDeclaration;
		      }
	      }
	   }
	}
    } elsif ($token =~ /[;,}]/o) {
	print "SETTING LASTDEC TO $curDeclaration\n" if ($localDebug);
	$lastDeclaration = "$curDeclaration\n";
	if ($pendingHDcomment) {
                # If we're at the end of a declaration (prior to the
                # following newline) and the declaration starts with
                # a string of text (JavaDoc-style markup), we need to
                # figure out the name of the previous declaration and
                # insert it.

			print "PRE2\n" if ($localDebug);
		print "calling getNameAndFieldTypeFromDeclaration\n" if ($localDebug);
                my ($name, $type) = $self->getNameAndFieldTypeFromDeclaration($lastDeclaration, $apio, $typedefname, $case_sensitive, $keywordhashref);
                my $string = "$type $name\n$pendingHDcomment";
		my $filename = $apio->filename();
		my $linenum = $apio->linenum();

                my $fieldref = stringToFields($string, $filename, $linenum);
		print "COMMENTSTRING: $string\n" if ($localDebug);
		foreach my $owner (@{$apiolist}) {
			my $copy = $fieldref;
			print "OWNER[2]: $owner\n" if ($localDebug);
			if ($owner) {
			    if (!$inBlockDefine || $do_process == $owner) {
				my @copyarray = @{$copy};
				# print "COPY[1]: ".$copyarray[1]."\n";
				if ($inBlockDefine && !length($copyarray[0])) { $copyarray[1] =~ s/^field .*?\n/discussion /s; $copy = \@copyarray; }
				# print "COPY[1]: ".$copyarray[1]."\n";
				$owner->processComment($copy, 1, $sodec, $soc, $ilc);
			    }
			}
		}
# print "APIO: $apio\n";
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
	my $nestallowed = commentsNestedIn($token, $soc, $eoc, $ilc, $lbrace, $case_sensitive);
	if ($nestallowed == 1) {
		my $newcontinue;
		($eoDeclaration, $pendingHDcomment, $newcontinue) = $firstchild->processEmbeddedTagsRec($xmlmode, $eoDeclaration, $soc, $eoc, $eocquot, $ilc, $lbrace, $rbrace, $typedefname, $case_sensitive, $keywordhashref, "", "", "", $apio, $apiolist, $sodec, $lastTreeNode);
		if ($continue) { $continue = $newcontinue; }
	} else {
		my $newcontinue;
		($eoDeclaration, $pendingHDcomment, $newcontinue) = $firstchild->processEmbeddedTagsRec($xmlmode, $eoDeclaration, $soc, $eoc, $eocquot, $ilc, $lbrace, $rbrace, $typedefname, $case_sensitive, $keywordhashref, "", "$curDeclaration", "", $apio, $apiolist, $sodec, $lastTreeNode);
		if ($continue) { $continue = $newcontinue; }
	}
	$curDeclaration .= textTree($firstchild);
    } elsif ($firstchild && !$skipchildren) {
	$curDeclaration .= textTree($firstchild);
    }

    if ($ntoken) {
	print "NTOKEN: $ntoken\n" if ($localDebug);
    } else {
	print "NTOKEN: (null)\n" if ($localDebug);
    }

    if (!$ntoken || $ntoken =~ /[)]/o || casecmp($ntoken, $rbrace, $case_sensitive)) {
	# Last-ditch chance to process pending comment.
	# This takes care of the edge case where some languages
	# do not require the last item in a struct/record to be
	# terminated by a semicolon or comma.

	if ($ntoken =~ /[)}]/o || casecmp($ntoken, $rbrace, $case_sensitive)) {
		if ($oldCurDeclaration =~ /\S/) {
			print "CLOSEBRACE LASTDITCH: SETTING LASTDEC TO $curDeclaration\n" if ($localDebug);
			$lastDeclaration = $oldCurDeclaration;
		}
	} else {
		if ($oldCurDeclaration =~ /\S/) {
			print "NONCLOSEBRACE LASTDITCH: SETTING LASTDEC TO $curDeclaration\n" if ($localDebug);
			$lastDeclaration = $curDeclaration;
		}
	}
	if ($pendingHDcomment) {
		print "LASTDITCH\n" if ($localDebug);

                # If we're at the end of a declaration (prior to the
                # following newline) and the declaration starts with
                # a string of text (JavaDoc-style markup), we need to
                # figure out the name of the previous declaration and
                # insert it.

			print "PRE3\n" if ($localDebug);
		print "calling getNameAndFieldTypeFromDeclaration\n" if ($localDebug);
                my ($name, $type) = $self->getNameAndFieldTypeFromDeclaration($lastDeclaration, $apio, $typedefname, $case_sensitive, $keywordhashref);
                my $string = "$type $name\n$pendingHDcomment";
		my $filename = $apio->filename();
		my $linenum = $apio->linenum();

                my $fieldref = stringToFields($string, $filename, $linenum);
		print "COMMENTSTRING: $string\n" if ($localDebug);
		foreach my $owner (@{$apiolist}) {
			my $copy = $fieldref;
			print "OWNER[3]: $owner\n" if ($localDebug);
			if ($owner) {
			    if (!$inBlockDefine || $do_process == $owner) {
				my @copyarray = @{$copy};
				# print "COPY[1]: ".$copyarray[1]."\n";
				if ($inBlockDefine && !length($copyarray[0])) { $copyarray[1] =~ s/^field .*?\n/discussion /s; $copy = \@copyarray; }
				# print "COPY[1]: ".$copyarray[1]."\n";
				$owner->processComment($copy, 1, $sodec, $soc, $ilc);
			    }
			}
		}
# print "APIO: $apio\n";
		$apio->{APIREFSETUPDONE} = 0;

		$pendingHDcomment = "";
	}
    }
			# $sodec = $oldsodec;
    if ($next && $continue) {
	($eoDeclaration, $pendingHDcomment, $continue) = $next->processEmbeddedTagsRec($xmlmode, $eoDeclaration, $soc, $eoc, $eocquot, $ilc, $lbrace, $rbrace, $typedefname, $case_sensitive, $keywordhashref, $lastDeclaration, $curDeclaration, $pendingHDcomment, $apio, $apiolist, $sodec, $lastTreeNode);
    }

    return ($eoDeclaration, $pendingHDcomment, $continue);
}

# THIS CODE USED TO PROCESS COMMENTS WHENEVER IT IS TIME.
	      # my $fieldref = stringToFields($string, $filename, $linenum);
	      # $apio->processComment($fieldref, 1, $self, $soc, $ilc);
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

    print "BEGINPRINTTREE\n";
    print $self->textTree();
    print "ENDPRINTTREE\n";
}

sub textTree {
    my $self = shift;
    my $parserState = $self->parserState();
    my $lastnode = undef;

    if ($parserState) {
	$lastnode = $parserState->{lastTreeNode};
    }
    # print "TEXTTREE: LASTNODE: $lastnode\n";
    # if ($lastnode) { print "LASTNODE TEXT: \"".$lastnode->token()."\"\n"; }

    my ($string, $continue) = $self->textTreeSub(0, "", "", $lastnode);
    return $string;
}

sub textTreeNC {
    my $self = shift;
    my $lang = shift;
    my $sublang = shift;
    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macronamesref,
        $classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
	$propname) = parseTokens($lang, $sublang);

    my ($string, $continue) = $self->textTreeSub(1, $soc, $ilc);
    return $string;
}

sub textTreeSub
{
    my $self = shift;
    my $nc = shift;
    my $soc = shift;
    my $ilc = shift;
    my $lastnode = shift;

    my $localDebug = 0;
    my $continue = 1;

    # print "TTSUB: LN: $lastnode SELF: $self\n";

    if ($lastnode == $self) {
	# print "TTSUB: CONTINUE -> 0\n";
	$continue = 0;
    }

    my $string = "";
    my $skip = 0;
    my $token = $self->token();
    if ($nc) {
	if ($localDebug) {
		print "NC\n";
		print "SOC: $soc\n";
		print "ILC: $ilc\n";
		print "TOK: $token\n";
	}
	if (($token eq "$soc") || ($token eq "$ilc")) {
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
		my ($newstring, $newcontinue) = $node->textTreeSub($nc, $soc, $ilc, $lastnode);
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
	my ($newstring, $newcontinue) = $node->textTreeSub($nc, $soc, $ilc, $lastnode);
	$continue = $newcontinue;
	$string .= $newstring;
    }

    return ($string, $continue);
}


sub xmlTree {
    my $self = shift;
    my $apio = $self->apiOwner();

    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macroListRef,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
        $propname) = parseTokens($apio->lang(), $apio->sublang());

    $self->processEmbeddedTags(1, $sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macroListRef);


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
		print $apiOwner->name()."\n" if ($localDebug);
	}

	if ($apiOwner->class() eq "HeaderDoc::Method") {
		$occmethod = 1;
	} else {
		$occmethod = 0;
	}

	# print "APIOWNER was type $apiOwner\n";
    } else {
	$apiOwner = HeaderDoc::HeaderElement->new();
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

    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macroListRef,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
        $propname) = parseTokens($lang, $sublang);

    my ($retvalref, $junka, $junkb, $junkc, $junkd, $junke, $lastTokenType, $spaceSinceLastToken) = $self->colorTreeSub($apiOwner, "", 0, 0, 0, $occmethod, "", $sotemplate, $soc, $eoc, $ilc, $lbrace, $rbrace, $sofunction, $soprocedure, $varname, $constname, $unionname, $structname, $typedefname, $structisbrace, $macroListRef, "", $lang, $sublang, 1, 0, 0, 0, 0, 0, "", "", $lastnode, "", 0);
    my $retval = ${$retvalref};

    # my $retval = "";
    # $retval = $self->textTree();
    # $self->printTree();

    $self->{XMLTREE} = $retval;

    return $retval;
}

sub htmlTree {
    my $self = shift;

    # print "TREE\n";
    # $self->printTree();
    # print "ENDTREE\n";
    my $apiOwner = undef;
    my $lang = undef;
    my $sublang = undef;
    my $occmethod = 0;
    my $localDebug = 0;

    my $debugName = ""; # "TypedefdStructWithCallbacksAndStructs";

    if ($self->{HTMLTREE}) {
	 print "SHORTCUT\n" if ($localDebug);
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
		print $apiOwner->name()."\n" if ($localDebug);
	}

	if ($apiOwner->class() eq "HeaderDoc::Method") {
		$occmethod = 1;
	} else {
		$occmethod = 0;
	}

	print "APIOWNER was type $apiOwner\n" if ($localDebug);
    } else {
	$apiOwner = HeaderDoc::HeaderElement->new();
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

    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macroListRef,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
        $propname) = parseTokens($lang, $sublang);

    $self->processEmbeddedTags(0, $sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macroListRef);

    my ($retvalref, $junka, $junkb, $junkc, $junkd, $junke, $lastTokenType, $spaceSinceLastToken) = $self->colorTreeSub($apiOwner, "", 0, 0, 0, $occmethod, "", $sotemplate, $soc, $eoc, $ilc, $lbrace, $rbrace, $sofunction, $soprocedure, $varname, $constname, $unionname, $structname, $typedefname, $structisbrace, $macroListRef, "", $lang, $sublang, 0, 0, 0, 0, 0, 0, "", "", $lastnode, "", 0);
    my $retval = ${$retvalref};

    # my $retval = "";
    # $retval = $self->textTree();
    # $self->printTree();

    if ($HeaderDoc::align_columns) {
	my @retarr = split(/(\n)/s, $retval);
	my $newret = "";
	foreach my $line (@retarr) {
		my $first = "";
		# print "LINE: $line\n";
		if ($line =~ s/^<tr><td nowrap=\"nowrap\"><nowrap>//s) {
			$first = "<tr><td nowrap=\"nowrap\"><nowrap>";
			# print "FIRST (line = \"$line\")\n";
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
    my $cache = $self->{CPNC};
    if ($cache) { return $cache; }

    my $node = $self->{FIRSTCHILD};

    if (!$node) { return ""; }

    bless($node, "HeaderDoc::ParseTree");

    if (!$node->token()) { return $node->childpeeknc($soc, $ilc) || return $node->nextpeeknc($soc, $ilc); }
    if ($node->token() =~ /\s/o) { return $node->childpeeknc($soc, $ilc) || return $node->nextpeeknc($soc, $ilc); }
    if ($node->token() eq $soc) { return $node->childpeeknc($soc, $ilc) || return $node->nextpeeknc($soc, $ilc); }
    if ($node->token() eq $ilc) { return $node->childpeeknc($soc, $ilc) || return $node->nextpeeknc($soc, $ilc); }

    $cache = $node->token();
    $self->{CPNC} = $cache;

    return $cache;
}

sub nextpeek
{
    my $self = shift;
    my $soc = shift;
    my $ilc = shift;

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
	my $ret = $node->nextpeek($soc, $ilc);
	# $self->{NPCACHE} = $ret;
	return $ret;
    }
    if ($node->hidden()) {
	my $ret = $node->nextpeek($soc, $ilc);
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

    my $node = $self->nextTokenNoComments($soc, $ilc);
    if (!$node) { return ""; }

    return $node->token();

}

sub nextnextpeeknc
{
    my $self = shift;
    my $soc = shift;
    my $ilc = shift;

    my $node = $self->nextTokenNoComments($soc, $ilc);
    if (!$node) { return ""; }

    my $nodeafter = $node->nextTokenNoComments($soc, $ilc);
    if (!$nodeafter) { return ""; }

    return $nodeafter->token();

}

sub nextTokenNoComments
{
    my $self = shift;
    my $soc = shift;
    my $ilc = shift;
    my $failOnHDComments = shift;

    my $localDebug = 0;

    my $cache = $self->{NTNC};
    if ($cache) { return $cache; }

    my $node = $self->{NEXT};

    if (!$node) { return undef }

    bless($node, "HeaderDoc::ParseTree");
# print "SOC: $soc ILC: $ilc\n" if ($colorDebug);

    # print "MAYBE ".$node->token()."\n";

    if ($failOnHDComments) {
	# print "FOHDC\n";
	# print "FC: ".$node->firstchild()."\n";
	if ($node->firstchild() && $node->firstchild()->next()) {
	    # print "POINT 1\n";
	    # first child always empty.
	    my $testnode = $node->firstchild()->next();
	    if ($node->token() eq $ilc) {
	    # print "ILC\n";
		if ($node->token() eq "//") {
			if ($testnode->token() eq "!") {
				print "Unexpected HD Comment\n" if ($localDebug);
				return undef;
			}
		} else {
			if ($testnode->token() eq "//") {
				if ($testnode->next() && ($testnode->next()->token() eq "!")) {
					print "Unexpected HD Comment\n" if ($localDebug);
					return undef;
				}
			}
		}
	    } elsif ($node->token() eq "/*") {
	    # print "SOC\n";
		if ($testnode->token() eq "!") {
			print "Unexpected HD Comment\n" if ($localDebug);
			return undef;
		}
	    # } else {
		# print "TOKEN: ".$node->token()."\n";
	    }
	}
    }

    if (!$node->token()) { return $self->{NTNC} = $node->nextTokenNoComments($soc, $ilc, $failOnHDComments); }
    if ($node->token() =~ /\s/o) { return $self->{NTNC} = $node->nextTokenNoComments($soc, $ilc, $failOnHDComments); }
    if ($node->token() eq $soc) { return $self->{NTNC} = $node->nextTokenNoComments($soc, $ilc, $failOnHDComments); }
    if ($node->token() eq $ilc) { return $self->{NTNC} = $node->nextTokenNoComments($soc, $ilc, $failOnHDComments); }

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
    my $lbrace = shift;
    my $rbrace = shift;
    my $sofunction = shift;
    my $soprocedure = shift;
    my $varname = shift;
    my $constname = shift;
    my $unionname = shift;
    my $structname = shift;
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
    my $continue = 1;

    if ($self == $lastTreeNode) { $continue = 0; }

    my %macroList = %{$macroListRef};
    my $oldLastBrace = $lastBrace;
    my $oldDepth = $depth;
    my $oldInMacro = $inMacro;
    my $oldInQuote = $inQuote;
    my $oldLastKeyword = $lastKeyword;
    my $oldInComment = $inComment;
    my $dropFP = 0;

    # This cache slows things down now that it works....
    # if ($self->{CTSUB}) { return (\$self->{CTSTRING}, $self->{CTSUB}); }
    my $localDebug = 0;
    my $psDebug = 0;
    my $treeDebug = 0;
    my $dropDebug = 0;
    my $tokenDebug = 0;

    if ($xmlmode && $localDebug) {
	print "XMLMODE.\n";
    }

    # foreach my $listitem (@macroList) { print "ML: $listitem\n"; }

    print "IM: $inMacro\n" if ($localDebug);

    my $mustbreak = 0;
    my $nextprespace = "";
    my $string = "";
    my $tailstring = "";
    my $token = $self->{TOKEN};
    my $escapetoken = "";
    my ($case_sensitive, $keywordhashref) = $apio->keywords();
    my $tokennl = 0;
    if ($token =~ /^[\r\n]/o) { $tokennl = 1; }

    # my $ctoken = $self->childpeek($soc, $ilc);
    # print "TK $token\n" if ($colorDebug);
    my $ctoken = $self->childpeeknc($soc, $ilc);
    my $ntoken = $self->nextpeek($soc, $ilc);
    my $ntokennc = $self->nextpeeknc($soc, $ilc);
    my $nntokennc = $self->nextnextpeeknc($soc, $ilc);
    my $tokenType = undef;
    my $drop = 0;
    my $firstCommentToken = 0;
    my $leavingComment = 0;
    my $hidden = ($self->hidden() && !$xmlmode);

    my $begintr = "";
    my $endtr = "";
    my $newtd = "";
    if (!$xmlmode && $HeaderDoc::align_columns) {
	$begintr = "<tr><td nowrap=\"nowrap\"><nowrap>";
	$endtr = "</nowrap></td></tr>";
	$newtd = "</nowrap></td><td nowrap=\"nowrap\"><nowrap>";
    }

    print "TOKEN: $token NTOKEN: $ntoken LASTNSTOKEN: $lastnstoken IC: $inComment\n" if ($treeDebug || $localDebug);
    print "OCC: $inObjCMethod\n" if ($colorDebug || $localDebug);
    print "HIDDEN: $hidden\n" if ($localDebug);

    # last one in each chain prior to a "," or at end of chain is "var"
    # or "parm" (functions)
    print "TK $token NT $ntoken NTNC $ntokennc NNTNC $nntokennc LB: $lastBrace PS: ".length($prespace)."\n" if ($colorDebug);

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

    if ($lang eq "C" && $token eq "enum") {
	my $curname = $apio->name();
	print "NAME: $curname\n" if ($localDebug);
	print "NOW ENUM\n" if ($localDebug);
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
print "SPLITCHAR IS $splitchar\n" if ($localDebug);
    if ($splitchar && ($token eq $splitchar)) { # && ($ntoken !~ /^[\r\n]/o)) {
	print "WILL SPLIT AFTER \"$token\" AND BEFORE \"$ntoken\".\n" if ($localDebug);
	$nextbreakable = 3;
    }

print "SOC: \"$soc\"\nEOC: \"$eoc\"\nILC: \"$ilc\"\nLBRACE: \"$lbrace\"\nRBRACE: \"$rbrace\"\nSOPROC: \"$soprocedure\"\nSOFUNC: \"$sofunction\"\nVAR: \"$varname\"\nSTRUCTNAME: \"$structname\"\nTYPEDEFNAME: \"$typedefname\"\n" if ($tokenDebug);

print "inQuote: $inQuote\noldInQuote: $oldInQuote\ninComment: $inComment\ninMacro: $inMacro\ninEnum: $inEnum\n" if ($localDebug);
print "oldInMacro: $oldInMacro\noldInComment: $oldInComment\n" if ($localDebug);

    # print "TOKEN: $token\n" if ($localDebug);

    if ($inEnum) {
	# If we see this, anything nested below here is clearly not a union.
	if (casecmp($token, $unionname, $case_sensitive)) { $inEnum = 0; };
	if (casecmp($token, $structname, $case_sensitive)) { $inEnum = 0; };
	if (casecmp($token, $typedefname, $case_sensitive)) { $inEnum = 0; };
    }

    my $nw = 0; my $pascal = 0;
    if ($token =~ /\W/) { $nw = 1; }
    if ($lang eq "pascal") { $pascal = 1; }

    if ($lang eq "C" || $lang eq "java" || $pascal ||
		$lang eq "php" || $lang eq "perl" ||
		$lang eq "Csource" || $lang eq "shell") {
	if ($inQuote == 1) {
		print "STRING\n" if ($localDebug);
		$tokenType = "string";
	} elsif ($inQuote == 2) {
		print "CHAR\n" if ($localDebug);
		$tokenType = "char";
	} elsif ($nw && $token eq $soc && $soc ne "") {
	    if (!$hidden) {
		$tokenType = "comment";
		print "COMMENT [1]\n" if ($localDebug);
		if (!$inComment) {
			$inComment = 1;
			$firstCommentToken = 1;
			if ($xmlmode) {
				$string .= "<declaration_comment>";
			} else {
				$string .= "<span class=\"comment\">";
			}
		} else {
			print "nested comment\n" if ($localDebug);
		}
	    }
	} elsif ($nw && ($token eq $ilc) && $ilc ne "") {
	    if (!$hidden) {
		print "ILCOMMENT [1]\n" if ($localDebug);
		$tokenType = "comment";
		if (!$inComment) {
			print "REALILCOMMENT\n" if ($localDebug);
			$inComment = 2;
			$firstCommentToken = 1;
			if ($xmlmode) {
				$string .= "<declaration_comment>";
			} else {
				$string .= "<span class=\"comment\">";
			}
		} else {
			print "nested comment\n" if ($localDebug);
		}
	    }
	} elsif ($nw && $token eq $eoc && $eoc ne "") {
		print "EOCOMMENT [1]\n" if ($localDebug);
		$tokenType = "comment";
		if ($xmlmode) {
			$tailstring .= "</declaration_comment>";
		} else {
			$tailstring = "</span>";
		}
		$leavingComment = 1;
		$inComment = 0;
	} elsif ($tokennl && $ntoken !~ /^[\r\n]/o) {
		if ($inComment == 2) {
			print "EOILCOMMENT [1]\n" if ($localDebug);
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
		} elsif ($inMacro) {
			$mustbreak = 1;
			$newlen = 0;
		} elsif ($inComment) {
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
		# print "NEXT TOKEN IS NLCR\n" if ($localDebug);
		# $breakable = 0;
		# $nextbreakable = 0;
	} elsif ($inComment) {
		print "COMMENT [2:$inComment]\n" if ($localDebug);
		$tokenType = "comment";
		if ($inComment == 1) {
			if ($token =~ /^\s/o && !$tokennl && $ntoken !~ /^\s/o) {
				# Only allow wrapping of multi-line comments.
				# Don't blow in extra newlines at existing ones.
				$breakable = 1;
			}
		}
	} elsif ($inMacro) {
		print "MACRO [IN]\n" if ($localDebug);
		$tokenType = "preprocessor";
	} elsif ($token eq "=") {
		$nextbreakable = 1;
		if ($type eq "pastd") {
			$type = "";
			print "END OF VAR\n" if ($localDebug);
		}
		if ($pascal) { $seenEquals = 1; }
	} elsif ($token eq "-") {
		if ($ntoken =~ /^\d/o) {
			$tokenType = "number";
			print "NUMBER [1]\n" if ($localDebug);
		} else {
			print "TEXT [1]\n" if ($localDebug);
			$tokenType = "";
		}
	} elsif ($token =~ /^\d+$/o || $token =~ /^0x[\dabcdef]+$/io) {
		$tokenType = "number";
		$type = "hexnumber";
		print "\nNUMBER [2]: $token\n" if ($localDebug);
	} elsif (!$nw && casecmp($token, $sofunction, $case_sensitive) || casecmp($token, $soprocedure, $case_sensitive)) {
		$tokenType = "keyword";
		$lastKeyword = $token;
		print "SOFUNC/SOPROC\n" if ($localDebug);
		$type = "funcproc";
		$lastBrace = "(";
		$oldLastBrace = "(";
	} elsif (!$nw && $type eq "funcproc") {
		if ($token =~ /^\;/o) {
			$type = "";
			$nextbreakable = 3;
		}
		print "FUNC/PROC NAME\n" if ($localDebug);
		$tokenType = "function";
	} elsif (!$nw && casecmp($token, $constname, $case_sensitive)) {
		$tokenType = "keyword";
		print "VAR\n" if ($localDebug);
		$type = "pasvar";
	} elsif (!$nw && casecmp($token, $varname, $case_sensitive)) {
		$tokenType = "keyword";
		print "VAR\n" if ($localDebug);
		$type = "pasvar";
	} elsif ($nw && ($type eq "pasvar" || $type eq "pastd") &&
		 ($token =~ /^[\;\:\=]/o)) {
			# NOTE: '=' is handled elsewhere,
			# but it is included above for clarity.
			$type = "";
			print "END OF VAR\n" if ($localDebug);
	} elsif ($type eq "pasvar" || $type eq "pastd") {
		print "VAR NAME\n" if ($localDebug);
		$tokenType = "var";
	} elsif (!$nw && ($pascal) && casecmp($token, $typedefname, $case_sensitive)) {
		# TYPE
		print "TYPE\n" if ($localDebug);
		$tokenType = "keyword";
		$type = "pastd";
	} elsif (!$nw && ($pascal) && casecmp($token, $structname, $case_sensitive)) {
		# RECORD
		print "RECORD\n" if ($localDebug);
		$lastBrace = $token;
		$tokenType = "keyword";
		$type = "pasrec";
	} elsif (!$nw && isKeyword($token, $keywordhashref, $case_sensitive)) {
		$tokenType = "keyword";

		# NOTE: If anybody ever wants "class" to show up colored
		# as a keyword within a template, the next block should be
		# made conditional on a command-line option.  Personally,
		# I find it distracting, hence the addition of these lines.

		if ($lastBrace eq $sotemplate && $sotemplate ne "") {
			$tokenType = "template";
		}

		print "KEYWORD\n" if ($localDebug);
		# $inMacro = $self->isMacro($token, $lang, $sublang);
		# We could have keywords in a macro, so don't set this
		# to zero.  It will get zeroed when we pop a level
		# anyway.  Just set it to 1 if needed.
		if ($case_sensitive) {
			if ($macroList{$token}) {
				print "IN MACRO\n" if ($localDebug);
				$inMacro = 1;
			}
		} else {
		    foreach my $cmpToken (keys %macroList) {
			if (casecmp($token, $cmpToken, $case_sensitive)) {
				$inMacro = 1;
			}
		    }
		}
		print "TOKEN IS $token, IM is now $inMacro\n" if ($localDebug);
		if (casecmp($token, $rbrace, $case_sensitive)) {
			print "PS: ".length($prespace)." -> " if ($psDebug);
			# $prespace = nspaces(4 * ($depth-1));
			$mustbreak = 2;
			print length($prespace)."\n" if ($psDebug);
		}
	} elsif (!$inQuote && !$inComment && isKnownMacroToken($token, \%macroList, $case_sensitive)) {
				print "IN MACRO\n" if ($localDebug);
				$inMacro = 1;
	} elsif (($token eq "*") && ($depth == 1) && ($lastTokenType eq "type")) {
		if (!$spaceSinceLastToken) {
			if ($prespace == "") { $prespace = " "; }
		}
		$nospaceafter = 1;
	} elsif ($ntokennc eq ":" && $inObjCMethod) {
		print "FUNCTION [1]\n" if ($localDebug);
		$tokenType = "function";
	} elsif ($token eq ":" && $inObjCMethod) {
		print "FUNCTION [2]\n" if ($localDebug);
		$tokenType = "function";
	} elsif ($token eq ":" && $ctoken) {
		$depth = $depth - 1; # We'll change it back before the next token.
	} elsif ($ntokennc eq "(" && !$seenEquals) {
		$tokenType = "function";
		print "FUNCTION [3]\n" if ($localDebug);
		if ($nntokennc eq "(") {
			$tokenType = "type";
			$type = "funcptr";
		}
		if ($inObjCMethod) {
			$tokenType = ""; # shouldn't happen 
		}
		if ($token eq "(") { $dropFP = 1; }
	} elsif ($ntokennc eq $lbrace && $lbrace ne "") {
		$tokenType = "type";
		print "TYPE [1]\n" if ($localDebug);

	} elsif ($token eq "(") {
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
		$lastBrace = $token;
		$nextbreakable = 0;
		$breakable = 0;
	} elsif (casecmp($token, $lbrace, $case_sensitive)) {
		$lastBrace = $token;
		$nextbreakable = 3;
		if (!casecmp($ctoken, $rbrace, $case_sensitive)) {
			$nextbreakable = 3;
		}
	} elsif ($token =~ /^\"/o) {
		$inQuote = 1;
	} elsif ($token =~ /^\'/o) {
		$inQuote = 2;
	} elsif ($ntokennc =~ /^(\)|\,|\;)/o || casecmp($ntokennc, $rbrace, $case_sensitive)) {
		# last token
		print "LASTTOKEN\n" if ($localDebug);
		if ($nextbreakable != 3) {
			$nextbreakable = 2;
		}
		if ($lastBrace eq $sotemplate && $sotemplate ne "") {
			$nextbreakable = 0;
		}
		if ($lastBrace eq "(") {
			if ($MIG || $pascal) {
				$tokenType = "type";
				print "TYPE [2]\n" if ($localDebug);
			} else {
				$tokenType = "param";
				print "PARAM [1]\n" if ($localDebug);
			}
		} elsif ($lastBrace eq $sotemplate && $sotemplate ne "") {
			print "TEMPLATE[1]\n" if ($localDebug);
			$tokenType = "template";
		} elsif ($type eq "funcptr") {
			$tokenType = "function";
			print "FUNCTION [1]\n" if ($localDebug);
			$breakable = 0;
			$nextbreakable = 0;
		} else {
			if ($MIG || $pascal) {
				$tokenType = "type";
				print "TYPE [2a]\n" if ($localDebug);
			} else {
				$tokenType = "var";
				print "VAR [1] (LB: $lastBrace)\n" if ($localDebug);
			}
		}
		if (casecmp($ntokennc, $rbrace, $case_sensitive) && $type eq "pasrec") {
			$type = "";
		}
		if ($ntokennc eq ")") {
			$nextbreakable = 0;
			if ($inObjCMethod || ($token eq "*")) {
				print "TYPE [3]\n" if ($localDebug);
				$tokenType = "type";
			}
		}
	} elsif ($prespace ne "" && ($token =~ /^\)/o || casecmp($token, $rbrace, $case_sensitive))) {
		print "PS: ".length($prespace)." -> " if ($psDebug);
		$prespace = nspaces(4 * ($depth-1));
		print length($prespace)."\n" if ($psDebug);
		$mustbreak = 2;
	} elsif (casecmp($token, $rbrace, $case_sensitive)) {
		$prespace = nspaces(4 * ($depth-1));
		print length($prespace)."\n" if ($psDebug);
		$mustbreak = 2;
	} else {
		if ($inObjCMethod) {
			if ($lastBrace eq "(") {
				print "TYPE [4]\n" if ($localDebug);
				$tokenType = "type";
			} else { 
				print "PARAM [2]\n" if ($localDebug);
				$tokenType = "param";
			}
		} elsif ($MIG || $pascal) {
			if ($lastBrace eq "(") {
				print "PARAM [3]\n" if ($localDebug);
				$tokenType = "param";
			}
		} else {
			if ($lastBrace eq $sotemplate && ($sotemplate ne "")) {
				print "TEMPLATE [5]\n" if ($localDebug);
				$tokenType = "template";
			} else {
				print "TYPE [5]\n" if ($localDebug);
				$tokenType = "type";
			}
		}
	}
    } else {
	my $filename = $apio->filename;
	my $linenum = $apio->linenum;
	warn "$filename:$linenum: warning: Unknown language $lang. Not coloring. Please file a bug.\n";
    }

    if ($hidden) {
	$tokenType = "ignore";
	$nextbreakable = 0;
	$mustbreak = 0;
	$breakable = 0;
    }
    if (($ntoken =~ /[,;]/) && ($token =~ /[ \t]/) && !$inComment && !$inMacro && !$inQuote) {
	# print "DROP\n";
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

    if (($ilc ne "") && $ntoken eq $ilc && !$inComment) {
	$breakable = 0; $nextbreakable = 0;
    } elsif (($soc ne "") && $ntoken eq $soc && !$inComment) {
	$breakable = 0; $nextbreakable = 0;
    }
print "NB: $nextbreakable\n" if ($localDebug);

    if ($inObjCMethod) {
	$nextbreakable = 0;
	$breakable = 0;
	$mustbreak = 0;
	if ($ntoken eq ":" && $tokenType eq "function") {
		$breakable = 1;
	}
    }

    if ($type eq "pasrec" && $tokenType eq "") { $tokenType = "var"; }
    else { print "TYPE: $type TT: $tokenType\n" if ($localDebug); }
    print "IM: $inMacro\n" if ($localDebug);

    if (!$inComment && $token =~ /^\s/o && !$tokennl && ($mustbreak || !$newlen)) {
	print "CASEA\n" if ($localDebug);
	print "NL: $newlen TOK: \"$token\" PS: \"$prespace\" NPS: \"$nextprespace\"\n" if ($localDebug);
	print "dropping leading white space\n" if ($localDebug);
	$drop = 1;
    } elsif (!$inComment && $tokennl) {
	print "CASEB\n" if ($localDebug);
	if ($lastnstoken ne $eoc) {
		# Insert a space instead.

		print "dropping newline\n" if ($localDebug);
		$drop = 1;
		$string .= " ";
	} else {
		$mustbreak = 1;
	}
    } elsif ($inComment || $token =~ /^\s/o || ($token =~ /^\W/o && $token ne "*") || !$tokenType) {
	print "CASEC\n" if ($localDebug);
	my $macroTail = "";
	$escapetoken = $apio->textToXML($token);
	print "OLDPS: \"$prespace\" ET=\"$escapetoken\" DROP=$drop\n" if ($localDebug);
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
		print "HD: DROPPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
		$escapetoken = "";
	    } else {
		print "HD BASIC: KEEPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
	    }
	}

	$string .= "$escapetoken$macroTail";
	print "comment: $token\n" if ($localDebug);
    } else {
	print "CASED\n" if ($localDebug);

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
			print "HD: DROPPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
			$fontToken = "";
		    } else {
			print "HD BASIC: KEEPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
			$fontToken = $escapetoken;
		    }
		}
		my $refToken = $apio->genRef($lastKeyword, $escapetoken, $fontToken, $tokenType);

		# Don't add noisy link requests in XML.
		if ($HeaderDoc::add_link_requests && $tokenType =~ /^(function|type|preprocessor)/o && !$xmlmode) {
			$string .= "$prespace$refToken";
		} else {
			$string .= "$prespace$fontToken";
		}
	} else {
		$escapetoken = $apio->textToXML($token);
		if ($tokenType eq "ignore") {
		    if (!$HeaderDoc::dumb_as_dirt) {
			# Drop token.
			print "HD: DROPPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
			$escapetoken = "";
		    } else {
			print "HD BASIC: KEEPING IGNORED TOKEN $escapetoken\n" if ($dropDebug);
		    }
		}
		$string .= "$prespace$escapetoken";
	}
	print "$tokenType: $token\n" if ($localDebug);
    }
    $prespace = $nextprespace;

    if (!$drop) {
	$newlen += length($token);
    }

    print "NL $newlen MDL $HeaderDoc::maxDecLen BK $breakable IM $inMacro\n" if ($localDebug);
    if ($mustbreak ||
		(($newlen > $HeaderDoc::maxDecLen) &&
		    $breakable && !$inMacro && !$hidden)) {
	if ($token =~ /^\s/o || $token eq "") {
		$nextprespace = nspaces(4 * ($depth+(1-$mustbreak)));
		print "PS WILL BE \"$nextprespace\"\n" if ($localDebug);
		$nextbreakable = 3;
	} else {
		print "NEWLEN: $newlen\n" if ($localDebug);
		$newlen = length($token);
		print "NEWLEN [2]: $newlen\n" if ($localDebug);
		print "MB: $mustbreak, DP: $depth\n" if ($localDebug);
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
		print "PS WAS \"$ps\"\n" if ($localDebug);
	}
    }

    if ($token !~ /^\s/o) { $lastnstoken = $token; }

    if ($token !~ /\s/) {
    	$lastTokenType = $tokenType;
	$spaceSinceLastToken = 0;
    } else {
	$spaceSinceLastToken = 1;
    }

    my $newstring = "";
    my $node = $self->{FIRSTCHILD};
    my $newstringref = undef;
    if ($node && $continue) {
	if ($nospaceafter == 1) { $nospaceafter = 0; }
	print "BEGIN CHILDREN\n" if ($localDebug || $colorDebug || $treeDebug);
	bless($node, "HeaderDoc::ParseTree");
	($newstringref, $newlen, $nextbreakable, $prespace, $lastnstoken, $continue, $lastTokenType, $spaceSinceLastToken) = $node->colorTreeSub($apio, $type, $depth + 1, $inComment, $inQuote, $inObjCMethod, $lastBrace, $sotemplate, $soc, $eoc, $ilc, $lbrace, $rbrace, $sofunction, $soprocedure, $varname, $constname, $unionname, $structname, $typedefname, $structisbrace, $macroListRef, $prespace, $lang, $sublang, $xmlmode, $newlen, $nextbreakable, $inMacro, $inEnum, $seenEquals, $lastKeyword, $lastnstoken, $lastTreeNode, $lastTokenType, $spaceSinceLastToken);
	$newstring = ${$newstringref};
	print "END CHILDREN\n" if ($localDebug || $colorDebug || $treeDebug);
    }
    $string .= $newstring; $newstring = "";
    print "SET STRING TO $string\n" if ($localDebug);

    if (($prespace ne "")) {
	# if we inherit a need for prespace from a descendant, it means
	# that the descendant ended with a newline.  We don't want to
	# propagate the extra indentation to the next node, though, so
	# we'll regenerate the value of prespace here.
	$prespace = nspaces(4 * $depth);
    }

    $string .= $tailstring;
    $tailstring = "";
    print "LB $lastBrace -> $oldLastBrace\n" if ($colorDebug || $localDebug);
    $lastBrace = $oldLastBrace;
    $depth = $oldDepth;
    $inMacro = $oldInMacro;
    $lastKeyword = $oldLastKeyword;
    $inComment = $oldInComment;
    $inQuote = $oldInQuote;
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
			$node = $node->next;
			bless($node, "HeaderDoc::ParseTree");
		}
	}
	if ($node) {
		($newstringref, $newlen, $nextbreakable, $prespace, $lastnstoken, $continue, $lastTokenType, $spaceSinceLastToken) = $node->colorTreeSub($apio, $type, $depth, $inComment, $inQuote, $inObjCMethod, $lastBrace, $sotemplate, $soc, $eoc, $ilc, $lbrace, $rbrace, $sofunction, $soprocedure, $varname, $constname, $unionname, $structname, $typedefname, $structisbrace, $macroListRef, $prespace, $lang, $sublang, $xmlmode, $newlen, $nextbreakable, $inMacro, $inEnum, $seenEquals, $lastKeyword, $lastnstoken, $lastTreeNode, $lastTokenType, $spaceSinceLastToken);
		$newstring = ${$newstringref};
	}
    }
    $string .= $newstring;
    print "SET STRING TO $string\n" if ($localDebug);

    # $self->{CTSTRING} = $string;
    # $self->{CTSUB} = ($newlen, $nextbreakable, $prespace, $lastnstoken);
    return (\$string, $newlen, $nextbreakable, $prespace, $lastnstoken, $continue, $lastTokenType, $spaceSinceLastToken);
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
	print "|   ";
	$i--;
      }
      if ($depth) {
	print "+---";
      }
      print $self->token;
      if ($self->token !~ /\n$/) { print "\n"; }
    }

    if ($self == $lastnode) {
	printf("-=-=-=-=-=-=- EODEC -=-=-=-=-=-=-\n");
    }

    if ($self->firstchild()) {
	$self->firstchild()->dbprintrec($depth+1, $lastnode);
    }
    if ($self->next()) {
	$self->next()->dbprintrec($depth, $lastnode);
    }
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
	$self->{LINENUM} = shift;
    }

    return $self->{FILENAME};
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
 
    print "----- ParseTree Object ------\n";
    print "token: $self->{TOKEN}\n";
    print "next: $self->{NEXT}\n";
    print "firstchild: $self->{FIRSTCHILD}\n";
    print "\n";
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
	print "Setting parser state for $self\n" if ($localDebug);
	print "Last token (raw) is $state->{lastTreeNode}\n" if ($localDebug);
	print "Last token (text) is ".$state->{lastTreeNode}->token()."\n" if ($localDebug);
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

    if (open(GCCFILE, ">/tmp/headerdoc-gcctemp-$timestamp.c")) {
	print GCCFILE "#include <inttypes.h>\nmain(){printf(\"%d\\n\", $rawvalue);}\n";
	close(GCCFILE);

	if (open(GCCPIPE, "/usr/bin/gcc /tmp/headerdoc-gcctemp-$timestamp.c -o /tmp/headerdoc-gcctemp-$timestamp >& /dev/null |")) {
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
				print "VALUE: $value\nSUCCESS: $success\n" if ($localDebug);
			} else {
				$success = 0;
			}
		}
	}
	unlink("/tmp/headerdoc-gcctemp-$timestamp.c");
	unlink("/tmp/headerdoc-gcctemp-$timestamp");
    }

    print "RET $success, $value\n" if ($localDebug);
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

    while ($pos && ($pos->hidden != 3)) {
	$pos = $pos->next();
    }

    if ($pos) {
	my $rawvalue = $pos->textTree();
	print "getPTvalue: WE HAVE A WINNER.\n" if ($localDebug);
	print "RAWVALUE IS: $rawvalue\n" if ($localDebug);

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

    print "Disposing of tree\n" if ($localDebug);

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
    my $filename = shift;
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
	# my $filename = $apio->filename();
	warn("$filename:0:Ran off top of stack looking for next node.\n");
	# $nextnode = $matchingnode->next();
	$nextnode = undef;
    }
    return $nextnode;
}

sub DESTROY
{
    my $self = shift;
    my $localDebug = 0;

    print "Destroying $self\n" if ($localDebug);
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

    print "SKIPPING $count curly braces (lbrace = '$lbrace') at POS=$pos\n" if ($localDebug);
    if (!$count) { return $self; }

    while ($pos) {
	my $tok = $pos->token();
	print "TOKEN: '$tok'\n" if ($localDebug);
	if ($tok eq "$lbrace") {
		print "MATCH\n" if ($localDebug);
		if (!--$count) {
			my $next = $pos->next;
			if ($localDebug) {
				print "FOUND ONE.  Next tree is:\n";
				if ($next) {
					$next->dbprint();
				} else {
					print "UNDEFINED!\n";
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

1;
