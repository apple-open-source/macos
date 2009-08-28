#! /usr/bin/perl -w
#
# Class name: HeaderElement
# Synopsis: Root class for Function, Typedef, Constant, etc. -- used by HeaderDoc.
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2009/04/17 21:38:01 $
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

package HeaderDoc::HeaderElement;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash unregisterUID registerUID quote html2xhtml sanitize parseTokens unregister_force_uid_clear dereferenceUIDObject filterHeaderDocTagContents validTag);
use File::Basename;
use strict;
use vars qw($VERSION @ISA);
use POSIX qw(strftime mktime localtime);
use Carp qw(cluck);

$HeaderDoc::HeaderElement::VERSION = '$Revision: 1.56 $';

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
    # cluck("Created header element\n"); # @@@
    
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
    # $self->{ABSTRACT} = undef;
    # $self->{DISCUSSION} = undef;
    # $self->{DECLARATION} = undef;
    # $self->{DECLARATIONINHTML} = undef;
    # $self->{PRIVATEDECLARATION} = undef;
    # $self->{OUTPUTFORMAT} = undef;
    # $self->{FILENAME} = undef;
    # $self->{NAME} = undef;
    # $self->{RAWNAME} = undef;
    $self->{GROUP} = $HeaderDoc::globalGroup;
    $self->{INDEXGROUP} = "";
    # $self->{THROWS} = undef;
    # $self->{XMLTHROWS} = undef;
    # $self->{UPDATED} = undef;
    # $self->{LINKAGESTATE} = undef;
    # $self->{ACCESSCONTROL} = undef;
    $self->{AVAILABILITY} = "";
    $self->{LANG} = $HeaderDoc::lang;
    $self->{SUBLANG} = $HeaderDoc::sublang;
    $self->{SINGLEATTRIBUTES} = ();
    $self->{LONGATTRIBUTES} = ();
    # $self->{ATTRIBUTELISTS} = undef;
    $self->{APIOWNER} = $HeaderDoc::currentClass;
    # $self->{APIUID} = undef;
    # $self->{LINKUID} = undef;
    $self->{ORIGCLASS} = "";
    # $self->{ISTEMPLATE} = 0;
    $self->{VALUE} = "UNKNOWN";
    $self->{RETURNTYPE} = "";
    $self->{TAGGEDPARAMETERS} = ();
    $self->{PARSEDPARAMETERS} = ();
    $self->{CONSTANTS} = ();
    # $self->{LINENUM} = 0;
    $self->{CLASS} = "HeaderDoc::HeaderElement";
    # $self->{CASESENSITIVE} = undef;
    # $self->{KEYWORDHASH} = undef;
    # $self->{MASTERENUM} = 0;
    # $self->{APIREFSETUPDONE} = 0;
    # $self->{TPCDONE} = 0;
    # $self->{NOREGISTERUID} = 0;
    # $self->{SUPPRESSCHILDREN} = 0;
    # $self->{NAMELINE_DISCUSSION} = undef;
}

my %CSS_STYLES = ();

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
	$clone = shift;
    } else {
	$clone = $self->new();
	$clone->{CLASS} = $self->{CLASS};
    }

    # $self->SUPER::clone($clone);

    # now clone stuff specific to header element

    $clone->{ABSTRACT} = $self->{ABSTRACT};
    $clone->{DISCUSSION} = $self->{DISCUSSION};
    $clone->{DECLARATION} = $self->{DECLARATION};
    $clone->{DECLARATIONINHTML} = $self->{DECLARATIONINHTML};
    $clone->{PRIVATEDECLARATION} = $self->{PRIVATEDECLARATION};
    $clone->{OUTPUTFORMAT} = $self->{OUTPUTFORMAT};
    $clone->{FILENAME} = $self->{FILENAME};
    $clone->{FULLPATH} = $self->{FULLPATH};
    $clone->{NAME} = $self->{NAME};
    $clone->{RAWNAME} = $self->{RAWNAME};
    $clone->{GROUP} = $self->{GROUP};
    $clone->{THROWS} = $self->{THROWS};
    $clone->{XMLTHROWS} = $self->{XMLTHROWS};
    $clone->{UPDATED} = $self->{UPDATED};
    $clone->{LINKAGESTATE} = $self->{LINKAGESTATE};
    $clone->{ACCESSCONTROL} = $self->{ACCESSCONTROL};
    $clone->{AVAILABILITY} = $self->{AVAILABILITY};
    $clone->{LANG} = $self->{LANG};
    $clone->{SUBLANG} = $self->{SUBLANG};
    $clone->{SINGLEATTRIBUTES} = $self->{SINGLEATTRIBUTES};
    $clone->{LONGATTRIBUTES} = $self->{LONGATTRIBUTES};
    $clone->{NAMELINE_DISCUSSION} = $self->{NAMELINE_DISCUSSION};
    $clone->{ATTRIBUTELISTS} = $self->{ATTRIBUTELISTS};
    $clone->{APIOWNER} = $self->{APIOWNER};
    $clone->{APIUID} = $self->{APIUID};
    $clone->{LINKUID} = undef; # Don't ever copy this.
    $clone->{ORIGCLASS} = $self->{ORIGCLASS};
    $clone->{ISTEMPLATE} = $self->{ISTEMPLATE};
    $clone->{VALUE} = $self->{VALUE};
    $clone->{RETURNTYPE} = $self->{RETURNTYPE};
    my $ptref = $self->{PARSETREE};
    if ($ptref) {
	bless($ptref, "HeaderDoc::ParseTree");
	$clone->{PARSETREE} = $ptref; # ->clone();
	my $pt = ${$ptref};
	if ($pt) {
		$pt->addAPIOwner($clone);
	}
    }
    $clone->{TAGGEDPARAMETERS} = ();
    if ($self->{TAGGEDPARAMETERS}) {
        my @params = @{$self->{TAGGEDPARAMETERS}};
        foreach my $param (@params) {
	    my $cloneparam = $param->clone();
	    push(@{$clone->{TAGGEDPARAMETERS}}, $cloneparam);
	    $cloneparam->apiOwner($clone);
	}
    }
    $clone->{PARSEDPARAMETERS} = ();
    if ($self->{PARSEDPARAMETERS}) {
        my @params = @{$self->{PARSEDPARAMETERS}};
        foreach my $param (@params) {
	    my $cloneparam = $param->clone();
	    push(@{$clone->{PARSEDPARAMETERS}}, $cloneparam);
	    $cloneparam->apiOwner($clone);
        }
    }
    $clone->{CONSTANTS} = ();
    if ($self->{CONSTANTS}) {
        my @params = @{$self->{CONSTANTS}};
        foreach my $param (@params) {
	    my $cloneparam = $param->clone();
	    push(@{$clone->{CONSTANTS}}, $cloneparam);
	    $cloneparam->apiOwner($clone);
	}
    }

    $clone->{LINENUM} = $self->{LINENUM};
    $clone->{CASESENSITIVE} = $self->{CASESENSITIVE};
    $clone->{KEYWORDHASH} = $self->{KEYWORDHASH};
    $clone->{MASTERENUM} = 0; # clones are never the master # $self->{MASTERENUM};
    $clone->{APIREFSETUPDONE} = 0;
    $clone->{APPLEREFISDOC} = $self->{APPLEREFISDOC};
    # $clone->{NOREGISTERUID} = 0;
    # $clone->{SUPPRESSCHILDREN} = 0;

    return $clone;
}


sub typedefContents {
    my $self = shift;
    if (@_) {
	my $newowner = shift;
	$self->{TYPEDEFCONTENTS} = $newowner;
    }
    return $self->{TYPEDEFCONTENTS};
}

sub origClass {
    my $self = shift;
    if (@_) {
	my $newowner = shift;
	$self->{ORIGCLASS} = $newowner;
    }
    return $self->{ORIGCLASS};
}

sub class {
    my $self = shift;
    return $self->{CLASS};
}

sub constructor_or_destructor {
    my $self = shift;
    my $localDebug = 0;

    if ($self->{CLASS} eq "HeaderDoc::Function") {
	my $apio = $self->apiOwner();
	if (!$apio) {
		print STDERR "MISSING API OWNER\n" if ($localDebug);
		return 0;
	} else {
	    my $apioclass = ref($apio) || $apio;
	    if ($apioclass ne "HeaderDoc::CPPClass") {
		print STDERR "Not in CPP Class\n" if ($localDebug);
		return 0;
	    }
	}
	my $name = $self->rawname();
	print STDERR "NAME: $name : " if ($localDebug);

	if ($name =~ /^~/o) {
		# destructor
		print STDERR "DESTRUCTOR\n" if ($localDebug);
		return 1;
	}
	$name =~ s/^\s*\w+\s*::\s*//so; # remove leading class part, if applicable
	$name =~ s/\s*$//so;

	my $classquotename = quote($apio->name());

	if ($name =~ /^$classquotename$/) {
		print STDERR "CONSTRUCTOR\n" if ($localDebug);
		return 1;
	}
	print STDERR "FUNCTION\n" if ($localDebug);
	return 0;
    } elsif ($self->{CLASS} eq "HeaderDoc::Method") {
	# @@@ DAG: If ObjC methods ever get any syntactically-special
	# constructors or destructors, add the checks here.
	return 0;
    } else {
	return 0;
    }
}

sub constants {
    my $self = shift;
    if (@_) { 
        @{ $self->{CONSTANTS} } = @_;
    }
    # foreach my $const (@{ $self->{CONSTANTS}}) {print STDERR $const->name()."\n";}
    ($self->{CONSTANTS}) ? return @{ $self->{CONSTANTS} } : return ();
}

sub masterEnum {
    my $self = shift;
    if (@_) {
	my $masterenum = shift;
	$self->{MASTERENUM} = $masterenum;
    }
    return $self->{MASTERENUM};
}

sub addConstant {
    my $self = shift;
    if (@_) { 
	foreach my $item (@_) {
        	push (@{$self->{CONSTANTS}}, $item);
	}
    }
    return @{ $self->{CONSTANTS} };
}

sub addToFields {
    my $self = shift;
    if (@_) { 
        push (@{$self->{FIELDS}}, @_);
    }
    return @{ $self->{FIELDS} };
}

sub isTemplate {
    my $self = shift;
    if (@_) {
        $self->{ISTEMPLATE} = shift;
    }
    return $self->{ISTEMPLATE};
}

sub isCallback {
    my $self = shift;
    if (@_) {
        $self->{ISCALLBACK} = shift;
    }
    if ($self->can("type")) {
	if ($self->type() eq "callback") { return 1; }
    }
    return $self->{ISCALLBACK};
}

sub isAPIOwner {
    return 0;
}

# /*! @function inheritDoc
#     @abstract Parent discussion for inheritance
#     @discussion We don't want to show this, so we can't use an
#        attribute.  This is private.
#  */
sub inheritDoc {
    my $self = shift;

    if (@_) {
        my $inheritDoc = shift;
        $self->{INHERITDOC} = $inheritDoc;
    }
    return $self->{INHERITDOC};
}

# /*! @function linenuminblock
#     @abstract line number where a declaration began relative to the block
#     @discussion We don't want to show this, so we can't use an
#        attribute.  This is private.
#  */
sub linenuminblock
{
    my $self = shift;

    if (@_) {
        my $linenum = shift;
        $self->{RAWLINENUM} = $linenum;
    }
    return $self->{RAWLINENUM};
}


# /*! @function blockoffset
#     @abstract line number of the start of the block containing a declaration
#     @discussion We don't want to show this, so we can't use an
#        attribute.  This is private.
#  */
sub blockoffset
{
    my $self = shift;
    # my $localDebug = 0;

    if (@_) {
        my $linenum = shift;
        $self->{BLOCKOFFSET} = $linenum;

	# cluck "For object ".$self->name()." set blockoffset to $linenum.\n" if ($localDebug);
    }
    return $self->{BLOCKOFFSET};
}


# /*! @function linenum
#     @abstract line number where a declaration began
#     @discussion We don't want to show this, so we can't use an
#        attribute.  This is private.
#
#        This uses linenuminblock and blockoffset to get the values.
#        Setting this attribute is no longer supported.
#  */
sub linenum {
    my $self = shift;

    if (@_) {
        my $linenum = shift;
        $self->{LINENUM} = $linenum;
    }
    # return $self->{LINENUM};
    return $self->{RAWLINENUM} + $self->{BLOCKOFFSET};
}

# /*! @function value
#     @abstract Value for constants, variables, etc.
#     @discussion We don't want to show this, so we can't use an
#        attribute.  This is private.
#  */
sub value {
    my $self = shift;

    if (@_) {
        my $value = shift;
        $self->{VALUE} = $value;
    }
    return $self->{VALUE};
}

sub outputformat {
    my $self = shift;

    if (@_) {
        my $outputformat = shift;
        $self->{OUTPUTFORMAT} = $outputformat;
    } else {
    	my $o = $self->{OUTPUTFORMAT};
		return $o;
	}
}

sub use_stdout {
    my $self = shift;

    if (@_) {
        my $use_stdout = shift;
        $self->{USESTDOUT} = $use_stdout;
    } else {
    	my $o = $self->{USESTDOUT};
		return $o;
	}
}

sub functionContents {
    my $self = shift;

    if (@_) {
	my $string = shift;
        $self->{FUNCTIONCONTENTS} = $string;
	# cluck("SET CONTENTS OF $self TO $string\n");
    }
    return $self->{FUNCTIONCONTENTS};
}

sub fullpath {
    my $self = shift;

    if (@_) {
        my $fullpath = shift;
        $self->{FULLPATH} = $fullpath;
    } else {
    	my $n = $self->{FULLPATH};
		return $n;
	}
}

sub filename {
    my $self = shift;

    if (@_) {
        my $filename = shift;
        $self->{FILENAME} = $filename;
    } else {
    	my $n = $self->{FILENAME};
		return $n;
	}
}

sub firstconstname {
    my $self = shift;
    my $localDebug = 0;

    my($class) = ref($self) || $self;

    print STDERR "$class\n" if ($localDebug);

    if (@_) {
        my $name = shift;
	print STDERR "Set FIRSTCONSTNAME to $name\n" if ($localDebug);
	$self->{FIRSTCONSTNAME} = $name;
    }
    return $self->{FIRSTCONSTNAME};
}

sub name {
    my $self = shift;
    my $localDebug = 0;

    # cluck("namebacktrace\n");

    my($class) = ref($self) || $self;

    print STDERR "$class\n" if ($localDebug);

    if (@_) {
        my $name = shift;

	# cluck("name set to $name\n");

	my $oldname = $self->{NAME};
	# cluck("namebacktrace: set to \"$name\", prev was \"$oldname\".\n");
	my $fullpath = $self->fullpath();
	my $linenum = $self->linenum();
	my $class = ref($self) || $self;

	print STDERR "NAMESET: $self -> $name\n" if ($localDebug);

	if (!($class eq "HeaderDoc::Header") && ($oldname && length($oldname))) {
		# Don't warn for headers, as they always change if you add
		# text after @header.  Also, don't warn if the new name
		# contains the entire old name, to avoid warnings for
		# multiword names.  Otherwise, warn the user because somebody
		# probably put multiple @function tags in the same comment
		# block or similar....

		my $nsoname = $oldname;
		my $nsname = $name;
		if ($class =~ /^HeaderDoc::ObjC/) {
			$nsname =~ s/\s//g;
			$nsoname =~ s/\s//g;
		} elsif ($class =~ /^HeaderDoc::Method/) {
			$nsname =~ s/:$//g;
			$nsoname =~ s/:$//g;
		# } else {
			# warn("CLASS: $class\n");
		}
		my $qnsoname = quote($nsoname);

		if ($nsname !~ /$qnsoname/) {
			if (!$HeaderDoc::ignore_apiuid_errors) {
				warn("$fullpath:$linenum: warning: Name being changed ($oldname -> $name)\n");
			}
		} elsif (($class eq "HeaderDoc::CPPClass" || $class =~ /^HeaderDoc::ObjC/o) && $name =~ /:/o) {
			warn("$fullpath:$linenum: warning: Class name contains colon, which is probably not what you want.\n");
		}
	}

	$name =~ s/\n$//sgo;
	$name =~ s/\s$//sgo;

        $self->{NAME} = $name;
    }

    my $n = $self->{NAME};

    # Append the rest of the name line if necessary.
    if ($self->{DISCUSSION_SET}) {
	# print STDERR "DISCUSSION IS: ".$self->{DISCUSSION}."\n";
	# print STDERR "ISAPIO: ".$self->isAPIOwner()."\n";
	# print STDERR "ISFW: ".$self->isFramework()."\n";
	if ((!$HeaderDoc::ignore_apiowner_names) || (!$self->isAPIOwner() && ($HeaderDoc::ignore_apiowner_names != 2)) || $self->isFramework()) {
		print STDERR "NAMELINE DISCUSSION for $self CONCATENATED (".$self->{NAMELINE_DISCUSSION}.")\n" if ($localDebug);
		print STDERR "ORIGINAL NAME WAS \"$n\"\n" if ($localDebug);
		if (length($self->{NAMELINE_DISCUSSION})) {
			$n .= " ".$self->{NAMELINE_DISCUSSION};
		}
	}
    }

    if ($class eq "HeaderDoc::Function" && $self->conflict()) {
	$n .= "(";
	$n .= $self->getParamSignature(1);
	$n .= ")";
    }

    # If there's nothing to return, try returning the first constant in the case of enums.
    if ($n !~ /\S/) {
	$n = $self->firstconstname();
    }

    return $n;
}

sub seeDupCheck {
    my $self = shift;
    my $name = shift;
    my $set = 0;
    if (@_) {
	$set = shift;
    }

    my %dupcheck = ();
    if ($self->{SEEALSODUPCHECK}) {
	%dupcheck = %{$self->{SEEALSODUPCHECK}};
    }

    my $retval = $dupcheck{$name};

    if ($set) {
	$dupcheck{$name} = $name;
    }

    $self->{SEEALSODUPCHECK} = \%dupcheck;

    return $retval;
}

# /*! @function see
#     @abstract Add see/seealso (JavaDoc compatibility enhancement)
#  */
sub see {
    my $self = shift;
    my $liststring = shift;
    my $type = "See";
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();    

    my $dupDebug = 0;

    print STDERR "see called\n" if ($dupDebug);

#    $liststring =~ s/<br>/\n/sg; # WARNING \n would break attributelist!

    # Is it a see or seealso?

    if ($liststring =~ s/^seealso\s+//iso) {
	$type = "See Also";
    } else {
	$liststring =~ s/^see\s+//iso;
    }
    # $liststring =~ s/(\n|\r|\<br\>|\s)+$//sgi;
    my @items = split(/[\n\r]/, $liststring);

# print STDERR "LS: $liststring\n";
    foreach my $item (@items) {
      if ($item !~ /^\/\/\w+\// && $item =~ /\S/) { ## API anchor (apple_ref or other)
	my @list = split(/\s+/, $item, 2);

	# Generate it with its own name as the title.  We're just going
	# to rip it back apart anyway.
	my $see = $list[0];
	my $name = $list[1];
	my $apiref = $self->genRef("", $see, $see);

	print STDERR "Checking \"$see\" for duplicates.\n" if ($dupDebug);
	if (!$self->seeDupCheck($see)) {
		my $apiuid = $apiref;
		$name =~ s/\s+/ /sg;
		$name =~ s/\s+$//sg;
		$apiuid =~ s/^<!--\s*a\s+logicalPath\s*=\s*\"//so;
		$apiuid =~ s/"\s*-->\s*\Q$see\E\s*<!--\s*\/a\s*-->//s;
		$self->attributelist($type, $name."\n$apiuid");
		$self->seeDupCheck($see, 1);
		print STDERR "Not found.  Adding.\n" if ($dupDebug);
	} else {
		print STDERR "Omitting duplicate \"$see\"[1]\n" if ($dupDebug);
	}
      } elsif ($item =~ /\S/) {
	$item =~ s/^\s*//s;
	$item =~ s/\s+/ /sg;
	my @parts = split(/\s+/, $item, 2);
	my $name = $parts[1];
	$name =~ s/\s+$//sg;

	print STDERR "Checking \"$name\" for duplicates.\n" if ($dupDebug);
	if (!$self->seeDupCheck($name)) {
		# print STDERR "$type -> '".$name."' -> '".$parts[0]."'\n";

        	$self->attributelist($type, $name."\n".$parts[0]);
		$self->seeDupCheck($name, 1);
		print STDERR "Not found.  Adding.\n" if ($dupDebug);
	} else {
		print STDERR "Omitting duplicate \"$name\"[2]\n" if ($dupDebug);
	}
      }
    }

}

sub mediumrarename {
    my $self = shift;
    return $self->{NAME};
}

sub rawname {
    my $self = shift;
    my $localDebug = 0;

    if (@_) {
	my $name = shift;
	$self->{RAWNAME} = $name;
	print STDERR "RAWNAME: $name\n" if ($localDebug);
    }

    my $n = $self->{RAWNAME};
    if (!($n) || !length($n)) {
	$n = $self->{NAME};
    }


    return $n;
}

sub group {
    my $self = shift;

    if (@_) {
        my $group = shift;
        $self->{GROUP} = $group;
	if (!$self->isAPIOwner()) {
		# cluck("SELF: $self\nAPIO: ".$self->apiOwner()."\n");
		# $self->dbprint();
		$self->apiOwner()->addGroup($group);
		$self->apiOwner()->addToGroup($group, $self);
	}
    } else {
    	my $n = $self->{GROUP};
		return $n;
	}
}

# /*! @function attribute
#     @abstract This function adds an attribute for a class or header.
#     @param name The name of the attribute to be added
#     @param attribute The contents of the attribute
#     @param long 0 for single line, 1 for multi-line.
#  */
sub attribute {
    my $self = shift;
    my $name = shift;
    my $attribute = shift;
    my $long = shift;
    my $programmatic = 0;
    if (@_) {
	$programmatic = shift;
    }
    my $localDebug = 0;

    cluck("Attribute added:\nNAME => $name\nATTRIBUTE => $attribute\nLONG => $long\nPROGRAMMATIC => $programmatic\n") if ($localDebug);

    my %attlist = ();
    if ($long) {
        if ($self->{LONGATTRIBUTES}) {
	    %attlist = %{$self->{LONGATTRIBUTES}};
        }
    } else {
        if ($self->{SINGLEATTRIBUTES}) {
	    %attlist = %{$self->{SINGLEATTRIBUTES}};
        }
	$attribute =~ s/\n/ /sgo;
	$attribute =~ s/^\s*//so;
	$attribute =~ s/\s*$//so;
    }

    if ($programmatic || !$long) {
	$attlist{$name}=$attribute;
    } else {
	$attlist{$name}=filterHeaderDocTagContents($attribute);
    }

    if ($long) {
        $self->{LONGATTRIBUTES} = \%attlist;
    } else {
        $self->{SINGLEATTRIBUTES} = \%attlist;
    }

    my $temp = $self->getAttributes(2);
    print STDERR "Attributes: $temp\n" if ($localDebug);

}

#/*! @function getAttributes
#    @param long 0 for short only, 1 for long only, 2 for both
# */
sub getAttributes
{
    my $self = shift;
    my $long = shift;
    my %attlist = ();
    my $localDebug = 0;
    my $xml = 0;
    my $newTOC = $HeaderDoc::newTOC;

    my $class = ref($self) || $self;
    my $uid = $self->apiuid();

    # Only use this style for API Owners.
    if (!$self->isAPIOwner()) { $newTOC = 0; }

    my $apiowner = $self->apiOwner();
    if ($apiowner->outputformat() eq "hdxml") { $xml = 1; }
    my $first = 1;

    my $declaredin = $self->declaredIn();
	# print STDERR "DECLARED IN: $declaredin\n";

    my $retval = "";
    if ($long != 1) {
        if ($self->{SINGLEATTRIBUTES}) {
	    %attlist = %{$self->{SINGLEATTRIBUTES}};
        }

        foreach my $key (sort strcasecmp keys %attlist) {
	    my $keyname = $key; # printed name.
	    if ($key eq "Superclass" && ($HeaderDoc::superclassName =~ /\S/)) {
		$keyname = $HeaderDoc::superclassName;
	    }
	    print STDERR "KEY NAME CHANGED TO \"$keyname\"\n" if ($localDebug);

	    my $value = $attlist{$key};
	    my $newatt = $value;
	    if (($key eq "Superclass" || $key eq "Extends&nbsp;Class" || $key eq "Extends&nbsp;Protocol" || $key eq "Conforms&nbsp;to" || $key eq "Implements&nbsp;Class") && !$xml) {
		my $classtype = "class";
		my $selfclass = ref($self) || $self;
		if ($selfclass =~ /HeaderDoc::ObjC/) {
			if ($key eq "Conforms&nbsp;to" || $key eq "Extends&nbsp;Protocol") {
				$classtype = "protocol";
			}
		}
		my @valparts = split(/\cA/, $value);
		$newatt = "";
		# print STDERR "CLASSTYPE: $classtype\n";
		foreach my $valpart (@valparts) {
			# print STDERR "VALPART: $valpart\n";
			if (length($valpart)) {
				$valpart =~ s/^\s*//s;
				if ($valpart =~ /^(\w+)(\W.*)$/) {
					$newatt .= $self->genRef("$classtype", $1, $1).$self->textToXML($2).", ";
				} else {
					$newatt .= $self->genRef("$classtype", $valpart, $valpart).", ";
				}
			}
		}
		$newatt =~ s/, $//s;
	    } elsif ($key eq "Framework Path") {
		$newatt = "<!-- headerDoc=frameworkpath;uid=".$uid.";name=start -->\n$value\n<!-- headerDoc=frameworkpath;uid=".$uid.";name=end -->\n";
	    } elsif ($key eq "Requested Copyright") {
		$newatt = "<!-- headerDoc=frameworkcopyright;uid=".$uid.";name=start -->\n$value\n<!-- headerDoc=frameworkpath;uid=".$uid.";name=end -->\n";
	    } elsif ($key eq "Requested UID") {
		$newatt = "<!-- headerDoc=frameworkuid;uid=".$uid.";name=start -->\n$value\n<!-- headerDoc=frameworkuid;uid=".$uid.";name=end -->\n";
	    } else {
		print STDERR "KEY: $key\n" if ($localDebug);
	    }
	    if ($xml) {
		$retval .= "<attribute><name>$keyname</name><value>$newatt</value></attribute>\n";
	    } else {
		if ($newTOC) {
			if ($first) { $retval .= "<div class=\"spec_sheet_info_box\"><table cellspacing=\"0\" class=\"specbox\">\n"; $first = 0; }
			$retval .= "<tr><td scope=\"row\"><b>$keyname:</b></td><td><div style=\"margin-bottom:1px\"><div class=\"content_text\">$newatt</div></div></td></tr>\n";
		} else {
			$retval .= "<b>$keyname:</b> $newatt<br>\n";
		}
	    }
        }
	if ($declaredin) {
	    if (!$xml) {
		if ($newTOC) {
			if ($first) { $retval .= "<div class=\"spec_sheet_info_box\"><table cellspacing=\"0\" class=\"specbox\">\n"; $first = 0; }
			$retval .= "<tr><td scope=\"row\"><b>Declared In:</b></td><td><div style=\"margin-bottom:1px\"><div class=\"content_text\">$declaredin</div></div></td></tr>\n";
		} else {
			$retval .= "<b>Declared In:</b> $declaredin<br>\n";
		}
	    }
	# cluck("Backtrace\n");
	# warn "DECLAREDIN: $declaredin\n";
	# warn "RV: $retval\n";
	}
    }

    if ($long != 0) {
        if ($self->{LONGATTRIBUTES}) {
	    %attlist = %{$self->{LONGATTRIBUTES}};
        }

        foreach my $key (sort strcasecmp keys %attlist) {
	    my $value = $attlist{$key};
	    if ($xml) {
		$retval .= "<longattribute><name>$key</name><value>$value</value></longattribute>\n";
	    } else {
		if ($newTOC) {
			if ($first) { $retval .= "<div class=\"spec_sheet_info_box\"><table cellspacing=\"0\" class=\"specbox\">\n"; $first = 0; }
			$retval .= "<tr><td scope=\"row\"><b>$key:</b></td><td><div style=\"margin-bottom:1px\"><div class=\"content_text\">$value</div></div></td></tr>\n";
		} else {
			$retval .= "<b>$key:</b>\n\n<p>$value<p>\n";
		}
	    }
        }
    }
    if ($newTOC && !$first) { $retval .= "</table></div>\n"; }
    elsif (!$newTOC) { $retval = "<p>$retval</p>"; } # libxml parser quirk workaround

    return $retval;
}

sub checkShortLongAttributes
{
    my $self = shift;
    my $name = shift;
    my $localDebug = 0;

    my %singleatts = ();
    if ($self->{SINGLEATTRIBUTES}) {
	%singleatts = %{$self->{SINGLEATTRIBUTES}};
    }
    my %longatts = ();
    if ($self->{LONGATTRIBUTES}) {
	%longatts = %{$self->{LONGATTRIBUTES}};
    }

    my $value = $singleatts{$name};
    if ($value && length($value)) {return $value;}

    $value = $longatts{$name};
    if ($value && length($value)) {return $value;}

    return 0;
}

sub checkAttributeLists
{
    my $self = shift;
    my $name = shift;
    my $localDebug = 0;

    my %attlists = ();
    if ($self->{ATTRIBUTELISTS}) {
	%attlists = %{$self->{ATTRIBUTELISTS}};
    }

    # print STDERR "list\n";
    my $retval = "";

    my $value = $attlists{$name};
    if ($value) { return $value; }

    return 0;
}

sub getAttributeLists
{
    my $self = shift;
    my $composite = shift;
    my $localDebug = 0;
    my $xml = 0;
    my $newTOC = $HeaderDoc::newTOC;

    my $uid = $self->apiuid();

    my $isFramework = 0;
    if ($self->can('isFramework') && $self->isFramework()) {
	$isFramework = 1;
    }

    # Only use this style for API Owners.
    if (!$self->isAPIOwner()) { $newTOC = 0; }

    my $apiowner = $self->apiOwner();
    if ($apiowner->outputformat() eq "hdxml") { $xml = 1; }

    my %attlists = ();
    if ($self->{ATTRIBUTELISTS}) {
	%attlists = %{$self->{ATTRIBUTELISTS}};
    }

    # print STDERR "list\n";
    my $retval = "";
    my $first = 1;
    foreach my $key (sort strcasecmp keys %attlists) {
	my $prefix = "";
	my $suffix = "";

	if ($isFramework && ($key eq "See" || $key eq "See Also")) {
		$prefix = "<!-- headerDoc=frameworkrelated;uid=".$uid.";name=start -->\n";
		$suffix = "\n<!-- headerDoc=frameworkrelated;uid=".$uid.";name=end -->\n";
	}

	if ($xml) {
	    $retval .= "<listattribute><name>$key</name><list>\n";
	} else {
	    $retval .= $prefix;
	    if ($newTOC) {
		if ($first) { $retval .= "<div class=\"spec_sheet_info_box\"><table cellspacing=\"0\" class=\"specbox\">\n"; $first = 0; }
		$retval .= "<tr><td scope=\"row\"><b>$key:</b></td><td><div style=\"margin-bottom:1px\"><div class=\"content_text\"><dl>\n";
	    } else {
		$retval .= "<b>$key:</b><div class='list_indent'><dl>\n";
	    }
	}
	print STDERR "key $key\n" if ($localDebug);
	my @list = @{$attlists{$key}};
	foreach my $item (@list) {
	    if ($item !~ /\S/s) { next; }
	    print STDERR "item: $item\n" if ($localDebug);
	    my ($name, $disc, $namedisc) = &getAPINameAndDisc($item);

	    if ($key eq "Included Defines") {
		# @@@ CHECK SIGNATURE
		my $apiref = $self->apiref($composite, "macro", $name);
		$name .= "$apiref";
	    }
	    if (($key eq "See Also" || $key eq "See") && !$xml) {
		$disc =~ s/^(\s|<br>|<p>)+//sgio;
		$disc =~ s/(\s|<br>|<\/p>)+$//sgio;
		$name =~ s/\cD/ /sgo;
		$name = "<!-- a logicalPath=\"$disc\" -->$name<!-- /a -->";
		$disc = "";
	    }
	    if ($xml) {
		$retval .= "<item><name>$name</name><value>$disc</value></item>";
	    } else {
		$retval .= "<dt>$name</dt><dd>$disc</dd>";
	    }
	}
	if ($xml) {
	    $retval .= "</list></listattribute>\n";
	} else {
	    if ($newTOC) {
		$retval .= "</dl></div></div></td></tr>\n";
	    } else {
		$retval .= "</dl></div>\n";
	    }
	    $retval .= $suffix;
	}
    }
    if ($newTOC) {
	if (!$first) { $retval .= "</table></div>\n"; }
    }
    # print STDERR "done\n";
    return $retval;
}

# /*! @function attributelist
#     @abstract Add an attribute list.
#     @param name The name of the list
#     @param attribute
#          A string in the form "term description..."
#          containing a term and description to be inserted
#          into the list named by name.
#  */
sub attributelist {
    my $self = shift;
    my $name = shift;
    my $attribute = shift;

    my %attlists = ();

    # cluck "Add attributelist MAPPING $name -> $attribute\n";

    if ($self->{ATTRIBUTELISTS}) {
        %attlists = %{$self->{ATTRIBUTELISTS}};
    }

    my @list = ();
    my $listref = $attlists{$name};
    if ($listref) {
	@list = @{$listref};
    }
    push(@list, filterHeaderDocTagContents($attribute));

    $attlists{$name}=\@list;
    $self->{ATTRIBUTELISTS} = \%attlists;
    # print STDERR "AL = $self->{ATTRIBUTELISTS}\n";

    # print STDERR $self->getAttributeLists()."\n";
}

sub apiOwner {
    my $self = shift;
    if (@_) {
	my $temp = shift;
	$self->{APIOWNER} = $temp;
    }
    return $self->{APIOWNER};
}

# use Devel::Peek;

# /*! Generates the API ref (apple_ref) for a function, data type, etc. */
sub apiref {
    my $self = shift;
    # print STDERR "IY0\n"; Dump($self);
    my $filename = $self->filename();
    my $linenum = $self->linenum();
    my $composite = shift;
    my $args = 0;
    my $type = "";
    my $apiowner = $self->apiOwner();
    # print STDERR "IY0a\n"; Dump($self);
    my $owningclass = ref($apiowner) || $self;
    my $paramSignature = "";
    # print STDERR "IY1\n"; Dump($self);

    if (@_) {
      $args = 1;
      $type = shift;
      if (@_) {
	$paramSignature = shift;
      }
    } else {
	my $uid = $self->apiuid();
	if ($uid =~ /\/\/.*?\/.*?\/(.*?)\//o) {
		$type = $1;
	}
    }

# print STDERR "DEBUG: SELF: $self NAME: ".$self->name()." FILENAME: $filename LINENUM: $linenum\n";
# print STDERR "DEBUG: COMPOSITE: $composite PARAMSIGNATURE: $paramSignature TYPE: $type\n";

    # Don't provide API refs for inherited data or functions.
    my $forceuid = "";
    if ($self->origClass() ne "") {
	$forceuid = $self->generateLinkUID($composite);
    }

    # we sanitize things now.
    # if ($paramSignature =~ /[ <>\s\n\r]/o) {
	# my $fullpath = $self->fullpath();
	# warn("$fullpath:$linenum:apiref: bad signature \"$paramSignature\".  Dropping ref.\n");
	# return "";
    # }

    # print STDERR "IY3\n"; Dump($self);
    my $uid = "";
    if ($args && !$forceuid) {
      # Do this first to assign a UID, even if we're doing the composite page.
      $uid = $self->apiuid($type, $paramSignature);
    } else {
      $uid = $self->apiuid();
    }
    # print STDERR "IY4\n"; Dump($self);

# print STDERR "COMPO: $composite CAC: ".$HeaderDoc::ClassAsComposite."\n";

    if ($composite && !$HeaderDoc::ClassAsComposite) {
	$uid = $self->compositePageUID();
    } elsif (!$composite && $HeaderDoc::ClassAsComposite) {
	# The composite page is the master, so give the individual
	# pages composite page UIDs.  These never get generated
	# anyway.
	$uid = $self->compositePageUID();
    }
    if ($forceuid) { $uid = $forceuid; }
    # print STDERR "IY5\n"; Dump($self);

    my $ret = "";
    if (length($uid)) {
	my $name = $self->name();
	if ($self->can("rawname")) {
		if (!$self->{DISCUSSION} || !$self->{NAMELINE_DISCUSSION}) {
			$name = $self->rawname();
		}
	}
	my $extendedname = $name;
	# print STDERR "NAME: ".$self->name()."\n";
	# print STDERR "RAWNAME: ".$self->rawname()."\n";
	if ($owningclass ne "HeaderDoc::Header" && $self->sublang() ne "C") {
		# Don't do this for COM interfaces and C pseudoclasses
		$extendedname = $apiowner->rawname() . "::" . $name;
	}
	# $extendedname =~ s/\s//sgo;
	$extendedname =~ s/<.*?>//sgo;
        $extendedname =~ s/;//sgo;
	my $uidstring = "";
	my $indexgroup = $self->indexgroup();
	if (length($uid)) { $uidstring = " uid=$uid; "; }
	if (length($indexgroup)) { $uidstring .= " indexgroup=$indexgroup; "; }

	# if ($type eq "") {
		# cluck("empty type field\n");
	# }

	my $fwshortname = "";
	if ($self->isFramework()) {
		$fwshortname = $self->filename();
		$fwshortname =~ s/\.hdoc$//so;
		$fwshortname = sanitize($fwshortname, 1);
		$fwshortname = "shortname=$fwshortname;";
	}
	$ret .= "<!-- headerDoc=$type; $uidstring $fwshortname name=$extendedname -->\n";
	if (length($uid)) { $ret .= "<a name=\"$uid\"></a>\n"; }
    }
    # print STDERR "IY8\n"; Dump($self);
    $owningclass = undef;
    # print STDERR "IY9\n"; Dump($self);
    return $ret;
}

sub indexgroup
{
    my $self = shift;
    if (@_) {
	my $group = shift;
	$group =~ s/^\s*//sg;
	$group =~ s/\s*$//sg;
	$group =~ s/;/\\;/sg;
	$group .= " ";
	$self->{INDEXGROUP} = $group;
    }

    my $ret = $self->{INDEXGROUP};

    if (!length($ret)) {
	my $apio = $self->apiOwner();
	if ($apio && ($apio != $self)) {
		return $apio->indexgroup();
	}
    }

    return $ret;
}

sub generateLinkUID
{
    my $self = shift;
    my $composite = shift;

    if ($self->{LINKUID}) {
	# print STDERR "LINKUID WAS ".$self->{LINKUID}."\n";
	if ($composite) {
		return $self->compositePageUID();
	}
	return $self->{LINKUID};
    }

    my $classname = sanitize($self->apiOwner()->rawname());
    my $name = sanitize($self->rawname(), 1);
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();    
    my $uniquenumber = $HeaderDoc::uniquenumber++;
    my $uid = "//$apiUIDPrefix/doc/inheritedContent/$classname/$name/$uniquenumber";

    $self->{LINKUID} = $uid;
    # registerUID($uid);

    if ($composite) {
	return $self->compositePageUID()
    }

    return $uid;
}

# /*! Sets the apple_ref ID for a data type, function, etc.  Generation occurs in apirefSetup. */
sub apiuid {
    my $self = shift;
    my $type = "AUTO";
    my $paramSignature_or_alt_define_name = "";
    my $filename = $self->filename();
    my $linenum = $self->linenum();

    my $localDebug = 0; # NOTE: localDebug is reset to 0 below.

    if (@_) {
	$type = shift;
	if (@_) {
		$paramSignature_or_alt_define_name = shift;
	}
    } else {
	if ($self->{LINKUID}) {
		print STDERR "RETURNING CACHED LINKUID ".$self->{LINKUID}."\n" if ($localDebug);
		return $self->{LINKUID};
	}
	# if (!$self->{APIUID}) {
		# $self->apirefSetup(1);
	# }
	# if ($self->{APIUID}) {
		print STDERR "RETURNING CACHED APIUID ".$self->{APIUID}."\n" if ($localDebug);
		return $self->{APIUID};
	# }
    }

    print STDERR "GENERATING NEW APIUID FOR $self (".$self->name.")\n" if ($localDebug);

    my $olduid = $self->{APIUID};
    if ($self->{LINKUID}) { $olduid = $self->{LINKUID}; }

    my $name = $self->mediumrarename();
    $localDebug = 0;
    my $className; 
    my $lang = $self->sublang();
    my $class = ref($self) || $self;

    cluck("Call trace\n") if ($localDebug);

    if (!($self->can("conflict")) || ($self->can("conflict") && !($self->conflict()))) {
	print STDERR "No conflict.\n" if ($localDebug);
	$name = $self->rawname();
	if ($class eq "HeaderDoc::ObjCCategory") {
		# Category names are in the form "ClassName(DelegateName)"
		if ($name =~ /\s*\w+\s*\(.+\).*/o) {
			$name =~ s/\s*(\w+)\s*\(\s*(\w+)\s*\).*/$1($2)/o;
		}
	}
	# Silently drop leading and trailing space
	$name =~ s/^\s*//so;
	$name =~ s/\s*$//so;
	# Don't silently drop spaces.
        # We sanitize things now.
	# $name =~ s/\s//sgo;
	# $name =~ s/<.*?>//sgo;
	# if ($name =~ /[ \(\)<>\s\n\r]/o) {
	    # if (!$HeaderDoc::ignore_apiuid_errors) {
		# my $fullpath = $self->fullpath();
		# warn("$fullpath:$linenum:apiref: bad name \"$name\".  Dropping ref.\n");
	    # }
	    # return "";
	# }
	print STDERR "Sanitized name: $name\n" if ($localDebug);
    } else {
	print STDERR "Conflict detected.\n" if ($localDebug);
	my $apiOwner = $self->apiOwner();
	my $apiOwnerClass = ref($apiOwner) || $apiOwner;
	if ($apiOwnerClass eq "HeaderDoc::CPPClass") {
		$name = $self->rawname();
	} else {
		$name =~ s/ //sgo;
	}
	# Silently drop leading and trailing space
	$name =~ s/^\s*//so;
	$name =~ s/\s*$//so;
	# Don't silently drop spaces.
        # We sanitize things now.
	# $name =~ s/\s//sgo;
	# $name =~ s/<.*?>//sgo;
	# if ($name =~ /[\s\n\r]/o) {
	    # if (!$HeaderDoc::ignore_apiuid_errors) {
		# my $fullpath = $self->fullpath();
		# warn("$fullpath:$linenum:apiref: bad name \"$name\".  Dropping ref.\n");
	    # }
	    # return "";
	# }
    }

    my $parentClass = $self->apiOwner();
    my $parentClassType = ref($parentClass) || $parentClass;
    if ($parentClassType eq "HeaderDoc::Header") {
	# Generate requests with sublang always (so that, for
	# example, a c++ header can link to a class from within
	# a typedef declaration.

	# Generate anchors (except for class anchors) with lang
	# if the parent is a header, else sublang for stuff
	# within class braces so that you won't get name
	# resolution conflicts if something in a class has the
	# same name as a generic C entity, for example.

	if (!($class eq "HeaderDoc::CPPClass" || $class =~ /^HeaderDoc::ObjC/o)) {
		# print STDERR "LANG $lang\n";
	    if ($lang ne "IDL" && $lang ne "MIG" && $lang ne "javascript") {
		$lang = $self->lang();
	    }
	}
    }

    $lang = $self->apiRefLanguage($lang);

    # if ($lang eq "MIG") {
	# $lang = "mig";
    # } elsif ($lang eq "IDL") {
	# $lang = $HeaderDoc::idl_language;
    # }
    # if ($lang eq "C") { $lang = "c"; }
    # if ($lang eq "Csource") { $lang = "c"; }
    # if ($lang eq "occCat") { $lang = "occ"; }
    # if ($lang eq "intf") { $lang = "occ"; }

# print STDERR "SUBLANG: $lang\n";

    $name =~ s/\n//smgo;

    if ($name =~ /^operator\s+\w/) {
	$name =~ s/^operator\s+/operator_/;
    } else {
	$name =~ s/^operator\s+/operator/;
    }

    # my $lang = "c";
    # my $class = ref($HeaderDoc::APIOwner) || $HeaderDoc::APIOwner;

    # if ($class =~ /^HeaderDoc::CPPClass$/o) {
        # $lang = "cpp";
    # } elsif ($class =~ /^HeaderDoc::ObjC/o) {
        # $lang = "occ";
    # }

    print STDERR "LANG: $lang\n" if ($localDebug);
    # my $classHeaderObject = HeaderDoc::APIOwner->headerObject();
    # if (!$classHeaderObject) { }
    if ($parentClassType eq "HeaderDoc::Header") {
        # We're not in a class.  We used to give the file name here.

	if (!$HeaderDoc::headerObject) {
		die "headerObject undefined!\n";
	}
        # $className = $HeaderDoc::headerObject->name();
	# if (!(length($className))) {
		# die "Header Name empty!\n";
	# }
	$className = "";
    } else {
        # We're in a class.  Give the class name.
	# cluck("PC: $parentClass\n");
	# $self->dbprint();
        $className = $parentClass->name();
	if (length($name)) { $className .= "/"; }
    }
    $className =~ s/\s//sgo;
    $className =~ s/<.*?>//sgo;

    # Macros are not part of a class in any way.
    $class = ref($self) || $self;
    if ($class eq "HeaderDoc::PDefine") {
	$className = "";
	if ($paramSignature_or_alt_define_name) {
		$name = $paramSignature_or_alt_define_name;
		$paramSignature_or_alt_define_name = "";
	}
    }
    if ($class eq "HeaderDoc::Header") {
	# Headers are a "doc" reference type.
	$className = "";
	$lang = "doc";
	if ($self->isFramework()) {
		$type = "framework";
	} else {
		$type = "header";
	}
	$name = $self->filename();
	if ($self->can('isFramework') && $self->isFramework()) {
		$name =~ s/\.hdoc$//s;
	}
    }

    my $apio = $self->apiOwner();
    my $sublang = $self->sublang();
    if ($type eq "intfm" && ($sublang eq "c" || $sublang eq "C") && $apio =~ /HeaderDoc::CPPClass/) {
	$lang = "doc/com";
	$type = "intfm";
    }

    if ($self->appleRefIsDoc()) {
	$lang = "doc";
	$type = "title:$type";
	$name = $self->rawname_extended();
    }

    print STDERR "genRefSub: \"$lang\" \"$type\" \"$name\" \"$className\" \"$paramSignature_or_alt_define_name\"\n" if ($localDebug);

    my $uid = $self->genRefSub($lang, $type, $name, $className, $paramSignature_or_alt_define_name);

    if (length($name)) {
	unregisterUID($olduid, $name, $self);
	$uid = registerUID($uid, $name, $self); # This call resolves conflicts where needed....
    }

    print STDERR "APIUID SET TO $uid\n" if ($localDebug);
    $self->{APIUID} = $uid;

    return $uid;

    # my $ret .= "<a name=\"$uid\"></a>\n";
    # return $ret;
}

# /*! @function appleRefIsDoc
#     @param value
#     @abstract Sets or gets a state flag.
#     @discussion The APPLEREFISDOC state flag controls whether to use a
#     language-specific or doc-specific apple_ref marker for a doc block.
#  */
sub appleRefIsDoc
{
    my $self = shift;
    if (@_) {
	my $value = shift;
	$self->{APPLEREFISDOC} = $value;
    }
	# print STDERR "ARID: ".$self->{APPLEREFISDOC}." for $self\n";
    return $self->{APPLEREFISDOC};
}

# /*! @function genRefSub
#     @param lang Language
#     @param type
#     @param name
#     @param className
#  */
sub genRefSub($$$$)
{
    my $self = shift;
    my $orig_lang = shift;
    my $orig_type = shift;
    my $orig_name = shift;
    my $orig_className = shift;
    my $orig_paramSignature = "";
    if (@_) {
	$orig_paramSignature = shift;
    }

    my $lang = sanitize($orig_lang);
    my $type = sanitize($orig_type);
    my $name = sanitize($orig_name, 1);
    my $className = sanitize($orig_className);
    my $paramSignature = sanitize($orig_paramSignature);

    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();    
    my $localDebug = 0;

    $lang = $self->apiRefLanguage($lang);

    my $uid = "//$apiUIDPrefix/$lang/$type/$className$name$paramSignature";
    return $uid;
}

sub apiRefLanguage
{
    my $self = shift;
    my $lang = shift;

    if ($lang eq "csh") {
	$lang = "shell";
    }
    if ($lang eq "MIG") {
	$lang = "mig";
    } elsif ($lang eq "IDL") {
	$lang = $HeaderDoc::idl_language;
    }
    if ($lang eq "C") { $lang = "c"; }
    if ($lang eq "Csource") { $lang = "c"; }
    if ($lang eq "occCat") { $lang = "occ"; }
    if ($lang eq "intf") { $lang = "occ"; }

    return $lang;
}

sub throws {
    my $self = shift;

    my $newTOC = $HeaderDoc::newTOC;
    if (!$self->isAPIOwner()) { $newTOC = 0; }

    if (@_) {
	my $new = shift;
	# $new =~ s/\n//smgo;
	$new =~ s/\n/ /smgo; # Replace line returns by spaces
	$new =~ s/\s+$//smgo; # Remove trailing spaces
	if ($newTOC) {
        	$self->{THROWS} .= "$new<br>\n";
	} else {
        	$self->{THROWS} .= "<li>$new</li>\n";
	}
	$self->{XMLTHROWS} .= "<throw>$new</throw>\n";
	# print STDERR "Added $new to throw list.\n";
    }
    # print STDERR "dumping throw list.\n";
    if (length($self->{THROWS})) {
	if ($newTOC) {
    		return $self->{THROWS};
	} else {
    		return ("<ul>\n" . $self->{THROWS} . "</ul>");
	}
    } else {
	return "";
    }
}

sub XMLthrows {
    my $self = shift;
    my $string = $self->htmlToXML($self->{XMLTHROWS});

    my $ret;

    if (length($string)) {
	$ret = "<throwlist>\n$string</throwlist>\n";
    } else {
	$ret = "";
    }
    return $ret;
}

sub abstract {
    my $self = shift;
    my $isbrief = 0;

    if (@_) {
	my $abs = shift;

	if (@_) {
		$isbrief = 1;
	}
	if ($isbrief) {
		my ($newabs, $newdisc) = split(/[\n\r][ \t]*[\n\r]/, $abs, 2);
		$abs = $newabs;
		$newdisc =~ s/^(\n|\r|< *br *\/? *>)*//si;
        	$self->discussion($newdisc);
	}
        $self->{ABSTRACT} = $self->linkfix(filterHeaderDocTagContents($abs));
    }

    my $ret = $self->{ABSTRACT};

    # print STDERR "RET IS \"$ret\"\n";

    # $ret =~ s/(\s*<p><\/p>\s*)*$//sg; # This should no longer be needed.
    # $ret =~ s/(\s*<br><br>\s*)*$//sg;

    # print STDERR "RET IS \"$ret\"\n";

    return $ret;
}

sub XMLabstract {
    my $self = shift;

    if (@_) {
        $self->{ABSTRACT} = shift;
    }
    return $self->htmlToXML($self->{ABSTRACT});
}

sub raw_nameline_discussion {
	my $self = shift;
	return $self->{NAMELINE_DISCUSSION};
}

sub rawname_extended {
	my $self = shift;
	my $localDebug = 0;
	my $n = $self->rawname();

	# Append the rest of the name line if necessary.
	if ($self->{DISCUSSION_SET}) {
		# print STDERR "DISCUSSION IS: ".$self->{DISCUSSION}."\n";
		# print STDERR "ISAPIO: ".$self->isAPIOwner()."\n";
		# print STDERR "ISFW: ".$self->isFramework()."\n";
		if ((!$HeaderDoc::ignore_apiowner_names) || (!$self->isAPIOwner()) || $self->isFramework()) {
			print STDERR "NAMELINE DISCUSSION for $self CONCATENATED (".$self->{NAMELINE_DISCUSSION}.")\n" if ($localDebug);
			print STDERR "ORIGINAL NAME WAS \"$n\"\n" if ($localDebug);
			if (length($self->{NAMELINE_DISCUSSION})) {
				$n .= " ".$self->{NAMELINE_DISCUSSION};
			}
		}
	}

	return $n;
}


sub nameline_discussion {
    my $self = shift;
    my $localDebug = 0;

    if (@_) {
        $self->{NAMELINE_DISCUSSION} = shift;
	print STDERR "nameline discussion set to ".$self->{NAMELINE_DISCUSSION}."\n" if ($localDebug);
    }
    return $self->htmlToXML(filterHeaderDocTagContents($self->{NAMELINE_DISCUSSION}));
}

sub raw_discussion {
	my $self = shift;
	return $self->{DISCUSSION};
}

sub discussion_set {
	my $self = shift;
	return $self->{DISCUSSION_SET};
}

sub halfbaked_discussion {
	my $self = shift;
	return $self->discussion_sub(0, 0);
}

sub discussion {
    my $self = shift;
    my $discDebug = 0;

    if (@_) {
	my $olddisc = $self->{DISCUSSION};

	$self->{DISCUSSION_SET} = 1;

	print STDERR "DISCUSSION SET: $self : $olddisc -> \n" if ($discDebug);

        my $discussion = "";

	if ($olddisc ne "" && $discussion ne "") {
		# Warn if we have multiple discussions.
		# We'll be quiet if we're in a define block, as
		# This is just the natural course of things.
		# Clear the old value out first, though.
		if (!$self->inDefineBlock()) {
			my $fullpath = $self->fullpath();
			my $linenum = $self->linenum();

			warn("$fullpath:$linenum: warning: Multiple discussions found for ".$self->name()." ($self).  Merging.\n");
			# print STDERR "OLDDISC: \"$olddisc\"\n";
			# print STDERR "DISC: \"$discussion\"\n";

			$discussion = $olddisc."<br /><br />\n";;
		}
	}
        $discussion .= filterHeaderDocTagContents(shift);

	print STDERR "$discussion\n" if ($discDebug);

	# $discussion =~ s/<br>/\n/sgo;
	$discussion = $self->listfixup($discussion);

        # $discussion =~ s/\n\n/<br>\n/go;
        $self->{DISCUSSION} = $self->linkfix($discussion);

	# Ensure that the discussion is not completely blank.
	if ($self->{DISCUSSION} eq "") {
		$self->{DISCUSSION} .= " ";
	}
    }
    return $self->discussion_sub(1, $discDebug);
}

sub discussion_sub
{
    my $self = shift;
    my $bake = shift;
    my $discDebug = shift;

    # cluck("backtrace\n");
    print STDERR "OBJ is \"".$self."\"\n" if ($discDebug);
    print STDERR "NAME is \"".$self->{NAME}."\"\n" if ($discDebug);
    print STDERR "DISC WAS \"".$self->{DISCUSSION}."\"\n" if ($discDebug);
    print STDERR "NAMELINE DISC WAS \"".$self->{NAMELINE_DISCUSSION}."\"\n" if ($discDebug);

    # Return the real discussion if one exists.
    if ($self->{DISCUSSION}) {
	return $self->{DISCUSSION};
    } else {
	print STDERR "RETURNING NAMELINE DISC\n" if ($discDebug);
    }

    # Return the contents of the name line (e.g. @struct foo Discussion goes here.)
    # beginning after the first token if no discussion exists.
    if ($bake) {
	return "<p>".$self->{NAMELINE_DISCUSSION}."</p>";
    }
    return $self->{NAMELINE_DISCUSSION};
}

sub listfixup
{
    my $self = shift;
    my $olddiscussion = shift;
    my $discussion = "";

    my $numListDebug = 0;

    if ($HeaderDoc::dumb_as_dirt) {
	print STDERR "BASIC MODE: LIST FIXUP DISABLED\n" if ($numListDebug);
	return $olddiscussion;
    }

    print STDERR "processing dicussion for ".$self->name().".\n" if ($numListDebug);

    my @disclines = split(/([\n\r])/, $olddiscussion);
    my $curpos = 0;
    my $seekpos = 0;
    my $nlines = scalar(@disclines);

    my $oldinList = 0;
    my $intextblock = 0;
    my $inpre = 0;

    while ($curpos < $nlines) {
	my $line = $disclines[$curpos];
	if ($line =~ /\@textblock/) {
		$intextblock = 1;
		print STDERR "intextblock -> 1\n" if ($numListDebug);
	}
	if ($line =~ /\@\/textblock/) {
		$intextblock = 0;
		print STDERR "intextblock -> 0\n" if ($numListDebug);
	}
	if ($line =~ /<pre>/) {
		$inpre = 1;
		print STDERR "inpre -> 1\n" if ($numListDebug);
	}
	if ($line =~ /<\/pre>/) {
		$inpre = 0;
		print STDERR "inpre -> 0\n" if ($numListDebug);
	}
	if ($intextblock || $inpre) {
		$discussion .= $line;
	} else {
		print STDERR "LINE: \"$line\"\n" if ($numListDebug);
			print STDERR "TOP OLDINLIST: $oldinList\n" if ($numListDebug);
		if ($line =~ /^\s*((?:-)?\d+)[\)\.\:\s]/o) {
			# this might be the first entry in a list.
			print STDERR "MAYBELIST: $line\n" if ($numListDebug);
			my $inList = 1;
			my $foundblank = 0;
			my $basenum = $1;
			$seekpos = $curpos + 1;
			my $added = 0;
			if (($seekpos >= $nlines) && !$oldinList) {
				$discussion .= "$line";
				$added = 1;
			} else {
			    while (($seekpos < $nlines) && ($inList == 1)) {
				my $scanline = $disclines[$seekpos];
				print STDERR "INLIST: $inList, OLDINLIST: $oldinList\n" if ($numListDebug);
				if ($scanline =~ /^<\/p><p>$/o) {
					# empty line
					$foundblank = 1;
					print STDERR "BLANKLINE\n" if ($numListDebug);
				} elsif ($scanline =~ /^\s*((?:-)?\d+)[\)\.\:\s]/o) {
					# line starting with a number
					$foundblank = 0;
					# print STDERR "D1 is $1\n";
					if ($1 != ($basenum + 1)) {
						# They're noncontiguous.  Not a list.
						print STDERR "NONCONTIG\n" if ($numListDebug);
						if (!$oldinList) {
							print STDERR "ADDED $line\n" if ($numListDebug);
							$discussion .= "$line";
							$added = 1;
						}
						$inList = 0;
					} else {
						# They're contiguous.  It's a list.
						print STDERR "CONTIG\n" if ($numListDebug);
						$inList = 2;
					}
				} else {
					# text.
					if ($foundblank && ($scanline =~ /\S+/o)) {
						# skipped a line and more text.
						# end the list here.
						print STDERR "LIST MAY END ON $scanline\n" if ($numListDebug);
						print STDERR "BASENUM IS $basenum\n" if ($numListDebug);
						$inList = 3;
					}
				}
				$seekpos++;
			    }
			}
			if ($oldinList) {
				# we're finishing an existing list.
				$line =~ s/^\s*((?:-)?\d+)[\)\.\:\s]//so;
				$basenum = $1;
				$discussion .= "</li><li>$line";
				print STDERR "LISTCONTINUES: $line\n" if ($numListDebug);
			} elsif ($inList == 3) {
				# this is a singleton.  Don't touch it.
				$discussion .= $line;
				print STDERR "SINGLETON: $line\n" if ($numListDebug);
			} elsif ($inList == 2) {
				# this is the first entry in a list
				$line =~ s/^\s*((?:-)?\d+)[\)\.\:\s]//so;
				$basenum = $1;
				$discussion .= "<ol start=\"$basenum\"><li>$line";
				print STDERR "FIRSTENTRY: $line\n" if ($numListDebug);
			} elsif (!$added) {
				$discussion .= $line;
			}
			if ($oldinList && !$inList) {
				$discussion .= "</li></ol>";
			}
			$oldinList = $inList;
		} elsif ($line =~ /^<\/p><p>$/o) {
			if ($oldinList == 3 || $oldinList == 1) {
				# If 3, this was last entry in list before next
				# text.  If 1, this was last entry in list before
				# we ran out of lines.  In either case, it's a
				# blank line not followed by another numbered
				# line, so we're done.
	
				print STDERR "OUTERBLANKLINE\n" if ($numListDebug);
				$discussion .= "</li></ol>";
				$oldinList = 0;
			} else {
				 print STDERR "OIL: $oldinList\n" if ($numListDebug);
				$discussion .= "$line";
			}
		} else {
			# $oldinList = 0;
			print STDERR "TEXTLINE: \"$line\"\n" if ($numListDebug);
			$discussion .= $line;
		}
    	}
	$curpos++;
    }
    if ($oldinList) {
	$discussion .= "</li></ol>";
    }

    print STDERR "done processing dicussion for ".$self->name().".\n" if ($numListDebug);
    # $newdiscussion = $discussion;
    return $discussion;
}

sub XMLdiscussion {
    my $self = shift;

    if (@_) {
        my $discussion = "";
        $discussion = shift;
        # $discussion =~ s/\n\n/<br>\n/go;
        $self->{DISCUSSION} = $discussion;
    }
    return $self->htmlToXML($self->{DISCUSSION});
}


sub declaration {
    my $self = shift;
    # my $dec = $self->declarationInHTML();
    # remove simple markup that we add to declarationInHTML
    # $dec =~s/<br>/\n/gio;
    # $dec =~s/<font .*?>/\n/gio;
    # $dec =~s/<\/font>/\n/gio;
    # $dec =~s/<(\/)?tt>//gio;
    # $dec =~s/<(\/)?b>//gio;
    # $dec =~s/<(\/)?pre>//gio;
    # $dec =~s/\&nbsp;//gio;
    # $dec =~s/\&lt;/</gio;
    # $dec =~s/\&gt;/>/gio;
    # $self->{DECLARATION} = $dec;  # don't really have to have this ivar
    if (@_) {
	$self->{DECLARATION} = shift;
    }
    return $self->{DECLARATION};
}

sub privateDeclaration {
    my $self = shift;
    if (@_) {
	$self->{PRIVATEDECLARATION} = shift;
    }
    return $self->{PRIVATEDECLARATION};
}


# /*! @function genRef
#     @abstract generate a cross-reference request
#     @param keystring string containing the keywords, e.g. stuct, enum
#     @param namestring string containing the type name itself
#     @param linktext link text to generate
#     @param optional_expected_type general genre of expected types
#  */
sub genRef($$$)
{
    my $self = shift;
    my $keystring = shift;
    my $name = shift;
    my $linktext = shift;
    my $optional_expected_type = "";
    if (@_) {
	$optional_expected_type = shift;
    }
    my $fullpath = $self->fullpath();
    my $linenum = $self->linenum();
    my $tail = "";
    my $xml = 0;
    my $localDebug = 0;

    print STDERR "NAME IS $name\n" if ($localDebug);

    if ($self->outputformat() eq "hdxml") { $xml = 1; }

    # Generate requests with sublang always (so that, for
    # example, a c++ header can link to a class from within
    # a typedef declaration.  Generate anchors with lang
    # if the parent is a header, else sublang for stuff
    # within class braces so that you won't get name
    # resolution conflicts if something in a class has the
    # same name as a generic C entity, for example.

    my $lang = $self->sublang();
    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
	$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
	$typedefname, $varname, $constname, $structisbrace, $macronamesref,
	$classregexp, $classbraceregexp, $classclosebraceregexp,
	$accessregexp, $requiredregexp, $propname, 
        $objcdynamicname, $objcsynthesizename, $moduleregexp, $definename) = parseTokens($self->lang(), $self->sublang());

    if ($name =~ /^[\d\[\]]/o) {
	# Silently fail for [4] and similar.
	print STDERR "Silently fail[1]\n" if ($localDebug);
	return $linktext;
    }

    if (($name =~ /^[=|+-\/&^~!*]/o) || ($name =~ /^\s*\.\.\.\s*$/o)) {
	# Silently fail for operators
	# and varargs macros.

	print STDERR "Silently fail[2]\n" if ($localDebug);
	return $linktext;
    }
    # if (($name =~ /^\s*public:/o) || ($name =~ /^\s*private:/o) ||
	# ($name =~ /^\s*protected:/o)) {
    if (length($accessregexp) && ($name =~ /$accessregexp(:)?/)) {
	# Silently fail for these, too.

	print STDERR "Silently fail[3]\n" if ($localDebug);
	return $linktext;
    }

    if ($name =~ s/\)\s*$//o) {
	if ($linktext =~ s/\)\s*$//o) {
		$tail = ")";
	} else {
		warn("$fullpath:$linenum: warning: Parenthesis in ref name, not in link text\n");
		warn("name: $name) linktext: $linktext\n");
	}
    }

    # I haven't found any cases where this would trigger a warning
    # that don't already trigger a warning elsewhere.
    my $testing = 0;
    if ($testing && ($name =~ /&/o || $name =~ /\(/o || $name =~ /\)/o || $name =~ /.:(~:)./o || $name =~ /;/o || $name eq "::" || $name =~ /^::/o)) {
	my $classname = $self->name();
	my $class = ref($self) || $self;
	my $declaration = $self->declaration();
	if (($name eq "(") && $class eq "HeaderDoc::PDefine") {
		warn("$fullpath:$linenum: warning: bogus paren in #define\n");
	} elsif (($name eq "(") && $class eq "HeaderDoc::Function") {
		warn("$fullpath:$linenum: warning: bogus paren in function\n");
	} elsif ($class eq "HeaderDoc::Function") {
		warn("$fullpath:$linenum: warning: bogus paren in function\n");
	} else {
		warn("$fullpath:$linenum: warning: $fullpath $classname $class $keystring generates bad crossreference ($name).  Dumping trace.\n");
		# my $declaration = $self->declaration();
		# warn("BEGINDEC\n$declaration\nENDDEC\n");
		$self->printObject();
	}
    }

    if ($name =~ /(.+)::(.+)/o) {
	my $classpart = $1;
	my $type = $2;
	if ($linktext !~ /::/o) {
		warn("$fullpath:$linenum: warning: Bogus link text generated for item containing class separator.  Ignoring.\n");
	}
	my $ret = $self->genRef("class", $classpart, $classpart);
	$ret .= "::";

	# This is where it gets ugly.  C++ allows use of struct,
	# enum, and other similar types without preceding them
	# with struct, enum, etc....

	$classpart .= "/";

        my $ref1 = $self->genRefSub($lang, "instm", $type, $classpart);
        my $ref2 = $self->genRefSub($lang, "clm", $type, $classpart);
        my $ref3 = $self->genRefSub($lang, "func", $type, $classpart);
        my $ref4 = $self->genRefSub($lang, "ftmplt", $type, $classpart);
        my $ref5 = $self->genRefSub($lang, "defn", $type, "");
        my $ref6 = $self->genRefSub($lang, "macro", $type, "");
	# allow classes within classes for certain languages.
        my $ref7 = $self->genRefSub($lang, "cl", $type, $classpart);
        my $ref8 = $self->genRefSub($lang, "tdef", $type, "");
        my $ref9 = $self->genRefSub($lang, "tag", $type, "");
        my $ref10 = $self->genRefSub($lang, "econst", $type, "");
        my $ref11 = $self->genRefSub($lang, "struct", $type, "");
        my $ref12 = $self->genRefSub($lang, "data", $type, $classpart);
        my $ref13 = $self->genRefSub($lang, "clconst", $type, $classpart);
	my $ref99 = $self->genRefSub("doc/com", "intfm", $name, $classpart);
	if (!$xml) {
        	$ret .= "<!-- a logicalPath=\"$ref1 $ref2 $ref3 $ref4 $ref5 $ref6 $ref7 $ref8 $ref9 $ref10 $ref11 $ref12 $ref13 $ref99\" -->$type<!-- /a -->";
	} else {
        	$ret .= "<hd_link logicalPath=\"$ref1 $ref2 $ref3 $ref4 $ref5 $ref6 $ref7 $ref8 $ref9 $ref10 $ref11 $ref12 $ref13 $ref99\">$type</hd_link>";
	}

	print STDERR "Double-colon case\n" if ($localDebug);
	return $ret.$tail;
    }

    my $ret = "";
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();    
    my $type = "";
    my $className = "";

    my $class_or_enum_check = " $keystring ";
    # if ($lang eq "pascal") { $class_or_enum_check =~ s/\s+var\s+/ /sgo; }
    # if ($lang eq "MIG") { $class_or_enum_check =~ s/\s+(in|out|inout)\s+/ /sgo; }
    # $class_or_enum_check =~ s/\s+const\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+static\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+virtual\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+auto\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+extern\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+__asm__\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+__asm\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+__inline__\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+__inline\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+inline\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+register\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+template\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+unsigned\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+signed\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+volatile\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+private\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+protected\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+public\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+synchronized\s+/ /sgo;
    # $class_or_enum_check =~ s/\s+transient\s+/ /sgo;

    my ($case_sensitive, $keywordhashref) = $self->keywords();
    my @keywords = keys %{$keywordhashref};
    foreach my $keyword (@keywords) {
	my $keywordquot = quote($keyword);
	if ($case_sensitive) {
		$class_or_enum_check =~ s/(^|\s+)$keywordquot(\s+|$)/ /sg;
	} else {
		$class_or_enum_check =~ s/(^|\s+)$keywordquot(\s+|$)/ /sgi;
	}
    }

    $class_or_enum_check =~ s/\s*//smgo;

    if (length($class_or_enum_check)) {
	SWITCH: {
	    ($keystring =~ /type/o && $lang eq "pascal") && do { $type = "tdef"; last SWITCH; };
	    ($keystring =~ /type/o && $lang eq "MIG") && do { $type = "tdef"; last SWITCH; };
	    ($keystring =~ /record/o && $lang eq "pascal") && do { $type = "struct"; last SWITCH; };
	    ($keystring =~ /procedure/o && $lang eq "pascal") && do { $type = "*"; last SWITCH; };
	    ($keystring =~ /of/o && $lang eq "pascal") && do { $type = "*"; last SWITCH; };
	    ($keystring =~ /typedef/o) && do { $type = "tdef"; last SWITCH; };
	    (($keystring =~ /sub/o) && ($lang eq "perl")) && do { $type = "*"; last SWITCH; };
	    ($keystring =~ /function/o) && do { $type = "*"; last SWITCH; };
	    ($keystring =~ /typedef/o) && do { $type = "tdef"; last SWITCH; };
	    ($keystring =~ /struct/o) && do { $type = "tag"; last SWITCH; };
	    ($keystring =~ /union/o) && do { $type = "tag"; last SWITCH; };
	    ($keystring =~ /operator/o) && do { $type = "*"; last SWITCH; };
	    ($keystring =~ /enum/o) && do { $type = "tag"; last SWITCH; };
	    ($keystring =~ /protocol/o) && do { $type = "intf"; $className=$name; $name=""; last SWITCH; };
	    ($keystring =~ /class/o) && do { $type = "cl"; $className=$name; $name=""; last SWITCH; };
	    ($keystring =~ /#(define|ifdef|ifndef|if|endif|undef|elif|error|warning|pragma|include|import)/o) && do {
		    # Used to include || $keystring =~ /class/o
		    # defines and similar aren't followed by a type

		    print STDERR "Keyword case\n" if ($localDebug);
		    return $linktext.$tail;
		};
	    {
		$type = "";
		my $name = $self->name();
		warn "$fullpath:$linenum: warning: keystring ($keystring) in $name type link markup\n";
		return $linktext.$tail;
	    }
	}
	if ($type eq "*") {
	    # warn "Function requested with genRef.  This should not happen.\n";
	    # This happens now, at least for operators.

	    my $ref1 = $self->genRefSub($lang, "instm", $name, $className);
	    my $ref2 = $self->genRefSub($lang, "clm", $name, $className);
	    my $ref3 = $self->genRefSub($lang, "func", $name, $className);
	    my $ref4 = $self->genRefSub($lang, "ftmplt", $name, $className);
	    my $ref5 = $self->genRefSub($lang, "defn", $name, $className);
	    my $ref6 = $self->genRefSub($lang, "macro", $name, $className);
	    my $ref99 = $self->genRefSub("doc/com", "intfm", $name, $className);

	    print STDERR "Class or enum check case: Type is \"*\" case\n" if ($localDebug);
	    if (!$xml) {
	        return "<!-- a logicalPath=\"$ref1 $ref2 $ref3 $ref4 $ref5 $ref6 $ref99\" -->$linktext<!-- /a -->".$tail;
	    } else {
	        return "<hd_link logicalPath=\"$ref1 $ref2 $ref3 $ref4 $ref5 $ref6 $ref99\">$linktext</hd_link>".$tail;
	    }
	} else {
	    print STDERR "Class or enum check case: Type is not \"*\" case\n" if ($localDebug);
	    if (!$xml) {
	        return "<!-- a logicalPath=\"" . $self->genRefSub($lang, $type, $className, $name) . "\" -->$linktext<!-- /a -->".$tail;
	    } else {
	        return "<hd_link logicalPath=\"" . $self->genRefSub($lang, $type, $className, $name) . "\">$linktext</hd_link>".$tail;
	    }
	}
    } else {
	# We could be looking for a class or a typedef.  Unless it's local, put in both
	# and let the link resolution software sort it out.  :-)

        # allow classes within classes for certain languages.
        my $ref7 = $self->genRefSub($lang, "cl", $name, "");
        my $ref7a = "";
	$ref7a = $self->genRefSub($lang, "cl", $name, $className) if ($className ne "");
        my $ref8 = $self->genRefSub($lang, "tdef", $name, "");
        my $ref9 = $self->genRefSub($lang, "tag", $name, "");
        my $ref10 = $self->genRefSub($lang, "econst", $name, "");
        my $ref11 = $self->genRefSub($lang, "struct", $name, "");
        my $ref12 = $self->genRefSub($lang, "data", $name, $className);
        my $ref13 = $self->genRefSub($lang, "clconst", $name, $className);
        my $ref14 = $self->genRefSub($lang, "intf", $name, "");

        my $ref1 = $self->genRefSub($lang, "instm", $name, $className);
        my $ref2 = $self->genRefSub($lang, "clm", $name, $className);
        my $ref2a = $self->genRefSub($lang, "intfcm", $name, $className);
        my $ref2b = $self->genRefSub($lang, "intfm", $name, $className);
        my $ref3 = $self->genRefSub($lang, "func", $name, $className);
        my $ref4 = $self->genRefSub($lang, "ftmplt", $name, $className);
        my $ref5 = $self->genRefSub($lang, "defn", $name, "");
        my $ref6 = $self->genRefSub($lang, "macro", $name, "");
	my $ref99 = $self->genRefSub("doc/com", "intfm", $name, $className);

	my $masterref = "$ref7 $ref7a $ref8 $ref9 $ref10 $ref11 $ref12 $ref13 $ref14 $ref1 $ref2 $ref2a $ref2b $ref3 $ref4 $ref5 $ref6 $ref99";
	print STDERR "Default case (OET: $optional_expected_type)" if ($localDebug);

	if (length($optional_expected_type)) {
		SWITCH: {
			($optional_expected_type eq "string") && do {
					return $linktext.$tail;
				};
			($optional_expected_type eq "char") && do {
					return $linktext.$tail;
				};
			($optional_expected_type eq "comment") && do {
					return $linktext.$tail;
				};
			($optional_expected_type eq "preprocessor") && do {
					# We want to add links, but all we can
					# do is guess about the type.
					last SWITCH;
				};
			($optional_expected_type eq "number") && do {
					return $linktext.$tail;
				};
			($optional_expected_type eq "keyword") && do {
					return $linktext.$tail;
				};
			($optional_expected_type eq "function") && do {
					$masterref = "$ref1 $ref2 $ref2a $ref2b $ref3 $ref4 $ref5 $ref6";
					last SWITCH;
				};
			($optional_expected_type eq "var") && do {
					# Variable name.
					$masterref = "$ref10 $ref12";
					last SWITCH;
				};
			($optional_expected_type eq "template") && do {
					# Could be any template parameter bit
					# (type or name).  Since we don't care
					# much if a parameter name happens to
					# something (ideally, it shouldn't),
					# we'll just assume we're getting a
					# type and be done with it.
					$masterref = "$ref7 $ref7a $ref8 $ref9 $ref10 $ref11 $ref13 $ref14";
					last SWITCH;
				};
			($optional_expected_type eq "type") && do {
					$masterref = "$ref7 $ref7a $ref8 $ref9 $ref10 $ref11 $ref13 $ref14";
					last SWITCH;
				};
			($optional_expected_type eq "param") && do {
					# parameter name.  Don't link.
					return $linktext.$tail;
				};
			($optional_expected_type eq "ignore") && do {
					# hidden token.
					return $linktext.$tail;
				};
			{
				warn("$fullpath:$linenum: warning: Unknown reference class \"$optional_expected_type\" in genRef\n");
			}
		}
	}
	print STDERR "Default case: No OET.  MR IS $masterref\n" if ($localDebug);

    $masterref =~ s/\s+/ /g;
	if ($xml) {
            return "<hd_link logicalPath=\"$masterref\">$linktext</hd_link>".$tail;
	} else {
            return "<!-- a logicalPath=\"$masterref\" -->$linktext<!-- /a -->".$tail;
	}

    # return "<!-- a logicalPath=\"$ref1 $ref2 $ref3\" -->$linktext<!-- /a -->".$tail;
    }

}

# /*! @function keywords
#     @abstract returns all known keywords for the current language
#  */
sub keywords
{
    my $self = shift;
    my $class = ref($self) || $self;
    # my $declaration = shift;
    # my $functionBlock = shift;
    # my $orig_declaration = $declaration;
    my $localDebug = 0;
    my $parmDebug = 0;
    my $lang = $self->lang();
    my $sublang = $self->sublang();
    # my $fullpath = $HeaderDoc::headerObject->fullpath();
    my $fullpath = $self->fullpath();
    my $linenum = $self->linenum();
    my $case_sensitive = 1;

    if (!$self->isAPIOwner()) {
	# $self->dbprint();
	my $apio = $self->apiOwner();
	# print STDERR "APIO: \"$apio\"\n";
	return $apio->keywords();
    }
    if ($self->{KEYWORDHASH}) { return ($self->{CASESENSITIVE}, $self->{KEYWORDHASH}); }

    print STDERR "keywords\n" if ($localDebug);

    # print STDERR "Color\n" if ($localDebug);
    # print STDERR "lang = $HeaderDoc::lang\n";

    # Note: these are not all of the keywords of each language.
    # This should, however, be all of the keywords that can occur
    # in a function or data type declaration (e.g. the sort
    # of material you'd find in a header).  If there are missing
    # keywords that meet that criterion, please file a bug.

    my %CKeywords = ( 
	"assert" => 1, 
	"break" => 1, 
	"auto" => 1, "const" => 1, "enum" => 1, "extern" => 7, "inline" => 1,
	"__inline__" => 1, "__inline" => 1, "__asm" => 2, "__asm__" => 2,
        "__attribute__" => 2, "__typeof__" => 6,
	"register" => 1, "signed" => 1, "static" => 1, "struct" => 1, "typedef" => 1,
	"union" => 1, "unsigned" => 1, "volatile" => 1, "#define" => 1,
	"#ifdef" => 1, "#ifndef" => 1, "#if" => 1, "#endif" => 1,
	"#undef" => 1, "#elif" => 1, "#error" => 1, "#warning" => 1,
 	"#pragma" => 1, "#include" => 1, "#import" => 1 , "NULL" => 1,
	"true" => 1, "false" => 1);
    my %CppKeywords = (%CKeywords,
	("class" => 1, 
	"friend" => 1,
	"mutable" => 1,
	"namespace" => 1,
	"operator" => 1,
	"private" => 1,
	"protected" => 1,
	"public" => 1,
	"template" => 1,
	"virtual" => 1));
    my %ObjCKeywords = (%CKeywords,
	("\@class" => 1,
	"\@interface" => 1,
	"\@protocol" => 1,
	"\@property" => 1,
	"\@public" => 1,
	"\@private" => 1,
	"\@protected" => 1,
	"\@package" => 1,
	"\@synthesize" => 1,
	"\@dynamic" => 1,
	"\@optional" => 1,
	"\@required" => 1,
	"nil" => 1,
	"YES" => 1,
	"NO" => 1 ));
    my %phpKeywords = (%CKeywords, ("function" => 1));
    my %javascriptKeywords = (
	"abstract" => 1, 
	# "assert" => 1, 
	"break" => 1, 
	# "byte" => 1, 
	"case" => 1, 
	"catch" => 1, 
	# "char" => 1, 
	"class" => 1, 
	"const" => 1, 
	"continue" => 1, 
	"debugger" => 1, 
	"default" => 1, 
	"delete" => 1, 
	"do" => 1, 
	# "double" => 1, 
	"else" => 1,
	"enum" => 1,
	"export" => 1,
	"extends" => 3,
	"false" => 1,
	"final" => 1,
	"finally" => 1,
	# "float" => 1,
	"for" => 1,
	"function" => 1,
	"goto" => 1,
	"if" => 1,
	"implements" => 4,
	"import" => 1,
	"in" => 1,
	"instanceof" => 1,
	# "int" => 1,
	"interface" => 1,
	# "long" => 1,
	"native" => 1,
	"new" => 1,
	"null" => 1,
	"package" => 1,
	"private" => 1,
	"protected" => 1,
	"public" => 1,
	"return" => 1,
	# "short" => 1,
	"static" => 1,
	"super" => 1,
	"switch" => 1,
	"synchronized" => 1,
	"this" => 1,
	"throw" => 1,
	"throws" => 1,
	"transient" => 1,
	"true" => 1,
	"try" => 1,
	"typeof" => 1,
	"var" => 1,
	# "void" => 1,
	"volatile"  => 1,
	"while"  => 1,
	"with"  => 1);
    my %javaKeywords = (
	"abstract" => 1, 
	"assert" => 1, 
	"break" => 1, 
	"case" => 1, 
	"catch" => 1, 
	"class" => 1, 
	"const" => 1, 
	"continue" => 1, 
	"default" => 1, 
	"do" => 1, 
	"else" => 1,
	"enum" => 1,
	"extends" => 3,
	"false" => 1,
	"final" => 1,
	"finally" => 1,
	"for" => 1,
	"goto" => 1,
	"if" => 1,
	"implements" => 4,
	"import" => 1,
	"instanceof" => 1,
	"interface" => 1,
	"native" => 1,
	"new" => 1,
	"package" => 1,
	"private" => 1,
	"protected" => 1,
	"public" => 1,
	"return" => 1,
	"static" => 1,
	"strictfp" => 1,
	"super" => 1,
	"switch" => 1,
	"synchronized" => 1,
	"this" => 1,
	"throw" => 1,
	"throws" => 1,
	"transient" => 1,
	"true" => 1,
	"try" => 1,
	"volatile"  => 1,
	"while"  => 1);
    my %perlKeywords = ( "sub"  => 1, "my" => 1, "next" => 1, "last" => 1);
    my %shellKeywords = ( "sub"  => 1, "alias" => 1);
    my %cshKeywords = ( "set"  => 1, "setenv" => 1, "alias" => 1);
    my %pascalKeywords = (
	"absolute" => 1, "abstract" => 1, "all" => 1, "and" => 1, "and_then" => 1,
	"array" => 1, "asm" => 1, "begin" => 1, "bindable" => 1, "case" => 1, "class" => 1,
	"const" => 1, "constructor" => 1, "destructor" => 1, "div" => 1, "do" => 1,
	"downto" => 1, "else" => 1, "end" => 1, "export" => 1, "file" => 1, "for" => 1,
	"function" => 1, "goto" => 1, "if" => 1, "import" => 1, "implementation" => 1,
	"inherited" => 1, "in" => 1, "inline" => 1, "interface" => 1, "is" => 1, "label" => 1,
	"mod" => 1, "module" => 1, "nil" => 1, "not" => 1, "object" => 1, "of" => 1, "only" => 1,
	"operator" => 1, "or" => 1, "or_else" => 1, "otherwise" => 1, "packed" => 1, "pow" => 1,
	"procedure" => 1, "program" => 1, "property" => 1, "qualified" => 1, "record" => 1,
	"repeat" => 1, "restricted" => 1, "set" => 1, "shl" => 1, "shr" => 1, "then" => 1, "to" => 1,
	"type" => 1, "unit" => 1, "until" => 1, "uses" => 1, "value" => 1, "var" => 1, "view" => 1,
	"virtual" => 1, "while" => 1, "with" => 1, "xor"  => 1);
    my %IDLKeywords = (
	"abstract" => 1, "any" => 1, "attribute" => 1, "case" => 1,
# char
	"component" => 1, "const" => 1, "consumes" => 1, "context" => 1, "custom" => 1, "default" => 1,
# double
	"exception" => 1, "emits" => 1, "enum" => 1, "eventtype" => 1, "factory" => 1, "FALSE" => 1,
	"finder" => 1, "fixed" => 1,
# float
	"getraises" => 5, "getter" => 1, "home" => 1, "import" => 1, "in" => 1, "inout" => 1, "interface" => 1,
	"local" => 1, "long" => 1, "module" => 1, "multiple" => 1, "native" => 1, "Object" => 1,
	"octet" => 1, "oneway" => 1, "out" => 1, "primarykey" => 1, "private" => 1, "provides" => 1,
	"public" => 1, "publishes" => 1, "raises" => 5, "readonly" => 1, "setraises" => 5, "setter" => 1, "sequence" => 1,
# short
# string
	"struct" => 1, "supports" => 1, "switch" => 1, "TRUE" => 1, "truncatable" => 1, "typedef" => 1,
	"typeid" => 1, "typeprefix" => 1, "unsigned" => 1, "union" => 1, "uses" => 1, "ValueBase" => 1,
	"valuetype" => 1,
# void
# wchar
# wstring
	"#define" => 1,
	"#ifdef" => 1, "#ifndef" => 1, "#if" => 1, "#endif" => 1,
	"#undef" => 1, "#elif" => 1, "#error" => 1, "#warning" => 1,
 	"#pragma" => 1, "#include" => 1, "#import" => 1 );
    my %MIGKeywords = (
	"routine" => 1, "simpleroutine" => 1, "countinout" => 1, "inout" => 1, "in" => 1, "out" => 1,
	"subsystem" => 1, "skip" => 1, "#define" => 1,
	"#ifdef" => 1, "#ifndef" => 1, "#if" => 1, "#endif" => 1,
	"#undef" => 1, "#elif" =>1, "#error" => 1, "#warning" => 1,
 	"#pragma" => 1, "#include" => 1, "#import" => 1, "import" => 1, "simport" => 1, "type" => 1,
	"skip" => 1, "serverprefix" => 1, "serverdemux" => 1, "userprefix" => 1 );

    my $objC = 0;
    my %keywords = %CKeywords;
    # warn "Language is $lang, sublanguage is $sublang\n";

    if ($lang eq "C") {
	SWITCH: {
	    ($sublang eq "cpp") && do { %keywords = %CppKeywords; last SWITCH; };
	    ($sublang eq "C") && do { last SWITCH; };
	    ($sublang =~ /^occ/o) && do { %keywords = %ObjCKeywords; $objC = 1; last SWITCH; }; #occ, occCat
	    ($sublang eq "intf") && do { %keywords = %ObjCKeywords; $objC = 1; last SWITCH; };
	    ($sublang eq "MIG") && do { %keywords = %MIGKeywords; last SWITCH; };
	    ($sublang eq "IDL") && do { %keywords = %IDLKeywords; last SWITCH; };
	    warn "$fullpath:$linenum: warning: Unknown language ($lang:$sublang)\n";
	}
    }
    if ($lang eq "Csource") {
	SWITCH: {
	    ($sublang eq "Csource") && do { last SWITCH; };
	    warn "$fullpath:$linenum: warning: Unknown language ($lang:$sublang)\n";
	}
    }
    if ($lang eq "php") {
	SWITCH: {
	    ($sublang eq "php") && do { %keywords = %phpKeywords; last SWITCH; };
	    warn "$fullpath:$linenum: warning: Unknown language ($lang:$sublang)\n";
	}
    }
    if ($lang eq "java") {
	SWITCH: {
	    ($sublang eq "java") && do { %keywords = %javaKeywords; last SWITCH; };
	    ($sublang eq "javascript") && do { %keywords = %javascriptKeywords; last SWITCH; };
	    warn "$fullpath:$linenum: warning: Unknown language ($lang:$sublang)\n";
	}
    }
    if ($lang eq "perl") {
	SWITCH: {
	    ($sublang eq "perl") && do { %keywords = %perlKeywords; last SWITCH; };
	    warn "$fullpath:$linenum: warning: Unknown language ($lang:$sublang)\n";
	}
    }
    if ($lang eq "shell") {
	SWITCH: {
	    ($sublang eq "csh") && do { %keywords = %cshKeywords; last SWITCH; };
	    ($sublang eq "shell") && do { %keywords = %shellKeywords; last SWITCH; };
	    warn "$fullpath:$linenum: warning: Unknown language ($lang:$sublang)\n";
	}
    }
    if ($lang eq "pascal") {
	%keywords = %pascalKeywords;
	$case_sensitive = 0;
    }
    if ($lang eq "C" && $sublang eq "MIG") {
	$case_sensitive = 0;
    }

    # foreach my $keyword (sory %keywords) {
	# print STDERR "keyword $keyword\n";
    # }

    $self->{KEYWORDHASH} = \%keywords;
    $self->{CASESENSITIVE} = $case_sensitive;

# print STDERR "KEYS\n";foreach my $key (keys %keywords) { print STDERR "KEY: $key\n"; }print STDERR "ENDKEYS\n";

    return ($case_sensitive, \%keywords);
}

sub htmlToXML
{
    my $self = shift;
    my $xmldec = shift;
    my $droppara = shift;
    my $debugname = shift;

    my $localDebug = 0;

    if ($xmldec !~ /[<>&]/o) {
	print STDERR "FASTPATH FOR $debugname\n" if ($localDebug);
	return $xmldec;
    }

    # print STDERR "RETURNING:\n$xmldec\nENDRETURN\n";

    return html2xhtml($xmldec, $debugname);
}

sub textToXML
{
    my $self = shift;
    my $xmldec = shift;

    $xmldec =~ s/&/&amp;/sgo;
    $xmldec =~ s/</&lt;/sgo;
    $xmldec =~ s/>/&gt;/sgo;

    return $xmldec;
}


sub declarationInHTML {
    my $self = shift;
    my $class = ref($self) || $self;
    my $localDebug = 0;
    # my $lang = $self->lang();
    my $xml = 0;
    # my $priDec = wrap($self->privateDeclaration());
    if ($self->outputformat() eq "hdxml") { $xml = 1; }

    if (@_) {
	# @@@ DISABLE STYLES FOR DEBUGGING HERE @@@
	my $disable_styles = 0;
	if ($xml) {
		my $xmldec = shift;

		if (1 || $HeaderDoc::use_styles && !$disable_styles) {
			my $parseTree_ref = $self->parseTree();
			my $parseTree = ${$parseTree_ref};
			bless($parseTree, "HeaderDoc::ParseTree");
			if ($self->can("isBlock") && $self->isBlock()) {
				$xmldec = "";
				my @tree_refs = @{$self->parseTreeList()};

				foreach my $tree_ref (@tree_refs) {
					my $tree = ${$tree_ref};
					bless($tree,  "HeaderDoc::ParseTree");
					$xmldec .= $tree->xmlTree($self->preserve_spaces(), $self->hideContents())."\n";
				}

			} else {
				$xmldec = $parseTree->xmlTree($self->preserve_spaces(), $self->hideContents());
			}
			$self->{DECLARATIONINHTML} = $xmldec;
		} else {
        		$self->{DECLARATIONINHTML} = $self->textToXML($xmldec);
		}


		return $xmldec;
	}
		
	my $declaration = shift;

	if (1 || $HeaderDoc::use_styles && !$disable_styles) {
	  # print STDERR "I AM ".$self->name()." ($self)\n";
	  if ($self->can("isBlock") && $self->isBlock()) {
		# my $declaration = "";
		my @defines = $self->parsedParameters();

		foreach my $define (@defines) {
			$declaration .= $define->declarationInHTML();
			$declaration .= "\n";
		}
		$declaration = "";

		my @tree_refs = @{$self->parseTreeList()};
		foreach my $tree_ref (@tree_refs) {
			my $tree = ${$tree_ref};
			bless($tree,  "HeaderDoc::ParseTree");
			# print STDERR "Processing tree $tree\n";
			# print STDERR $tree->htmlTree();
			# print STDERR "\nEND TREE\n";
			# $tree->dbprint();
			$declaration .= $tree->htmlTree($self->preserve_spaces(), $self->hideContents())."\n";
		}
		# print STDERR "END PROCESSING.  DECLARATION IS:\n$declaration\nEND DECLARATION\n";
	  } else {
		my $parseTree_ref = $self->parseTree();
		my $parseTree = ${$parseTree_ref};
		bless($parseTree, "HeaderDoc::ParseTree");
		# print STDERR "PT: ".$parseTree."\n";
		# $parseTree->printTree();
		$declaration = $parseTree->htmlTree($self->preserve_spaces(), $self->hideContents());
		# print STDERR "HTMLTREE: $declaration\n";
	  }
	}

	# print STDERR "SET DECLARATION TO $declaration\n";
        $self->{DECLARATIONINHTML} = $declaration;
    }
    return $self->{DECLARATIONINHTML};
}

sub parseTree {
    my $self = shift;

    if (@_) {
	my $parsetree = shift;
	if ($self->can("isBlock") && $self->isBlock()) {
		$self->addParseTree($parsetree);
	}
        $self->{PARSETREE} = $parsetree;
    }
    return $self->{PARSETREE};
}

sub parseTreeList
{
    my $self = shift;
    my $localDebug = 0;

    if ($localDebug) {
      print STDERR "OBJ ".$self->name().":\n";
      foreach my $treeref (@{$self->{PARSETREELIST}}) {
	my $tree = ${$treeref};
	bless($tree, "HeaderDoc::ParseTree");
	print STDERR "PARSE TREE: $tree\n";
	$tree->dbprint();
      }
    }

    if ($self->{PARSETREELIST}) {
	return $self->{PARSETREELIST};
    }

    if (!$HeaderDoc::test_mode) {
	die("No parse trees for object $self.\n".
	    "Name is ".$self->name().".  This usually\n".
	    "points to a headerdoc comment before a #if whose contents are not a\n".
	    "complete declaraction.  This is a fatal error.  Exiting.\n");
    }

    # If we get here, we're being lazily called from the test framework, so return
    # an empty list of parse trees.
    my @arr = ();

    return \@arr;
}

sub addParseTree
{
    my $self = shift;
    my $tree = shift;

    my $localDebug = 0;

    push(@{$self->{PARSETREELIST}}, $tree);

    if ($localDebug) {
      print STDERR "OBJ ".$self->name().":\n";
      foreach my $treeref (@{$self->{PARSETREELIST}}) {
	my $tree = ${$treeref};
	print STDERR "PARSE TREE: $tree\n";
	bless($tree, "HeaderDoc::ParseTree");
	$tree->dbprint();
      }
    }

}

sub fixParseTrees
{
	my $self = shift;
	my $localDebug = 0;

	if (!$self->{PARSETREE}) {
		# Nothing to do.
		return;
	}

	my @trees = ();
	if ($self->{PARSETREELIST}) {
		@trees = @{$self->{PARSETREELIST}};
	}
	my $match = 0;
	my $searchtree = ${$self->{PARSETREE}};
	print STDERR "Looking for tree $searchtree\n" if ($localDebug);
	foreach my $treeref (@trees) {
		my $tree = ${$treeref};
		print STDERR "Comparing with $tree\n" if ($localDebug);
		if ($tree == $searchtree) { $match = 1; }
	}
	if (!$match) {
		print STDERR "Not found.  Adding\n" if ($localDebug);
		$self->addParseTree($self->{PARSETREE});
	} else {
		print STDERR "Found.  Not adding\n" if ($localDebug);
	}
}

sub availabilityAuto
{
    my $self = shift;
    my $orig = shift;

    my $localDebug = 0;

    my $fullpath = $self->fullpath();
    my $rangeref = $HeaderDoc::perHeaderRanges{$fullpath};

    if ($localDebug) {
	print STDERR "FULLPATH: $fullpath\n";

	foreach my $x (keys %HeaderDoc::perHeaderRanges) {
		print STDERR "PHR{$x} = ".$HeaderDoc::perHeaderRanges{$x}."\n";
	}
    }

    my @ranges = @{$rangeref};
    my $linenum = $self->linenum();

    my $string = "";

    print STDERR "IN AVAILABILITYAUTO (name is ".$self->name()."\n" if ($localDebug);
    foreach my $rangeref (@ranges) {
	print STDERR "RANGE $rangeref\n" if ($localDebug);
	my $range = ${$rangeref};
	bless($range, "HeaderDoc::LineRange");
	if ($range->inrange($linenum)) {
	    my $newbit = $range->text();
	    my @pieces = split(/\;/, $newbit);
	    foreach my $piece (@pieces) {
	      my $nvpiece = $piece; $nvpiece =~ s/10\..*$//s;
		# print STDERR "SEARCH $string $newbit";
	      my $found = -1;
	      if (($found = index(lc $orig, lc $nvpiece)) == -1) {
	        if (($found = index(lc $string, lc $nvpiece)) == -1) {
		    if (length($string)) {
			$string .= "  ";
		    }
		    $string .= $piece.".";
		}
	      }
	    }
	}
    }
    print STDERR "LEAVING AVAILABILITYAUTO (RETURN IS $string)\n" if ($localDebug);
    return $string;
}

sub availability {
    my $self = shift;

    if (@_) {
        $self->{AVAILABILITY} = shift;
    }
    my $string = $self->{AVAILABILITY};
    my $add = $self->availabilityAuto($string);
    if (length($string) && length($add)) {
	$string .= "  ";
    }
    return $string.$add;
}

sub lang {
    my $self = shift;

    if (@_) {
        $self->{LANG} = shift;
    }
    return $self->{LANG};
}

sub sublang {
    my $self = shift;

    if (@_) {
	my $sublang = shift;

	if ($sublang eq "occCat") { $sublang = "occ"; }
        $self->{SUBLANG} = $sublang;
    }
    return $self->{SUBLANG};
}

sub updated {
    my $self = shift;
    my $localdebug = 0;
    
    if (@_) {
	my $updated = shift;
        # $self->{UPDATED} = shift;
	my $month; my $day; my $year;

	$month = $day = $year = $updated;

	print STDERR "updated is $updated\n" if ($localdebug);
	if (!($updated =~ /\d\d\d\d-\d\d-\d\d/o )) {
	    if (!($updated =~ /\d\d-\d\d-\d\d\d\d/o )) {
		if (!($updated =~ /\d\d-\d\d-\d\d/o )) {
		    # my $fullpath = $HeaderDoc::headerObject->fullpath();
		    my $fullpath = $self->fullpath();
		    my $linenum = $self->linenum();
		    warn "$fullpath:$linenum: warning: Bogus date format: $updated.\n";
		    warn "$fullpath:$linenum: warning: Valid formats are MM-DD-YYYY, MM-DD-YY, and YYYY-MM-DD\n";
		    return $self->{UPDATED};
		} else {
		    $month =~ s/(\d\d)-\d\d-\d\d/$1/smog;
		    $day =~ s/\d\d-(\d\d)-\d\d/$1/smog;
		    $year =~ s/\d\d-\d\d-(\d\d)/$1/smog;

                    my $century;
                    $century = `date +%C`;
                    $century *= 100; 
                    $year += $century;
                    # $year += 2000;
                    print STDERR "YEAR: $year" if ($localdebug);
		}
	    } else {
		print STDERR "03-25-2003 case.\n" if ($localdebug);
		    $month =~ s/(\d\d)-\d\d-\d\d\d\d/$1/smog;
		    $day =~ s/\d\d-(\d\d)-\d\d\d\d/$1/smog;
		    $year =~ s/\d\d-\d\d-(\d\d\d\d)/$1/smog;
	    }
	} else {
		    $year =~ s/(\d\d\d\d)-\d\d-\d\d/$1/smog;
		    $month =~ s/\d\d\d\d-(\d\d)-\d\d/$1/smog;
		    $day =~ s/\d\d\d\d-\d\d-(\d\d)/$1/smog;
	}
	$month =~ s/\n//smog;
	$day =~ s/\n//smog;
	$year =~ s/\n//smog;
	$month =~ s/\s*//smog;
	$day =~ s/\s*//smog;
	$year =~ s/\s*//smog;

	# Check the validity of the modification date

	my $invalid = 0;
	my $mdays = 28;
	if ($month == 2) {
		if ($year % 4) {
			$mdays = 28;
		} elsif ($year % 100) {
			$mdays = 29;
		} elsif ($year % 400) {
			$mdays = 28;
		} else {
			$mdays = 29;
		}
	} else {
		my $bitcheck = (($month & 1) ^ (($month & 8) >> 3));
		if ($bitcheck) {
			$mdays = 31;
		} else {
			$mdays = 30;
		}
	}

	if ($month > 12 || $month < 1) { $invalid = 1; }
	if ($day > $mdays || $day < 1) { $invalid = 1; }
	if ($year < 1970) { $invalid = 1; }

	if ($invalid) {
		# my $fullpath = $HeaderDoc::headerObject->fullpath();
		my $fullpath = $self->fullpath();
		my $linenum = $self->linenum();
		warn "$fullpath:$linenum: warning: Invalid date (year = $year, month = $month, day = $day).\n";
		warn "$fullpath:$linenum: warning: Valid formats are MM-DD-YYYY, MM-DD-YY, and YYYY-MM-DD\n";
		return $self->{UPDATED};
	} else {
		$self->{UPDATED} = HeaderDoc::HeaderElement::strdate($month-1, $day, $year);
		print STDERR "date set to ".$self->{UPDATED}."\n" if ($localdebug);
	}
    }
    return $self->{UPDATED};
}

sub linkageState {
    my $self = shift;
    
    if (@_) {
        $self->{LINKAGESTATE} = shift;
    }
    return $self->{LINKAGESTATE};
}

sub accessControl {
    my $self = shift;
    
    if (@_) {
        $self->{ACCESSCONTROL} = shift;
    }
    return $self->{ACCESSCONTROL};
}


sub printObject {
    my $self = shift;
    my $dec = $self->declaration();
 
    print STDERR "------------------------------------\n";
    print STDERR "HeaderElement\n";
    print STDERR "name: $self->{NAME}\n";
    print STDERR "abstract: $self->{ABSTRACT}\n";
    print STDERR "declaration: $dec\n";
    print STDERR "declaration in HTML: $self->{DECLARATIONINHTML}\n";
    print STDERR "discussion: $self->{DISCUSSION}\n";
    print STDERR "linkageState: $self->{LINKAGESTATE}\n";
    print STDERR "accessControl: $self->{ACCESSCONTROL}\n\n";
    print STDERR "Tagged Parameters:\n";
    my $taggedParamArrayRef = $self->{TAGGEDPARAMETERS};
    if ($taggedParamArrayRef) {
	my $arrayLength = @{$taggedParamArrayRef};
	if ($arrayLength > 0) {
	    &printArray(@{$taggedParamArrayRef});
	}
	print STDERR "\n";
    }
    my $fieldArrayRef = $self->{CONSTANTS};
    if ($fieldArrayRef) {
        my $arrayLength = @{$fieldArrayRef};
        if ($arrayLength > 0) {
            &printArray(@{$fieldArrayRef});
        }
        print STDERR "\n";
    }
}

sub linkfix {
    my $self = shift;
    my $inpString = shift;
    my @parts = split(/\</, $inpString);
    my $first = 1;
    my $outString = "";
    my $localDebug = 0;

    print STDERR "Parts:\n" if ($localDebug);
    foreach my $part (@parts) {
	print STDERR "$part\n" if ($localDebug);
	if ($first) {
		$outString .= $part;
		$first = 0;
	} else {
		if ($part =~ /^\s*A\s+/sio) {
			$part =~ /^(.*?>)/so;
			my $linkpart = $1;
			my $linkre = quote($linkpart);
			my $rest = $part;
			$rest =~ s/^$linkre//s;

			print STDERR "Found link.\nlinkpart: $linkpart\nrest: $rest\n" if ($localDebug);

			if ($linkpart =~ /target\=\".*\"/sio) {
			    print STDERR "link ok\n" if ($localDebug);
			    $outString .= "<$part";
			} else {
			    print STDERR "needs fix.\n" if ($localDebug);
			    $linkpart =~ s/\>$//so;
			    $outString .= "<$linkpart target=\"_top\">$rest";
			}
		} else {
			$outString .= "<$part";
		}
	}
    }

    return $outString;
}

# /*! strdate WARNING: Month is 0-11, not 1-12. */
sub strdate
{
    my $month = shift;
    my $day = shift;
    my $year = shift;
    my $format = $HeaderDoc::datefmt;
    if (!defined $format) {
	$format = "%B %d, %Y";
    }

    my $time_t = mktime(0, 0, 0, $day, $month, $year-1900);
    my ($sec,$min,$hour,$mday,$mon,$yr,$wday,$yday,$isdst) = localtime($time_t);
    my $time = strftime($format, $sec, $min, $hour,
	$mday, $mon, $yr, $wday, $yday, $isdst);
    return $time;

    # print STDERR "format $format\n";

    if ($format eq "") {
	return "$month/$day/$year";
    } else  {
	my $dateString = "";
	my $firstsep = "";
	if ($format =~ /^.(.)/o) {
	  $firstsep = $1;
	}
	my $secondsep = "";
	if ($format =~ /^...(.)./o) {
	  $secondsep = $1;
	}
	SWITCH: {
	  ($format =~ /^M/io) && do { $dateString .= "$month$firstsep" ; last SWITCH; };
	  ($format =~ /^D/io) && do { $dateString .= "$day$firstsep" ; last SWITCH; };
	  ($format =~ /^Y/io) && do { $dateString .= "$year$firstsep" ; last SWITCH; };
	  print STDERR "Unknown date format ($format) in config file[1]\n";
	  print STDERR "Assuming MDY\n";
	  return "$month/$day/$year";
	}
	SWITCH: {
	  ($format =~ /^..M/io) && do { $dateString .= "$month$secondsep" ; last SWITCH; };
	  ($format =~ /^..D/io) && do { $dateString .= "$day$secondsep" ; last SWITCH; };
	  ($format =~ /^..Y/io) && do { $dateString .= "$year$secondsep" ; last SWITCH; };
	  ($firstsep eq "") && do { last SWITCH; };
	  print STDERR "Unknown date format ($format) in config file[2]\n";
	  print STDERR "Assuming MDY\n";
	  return "$month/$day/$year";
	}
	SWITCH: {
	  ($format =~ /^....M/io) && do { $dateString .= "$month" ; last SWITCH; };
	  ($format =~ /^....D/io) && do { $dateString .= "$day" ; last SWITCH; };
	  ($format =~ /^....Y/io) && do { $dateString .= "$year" ; last SWITCH; };
	  ($secondsep eq "") && do { last SWITCH; };
	  print STDERR "Unknown date format ($format) in config file[3]\n";
	  print STDERR "Assuming MDY\n";
	  return "$month/$day/$year";
	}
	return $dateString;
    }
}

sub setStyle
{
    my $self = shift;
    my $name = shift;
    my $style = shift;

    $style =~ s/^\s*//sgo;
    $style =~ s/\s*$//sgo;

    if (length($style)) {
	$CSS_STYLES{$name} = $style;
	$HeaderDoc::use_styles = 1;
    }
}

# /*! 
#     This code inserts the discussion from the superclass wherever
#     <hd_ihd/> appears if possible (i.e. where @inheritDoc (HeaderDoc)
#     or {@inheritDoc} (JavaDoc) appears in the original input material.
#     @abstract HTML/XML fixup code to insert superclass discussions
#  */
sub fixup_inheritDoc
{
    my $self = shift;
    my $html = shift;
    my $newhtml = "";

    my @pieces = split(/</, $html);

    foreach my $piece (@pieces) {
	if ($piece =~ s/^hd_ihd\/>//so) {
		if ($self->outputformat() eq "hdxml") {
			$newhtml .= "<hd_ihd>";
		}
		$newhtml .= $self->inheritDoc();
		if ($self->outputformat() eq "hdxml") {
			$newhtml .= "</hd_ihd>";
		}
		$newhtml .= "$piece";
	} else {
		$newhtml .= "<$piece";
	}
    }
    $newhtml =~ s/^<//so;

    return $newhtml;
}

# /*! @function
#     This code inserts values wherever <hd_value/> appears (i.e. where
#     @value (HeaderDoc) or {@value} (JavaDoc) appears in the original
#     input material.
#     @abstract HTML/XML fixup code to insert values
#  */
sub fixup_values
{
    my $self = shift;
    my $html = shift;
    my $newhtml = "";

    my @pieces = split(/</, $html);

    foreach my $piece (@pieces) {
	if ($piece =~ s/^hd_value\/>//so) {
		if ($self->outputformat() eq "hdxml") {
			$newhtml .= "<hd_value>";
		}
		$newhtml .= $self->value();
		if ($self->outputformat() eq "hdxml") {
			$newhtml .= "</hd_value>";
		}
		$newhtml .= "$piece";
	} else {
		$newhtml .= "<$piece";
	}
    }
    $newhtml =~ s/^<//so;

    return $newhtml;
}

sub checkDeclaration
{
    my $self = shift;
    my $class = ref($self) || $self;
    my $keyword = "";
    my $lang = $self->lang();
    my $name = $self->name();
    my $fullpath = $self->fullpath();
    my $line = $self->linenum();
    my $exit = 0;

    # This function, bugs notwithstanding, is no longer useful.
    return 1;
}

sub getStyle
{
    my $self = shift;
    my $name = shift;

   return $CSS_STYLES{$name};
}

sub styleSheet
{
    my $self = shift;
    my $TOC = shift;
    my $css = "";
    my $stdstyles = 1;

# {
# print STDERR "style test\n";
# $self->setStyle("function", "background:#ffff80; color:#000080;");
# $self->setStyle("text", "background:#000000; color:#ffffff;");
# print STDERR "results:\n";
	# print STDERR "function: \"".$self->getStyle("function")."\"\n";
	# print STDERR "text: \"".$self->getStyle("text")."\"\n";
# }


    if ($TOC) {
	if (defined($HeaderDoc::externalTOCStyleSheets)) {
		$css .= $self->doExternalStyle($HeaderDoc::externalTOCStyleSheets);
		$stdstyles = 0;
	} elsif ($HeaderDoc::externalStyleSheets) {
		$css .= $self->doExternalStyle($HeaderDoc::externalStyleSheets);
		$stdstyles = 0;
	}
    } elsif ($HeaderDoc::externalStyleSheets) {
	$css .= $self->doExternalStyle($HeaderDoc::externalStyleSheets);
	$stdstyles = 0;
    }
    $css .= "<style type=\"text/css\">";
    $css .= "<!--";
    if ($TOC) {
	if (defined($HeaderDoc::tocStyleImports)) {
		$css .= "$HeaderDoc::tocStyleImports ";
		$stdstyles = 0;
	} elsif ($HeaderDoc::styleImports) {
		$css .= "$HeaderDoc::styleImports ";
		$stdstyles = 0;
	}
    } else {
	if ($HeaderDoc::styleImports) {
		$css .= "$HeaderDoc::styleImports ";
		$stdstyles = 0;
	}
    }
    foreach my $stylename (sort strcasecmp keys %CSS_STYLES) {
	my $styletext = $CSS_STYLES{$stylename};
	$css .= ".$stylename {$styletext}";
    }

    if ($stdstyles) {
	$css .= "a:link {text-decoration: none; font-family: lucida grande, geneva, helvetica, arial, sans-serif; font-size: small; color: #0000ff;}";
	$css .= "a:visited {text-decoration: none; font-family: lucida grande, geneva, helvetica, arial, sans-serif; font-size: small; color: #0000ff;}";
	$css .= "a:visited:hover {text-decoration: underline; font-family: lucida grande, geneva, helvetica, arial, sans-serif; font-size: small; color: #ff6600;}";
	$css .= "a:active {text-decoration: none; font-family: lucida grande, geneva, helvetica, arial, sans-serif; font-size: small; color: #ff6600;}";
	$css .= "a:hover {text-decoration: underline; font-family: lucida grande, geneva, helvetica, arial, sans-serif; font-size: small; color: #ff6600;}";
	$css .= "h4 {text-decoration: none; font-family: lucida grande, geneva, helvetica, arial, sans-serif; font-size: tiny; font-weight: bold;}"; # bold
	$css .= "body {text-decoration: none; font-family: lucida grande, geneva, helvetica, arial, sans-serif; font-size: 10pt;}"; # bold
	$css .= ".list_indent { margin-left: 40px; }";
	$css .= ".declaration_indent { margin-left: 40px; }";
	$css .= ".param_indent { margin-left: 40px; }";
	$css .= ".group_indent { margin-left: 40px; }";
	$css .= ".group_desc_indent { margin-left: 20px; }";
	$css .= ".warning_indent { margin-left: 40px; }";
	$css .= ".important_indent { margin-left: 40px; }";
	$css .= ".hd_tocAccess { font-style: italic; font-size: 10px; font-weight: normal; color: #303030; }";
    }

    if ($HeaderDoc::styleSheetExtras) {
	$css .= $HeaderDoc::styleSheetExtras;
    }

    $css .= "-->";
    $css .= "</style>";

    return $css;
}

sub documentationBlock
{
    my $self = shift;
    my $composite = shift;
    my $contentString;
    my $name = $self->name();
    my $desc = $self->discussion();
    my $checkDisc = $self->halfbaked_discussion();
    my $throws = "";
    my $abstract = $self->abstract();
    my $availability = $self->availability();
    my $namespace = ""; if ($self->can("namespace")) { $namespace = $self->namespace(); }
    my $updated = $self->updated();
    my $declaration = "";
    my $result = "";
    my $localDebug = 0;
    # my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();
    my $fullpath = $self->fullpath();
    my $linenum = $self->linenum();
    my $list_attributes = $self->getAttributeLists($composite);
    my $short_attributes = $self->getAttributes(0);
    my $long_attributes = $self->getAttributes(1);
    my $class = ref($self) || $self;
    my $apio = $self->apiOwner();
    my $apioclass = ref($apio) || $apio;
    my $apiref = "";
    my $headlevel = "h3";

    my $newTOC = $HeaderDoc::newTOC;
    my $showDiscussionHeading = 1;

    if ($self->{HIDEDOC}) { return ""; }

    # Only use this style for API Owners.
    if ($self->isAPIOwner()) {
	$headlevel = "h1";
	if ($newTOC) {
		$showDiscussionHeading = 0;
	}

	if (($checkDisc !~ /\S/) && ($abstract !~ /\S/)) {
		my $linenum = $self->linenum();
        	warn "$fullpath:$linenum: No header or class discussion/abstract found. Creating dummy file for default content page.\n";
		$abstract .= $HeaderDoc::defaultHeaderComment; # "Use the links in the table of contents to the left to access documentation.<br>\n";    
	}
    } else {
	$newTOC = 0;
	$declaration = $self->declarationInHTML();
    }

# print STDERR "NAME: $name APIOCLASS: $apioclass APIUID: ".$self->apiuid()."\n";

    if ($self->can("result")) { $result = $self->result(); }
    if ($self->can("throws")) { $throws = $self->throws(); }

    if ($self->noRegisterUID()) {
	cluck("BT\n");
	die("Unexpected unregistered object being inserted into content.  Object is $self, name is ".$self->name().", header is ".$apio->name()."\n");
    }


    # $name =~ s/\s*//smgo;

    $contentString .= "<hr>";
    # my $uid = "//$apiUIDPrefix/c/func/$name";
       
    # registerUID($uid);
    # $contentString .= "<a name=\"$uid\"></a>\n"; # apple_ref marker

    my ($constantsref, $fieldsref, $paramsref, $fieldHeading, $func_or_method)=$self->apirefSetup();
    my @constants = @{$constantsref};
    my @fields = @{$fieldsref};
    my @params = @{$paramsref};

    $apiref = $self->apiref($composite);

    if (!$self->isAPIOwner()) {
	$contentString .= $apiref;
    }

    $contentString .= "<table border=\"0\"  cellpadding=\"2\" cellspacing=\"2\" width=\"300\">";
    $contentString .= "<tr>";
    $contentString .= "<td valign=\"top\" height=\"12\" colspan=\"5\">";
    my $urlname = sanitize($name, 1);
    $contentString .= "<$headlevel><a name=\"$urlname\">$name</a></$headlevel>\n";
    $contentString .= "</td>";
    $contentString .= "</tr></table>";
    if (!$newTOC) { $contentString .= "<hr>"; }
    my $attstring = ""; my $c = 0;
    if (length($short_attributes)) {
        $attstring .= $short_attributes;
	$c++;
    }
    if (length($list_attributes)) {
        $attstring .= $list_attributes;
	$c++;
    }
    # print STDERR "ATS: $attstring\n";
    if ($newTOC) {
	if ($c == 2) {
		$attstring =~ s/<\/table><\/div>\s*<div.*?><table.*?>//s;
	}
	$attstring =~ s/<\/table><\/div>\s*$//s;
    }
    if (!$newTOC) { $attstring .= "<dl>"; }
    if (length($throws)) {
	if ($newTOC) {
		if (!$c) {
			$attstring .= "<div class=\"spec_sheet_info_box\"><table cellspacing=\"0\" class=\"specbox\">\n";
		}
		$attstring .= "<tr><td scope=\"row\"><b>Throws:</b></td><td><div style=\"margin-bottom:1px\"><div class=\"content_text\">$throws</div></div></td></tr>\n";
	} else {
        	$attstring .= "<dt><i>Throws:</i></dt>\n<dd>$throws</dd>\n";
	}
	$c++;
    }
	my $includeList = "";
	if ($class eq "HeaderDoc::Header") {
	    my $includeref = $HeaderDoc::perHeaderIncludes{$fullpath};
	    if ($includeref) {
		my @includes = @{$includeref};

		my $first = 1;
		foreach my $include (@includes) {
			my $localDebug = 0;
			print STDERR "Included file: $include\n" if ($localDebug);

			if (!$first) {
				if ($newTOC) {$includeList .= "<br>\n"; }
				else {$includeList .= ",\n"; }
			}
			my $xmlinc = $self->textToXML($include);

			my $includeguts = $include;
			$includeguts =~ s/[<\"](.*)[>\"]/$1/so;

			my $includefile = basename($includeguts);

			my $ref = $self->genRefSub("doc", "header", $includefile, "");

			$includeList .= "<!-- a logicalPath=\"$ref\" -->$xmlinc<!-- /a -->";
			$first = 0;
		}

	    }
	}
	if (length($includeList)) {
		if ($newTOC) {
			if (!$c) {
				$attstring .= "<div class=\"spec_sheet_info_box\"><table cellspacing=\"0\" class=\"specbox\">\n";
			}
			$attstring .= "<tr><td scope=\"row\"><b>Includes:</b></td><td><div style=\"margin-bottom:1px\"><div class=\"content_text\">$includeList</div></div></td></tr>\n";
		} else {
			$attstring .= "<b>Includes:</b> ";
			$attstring .= $includeList;
			$attstring .= "<br>\n";
		}
		$c++;
	}


    # if (length($abstract)) {
        # $contentString .= "<dt><i>Abstract:</i></dt>\n<dd>$abstract</dd>\n";
    # }
    if ($newTOC) { 
	if ($c) { $attstring .= "</table></div>\n"; }

	# Otherwise we do this later.
	$contentString .= $attstring;
    } else {
	if ($attstring =~ /<dl>\s*$/s) {
		$attstring =~ s/<dl>\s*$//s;
	} else {
		$attstring .= "</dl>";
	}
    }

    if ($newTOC) {
	$contentString .= "<h2>".$HeaderDoc::introductionName."</h2>\n";
    }

    my $uid = $self->apiuid();

    if (length($abstract)) {
	$showDiscussionHeading = 1;
        # $contentString .= "<dt><i>Abstract:</i></dt>\n<dd>$abstract</dd>\n";
        $contentString .= "<p><!-- begin abstract -->";
	if ($self->can("isFramework") && $self->isFramework()) {
		$contentString .= "<!-- headerDoc=frameworkabstract;uid=".$uid.";name=start -->\n";
	}
	$contentString .= "$abstract";
	if ($self->can("isFramework") && $self->isFramework()) {
		$contentString .= "<!-- headerDoc=frameworkabstract;uid=".$uid.";name=end -->\n";
	}
	$contentString .= "<!-- end abstract --></p>\n";
    }

    if (!$newTOC) {
	# Otherwise we do this earlier.
	$contentString .= $attstring;
    }

    my $accessControl = "";
    if ($self->can("accessControl")) {
	$accessControl = $self->accessControl();
    }
    my $includeAccess = 0;
    if ($accessControl ne "") { $includeAccess = 1; }
    if ($self->can("isProperty") && $self->isProperty()) { $includeAccess = 0; }
    if ($self->class eq "HeaderDoc::Method") { $includeAccess = 0; }
    if ($self->class eq "HeaderDoc::PDefine") { $includeAccess = 0; }

    $contentString .= "<div class='declaration_indent'>\n";
    if (!$self->isAPIOwner()) {
	if ($includeAccess) {
		$contentString .= "<pre><tt>$accessControl</tt>\n<br>$declaration</pre>\n";
	} else {
		$contentString .= "<pre>$declaration</pre>\n";
	}
    }
    $contentString .= "</div>\n";

    my @parameters_or_fields = ();
    my @callbacks = ();
    foreach my $element (@params) {
	if ($element->isCallback()) {
		push(@callbacks, $element);
	} elsif (!$element->{ISDEFINE}) {
		push(@parameters_or_fields, $element);
	}
    }
    foreach my $element (@fields) {
	if ($element->isCallback()) {
		push(@callbacks, $element);
	} elsif (!$element->{ISDEFINE}) {
		push(@parameters_or_fields, $element);
	}
    }

    my @includedDefines = ();
    if ($self->{INCLUDED_DEFINES}) {
	@includedDefines = @{$self->{INCLUDED_DEFINES}};
    }
    my $arrayLength = @includedDefines;
    if (($arrayLength > 0)) {
        my $paramContentString;

	$showDiscussionHeading = 1;
        foreach my $element (@includedDefines) {

	# print "ELT IS $element\n";

	    if ($self->{HIDESINGLETONS}) {
		if ($element->{MAINOBJECT}) {
			$element = ${$element->{MAINOBJECT}};
			# print "ELEMENT NOW $element\n";
			bless($element, "HeaderDoc::HeaderElement");
			# print "ELEMENT NOW $element\n";
			bless($element, $element->class());
			# print "ELEMENT NOW $element\n";
			# print "ELEMENT NAME ".$element->{NAME}."\n";
		}
	    }
            my $fName = $element->name();
            my $fDesc = $element->discussion();
	    my $fType = "";
	    my $apiref = "";

	    if ($self->can("type")) { $fType = $element->type(); }

	    $apiref = $element->apiref($composite); # , $apiRefType);

            if (length ($fName) &&
		(($fType eq 'field') || ($fType eq 'constant') || ($fType eq 'funcPtr') ||
		 ($fType eq ''))) {
                    # $paramContentString .= "<tr><td align=\"center\"><code>$fName</code></td><td>$fDesc</td></tr>\n";
                    $paramContentString .= "<dt>$apiref<code>$fName</code></dt><dd>$fDesc</dd>\n";
            } elsif ($fType eq 'callback') {
		my @userDictArray = $element->userDictArray(); # contains elements that are hashes of param name to param doc
		my $paramString;
		foreach my $hashRef (@userDictArray) {
		    while (my ($param, $disc) = each %{$hashRef}) {
			$paramString .= "<dt><b><code>$param</code></b></dt>\n<dd>$disc</dd>\n";
		    }
    		    if (length($paramString)) {
			$paramString = "<dl>\n".$paramString."\n</dl>\n";
		    };
		}
		# $contentString .= "<tr><td><code>$fName</code></td><td>$fDesc<br>$paramString</td></tr>\n";
		$contentString .= "<dt><code>$fName</code></dt><dd>$fDesc<br>$paramString</dd>\n";
	    } else {
		# my $fullpath = $HeaderDoc::headerObject->name();
		my $classname = ref($self) || $self;
		$classname =~ s/^HeaderDoc:://o;
		if (!$HeaderDoc::ignore_apiuid_errors) {
			print STDERR "$fullpath:$linenum: warning: $classname ($name) field with name $fName has unknown type: $fType\n";
		}
	    }
        }
        if (length ($paramContentString)){
            $contentString .= "<h5 class=\"tight\"><font face=\"Lucida Grande,Helvetica,Arial\">Included Defines</font></h5>\n";       
            $contentString .= "<div class='param_indent'>\n";
            # $contentString .= "<table border=\"1\"  width=\"90%\">\n";
            # $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
            $contentString .= "<dl>\n";
            $contentString .= $paramContentString;
            # $contentString .= "</table>\n</div>\n";
            $contentString .= "</dl>\n";
	    $contentString .= "</div>\n";
        }
    }

    $arrayLength = @parameters_or_fields;
    if (($arrayLength > 0) && (length($fieldHeading))) {
        my $paramContentString;

	$showDiscussionHeading = 1;
        foreach my $element (@parameters_or_fields) {
            my $fName = $element->name();
            my $fDesc = $element->discussion();
	    my $fType = "";
	    my $apiref = "";

	    if ($self->can("type")) { $fType = $element->type(); }

	    $apiref = $element->apiref($composite); # , $apiRefType);

            if (length ($fName) &&
		(($fType eq 'field') || ($fType eq 'constant') || ($fType eq 'funcPtr') ||
		 ($fType eq ''))) {
                    # $paramContentString .= "<tr><td align=\"center\"><code>$fName</code></td><td>$fDesc</td></tr>\n";
                    $paramContentString .= "<dt>$apiref<code>$fName</code></dt><dd>$fDesc</dd>\n";
            } elsif ($fType eq 'callback') {
		my @userDictArray = $element->userDictArray(); # contains elements that are hashes of param name to param doc
		my $paramString;
		foreach my $hashRef (@userDictArray) {
		    while (my ($param, $disc) = each %{$hashRef}) {
			$paramString .= "<dt><b><code>$param</code></b></dt>\n<dd>$disc</dd>\n";
		    }
    		    if (length($paramString)) {
			$paramString = "<dl>\n".$paramString."\n</dl>\n";
		    };
		}
		# $contentString .= "<tr><td><code>$fName</code></td><td>$fDesc<br>$paramString</td></tr>\n";
		$contentString .= "<dt><code>$fName</code></dt><dd>$fDesc<br>$paramString</dd>\n";
	    } else {
		# my $fullpath = $HeaderDoc::headerObject->name();
		my $classname = ref($self) || $self;
		$classname =~ s/^HeaderDoc:://o;
		if (!$HeaderDoc::ignore_apiuid_errors) {
			print STDERR "$fullpath:$linenum: warning: $classname ($name) field with name $fName has unknown type: $fType\n";
		}
	    }
        }
        if (length ($paramContentString)){
            $contentString .= "<h5 class=\"tight\"><font face=\"Lucida Grande,Helvetica,Arial\">$fieldHeading</font></h5>\n";       
            $contentString .= "<div class='param_indent'>\n";
            # $contentString .= "<table border=\"1\"  width=\"90%\">\n";
            # $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
            $contentString .= "<dl>\n";
            $contentString .= $paramContentString;
            # $contentString .= "</table>\n</div>\n";
            $contentString .= "</dl>\n";
	    $contentString .= "</div>\n";
        }
    }
    if (@constants) {
	$showDiscussionHeading = 1;
        $contentString .= "<h5 class=\"tight\"><font face=\"Lucida Grande,Helvetica,Arial\">Constants</font></h5>\n";       
        $contentString .= "<div class='param_indent'>\n";
        $contentString .= "<dl>\n";
        # $contentString .= "<table border=\"1\"  width=\"90%\">\n";
        # $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
        foreach my $element (@constants) {
            my $cName = $element->name();
            my $cDesc = $element->discussion();
            # my $uid = "//$apiUIDPrefix/c/econst/$cName";
            # registerUID($uid);
            my $uid = $element->apiuid(); # "econst");
            # $contentString .= "<tr><td align=\"center\"><a name=\"$uid\"><code>$cName</code></a></td><td>$cDesc</td></tr>\n";

	    if (!$HeaderDoc::appleRefUsed{$uid} && !$HeaderDoc::ignore_apiuid_errors) {
		# print STDERR "MARKING APIREF $uid used\n";
		$HeaderDoc::appleRefUsed{$uid} = 1;
                $contentString .= "<dt><a name=\"$uid\"><code>$cName</code></a></dt><dd>$cDesc</dd>\n";
	    } else {
                $contentString .= "<dt><code>$cName</code></dt><dd>$cDesc</dd>\n";
	    }
        }
        # $contentString .= "</table>\n</div>\n";
        $contentString .= "</dl>\n</div>\n";
    }

    if (scalar(@callbacks)) {
	$showDiscussionHeading = 1;
        $contentString .= "<h5 class=\"tight\"><font face=\"Lucida Grande,Helvetica,Arial\">Callbacks</font></h5>\n";
        $contentString .= "<div class='param_indent'>\n";
        # $contentString .= "<table border=\"1\"  width=\"90%\">\n";
        # $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
        $contentString .= "<dl>";

	# foreach my $element (@callbacks) {
		# print STDERR "ETYPE: $element->{TYPE}\n";
	# }

        foreach my $element (@callbacks) {
            my $fName = $element->name();
            my $fDesc = $element->discussion();
            my $fType = $element->type();

            if (($fType eq 'field') || ($fType eq 'constant') || ($fType eq 'funcPtr')){
                # $contentString .= "<tr><td><code>$fName</code></td><td>$fDesc</td></tr>\n";
                $contentString .= "<dt><code>$fName</code></dt><dd>$fDesc</dd>\n";
            } elsif ($fType eq 'callback') {
                my @userDictArray = $element->userDictArray(); # contains elements that are hashes of param name to param doc
                my $paramString;
                foreach my $hashRef (@userDictArray) {
                    while (my ($param, $disc) = each %{$hashRef}) {
                        $paramString .= "<dt><b><code>$param</code></b></dt>\n<dd>$disc</dd>\n";
                    }
                    if (length($paramString)) {$paramString = "<dl>\n".$paramString."\n</dl>\n";};
                }
                # $contentString .= "<tr><td><code>$fName</code></td><td>$fDesc<br>$paramString</td></tr>\n";
                $contentString .= "<dt><code>$fName</code></dt><dd>$fDesc<br>$paramString</dd>\n";
            } else {
                my $fullpath = $HeaderDoc::headerObject->name();
		if (!$HeaderDoc::ignore_apiuid_errors) {
                	print STDERR "$fullpath:$linenum: warning: struct/typdef/union ($name) field with name $fName has unknown type: $fType\n";
			# $element->printObject();
		}
            }
        }

        # $contentString .= "</table>\n</div>\n";
        $contentString .= "</dl>\n</div>\n";
    }

    # if (length($desc)) {$contentString .= "<p>$desc</p>\n"; }
    # $contentString .= "<dl>"; # MOVED LOWER
    if (length($result)) { 
	$showDiscussionHeading = 1;
	$contentString .= "<h5 class=\"tight\"><font face=\"Lucida Grande,Helvetica,Arial\">Return Value</font></h5><p><!-- begin return value -->";
        # $contentString .= "$func_or_method result</i></dt><dd>
	$contentString .= "$result\n";
	$contentString .= "<!-- end return value --></p>";
    }
    my $stripdesc = $checkDisc;
    $stripdesc =~ s/<br>/\n/sg;
    if ($stripdesc =~ /\S/) {
	if ($showDiscussionHeading) {
		$contentString .= "<h5 class=\"tight\"><font face=\"Lucida Grande,Helvetica,Arial\">Discussion</font>\n";
	}
	$contentString .= "</h5><!-- begin discussion -->";
	if ($self->can("isFramework") && $self->isFramework()) {
		$contentString .= "<!-- headerDoc=frameworkdiscussion;uid=".$uid.";name=start -->\n";
	}
	$contentString .= $desc;
	if ($self->can("isFramework") && $self->isFramework()) {
		$contentString .= "<!-- headerDoc=frameworkdiscussion;uid=".$uid.";name=end -->\n";
	}
	$contentString .= "<!-- end discussion -->\n";
    }

    # if (length($desc)) {$contentString .= "<p>$desc</p>\n"; }
    if (length($long_attributes)) {
        $contentString .= $long_attributes;
    }

    my $late_attributes = "";
    if (length($namespace)) {
            $late_attributes .= "<dt><i>Namespace</i></dt><dd>$namespace</dd>\n";
    }
    if (length($availability)) {
        $late_attributes .= "<dt><i>Availability</i></dt><dd>$availability</dd>\n";
    }
    if (length($updated)) {
        $late_attributes .= "<dt><i>Updated:</i></dt><dd>$updated</dd>\n";
    }
    if (length($late_attributes)) {
	$contentString .= "<dl>".$late_attributes."</dl>\n";
    }
    # $contentString .= "<hr>\n";

    my $value_fixed_contentString = $self->fixup_values($contentString);

    return $value_fixed_contentString;    
}


sub taggedParameters {
    my $self = shift;
    if (@_) { 
        @{ $self->{TAGGEDPARAMETERS} } = @_;
    }
    ($self->{TAGGEDPARAMETERS}) ? return @{ $self->{TAGGEDPARAMETERS} } : return ();
}

sub compositePageUID {
    my $self = shift;

    my $uid = "";

    if ($self->can("compositePageAPIUID")) {
	$uid = $self->compositePageAPIUID();
    } else {
	my $apiUIDPrefix = quote(HeaderDoc::APIOwner->apiUIDPrefix());
	$uid = $self->apiuid();
	$uid =~ s/\/\/$apiUIDPrefix\//\/\/$apiUIDPrefix\/doc\/compositePage\//s;
    }

    # registerUID($uid);
    return $uid;
}

sub addTaggedParameter {
    my $self = shift;
    if (@_) { 
        push (@{$self->{TAGGEDPARAMETERS}}, @_);
	my @arr = @{$self->{TAGGEDPARAMETERS}};
	# print "OBJ IS ".\$arr[scalar(@arr) - 1]."\n";
	return \$arr[scalar(@arr) - 1];
    }
    return undef; # return @{ $self->{TAGGEDPARAMETERS} };
}

# sub parsedParameters
# {
    # # Override this in subclasses where relevant.
    # return ();
# }

# Compare tagged parameters to parsed parameters (for validation)
sub taggedParsedCompare {
    my $self = shift;
    my @tagged = $self->taggedParameters();
    my @parsed = $self->parsedParameters();
    my $funcname = $self->name();
    my $fullpath = $self->fullpath();
    my $linenum = $self->linenum();
    my $tpcDebug = 0;
    my $struct = 0;
    my $strict = $HeaderDoc::force_parameter_tagging;
    my %taggednames = ();
    my %parsednames = ();

    if ($self->{TPCDONE}) { return; }
    if (!$HeaderDoc::ignore_apiuid_errors) {
	$self->{TPCDONE} = 1;
    }

    my @fields = ();
    if ($self->can("fields")) {
	$struct = 1;
	@fields = $self->fields();
    }

    my @constants = $self->constants();

    my $apiOwner = $self->isAPIOwner();

    if (!$self->suppressChildren()) {
      foreach my $myfield (@fields) { 
	# $taggednames{$myfield} = $myfield;
	my $nscomp = $myfield->name();
	$nscomp =~ s/\s*//sgo;
	$nscomp =~ s/^\**//sso;
	if (!length($nscomp)) {
		$nscomp = $myfield->type();
		$nscomp =~ s/\s*//sgo;
	}
	$taggednames{$nscomp}=$myfield;
	print STDERR "Mapped Field $nscomp -> $myfield\n" if ($tpcDebug);
      }
      if (!$apiOwner) {
	foreach my $myconstant (@constants) {
		my $nscomp = $myconstant->name();
		print STDERR "CONST: $nscomp\n" if ($tpcDebug);
		$nscomp =~ s/\s*//sgo;
		$nscomp =~ s/^\**//sso;
		if (!length($nscomp)) {
			$nscomp = $myconstant->type();
			$nscomp =~ s/\s*//sgo;
		}
		$taggednames{$nscomp}=$myconstant;
		print STDERR "COUNT: ".(keys %taggednames)."\n" if ($tpcDebug);
		print STDERR "Mapped Constant $nscomp -> $myconstant\n" if ($tpcDebug);
	}
      }
    }
    foreach my $mytaggedparm (@tagged) { 
		my $nscomp = $mytaggedparm->name();
		$nscomp =~ s/\s*//sgo;
		$nscomp =~ s/^\**//sso;
		if (!length($nscomp)) {
			$nscomp = $mytaggedparm->type();
			$nscomp =~ s/\s*//sgo;
		}
		$taggednames{$nscomp}=$mytaggedparm;
		print STDERR "Mapped Tagged Parm $nscomp -> $mytaggedparm\n" if ($tpcDebug);
    }

    if ($HeaderDoc::ignore_apiuid_errors) {
	# This avoids warnings generated by the need to
	# run documentationBlock once prior to the actual parse
	# to generate API references.
	if ($tpcDebug) { print STDERR "ignore_apiuid_errors set.  Skipping tagged/parsed comparison.\n"; }
	# return;
    }

    if ($self->lang() ne "C") {
	if ($tpcDebug) { print STDERR "Language not C.  Skipping tagged/parsed comparison.\n"; }
	return;
    }

    if ($tpcDebug) {
	print STDERR "Tagged Parms:\n" if ($tpcDebug);
	foreach my $obj (@tagged) {
		bless($obj, "HeaderDoc::HeaderElement");
		bless($obj, $obj->class());
		print STDERR "TYPE: \"" .$obj->type . "\"\nNAME: \"" . $obj->name() ."\"\n";
	}
    }

	print STDERR "Parsed Parms:\n" if ($tpcDebug);
	foreach my $obj (@parsed) {
		bless($obj, "HeaderDoc::HeaderElement");
		bless($obj, $obj->class());
		my $type = "";
		if ($obj->can("type")) { $type = $obj->type(); }
		print STDERR "TYPE:" .$type . "\nNAME:\"" . $obj->name()."\"\n" if ($tpcDebug);
		my $nscomp = $obj->name();
		$nscomp =~ s/\s*//sgo;
		$nscomp =~ s/^\**//sso;
		if (!length($nscomp)) {
			$nscomp = $type;
			$nscomp =~ s/\s*//sgo;
		}
		$parsednames{$nscomp}=$obj;
	}

    print STDERR "Checking Parameters and Stuff.\n" if ($tpcDebug);
    foreach my $taggedname (keys %taggednames) {
	    my $searchname = $taggedname;
	    my $tp = $taggednames{$taggedname};
	    if ($tp->type eq "funcPtr") {
		$searchname = $tp->name();
		$searchname =~ s/\s*//sgo;
	    }
	    print STDERR "TN: $taggedname\n" if ($tpcDebug);
	    print STDERR "SN: $searchname\n" if ($tpcDebug);
	    if (!$parsednames{$searchname}) {
		my $apio = $tp->apiOwner();
		print STDERR "APIO: $apio SN: \"$searchname\"\n" if ($tpcDebug);
		my $tpname = $tp->type . " " . $tp->name();
		$tpname =~ s/^\s*//s;
		my $oldfud = $self->{PPFIXUPDONE};
		if (!$self->fixupParsedParameters($tp->name)) {
		    if (!$oldfud) {
			# Fixup may have changed things.
			my @newparsed = $self->parsedParameters();
			%parsednames = ();
			foreach my $obj (@newparsed) {
				bless($obj, "HeaderDoc::HeaderElement");
				bless($obj, $obj->class());
				print STDERR "TYPE:" .$obj->type . "\nNAME:" . $obj->name()."\n" if ($tpcDebug);
				my $nscomp = $obj->name();
				$nscomp =~ s/\s*//sgo;
				$nscomp =~ s/^\**//sso;
				if (!length($nscomp)) {
					$nscomp = $obj->type();
					$nscomp =~ s/\s*//sgo;
				}
				$parsednames{$nscomp}=$obj;
			}
		    }

    		    if (!$HeaderDoc::ignore_apiuid_errors) {
			warn("$fullpath:$linenum: warning: Parameter $tpname does not appear in $funcname declaration ($self).\n");
			print STDERR "---------------\n";
			print STDERR "Candidates are:\n";
			foreach my $ppiter (@parsed) {
				my $ppname = $ppiter->name();
				if (!length($ppname)) {
					$ppname = $ppiter->type();
				}
				print STDERR "   \"".$ppname."\"\n";
			}
			print STDERR "---------------\n";
		    }
		}
	    }
    }
    if ($strict) { #  && !$struct
	print STDERR "STRICT CHECK\n" if ($tpcDebug);
	foreach my $parsedname (keys %parsednames) {
		print STDERR "PN: $parsedname\n" if ($tpcDebug);
		if (!$taggednames{$parsedname}) {
			my $pp = $parsednames{$parsedname};
			my $ppname = $pp->type . " " . $pp->name();
    			if (!$HeaderDoc::ignore_apiuid_errors) {
			    warn("$fullpath:$linenum: warning: Parameter $ppname in $funcname declaration is not tagged.\n");
			} elsif ($tpcDebug) {
			    warn("Warning skipped\n");
			}
		}
	}
    }

}

sub fixupParsedParameters
{
    my $self = shift;
    my $name = shift;

    # Only do this once per typedef.
    if ($self->{PPFIXUPDONE}) { return 0; }
    $self->{PPFIXUPDONE} = 1;

    my $retval = 0;
    my $simpleTDcontents = $self->typedefContents();

	if (length($simpleTDcontents)) {
		my $addDebug = 0;

		$simpleTDcontents =~ s/\s+/ /sgo;
		$simpleTDcontents =~ s/^\s*//so;
		$simpleTDcontents =~ s/\s*$//so;

		my $origref = $HeaderDoc::namerefs{$simpleTDcontents};
		if ($origref && ($origref != $self)) {
			print STDERR "Associating additional fields.\n" if ($addDebug);
			# print STDERR "ORIG: $origref\n";
			bless($origref, "HeaderDoc::HeaderElement");
			# print STDERR "ORIG: $origref\n";
			bless($origref, $origref->class());
			foreach my $origpp ($origref->parsedParameters()) {
				print STDERR "adding \"".$origpp->type()."\" \"".$origpp->name()."\" to $name\n" if ($addDebug);
				my $newpp = $origpp->clone();
				$newpp->hidden(1);
				$self->addParsedParameter($newpp);
				if ($newpp->name() eq $name) {
					$retval = 1;
				}
			}
		}
	}

    return $retval;
}

sub parsedParameters {
    my $self = shift;
    if (@_) { 
        @{ $self->{PARSEDPARAMETERS} } = @_;
    }
    ($self->{PARSEDPARAMETERS}) ? return @{ $self->{PARSEDPARAMETERS} } : return ();
}

sub addParsedParameter {
    my $self = shift;
    if (@_) { 
        push (@{$self->{PARSEDPARAMETERS}}, @_);
    }
    return @{ $self->{PARSEDPARAMETERS} };
}

# Drop the last parsed parameter.  Used for rollback support.
sub dropParsedParameter {
    my $self = shift;
    my $last = pop(@{$self->{PARSEDPARAMETERS}});
    # print STDERR "DROPPED $last\n";
    # $last->dbprint();
    return $last;
}


# for subclass/superclass merging
sub parsedParamCompare {
    my $self = shift;
    my $compareObj = shift;
    my @comparelist = $compareObj->parsedParameters();
    my $name = $self->name();
    my $localDebug = 0;

    my @params = $self->parsedParameters();

    if (scalar(@params) != scalar(@comparelist)) { 
	print STDERR "parsedParamCompare: function $name arg count differs (".
		scalar(@params)." != ".  scalar(@comparelist) . ")\n" if ($localDebug);
	return 0;
    } # different number of args

    my $pos = 0;
    my $nparams = scalar(@params);
    while ($pos < $nparams) {
	my $compareparam = $comparelist[$pos];
	my $param = $params[$pos];
	if ($compareparam->type() ne $param->type()) {
	    print STDERR "parsedParamCompare: function $name no match for argument " .
		$param->name() . ".\n" if ($localDebug);
	    return 0;
	}
	$pos++;
    }

    print STDERR "parsedParamCompare: function $name matched.\n" if ($localDebug);
    return 1;
}

sub returntype {
    my $self = shift;
    my $localDebug = 0;

    if (@_) { 
        $self->{RETURNTYPE} = shift;
	print STDERR "$self: SET RETURN TYPE TO ".$self->{RETURNTYPE}."\n" if ($localDebug);
    }

    print STDERR "$self: RETURNING RETURN TYPE ".$self->{RETURNTYPE}."\n" if ($localDebug);
    return $self->{RETURNTYPE};
}

sub taggedParamMatching
{
    my $self = shift;
    my $name = shift;
    my $localDebug = 0;

    return $self->paramMatching($name, \@{$self->{TAGGEDPARAMETERS}});
}

sub parsedParamMatching
{
    my $self = shift;
    my $name = shift;
    my $localDebug = 0;

    return $self->paramMatching($name, \@{$self->{PARSEDPARAMETERS}});
}

sub paramMatching
{
    my $self = shift;
    my $name = shift;
    my $arrayref = shift;
    my @array = @{$arrayref};
    my $localDebug = 0;

print STDERR "SA: ".scalar(@array)."\n" if ($localDebug);

$HeaderDoc::count++;

    foreach my $param (@array) {
	my $reducedname = $name;
	my $reducedpname = $param->name;
	$reducedname =~ s/\W//sgo;
	$reducedpname =~ s/\W//sgo;
	print STDERR "comparing \"$reducedname\" to \"$reducedpname\"\n" if ($localDebug);
	if ($reducedname eq $reducedpname) {
		print STDERR "PARAM WAS $param\n" if ($localDebug);
		return $param;
	}
    }

    print STDERR "NO SUCH PARAM\n" if ($localDebug);
    return 0;
}

sub XMLdocumentationBlock {
    my $self = shift;
    my $class = ref($self) || $self;
    my $compositePageString = "";
    my $fullpath = $self->fullpath();
    my $linenum = $self->linenum();

    my $name = $self->textToXML($self->name(), 1, "$fullpath:$linenum:Name");
    my $availability = $self->htmlToXML($self->availability(), 1, "$fullpath:$linenum:Availability");
    my $updated = $self->htmlToXML($self->updated(), 1, "$fullpath:$linenum:Updated");
    my $abstract = $self->htmlToXML($self->abstract(), 1, "$fullpath:$linenum:Abstract");
    my $discussion = $self->htmlToXML($self->discussion(), 0, "$fullpath:$linenum:Discussion");
    my $group = $self->htmlToXML($self->group(), 0, "$fullpath:$linenum:Group");
    my $apio = $self->apiOwner();
    my $apioclass = ref($apio) || $apio;
    my $contentString;

    my $localDebug = 0;
    
    my $type = "";
    my $isAPIOwner = $self->isAPIOwner();
    my $lang = $self->lang();
    my $sublang = $self->sublang();
    my $langstring = "";
    my $fieldType = "";
    my $fieldHeading = "";

    my $uid = "";
    my $fielduidtag = "";
    my $extra = "";

    $langstring = $self->apiRefLanguage($sublang);

    # if ($sublang eq "cpp") {
	# $langstring = "cpp";
    # } elsif ($sublang eq "C") {
	# $langstring = "c";
    # } elsif ($lang eq "C") {
	# $langstring = "occ";
    # } else {
	# # java, javascript, et al
	# $langstring = "$sublang";
    # }

    SWITCH: {
	($class eq "HeaderDoc::Constant") && do {
		$fieldType = "field"; # this should never be needed
		$fieldHeading = "fieldlist"; # this should never be needed
		$type = "constant";
		if ($apioclass eq "HeaderDoc::Header") {
			# global variable
			$uid = $self->apiuid("data");
		} else {
			# class constant
			$uid = $self->apiuid("clconst");
		}
		$isAPIOwner = 0;
		last SWITCH;
	    };
	($class eq "HeaderDoc::CPPClass") && do {
		$fieldType = "field";
		$fieldHeading = "template_fields";

		# set the type for uid purposes
		$type = "class";
		if ($self->fields()) {
			$type = "tmplt";
		}
		$uid = $self->apiuid("$type");

		# set the type for xml tag purposes
		$type = "class";

		if ($self->isCOMInterface()) {
			$type = "com_interface";
		}
		$isAPIOwner = 1;
		last SWITCH;
	    };
	($class eq "HeaderDoc::Header") && do {
		$fieldType = "field";
		$fieldHeading = "fields";
		my $filename = $self->filename();
		my $fullpath = $self->fullpath();

		# set the type for uid purposes
		$type = "header";
		$uid = $self->apiuid("$type");

		# set the type for xml tag purposes
		$type = "header";
		$extra = " filename=\"$filename\" headerpath=\"$fullpath\"";

		$isAPIOwner = 1;
		last SWITCH;
	    };
	($class eq "HeaderDoc::Enum") && do {
		$fieldType = "constant";
		$fieldHeading = "constantlist";
		$type = "enum";
		$uid = $self->apiuid("tag");
		$fielduidtag = "econst";
		$isAPIOwner = 0;
		last SWITCH;
	    };

	($class eq "HeaderDoc::Function") && do {
		$fieldType = "parameter";
		$fieldHeading = "parameterlist";

		if ($apioclass eq "HeaderDoc::Header") {
			$type = "func";
		} else {
			# if ($langstring eq "c") {
				# $type = "intfm";
			# } else {
				# $type = "clm";
			# }
			$type = $apio->getMethodType($self->declaration)
		}
		# if ($self->isTemplate()) {
			# $type = "ftmplt";
		# }
		if ($apioclass eq "HeaderDoc::CPPClass") {
			my $paramSignature = $self->getParamSignature();

			if (length($paramSignature)) {
				$paramSignature = "/$paramSignature"; # @@@ SIGNATURE appended here
			}

			if ($self->sublang() eq "C") { $paramSignature = ""; }

			if ($self->isTemplate()) {
				my $apiref = $self->apiref(0, "ftmplt", "$paramSignature");
			} else {
				my $declarationRaw = $self->declaration();
				my $methodType = $apio->getMethodType($declarationRaw);
				my $apiref = $self->apiref(0, $methodType, "$paramSignature");
			}
			$uid = $self->apiuid();
		} else {
			$uid = $self->apiuid($type);
		}
		$type = "function";
		$isAPIOwner = 0;
		last SWITCH;
	    };
	($class eq "HeaderDoc::Method") && do {
		$fieldType = "parameter";
		$fieldHeading = "parameterlist";
		$type = "method";
		my $declarationRaw = $self->declaration();
		my $methodType = $self->getMethodType($declarationRaw);
		$uid = $self->apiuid($methodType);
		$isAPIOwner = 0;
		last SWITCH;
	    };
	($class eq "HeaderDoc::ObjCCategory") && do {
		$fieldType = "field";
		$fieldHeading = "template_fields";
		$type = "category";
		$self->apiuid("cat");
		$isAPIOwner = 1;
		last SWITCH;
	    };
	($class eq "HeaderDoc::ObjCClass") && do {
		$fieldType = "field";
		$fieldHeading = "template_fields";
		$type = "class";
		$self->apiuid("cl");
		$isAPIOwner = 1;
		last SWITCH;
	    };
	($class eq "HeaderDoc::ObjCContainer") && do {
		$fieldType = "field";
		$fieldHeading = "template_fields";
		$type = "class";
		$self->apiuid("cl");
		$isAPIOwner = 1;
		last SWITCH;
	    };
	($class eq "HeaderDoc::ObjCProtocol") && do {
		$fieldType = "field";
		$fieldHeading = "template_fields";
		$type = "protocol";
		$uid = $self->apiuid("intf");
		$isAPIOwner = 1;
		last SWITCH;
	    };
	($class eq "HeaderDoc::PDefine") && do {
		$fieldType = "parameter";
		$fieldHeading = "parameterlist";
		$type = "pdefine";
		$uid = $self->apiuid("macro");
		$isAPIOwner = 0;
		last SWITCH;
	    };
	($class eq "HeaderDoc::Struct") && do {
		$fieldType = "field";
		$fieldHeading = "fieldlist";
		if ($self->isUnion()) {
			$type = "union";
		} else {
			$type = "struct";
		}
		$uid = $self->apiuid("tag");
		$isAPIOwner = 0;
		last SWITCH;
	    };
	($class eq "HeaderDoc::Typedef") && do {
		if ($self->isEnumList()) {
			$fieldType = "constant";
			$fieldHeading = "constantlist";
		} elsif ($self->isFunctionPointer()) {
			$fieldType = "parameter";
			$fieldHeading = "parameterlist";
		} else {
			$fieldType = "field";
			$fieldHeading = "fieldlist";
		}
		$type = "typedef";
		$uid = $self->apiuid("tdef");
		if ($self->isFunctionPointer()) {
			$extra = " type=\"simple\"";
		} else {
			$extra = " type=\"funcPtr\"";
		}
		$isAPIOwner = 0;
		last SWITCH;
	    };
	($class eq "HeaderDoc::Var") && do {

		$fieldType = "field";
		$fieldHeading = "fieldlist";

		if ($self->can('isFunctionPointer')) {
			if ($self->isFunctionPointer()) {
				$fieldType = "parameter";
				$fieldHeading = "parameterlist";
			}
		}
		$type = "variable";
		my $isProperty = $self->can('isProperty') ? $self->isProperty() : 0;
		my $typename = "data";
		if ($isProperty) {
				$typename = "instp";
		}
		$uid = $self->apiuid($typename);
		$isAPIOwner = 0;
		last SWITCH;
	    };
	{
		warn "UNKNOWN CLASS $self in XMLdocumentationBlock\n";
		warn "OBJECT: TYPE: $self NAME: ".$self->name()."\n";
		warn "APIO: TYPE: $apio NAME: ".$apio->name()."\n";
	};
    }

    my $throws = $self->XMLthrows();
    $compositePageString .= "<$type id=\"$uid\" lang=\"$langstring\"$extra>"; # e.g. "<class type=\"C++\">";

    if (length($name)) {
	$compositePageString .= "<name>$name</name>\n";
    }

    if (length($abstract)) {
	$compositePageString .= "<abstract>$abstract</abstract>\n";
    }
    if (length($availability)) {
	$compositePageString .= "<availability>$availability</availability>\n";
    }
    if (length($updated)) {
	$compositePageString .= "<updated>$updated</updated>\n";
    }
    if (length($group)) {
	$compositePageString .= "<group>$group</group>\n";
    }
    my $value = "";
    if ($self->can('value')) {
	$value = $self->value();

	if (length($value) && ($value ne "UNKNOWN")) {
        	$compositePageString .= "<value>$value</value>\n";
	}
    }
    if (length($throws)) {
	$compositePageString .= "$throws\n";
    }

    my @params = ();
    my @origfields = ();
    if ($self->can("fields")) { @origfields = $self->fields(); }
    if ($self->can("taggedParameters")){
        print STDERR "setting params\n" if ($localDebug);
        @params = $self->taggedParameters();
        if ($self->can("parsedParameters")) {
            $self->taggedParsedCompare();
        }
    } elsif ($self->can("fields")) {
        if ($self->can("parsedParameters")) {
            $self->taggedParsedCompare();
        }
    } else {
        print STDERR "type $class has no taggedParameters function\n" if ($localDebug);
    }

    my @parsedparams = ();
    if ($self->can("parsedParameters")) {
	@parsedparams = $self->parsedParameters();
    }

    my @parameters_or_fields = ();
    my @callbacks = ();
    foreach my $element (@params) {
	if ($element->isCallback()) {
		push(@callbacks, $element);
	} elsif (!$element->{ISDEFINE}) {
		push(@parameters_or_fields, $element);
	}
    }
    foreach my $element (@origfields) {
        bless($element, "HeaderDoc::HeaderElement");
	bless($element, $element->class()); # MinorAPIElement");
        if ($element->can("hidden")) {
            if (!$element->hidden()) {
		if ($element->isCallback()) {
			push(@callbacks, $element);
		} elsif (!$element->{ISDEFINE}) {
			push(@parameters_or_fields, $element);
		}
	    }
	}
    }
    my @origconstants = $self->constants();
    my @constants = ();
    # my @fields = ();
    # foreach my $copyfield (@origfields) {
        # bless($copyfield, "HeaderDoc::HeaderElement");
	# bless($copyfield, $copyfield->class()); # MinorAPIElement");
        # # print STDERR "FIELD: ".$copyfield->name."\n";
        # if ($copyfield->can("hidden")) {
            # if (!$copyfield->hidden()) {
                # push(@fields, $copyfield);
            # }
        # }
    # }
    foreach my $copyconstant (@origconstants) {
        bless($copyconstant, "HeaderDoc::HeaderElement");
	bless($copyconstant, $copyconstant->class()); # MinorAPIElement");
        # print STDERR "CONST: ".$copyconstant->name."\n";
        if ($copyconstant->can("hidden")) {
            if (!$copyconstant->hidden()) {
                push(@constants, $copyconstant);
            }
        }
        # print STDERR "HIDDEN: ".$copyconstant->hidden()."\n";
    }

	# if (@parameters_or_fields) {
		# $contentString .= "<$fieldHeading>\n";
		# for my $field (@parameters_or_fields) {
			# my $name = $field->name();
			# my $desc = $field->discussion();
			# # print STDERR "field $name $desc\n";
			# $contentString .= "<$fieldType><name>$name</name><desc>$desc</desc></$fieldType>\n";
		# }
		# $contentString .= "</$fieldHeading>\n";
	# }

	# Insert declaration, fields, constants, etc.
	my $parseTree_ref = $self->parseTree();
	my $parseTree = undef;
	if (!$parseTree_ref) {
		if (!$parseTree_ref && !$self->isAPIOwner()) {
			warn "Missing parse tree for ".$self->name()."\n";
		}
	} else {
		$parseTree = ${$parseTree_ref};
	}
	my $declaration = "";

	if ($parseTree) {
		$declaration = $parseTree->xmlTree($self->preserve_spaces(), $self->hideContents());
	}

	if (@constants) {
		$compositePageString .= "<constantlist>\n";
                foreach my $field (@constants) {
                        my $name = $self->textToXML($field->name());
                        my $desc = $self->htmlToXML($field->discussion());
			my $fType = "";
			if ($field->can("type")) { $fType = $field->type(); }

			my $fielduidstring = "";
			if (length($fielduidtag)) {
				my $fielduid = $field->apiuid($fielduidtag);
				$fielduidstring = " id=\"$fielduid\"";
				if (!$HeaderDoc::appleRefUsed{$uid} && !$HeaderDoc::ignore_apiuid_errors) {
					# print STDERR "MARKING APIREF $uid used\n";
					$HeaderDoc::appleRefUsed{$uid} = 1;
				} else {
					# already used or a "junk" run to obtain
					# uids for another purpose.  Drop the
					# uid in case it is already used
					$fielduidstring = "";
				}
			}

			if ($fType eq "callback") {
				my @userDictArray = $field->userDictArray(); # contains elements that are hashes of param name to param doc
				my $paramString;
				foreach my $hashRef (@userDictArray) {
					while (my ($param, $disc) = each %{$hashRef}) {
						$param = $self->textToXML($param);
						$disc = $self->htmlToXML($disc);
						$paramString .= "<parameter><name>$param</name><desc>$disc</desc></parameter>\n";
					}
					$compositePageString .= "<constant$fielduidstring><name>$name</name><desc>$desc</desc><callback_parameters>$paramString</callback_parameters></constant>\n";
				}
			} else {
				$compositePageString .= "<constant$fielduidstring><name>$name</name><desc>$desc</desc></constant>\n";
			}
		}
                $compositePageString .= "</constantlist>\n";
	}

	if (@parameters_or_fields) {
		$compositePageString .= "<$fieldHeading>\n";
                foreach my $field (@parameters_or_fields) {
                        my $name = $self->textToXML($field->name());
                        my $desc = $self->htmlToXML($field->discussion());
			my $fType = "";
			if ($field->can("type")) { $fType = $field->type(); }

			if ($fType eq "callback") {
				my @userDictArray = $field->userDictArray(); # contains elements that are hashes of param name to param doc
				my $paramString;
				foreach my $hashRef (@userDictArray) {
					while (my ($param, $disc) = each %{$hashRef}) {
						$param = $self->textToXML($param);
						$disc = $self->htmlToXML($disc);
						$paramString .= "<parameter><name>$param</name><desc>$disc</desc></parameter>\n";
					}
					$compositePageString .= "<$fieldType><name>$name</name><desc>$desc</desc><callback_parameters>$paramString</callback_parameters></$fieldType>\n";
				}
			} else {
				$compositePageString .= "<$fieldType><name>$name</name><desc>$desc</desc></$fieldType>\n";
			}
		}
                $compositePageString .= "</$fieldHeading>\n";
	}

	if (@callbacks) {
		$compositePageString .= "<callbacks>\n";
                foreach my $field (@callbacks) {
                        my $name = $self->textToXML($field->name());
                        my $desc = $self->htmlToXML($field->discussion());
			my $fType = "";
			if ($field->can("type")) { $fType = $field->type(); }

			if ($fType eq "callback") {
				my @userDictArray = $field->userDictArray(); # contains elements that are hashes of param name to param doc
				my $paramString;
				foreach my $hashRef (@userDictArray) {
					while (my ($param, $disc) = each %{$hashRef}) {
						$param = $self->textToXML($param);
						$disc = $self->htmlToXML($disc);
						$paramString .= "<parameter><name>$param</name><desc>$disc</desc></parameter>\n";
					}
					$compositePageString .= "<$fieldType><name>$name</name><desc>$desc</desc><callback_parameters>$paramString</callback_parameters></$fieldType>\n";
				}
			} else {
				$compositePageString .= "<$fieldType><name>$name</name><desc>$desc</desc></$fieldType>\n";
			}
		}
                $compositePageString .= "</callbacks>\n";
	}

    if (scalar(@parsedparams) && (!$self->isBlock())) {
	# PDefine blocks use parsed parameters to store all of the defines
	# in a define block, so this would be bad.

        my $paramContentString;
        foreach my $element (@parsedparams) {
            my $pName = $self->textToXML($element->name());
	    # if (!$element->can("type")) {
		# cluck("ELEMENT TRACE: ".$element." (".$element->name().") in $self (".$self->name().") in ".$self->apiOwner()." (".$self->apiOwner()->name().")\n"); 
		# my $headerObj = $HeaderDoc::headerObject;
		# $headerObj->headerDump();
		# next;
	    # }
            my $pType = $self->textToXML($element->type());

            $pType =~ s/\s*$//so;
            if ($pName =~ s/^\s*(\*+)\s*//so) {
                $pType .= " $1";
            }

            $pType = $self->textToXML($pType);
            $pName = $self->textToXML($pName);

            if (length ($pName) || length($pType)) {
                $paramContentString .= "<parsedparameter><type>$pType</type><name>$pName</name></parsedparameter>\n";
            }
        }
        if (length ($paramContentString)){
            $compositePageString .= "<parsedparameterlist>\n";
            $compositePageString .= $paramContentString;
            $compositePageString .= "</parsedparameterlist>\n";
        }
    }

    my $returntype = $self->textToXML($self->returntype());
    my $result = "";
    if ($self->can('result')) { $result = $self->result(); }
    my $attlists = "";
    if ($self->can('getAttributeLists')) { $self->getAttributeLists(0); }
    my $atts = "";
    if ($self->can('getAttributes')) { $self->getAttributes(); }

    if (length($atts)) {
        $compositePageString .= "<attributes>$atts</attributes>\n";
    }
    if ($class eq "HeaderDoc::Header") {
	my $includeref = $HeaderDoc::perHeaderIncludes{$fullpath};
	if ($includeref) {
		my @includes = @{$includeref};

		$compositePageString .= "<includes>\n";
		foreach my $include (@includes) {
			print STDERR "Included file: $include\n" if ($localDebug);

			my $xmlinc = $self->textToXML($include);
			$compositePageString .= "<include>$xmlinc</include>\n";
		}
		$compositePageString .= "</includes>\n";
	}
    }
    if (length($attlists)) {
        $compositePageString .= "<attributelists>$attlists</attributelists>\n";
    }
    if (length($returntype)) {
        $compositePageString .= "<returntype>$returntype</returntype>\n";
    }
    if (length($result)) {
        $compositePageString .= "<result>$result</result>\n";
    }


    if (length($declaration)) {
	$compositePageString .= "<declaration>$declaration</declaration>\n";
    }

    if (length($discussion)) {
	$compositePageString .= "<desc>$discussion</desc>\n";
    }

    if ($isAPIOwner) {
	$contentString = $self->_getFunctionXMLDetailString();
	if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
		$compositePageString .= "<functions>$contentString</functions>\n";
	}

	$contentString= $self->_getMethodXMLDetailString();
	if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
		$compositePageString .= "<methods>$contentString</methods>\n";
	}

	$contentString= $self->_getVarXMLDetailString();
	if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
		$compositePageString .= "<globals>$contentString</globals>\n";
	}

	$contentString= $self->_getConstantXMLDetailString();
	if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
		$compositePageString .= "<constants>$contentString</constants>\n";
	}

	$contentString= $self->_getTypedefXMLDetailString();
	if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
		$compositePageString .= "<typedefs>$contentString</typedefs>";
	}

	$contentString= $self->_getStructXMLDetailString();
	if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
		$compositePageString .= "<structs_and_unions>$contentString</structs_and_unions>";
	}

	$contentString= $self->_getEnumXMLDetailString();
	if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
		$compositePageString .= "<enums>$contentString</enums>";
	}

	$contentString= $self->_getPDefineXMLDetailString();
	if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
		$compositePageString .= "<defines>$contentString</defines>";
	}  

	# @@@ Class generation code.  Important debug checkpoint.
	my $classContent = "";
	$contentString= $self->_getClassXMLDetailString();
	if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
		$classContent .= $contentString;
	}
	$contentString= $self->_getCategoryXMLDetailString();
	if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
		$classContent .= $contentString;
	}
	$contentString= $self->_getProtocolXMLDetailString();
	if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
		$classContent .= $contentString;
	}
	if (length($classContent)) {
		$compositePageString .= "<classes>$classContent</classes>\n";
	}

    }

    if ($isAPIOwner) {
	my $copyrightOwner = $self->copyrightOwner;
	if ($class eq "HeaderDoc::Header") {
		my $headercopyright = $self->htmlToXML($self->headerCopyrightOwner());
		if ($headercopyright ne "") {
			$copyrightOwner = $headercopyright;
		}
    	}
        $compositePageString .= "<copyrightinfo>&#169; $copyrightOwner</copyrightinfo>\n" if (length($copyrightOwner));

	my $dateStamp = $self->fix_date();
	$compositePageString .= "<timestamp>$dateStamp</timestamp>\n" if (length($dateStamp));
    }

    $compositePageString .= "</$type>"; # e.g. "</class>";
    return $compositePageString;
}


sub isFunctionPointer {
    my $self = shift;

    if (@_) {
        $self->{ISFUNCPTR} = shift;
    }
    return $self->{ISFUNCPTR};
}

sub addToIncludedDefines {
    my $self = shift;
    my $obj = shift;

    if (!$self->{INCLUDED_DEFINES}) {
	my @x = ();
	$self->{INCLUDED_DEFINES} = \@x;
    }
    push(@{$self->{INCLUDED_DEFINES}}, $obj);

    my @arr = @{$self->{INCLUDED_DEFINES}};
    # print "OBJ IS ".\$arr[scalar(@arr) - 1]."\n";
    return \$arr[scalar(@arr) - 1];
}

# /*! Collects data for generating the API ref (apple_ref) for a function, data type, etc.  See apiref() for the actual generation. */
sub apirefSetup
{
    my $self = shift;
    my $force = 0;

    if (@_) {
	$force = shift;
    }
    if ($self->noRegisterUID()) { return ($self->{KEEPCONSTANTS},
		$self->{KEEPFIELDS}, $self->{KEEPPARAMS},
		$self->{FIELDHEADING}, $self->{FUNCORMETHOD});
    }

    my $subreftitle = 0;
    if ($self->appleRefIsDoc() == 1) {
	$subreftitle = 1;
    }
    # print STDERR "OBJ: $self NAME: ".$self->name()." SRT: $subreftitle\n";

    my $class = ref($self) || $self;
    my $apio = $self->apiOwner();
    my $apioclass = ref($apio) || $apio;

    my $declarationRaw = $self->declaration();

    my @origconstants = $self->constants();
    my @origfields = ();
    my @params = ();
    my $apiref = "";
    my $typename = "";
    my $fieldHeading = "";
    my $className = "";
    my $localDebug = 0;
    my $apiRefType = "";
    my $func_or_method = "";


    if ($self->can("taggedParameters")){ 
	print STDERR "setting params\n" if ($localDebug);
	@params = $self->taggedParameters();
	if ($self->can("parsedParameters")) {
	    $self->taggedParsedCompare();
	}
    } elsif ($self->can("fields")) {
	if ($self->can("parsedParameters")) {
	    $self->taggedParsedCompare();
	}
    } else {
	print STDERR "type $class has no taggedParameters function\n" if ($localDebug);
    }

    if (!$force && $self->{APIREFSETUPDONE}) {
	# print STDERR "SHORTCUT: $self\n";
	return ($self->{KEEPCONSTANTS}, $self->{KEEPFIELDS}, $self->{KEEPPARAMS},
		$self->{FIELDHEADING}, $self->{FUNCORMETHOD});
    }
	# print STDERR "REDO: $self\n";

    if ($self->can("fields")) { @origfields = $self->fields(); }

    # my @constants = @origconstants;
    # my @fields = @origfields;
    my @constants = ();
    my @fields = ();

    if (!$self->suppressChildren()) {
      foreach my $copyfield (@origfields) {
        bless($copyfield, "HeaderDoc::HeaderElement");
	bless($copyfield, $copyfield->class()); # MinorAPIElement");
	print STDERR "FIELD: ".$copyfield->name."\n" if ($localDebug);
	if ($copyfield->can("hidden")) {
	    if (!$copyfield->hidden()) {
		push(@fields, $copyfield);
	    } else {
		print STDERR "HIDDEN\n" if ($localDebug);
	    }
	}
      }

      foreach my $copyconstant (@origconstants) {
        bless($copyconstant, "HeaderDoc::HeaderElement");
	bless($copyconstant, $copyconstant->class()); # MinorAPIElement");
	# print STDERR "CONST: ".$copyconstant->name."\n";
	if ($copyconstant->can("hidden")) {
	    if (!$copyconstant->hidden()) {
		push(@constants, $copyconstant);
	    }
	}
	# print STDERR "HIDDEN: ".$copyconstant->hidden()."\n";
      }
	# print STDERR "SELF WAS $self\n";
    # } else {
	# warn "CHILDREN SUPPRESSED\n";
    }

    $typename = "internal_temporary_object";
    SWITCH: {
	($class eq "HeaderDoc::Function") && do {
			print STDERR "FUNCTION\n" if ($localDebug);
			if ($apioclass eq "HeaderDoc::Header") {
				$typename = "func";
			} else {
				$typename = "clm";
				if ($apio->can("getMethodType")) {
					$typename = $apio->getMethodType($self->declaration);
				}
			}
			print STDERR "Function type: $typename\n" if ($localDebug);
			if ($self->isTemplate()) {
				$typename = "ftmplt";
			}
			if ($apioclass eq "HeaderDoc::CPPClass") {
				my $paramSignature = $self->getParamSignature();

				print STDERR "paramSignature: $paramSignature\n" if ($localDebug);

				if (length($paramSignature)) {
					$paramSignature = "/$paramSignature"; # @@@SIGNATURE appended here
				}

				if ($self->sublang() eq "C") { $paramSignature = ""; }

				if ($self->isTemplate()) {
					$apiref = $self->apiref(0, "ftmplt", "$paramSignature");
				} else {
					my $declarationRaw = $self->declaration();
					my $methodType = $apio->getMethodType($declarationRaw);
					$apiref = $self->apiref(0, $methodType, "$paramSignature");
				}
			}
			$fieldHeading = "Parameters";
			$apiRefType = "";
			$func_or_method = "function";
			last SWITCH;
		};
	($class eq "HeaderDoc::Constant") && do {
			print STDERR "CONSTANT\n" if ($localDebug);
			if ($apioclass eq "HeaderDoc::Header") {
				$typename = "data";
			} else {
				$typename = "clconst";
			}
			$fieldHeading = "Fields";
			$apiRefType = "";
			last SWITCH;
		};
	($class eq "HeaderDoc::Enum") && do {
			print STDERR "ENUM\n" if ($localDebug);
			$typename = "tag";
			$fieldHeading = "Constants";
			# if ($self->masterEnum()) {
				$apiRefType = "econst";
			# } else {
				# $apiRefType = "";
			# }
			last SWITCH;
		};
	($class eq "HeaderDoc::PDefine") && do {
			print STDERR "PDEFINE\n" if ($localDebug);
			$typename = "macro";
			$fieldHeading = "Parameters";
			$apiRefType = "";
			last SWITCH;
		};
	($class eq "HeaderDoc::Method") && do {
			print STDERR "METHOD\n" if ($localDebug);
			$typename = $self->getMethodType($declarationRaw);
			$fieldHeading = "Parameters";
			$apiRefType = "";
			if ($apio->can("className")) {  # to get the class name from Category objects
				$className = $apio->className();
			} else {
				$className = $apio->name();
			}
			$func_or_method = "method";
			last SWITCH;
		};
	($class eq "HeaderDoc::Struct") && do {
			print STDERR "TAG\n" if ($localDebug);
			$typename = "tag";
			$fieldHeading = "Fields";
			$apiRefType = "";
			last SWITCH;
		};
	($class eq "HeaderDoc::Typedef") && do {
			print STDERR "TDEF\n" if ($localDebug);
			$typename = "tdef";

        		if ($self->isFunctionPointer()) {
				$fieldHeading = "Parameters";
				last SWITCH;
			}
        		if ($self->isEnumList()) {
				$fieldHeading = "Constants";
				last SWITCH;
			}
        		$fieldHeading = "Fields";

			$apiRefType = "";
			$func_or_method = "function";
			last SWITCH;
		};
	($class eq "HeaderDoc::Var") && do {
			print STDERR "VAR\n" if ($localDebug);
			my $isProperty = $self->can('isProperty') ? $self->isProperty() : 0;
			if ($isProperty) {
				$typename = "instp";
			} else {
				$typename = "data";
			}
			$fieldHeading = "Fields";
			if ($self->can('isFunctionPointer')) {
			    if ($self->isFunctionPointer()) {
				$fieldHeading = "Parameters";
			    }
			}
			$apiRefType = "";
			last SWITCH;
		};
    }
    if (!length($apiref)) {
	# cluck( "TYPE NAME: $typename CLASS: $class\n");
	$apiref = $self->apiref(0, $typename);
    }


    if (@constants) {
	foreach my $element (@constants) {
	    $element->appleRefIsDoc($subreftitle);
	    my $uid = $element->apiuid("econst");
	}
    }

    if (@params) {
      foreach my $element (@params) {
	if (length($apiRefType)) {
	# print STDERR "APIREFTYPE: $apiRefType\n";
	    $element->appleRefIsDoc($subreftitle);
	    $apiref = $element->apiref(0, $apiRefType);
	}
      }
    }

    $self->{KEEPCONSTANTS} = \@constants;
    $self->{KEEPFIELDS} = \@fields;
    $self->{KEEPPARAMS} = \@params;
    $self->{FIELDHEADING} = $fieldHeading;
    $self->{FUNCORMETHOD} = $func_or_method;

    $self->{APIREFSETUPDONE} = 1;
    return (\@constants, \@fields, \@params, $fieldHeading, $func_or_method);
}

sub tagNameRegexpAndSuperclassFieldNameForType
{
    my $self = shift;
    my $class = ref($self) || $self;

    my $tagname = "";
    my $tag_re = "";
    my $superclassfieldname = "Superclass";

    SWITCH: {
	($class =~ /HeaderDoc\:\:Constant/) && do {
		$tag_re = "const(?:ant)?|var";
		$tagname = "constant";
		last SWITCH;
	};
	($class =~ /HeaderDoc\:\:Enum/) && do {
		$tag_re = "enum";
		$tagname = "enum";
		last SWITCH;
	};
	($class =~ /HeaderDoc\:\:Function/) && do {
		$tag_re = "method|function";
		$tagname = "function or method";
		last SWITCH;
	};
	($class =~ /HeaderDoc\:\:Method/) && do {
		$tag_re = "method";
		$tagname = "method";
		last SWITCH;
	};
	($class =~ /HeaderDoc\:\:PDefine/) && do {
		$tag_re = "define(?:d)?|function";
		$tagname = "CPP macro";
		last SWITCH;
	};
	($class =~ /HeaderDoc\:\:Struct/) && do {
		$tag_re = "struct|union";
                if ($self->isUnion()) { $tagname = "union"; }
		else { $tagname = "struct"; }
		last SWITCH;
	};
	($class =~ /HeaderDoc\:\:Typedef/) && do {
		$tag_re = "typedef|function|class";
		$tagname = "typedef";
		last SWITCH;
	};
	($class =~ /HeaderDoc\:\:Var/) && do {
		$tag_re = "var|property|const(?:ant)?";
		$tagname = "variable";
		last SWITCH;
	};
	($class =~ /HeaderDoc\:\:ObjCClass/) && do {
		$tag_re = "class|template";
		$tagname = "Objective-C class";
		$superclassfieldname = "Extends&nbsp;Class";
		last SWITCH;
	};
	($class =~ /HeaderDoc\:\:ObjCCategory/) && do {
		$tag_re = "category|template";
		$tagname = "Objectice-C category";
		$superclassfieldname = "Extends&nbsp;Class";
		last SWITCH;
	};
	($class =~ /HeaderDoc\:\:ObjCProtocol/) && do {
		$tag_re = "protocol|template";
		$tagname = "Objectice-C protocol";
                $superclassfieldname = "Extends&nbsp;Protocol";
		last SWITCH;
	};
	($class =~ /HeaderDoc\:\:CPPClass/) && do {
		$tag_re = "class|interface";
		$tagname = "class or interface";
		last SWITCH;
	};
	{
		print STDERR "Unknown type: $class\n";
	}
    };

    return ($tagname, $tag_re, $superclassfieldname);
}

sub processComment
{
    my($self) = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $fullpath = $self->fullpath();
    my $linenuminblock = $self->linenuminblock();
    my $blockoffset = $self->blockoffset();
    my $linenum = $self->linenum();
    my $localDebug = 0;
    my $olddisc = $self->halfbaked_discussion();
    my $isProperty = $self->can('isProperty') ? $self->isProperty() : 0;

    print STDERR "SELF IS $self\n" if ($localDebug);

    my $lang = $self->lang();
    my $sublang = $self->sublang();

    my $lastField = scalar(@fields);

    # warn "processComment called on raw HeaderElement\n";
    my $class = ref($self) || $self;
    if ($class =~ /HeaderDoc::HeaderElement/) {
	return;
    }

    my ($tagname, $tag_re, $superclassfieldname) = $self->tagNameRegexpAndSuperclassFieldNameForType();

    my $seen_top_level_field = 0;
    my $first_field = 1;

    my $callbackObj = 0;
    foreach my $field (@fields) {
    	print STDERR "Constant field is |$field|\n" if ($localDebug);
	print STDERR "Seen top level field: $seen_top_level_field\n" if ($localDebug);
	my $fieldname = "";
	my $top_level_field = 0;
	if ($field =~ /^(\w+)(\s|$)/) {
		$fieldname = $1;
		# print STDERR "FIELDNAME: $fieldname\n";
		$top_level_field = validTag($fieldname, 1);

		if ($top_level_field && $seen_top_level_field && ($fieldname !~ /const(ant)?/) &&
		    (!$self->isBlock() || $fieldname ne "define")) {
			# We've seen more than one top level field.

			$field =~ s/^(\w+)(\s)//s;
			my $spc = $2;

			my $newtag = "field";
			if ($class =~ /HeaderDoc\:\:Enum/) {
				$newtag = "constant";
			} elsif ($class =~ /HeaderDoc\:\:Function/ ||
			         $class =~ /HeaderDoc\:\:Method/) {
				$newtag = "param";
			} elsif ($class =~ /HeaderDoc\:\:ObjCClass/ ||
				 $class =~ /HeaderDoc\:\:ObjCProtocol/ ||
				 $class =~ /HeaderDoc\:\:ObjCCategory/ ||
				 $class =~ /HeaderDoc\:\:ObjCContainer/ ||
				 $class =~ /HeaderDoc\:\:CPPClass/) {
				$newtag = "discussion";
			} elsif ($class =~ /HeaderDoc\:\:Typedef/) {
				if ($fieldname eq "function") {
					$newtag = "callback";
				} elsif ($fieldname =~ /define(d)?/ || $fieldname eq "var") {
					$newtag = "constant";
				}
			}
			$field = "$newtag$spc$field";
			$top_level_field = 0;

			warn "$fullpath:$linenum: Duplicate top level tag \"$fieldname\" detected\n".
				"in comment.  Maybe you meant \"$newtag\".\n";

			# warn "Thunked field to \"$field\"\n";
		} elsif ($top_level_field && $seen_top_level_field && ($fieldname eq "constant")) {
			$top_level_field = 0;
		}
	}
	# print STDERR "TLF: $top_level_field, FN: \"$fieldname\"\n";
	# print STDERR "FIELD $field\n";
	SWITCH: {
            ($field =~ /^\/(\*|\/)\!/o && $first_field) && do {
                                my $copy = $field;
                                $copy =~ s/^\/(\*|\/)\!\s*//s;
                                if (length($copy)) {
                                        $self->discussion($copy);
					$seen_top_level_field = 1;
                                }
                        last SWITCH;
                        };

	    # (($lang eq "java") && ($field =~ /^\s*\/\*\*/o)) && do {
			# ignore opening /**
			# last SWITCH;
		# };

	    ($self->isAPIOwner() && $field =~ s/^alsoinclude\s+//io) && do {
			$self->alsoInclude($field);
			last SWITCH;
		};
	    ($callbackObj) &&
		do {
			my $fallthrough = 0;
			my $cbName = $callbackObj->name();
                        print STDERR "In callback: field is '$field'\n" if ($localDebug);
                        
                        if ($field =~ s/^(param|field)\s+//io) {
                            $field =~ s/^\s+|\s+$//go;
                            $field =~ /(\w*)\s*(.*)/so;

                            my $paramName = $1;
                            my $paramDesc = $2;
                            $callbackObj->addToUserDictArray({"$paramName" => "$paramDesc"});

                        } elsif ($field =~ s/^return\s+//io) {
                            $field =~ s/^\s+|\s+$//go;
                            $callbackObj->addToUserDictArray({"Returns" => "$field"});
                        } elsif ($field =~ s/^result\s+//io) {
                            $field =~ s/^\s+|\s+$//go;
                            $field =~ /(\w*)\s*(.*)/so;
                            $callbackObj->addToUserDictArray({"Result" => "$field"});
                        } else {
			    print STDERR "Adding callback field to typedef[1].  Callback name: $cbName.\n" if ($localDebug);
			    if ($callbackObj->{ISDEFINE}) {
				$self->addTaggedParameter($callbackObj);
			    } else {
				$self->addToFields($callbackObj);
			    }
                            $callbackObj = undef;
			    # next SWITCH;
			    $fallthrough = 1;
                        }

			if (!$fallthrough) {
				last SWITCH;
			}
                };

	    ($field =~ s/^serial\s+//io) && do {$self->attribute("Serial Field Info", $field, 1); last SWITCH;};
            ($field =~ s/^serialfield\s+//io) && do {
                    if (!($field =~ s/(\S+)\s+(\S+)\s+//so)) {
                        warn "$fullpath:$linenum: warning: serialfield format wrong.\n";
                    } else {
                        my $name = $1;
                        my $type = $2;
                        my $description = "(no description)";
                        my $att = "$name Type: $type";
                        $field =~ s/^(<br>|\s)*//sgio;
                        if (length($field)) {
                                $att .= "<br>\nDescription: $field";
                        }
                        $self->attributelist("Serial Fields", $att,  1);
                    }
                    last SWITCH;
                };
            ($field =~ s/^unformatted(\s+|$)//io) && do {
		$self->preserve_spaces(1);
		last SWITCH;
	    };

                        (!$self->isAPIOwner() && $field =~ s/^templatefield\s+//io) && do {
                                        $self->attributelist("Template Field", $
field);
                                        last SWITCH;
                        };      


                                ($self->isAPIOwner() && $field =~ s/^templatefield(\s+)/$1/io) && do {
                                        $field =~ s/^\s+|\s+$//go;
                                        $field =~ /(\w*)\s*(.*)/so;
                                        my $fName = $1;
                                        my $fDesc = $2;
                                        my $fObj = HeaderDoc::MinorAPIElement->new();
					$fObj->apiOwner($self);
                                        $fObj->linenuminblock($linenuminblock);
                                        $fObj->blockoffset($blockoffset);
                                        # $fObj->linenum($linenum);
                                        $fObj->apiOwner($self);
                                        $fObj->outputformat($self->outputformat);
                                        $fObj->name($fName);
                                        $fObj->discussion($fDesc);
                                        $self->addToFields($fObj);
# print STDERR "inserted field $fName : $fDesc";
                                        last SWITCH;
                                };

	    ($self->isAPIOwner() && $field =~ s/^super(class|)(\s+)/$2/io) && do {
			$self->attribute($superclassfieldname, $field, 0);
			$self->explicitSuper(1);
			last SWITCH;
		};
	    ($self->isAPIOwner() && $field =~ s/^instancesize(\s+)/$1/io) && do {$self->attribute("Instance Size", $field, 0); last SWITCH;};
	    ($self->isAPIOwner() && $field =~ s/^performance(\s+)/$1/io) && do {$self->attribute("Performance", $field, 1); last SWITCH;};
	    # ($self->isAPIOwner() && $field =~ s/^subclass(\s+)/$1/io) && do {$self->attributelist("Subclasses", $field); last SWITCH;};
	    ($self->isAPIOwner() && $field =~ s/^nestedclass(\s+)/$1/io) && do {$self->attributelist("Nested Classes", $field); last SWITCH;};
	    ($self->isAPIOwner() && $field =~ s/^coclass(\s+)/$1/io) && do {$self->attributelist("Co-Classes", $field); last SWITCH;};
	    ($self->isAPIOwner() && $field =~ s/^helper(class|)(\s+)/$2/io) && do {$self->attributelist("Helper Classes", $field); last SWITCH;};
	    ($self->isAPIOwner() && $field =~ s/^helps(\s+)/$1/io) && do {$self->attribute("Helps", $field, 0); last SWITCH;};
	    ($self->isAPIOwner() && $field =~ s/^classdesign(\s+)/$1/io) && do {$self->attribute("Class Design", $field, 1); last SWITCH;};
	    ($self->isAPIOwner() && $field =~ s/^dependency(\s+)/$1/io) && do {$self->attributelist("Dependencies", $field); last SWITCH;};
	    ($self->isAPIOwner() && $field =~ s/^ownership(\s+)/$1/io) && do {$self->attribute("Ownership Model", $field, 1); last SWITCH;};
	    ($self->isAPIOwner() && $field =~ s/^security(\s+)/$1/io) && do {$self->attribute("Security", $field, 1); last SWITCH;};
	    ($self->isAPIOwner() && $field =~ s/^whysubclass(\s+)/$1/io) && do {$self->attribute("Reason to Subclass", $field, 1); last SWITCH;};
	    # ($self->isAPIOwner() && $field =~ s/^charset(\s+)/$1/io) && do {$self->encoding($field); last SWITCH;};
	    # ($self->isAPIOwner() && $field =~ s/^encoding(\s+)/$1/io) && do {$self->encoding($field); last SWITCH;};

	    (($self->isAPIOwner() || $class =~ /HeaderDoc\:\:Function/ ||
		$class =~ /HeaderDoc\:\:Method/ ||
		($class =~ /HeaderDoc\:\:Var/ && $isProperty)) &&
	     $field =~ s/^(throws|exception)(\s+)/$2/io) && do {
			$self->throws($field);
			last SWITCH;
		};

	    ($self->isAPIOwner() && $field =~ s/^namespace(\s+)/$1/io) && do {$self->namespace($field); last SWITCH;};

            ($field =~ s/^abstract\s+//io) && do {$self->abstract($field); last SWITCH;};
            ($field =~ s/^brief\s+//io) && do {$self->abstract($field, 1); last SWITCH;};
            ($field =~ s/^(discussion|details|description)(\s+|$)//io) && do {
			if ($class =~ /HeaderDoc\:\:PDefine/ && $self->inDefineBlock() && length($olddisc)) {
				# Silently drop these....
				$self->{DISCUSSION} = "";
			}
			if (!length($field)) { $field = "\n"; }
			$self->discussion($field);
			last SWITCH;
		};
            ($field =~ s/^availability\s+//io) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^since\s+//io) && do {$self->availability($field); last SWITCH;};
            ($field =~ s/^author\s+//io) && do {$self->attribute("Author", $field, 0); last SWITCH;};
            ($field =~ s/^group\s+//io) && do {$self->group($field); last SWITCH;};

	    ($class =~ /HeaderDoc\:\:PDefine/ && $self->isBlock() && $field =~ s/^hidesingletons(\s+)/$1/io) && do {$self->{HIDESINGLETONS} = 1; last SWITCH;};
	    ($class =~ /HeaderDoc\:\:PDefine/ && $field =~ s/^hidecontents(\s+)/$1/io) && do {$self->hideContents(1); last SWITCH;};
	    (($class =~ /HeaderDoc\:\:Function/ || $class =~ /HeaderDoc\:\:Method/) && $field =~ s/^(function|method)group\s+//io) &&
		do {$self->group($field); last SWITCH;};


            ($class =~ /HeaderDoc\:\:PDefine/ && $field =~ s/^parseOnly(\s+|$)//io) && do { $self->parseOnly(1); last SWITCH; };
            ($class =~ /HeaderDoc\:\:PDefine/ && $field =~ s/^noParse(\s+|$)//io) && do { print STDERR "Parsing will be skipped.\n" if ($localDebug); $HeaderDoc::skipNextPDefine = 1; last SWITCH; };


            ($field =~ s/^indexgroup\s+//io) && do {$self->indexgroup($field); last SWITCH;};
            ($field =~ s/^version\s+//io) && do {$self->attribute("Version", $field, 0); last SWITCH;};
            ($field =~ s/^deprecated\s+//io) && do {$self->attribute("Deprecated", $field, 0); last SWITCH;};
            ($field =~ s/^updated\s+//io) && do {$self->updated($field); last SWITCH;};
	    ($field =~ s/^attribute\s+//io) && do {
		    my ($attname, $attdisc, $namedisc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$attdisc =~ s/^\s*//s;
			$attdisc =~ s/\s*$//s;
			$self->attribute($attname, $attdisc, 0);
		    } else {
			warn "$fullpath:$linenum: warning: Missing name/discussion for attribute\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributelist\s+//io) && do {
		    $field =~ s/^\s*//so;
		    $field =~ s/\s*$//so;
		    my ($name, $lines) = split(/\n/, $field, 2);
		    $name =~ s/^\s*//so;
		    $name =~ s/\s*$//so;
		    $lines =~ s/^\s*//so;
		    $lines =~ s/\s*$//so;
		    if (length($name) && length($lines)) {
			my @attlines = split(/\n/, $lines);
			foreach my $line (@attlines) {
			    $self->attributelist($name, $line);
			}
		    } else {
			warn "$fullpath:$linenum: warning: Missing name/discussion for attributelist\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributeblock\s+//io) && do {
		    my ($attname, $attdisc, $namedisc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$self->attribute($attname, $attdisc, 1);
		    } else {
			warn "$fullpath:$linenum: warning: Missing name/discussion for attributeblock\n";
		    }
		    last SWITCH;
		};
	    ($field =~ /^see(also|)\s+/io) &&
		do {
		    $self->see($field);
		    last SWITCH;
		};

            (($class =~ /HeaderDoc\:\:Enum/ || $class =~ /HeaderDoc\:\:Typedef/) && $field =~ s/^const(ant)?\s+//io) &&
                do {
                    $field =~ s/^\s+|\s+$//go;
#                   $field =~ /(\w*)\s*(.*)/so;
                    $field =~ /(\S*)\s*(.*)/so; # Let's accept any printable char for name, for pseudo enum values
                    my $cName = $1;
                    my $cDesc = $2;
                    my $cObj = HeaderDoc::MinorAPIElement->new();
		    $cObj->apiOwner($self);
                    $cObj->outputformat($self->outputformat);
                    $cObj->name($cName);
                    $cObj->discussion($cDesc);
                    $self->addConstant($cObj); 
                    my $name = $self->name();
                    if ($name eq "") {
                        $name = "$cName";
                        $self->firstconstname($name);
                    }   
                    last SWITCH;
                };

            (($class =~ /HeaderDoc\:\:Struct/ || $class =~ /HeaderDoc\:\:Typedef/) && $field =~ s/^callback\s+//io) &&
                do {
                    $field =~ s/^\s+|\s+$//go;
                    $field =~ /(\w*)\s*(.*)/so;
                    my $cbName = $1;
                    my $cbDesc = $2;

		    if ($callbackObj && $callbackObj->{ISDEFINE}) {
			$self->addTaggedParameter($callbackObj);
		    } elsif ($callbackObj) {
			$self->addToFields($callbackObj);
		    }
                    $callbackObj = HeaderDoc::MinorAPIElement->new();
		    $callbackObj->apiOwner($self);
                    $callbackObj->outputformat($self->outputformat);
                    $callbackObj->name($cbName);
                    $callbackObj->discussion($cbDesc);
                    $callbackObj->type("callback");

		    # $self->addToFields($callbackObj);
                    # now get params and result that go with this callback
                    print STDERR "Found callback.  Callback name: $cbName.\n" if ($localDebug);
		    last SWITCH;
		};

            # param and result have to come last, since they should be handled differently, if part of a callback
            # which is inside a struct (as above).  Otherwise, these cases below handle the simple typedef'd callback
            # (i.e., a typedef'd function pointer without an enclosing struct.

	    (($class =~ /HeaderDoc\:\:Function/ || $class =~ /HeaderDoc\:\:Method/ || $class =~ /HeaderDoc\:\:Struct/ ||
	      $class =~ /HeaderDoc\:\:Typedef/ || $class =~ /HeaderDoc\:\:PDefine/ ||
	      $class =~ /HeaderDoc\:\:Var/) && $field =~ s/^(param|field)\s+//io) &&
                        do {
				my $fieldname = $1;
				
                                $field =~ s/^\s+|\s+$//go; # trim leading and trailing whitespace
                                # $field =~ /(\w*)\s*(.*)/so;
                                $field =~ /(\S*)\s*(.*)/so;
                                my $pName = $1;
                                my $pDesc = $2;
                                my $param = HeaderDoc::MinorAPIElement->new();
				$param->apiOwner($self);
                                $param->outputformat($self->outputformat);
                                $param->name($pName);
                                $param->discussion($pDesc);
		                if (($class =~ /HeaderDoc\:\:Typedef/ || $class =~ /HeaderDoc\:\:Struct/) &&
				    $field =~ /^param\s+/) {
                                        $param->type("funcPtr");
                    	                $self->addToFields($param);
		                } else {
                    	            $self->addTaggedParameter($param);
		                }
				if ($class =~ /HeaderDoc\:\:Var/ && !$isProperty) {
					warn "Field \@$fieldname found in \@var declaration.\nYou should use \@property instead.\n" if (!$HeaderDoc::test_mode);
				}
                                last SWITCH;
                         };

	    (($class =~ /HeaderDoc\:\:Function/ || $class =~ /HeaderDoc\:\:Method/ ||
		$class =~ /HeaderDoc\:\:Typedef/ || $class =~ /HeaderDoc\:\:Struct/ ||
		$class =~ /HeaderDoc\:\:PDefine/ ||
		$class =~ /HeaderDoc\:\:Var/) && $field =~ s/^return\s+//io) &&
		do {
			if ($class =~ /HeaderDoc\:\:Typedef/) {
				$self->isFunctionPointer(1);
			}
			if ($class =~ /HeaderDoc\:\:Var/ && !$isProperty) {
				warn "Field \@return found in \@var declaration.\nYou should use \@property instead.\n" if (!$HeaderDoc::test_mode);
			}
			$self->result($field);
			last SWITCH;
		};
	    (($class =~ /HeaderDoc\:\:Function/ || $class =~ /HeaderDoc\:\:Method/ ||
		$class =~ /HeaderDoc\:\:Typedef/ || $class =~ /HeaderDoc\:\:Struct/ ||
		$class =~ /HeaderDoc\:\:PDefine/ ||
		$class =~ /HeaderDoc\:\:Var/) && $field =~ s/^result\s+//io) &&
		do {
			if ($class =~ /HeaderDoc\:\:Typedef/) {
				$self->isFunctionPointer(1);
			}
			if ($class =~ /HeaderDoc\:\:Var/ && !$isProperty) {
				warn "Field \@result found in \@var declaration.\nYou should use \@property instead.\n" if (!$HeaderDoc::test_mode);
			}
			$self->result($field);
			last SWITCH;
		};

                ($top_level_field == 1) &&
                        do {
                                my $keepname = 1;
				my $blockrequest = 0;

				print STDERR "TLF: $field\n" if ($localDebug);

				my $pattern = "";
				if ($class =~ /HeaderDoc\:\:PDefine/ && $field =~ s/^(availabilitymacro)(\s+|$)/$2/io) {
					$self->isAvailabilityMacro(1);
					$keepname = 1;
					$self->parseOnly(1);
				} elsif ($class =~ /HeaderDoc\:\:PDefine/ && $field =~ s/^(define(?:d)?block)(\s+)/$2/io) {
					$keepname = 1;
					$self->isBlock(1);
					$blockrequest = 1;
				} elsif ($class =~ /HeaderDoc\:\:CPPClass/) {
					$pattern = ":|public|private|[()]";
					# print STDERR "PATTERN: $pattern\n";
				} elsif ($class =~ /HeaderDoc\:\:Method/) {
					$pattern = ":|[()]";
				} elsif ($class =~ /HeaderDoc\:\:Function/) {
					$pattern = "::|[()]";
				} elsif ($field =~ /category\s+/) {
					$pattern = "[():]";
				}

				# If we begin with the correct @whatever tag,
				# process it.  If the tag isn't what is
				# expected based on what was parsed from the
				# code (e.g. @function for a #define), throw
				# away the name and tag type.

                                if ($field =~ s/^($tag_re)(\s+|$)/$2/i) {
					print STDERR "tag_re[1]: tag matches\n" if ($localDebug);
                                        $keepname = 1;
					$blockrequest = 0;
                                } elsif (!$blockrequest) {
					print STDERR "tag_re[2] tag does NOT match\n" if ($localDebug);
                                        # $field =~ s/(\w+)(\s|$)/$2/io;
                                        $keepname = 0;
					# $blockrequest = 0;
				}

				my ($name, $abstract_or_disc, $namedisc) = getAPINameAndDisc($field, $pattern);
				print STDERR "NAME: $name AOD: $abstract_or_disc ND: $namedisc" if ($localDebug);

				print STDERR "KEEPNAME: $keepname\n" if ($localDebug);

				if ($class =~ /HeaderDoc\:\:PDefine/ && $self->isBlock()) {
					print STDERR "ISBLOCK (BLOCKREQUEST=$blockrequest)\n" if ($localDebug);
					# my ($name, $abstract_or_disc, $namedisc) = getAPINameAndDisc($field);
					# In this case, we get a name and abstract.
					if ($blockrequest) {
						print STDERR "Added block name $name\n" if ($localDebug);
						$self->name($name);
						if (length($abstract_or_disc)) {
							if ($namedisc) {
								$self->nameline_discussion($abstract_or_disc);
							} else {
								$self->discussion($abstract_or_disc);
							}
						}
					} else {
						print STDERR "Added block member $name\n" if ($localDebug);

						if ($callbackObj && $callbackObj->{ISDEFINE}) {
							$self->addTaggedParameter($callbackObj);
						} elsif ($callbackObj) {
							$self->addToFields($callbackObj);
						}
						$callbackObj = HeaderDoc::MinorAPIElement->new();
						$callbackObj->apiOwner($self);
						$callbackObj->name($name);
						my $ref = $self->addToIncludedDefines($callbackObj);
						$callbackObj = ${$ref};
						bless($callbackObj, "HeaderDoc::HeaderElement");
						bless($callbackObj, $callbackObj->class());
						if (length($abstract_or_disc)) {
							if ($namedisc) {
								$callbackObj->nameline_discussion($abstract_or_disc);
							} else {
								$callbackObj->discussion($abstract_or_disc);
							}
						}
						$callbackObj->{ISDEFINE} = 1;
					}
				} else {
					if (length($name)) {
						# print STDERR "NOT BLOCK.  NAME IS \"$name\"\n";
						if ($keepname && (!$self->isAPIOwner() || !$HeaderDoc::ignore_apiowner_names)) {
							print STDERR "SET NAME TO $name\n" if ($localDebug);
                                			$self->name($name);
						}
					}
                                	if (length($abstract_or_disc)) {
                                        	if ($namedisc) {
                                                	$self->nameline_discussion($abstract_or_disc);
                                        	} else {
                                                	$self->discussion($abstract_or_disc);
                                        	}
                                	}

				}

                                last SWITCH;
                        };

	    # my $fullpath = $HeaderDoc::headerObject->fullpath();
            # warn "$fullpath:$linenum: warning: Unknown field in constant comment: $field\n";
		{
		    if (length($field)) { warn "$fullpath:$linenum: warning: Unknown field (\@$field) in $tagname comment (".$self->name().")\n"; }
		};

	}
	$first_field = 0;
	if ($top_level_field) { $seen_top_level_field = 1; }
    }

    if ($callbackObj) {
	if ($callbackObj->{ISDEFINE}) {
		$self->addTaggedParameter($callbackObj);
	} else {
		$self->addToFields($callbackObj);
	}
    }

    # print STDERR "CLASS: ".(ref($self) || $self)." NAME: ".$self->name()." LN: ".$self->linenum()." LNIB: ".$self->linenuminblock()." BO: ".$self->blockoffset()."\n";
    # $self->dbprint();

    return;
}

sub alsoInclude
{
    my $self = shift;
    if (@_) {
	my $value = shift;
	if (!$self->{ALSOINCLUDE}) {
		my @temp = ($value);
		$self->{ALSOINCLUDE} = \@temp;
	} else {
		push(@{$self->{ALSOINCLUDE}}, $value);
	}
    }
    return $self->{ALSOINCLUDE};
}

sub hideContents
{
    my $self = shift;
    if (@_) {
	$self->{HIDECONTENTS} = shift;
    }
    return $self->{HIDECONTENTS};
}

sub inDefineBlock
{
    my $self = shift;
    if (@_) {
	$self->{INDEFINEBLOCK} = shift;
    }
    return $self->{INDEFINEBLOCK};
}

sub strcasecmp
{
    my $a = shift;
    my $b = shift;

    return (lc($a) cmp lc($b));
}

sub unregister
{
    my $self = shift;

    my @arr = ();
    my $localDebug = 0;

    my $group = $self->group();
    if ($group) {
	$self->apiOwner()->removeFromGroup($group, $self);
    }
    foreach my $tp ($self->taggedParameters()) {
	push(@arr, $tp);
    }
    foreach my $const ($self->constants()) {
	push(@arr, $const);
    }
    if ($self->can("fields")) {
	foreach my $field ($self->fields()) {
		push(@arr, $field);
	}
    }
    foreach my $obj (@arr) {
	my $uid = $obj->apiuid();
	print STDERR "Unregistering UID $uid\n" if ($localDebug);
	unregisterUID($uid, $obj->name(), $obj);
	unregister_force_uid_clear($uid);
    }
    $self->noRegisterUID(1);
}

sub noRegisterUID
{
    my $self = shift;
    my $localDebug = 0;

    if (@_) {
	my $val = shift;
	print STDERR "No register uid set to $val ($self).\n" if ($localDebug);
	$self->{NOREGISTERUID} = $val;
    }
    return $self->{NOREGISTERUID};
}

sub wipeUIDCache
{
    my $self = shift;
    my $localDebug = 0;

    print STDERR "APIUID and LINKUID wiped ($self).\n" if ($localDebug);
    $self->{APIUID} = undef;
    $self->{LINKUID} = undef;
}

sub suppressChildren
{
    my $self = shift;
    my $localDebug = 0;

    if (@_) {
	my $val = shift;
	print STDERR "Suppress children set to $val ($self).\n" if ($localDebug);
	$self->{SUPPRESSCHILDREN} = $val;
    }
    return $self->{SUPPRESSCHILDREN};
}

sub declaredIn
{
    my $self = shift;
    my $class = ref($self) || $self;
    my $apio = $self->apiOwner();

	# warn $self->name()."\n";
    if (!$apio) { return ""; }
    if ($apio->outputformat() eq "hdxml") {
	# blank for XML.
	return "";
    }

    if ($self->isAPIOwner()) {
	if ($class =~ /HeaderDoc::Header/) {
		# warn $self->name.": Header\n";
		return "";
	} else {
		my $name = $apio->name();
		return "<a href=\"../../index.html\" target=\"_top\">$name</a>";
	}
    }

    return "";
}

sub doExternalStyle
{
    my $self = shift;
    my $liststring = shift;
    my @list = split(/\s/, $liststring);
    my $string = "";

    foreach my $styleSheet (@list) {
	$string .= "<link rel=\"stylesheet\" type=\"text/css\" href=\"$styleSheet\">\n";
    }
    return $string;
}

sub free
{
	my $self = shift;
	my $parseTree_ref = $self->parseTree();

	if ($parseTree_ref) {
		my $parseTree = ${$parseTree_ref};
		bless($parseTree, "HeaderDoc::ParseTree");

		$parseTree->dispose();
	}
	if (!$self->noRegisterUID()) {
		dereferenceUIDObject($self->apiuid(), $self);
	}
}

sub DESTROY
{
    my $self = shift;
    my $localDebug = 0;

    print STDERR "Destroying $self\n" if ($localDebug);
}

sub dbprint_expanded
{
    my $unknown_var = shift;
    my $leadspace = "";
    if ($_) {
	$leadspace = shift;
    }
    if ($unknown_var =~ "REF") { $unknown_var = ref($unknown_var); }

    my $retstring = $unknown_var;
    if (ref($unknown_var) eq "ARRAY") {
	print STDERR "REF IS ".ref($unknown_var)."\n";
	$retstring .= "\n".$leadspace."ARRAY ELEMENTS:\n";
	my $pos = 0;
	while ($pos < scalar(@{$unknown_var})) {
		$retstring .= $leadspace."     ".sprintf("%08d : ", $pos). dbprint_expanded(@{$unknown_var}[$pos], $leadspace."    ")."\n";
		$pos++;
	}
    } elsif (ref($unknown_var) ne "") {
	$retstring .= "\n".$leadspace."HASH ELEMENTS:\n";
	# print STDERR "REF IS ".ref($unknown_var)."\n";
	foreach my $elt (keys %{$unknown_var}) {
		if ($elt =~ "APIOWNER" || $elt =~ "MASTERENUM") { next; }
		$retstring .= $leadspace."     ".sprintf("%8s : ", $elt). dbprint_expanded($unknown_var->{$elt}, $leadspace."    ")."\n";
	}
    }

    return $retstring;;
}

sub isBlock {
    my $self = shift;

    if (@_) {
	$self->{ISBLOCK} = shift;
	if ($self->{ISBLOCK}) {
		$self->fixParseTrees();
	}
    }

    return $self->{ISBLOCK};
}

sub dbprint
{
    my $self = shift;
    my $expanded = shift;
    my @keys = keys %{$self};

    print STDERR "Dumping object $self...\n";
    foreach my $key (@keys) {
	if ($expanded) {
		print STDERR "$key => ".dbprint_expanded($self->{$key})."\n";
	} else {
		print STDERR "$key => ".$self->{$key}."\n";
	}
    }
    print STDERR "End dump of object $self.\n";
}

sub preserve_spaces
{
    my $self = shift;
    if (@_) {
	$self->{PRESERVESPACES} = shift;
    }

    if (!defined($self->{PRESERVESPACES})) {
	return 0;
    }
    return $self->{PRESERVESPACES};
}

sub isFramework
{
    return 0;
}

sub getDoxyType
{
    my $self = shift;
    my $class = ref($self) || $self;

    if ($self->isAPIOwner()) {
	if ($class =~ /HeaderDoc::Header/) {
		return "file";
	}
	return "class";
    }
    if ($class =~ /HeaderDoc::Function/ || $class =~ /HeaderDoc::Method/) {
	return "function";
    }
    if ($class =~ /HeaderDoc::Constant/) {
	return "variable";
    }
    if ($class =~ /HeaderDoc::Var/) {
	return "variable";
    }
    if ($class =~ /HeaderDoc::Struct/) {
	return "struct";
    }
    if ($class =~ /HeaderDoc::Typedef/) {
	return "typedef";
    }
    if ($class =~ /HeaderDoc::Union/) {
	return "union";
    }
    if ($class =~ /HeaderDoc::Enum/) {
	return "enumeration";
    }
    if ($class =~ /HeaderDoc::PDefine/) {
	return "define";
    }
    if ($class =~ /HeaderDoc::MinorAPIElement/) {
	my $parent = $self->apiOwner();
	my $parentclass = ref($parent) || $parent;

	if ($parentclass =~ /HeaderDoc::Enum/) {
		return "enumvalue";
	} else {
		return "variable";
	}
    }
}

sub _getDoxyTagString
{
    my $self = shift;
    my $prespace = shift;

    my $class = ref($self) || $self;
    my $doxyTagString = "";
    my $type = $self->getDoxyType();

    my $accessControl = "";
    if ($self->can("accessControl")) {
        $accessControl = $self->accessControl();
    }
    if ($accessControl =~ /\S/) {
	$accessControl = " protection=\"$accessControl\"";
    } else {
	$accessControl = "";
    }

    $doxyTagString .= "$prespace<member kind=\"$type\"$accessControl>\n";

    my $arglist = "";
    if ($class =~ /HeaderDoc::Function/ || $class =~ /HeaderDoc::Method/) {
	$type = $self->returntype();
	$type =~ s/^\s*//s;
	$type =~ s/\s*$//s;

	$doxyTagString .= "$prespace  <type>$type</type>\n";
	my @args = $self->parsedParameters();
	my $comma = "";
	foreach my $obj (@args) {
		$arglist .= $comma;
		my $type .= $obj->type();
		if ($class =~ /HeaderDoc::Method/) {
			my $tagname = $obj->tagname();
			if ($tagname =~ /\S/) {
				$arglist .= "[$tagname] ";
			}
			$type =~ s/^\s*\(//s;
			$type =~ s/\)\s*$//s;
		}
		$arglist .= $type;
		if ($arglist !~ /[ *]$/) {
			$arglist .= " ";
		}
		$arglist .= $obj->name();
		$comma = ",";
	}
	$arglist = "($arglist)";
    } elsif ($class =~ /HeaderDoc::Var/ || $class =~ /HeaderDoc::Constant/) {
	$type = $self->returntype();
	$type =~ s/^\s*//s;
	$type =~ s/\s*$//s;
	$doxyTagString .= "$prespace  <type>$type</type>\n";
    }
    $doxyTagString .= "$prespace  <name>".$self->textToXML($self->rawname())."</name>\n";
    $doxyTagString .= "$prespace  <anchorfile></anchorfile>\n";
    $doxyTagString .= "$prespace  <anchor></anchor>\n";
    $doxyTagString .= "$prespace  <arglist>$arglist</arglist>\n";
    $doxyTagString .= "$prespace</member>\n";

    return $doxyTagString;
}

sub headerDump
{
	my $self = shift;

		print "HEADER\n";
		print "  |\n";
		print "  +-- Functions\n";
		foreach my $obj ($self->functions()) {
		print "  |     +-- ".$obj->name()."\n";
		print "  |     |     +-- obj:     ".$obj."\n";
		print "  |     |     +-- isBlock: ".$obj->isBlock()."\n";
		}
		print "  |\n";
		print "  +-- #defines\n";
		foreach my $obj ($self->pDefines()) {
		print "  |     +-- ".$obj->name()."\n";
		print "  |     |     +-- obj:     ".$obj."\n";
		print "  |     |     +-- isBlock: ".$obj->isBlock()."\n";
		}
}

1;
