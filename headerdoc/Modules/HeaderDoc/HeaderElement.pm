#! /usr/bin/perl -w
#
# Class name: HeaderElement
# Synopsis: Root class for Function, Typedef, Constant, etc. -- used by HeaderDoc.
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2005/01/14 08:11:26 $
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

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash unregisterUID registerUID quote html2xhtml sanitize parseTokens);
use strict;
use vars qw($VERSION @ISA);
use POSIX qw(strftime);
$VERSION = '$Revision: 1.7.2.11.2.113 $';

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
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
}

my %CSS_STYLES = ();

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
	$clone = shift;
    } else {
	$clone = $self->new();
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
    $clone->{ATTRIBUTELISTS} = $self->{ATTRIBUTELISTS};
    $clone->{APIOWNER} = $self->{APIOWNER};
    $clone->{APIUID} = $self->{APIUID};
    $clone->{LINKUID} = undef; # Don't ever copy this.
    $clone->{ORIGCLASS} = $self->{ORIGCLASS};
    $clone->{ISTEMPLATE} = $self->{ISTEMPLATE};
    $clone->{VALUE} = $self->{VALUE};
    $clone->{RETURNTYPE} = $self->{RETURNTYPE};
    $clone->{CLASS} = $self->{CLASS};
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
		print "MISSING API OWNER\n" if ($localDebug);
		return 0;
	} else {
	    my $apioclass = ref($apio) || $apio;
	    if ($apioclass ne "HeaderDoc::CPPClass") {
		print "Not in CPP Class\n" if ($localDebug);
		return 0;
	    }
	}
	my $name = $self->rawname();
	print "NAME: $name : " if ($localDebug);

	if ($name =~ /^~/o) {
		# destructor
		print "DESTRUCTOR\n" if ($localDebug);
		return 1;
	}
	$name =~ s/^\s*\w+\s*::\s*//so; # remove leading class part, if applicable
	$name =~ s/\s*$//so;

	my $classquotename = quote($apio->name());

	if ($name =~ /^$classquotename$/) {
		print "CONSTRUCTOR\n" if ($localDebug);
		return 1;
	}
	print "FUNCTION\n" if ($localDebug);
	return 0;
    } elsif ($self->{CLASS} eq "HeaderDoc::Method") {
	# @@@ FOR NOW @@@
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
    # foreach my $const (@{ $self->{CONSTANTS}}) {print $const->name()."\n";}
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

sub isTemplate {
    my $self = shift;
    if (@_) {
        $self->{ISTEMPLATE} = shift;
    }
    return $self->{ISTEMPLATE};
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

# /*! @function linenum
#     @abstract line number where a declaration began
#     @discussion We don't want to show this, so we can't use an
#        attribute.  This is private.
#  */
sub linenum {
    my $self = shift;

    if (@_) {
        my $linenum = shift;
        $self->{LINENUM} = $linenum;
    }
    return $self->{LINENUM};
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

sub name {
    my $self = shift;
    my $localDebug = 0;

    my($class) = ref($self) || $self;

    print "$class\n" if ($localDebug);

    if (@_) {
        my $name = shift;
	my $oldname = $self->{NAME};
	my $filename = $self->filename();
	my $linenum = $self->linenum();
	my $class = ref($self) || $self;

	if (!($class eq "HeaderDoc::Header") && ($oldname && length($oldname))) {
		# Don't warn for headers, as they always change if you add
		# text after @header.  Also, don't warn if the new name
		# contains the entire old name, to avoid warnings for
		# multiword names.  Otherwise, warn the user because somebody
		# probably put multiple @function tags in the same comment
		# block or similar....

		$oldname = quote($oldname);

		if ($name !~ /$oldname/) {
			if (!$HeaderDoc::ignore_apiuid_errors) {
				warn("$filename:$linenum:Name being changed ($oldname -> $name)\n");
			}
		} elsif (($class eq "HeaderDoc::CPPClass" || $class =~ /^ObjC/o) && $name =~ /:/o) {
			warn("$filename:$linenum:Class name contains colon, which is probably not what you want.\n");
		}
	}

	$name =~ s/\n$//sgo;
	$name =~ s/\s$//sgo;

        $self->{NAME} = $name;
    }

    my $n = $self->{NAME};

    if (($class eq "HeaderDoc::Function") || 
	($class eq "HeaderDoc::Method")) {
	  my @params = $self->parsedParameters();
	  my $arrayLength = @params;
	  if ($self->conflict() && $arrayLength) {
		# print "CONFLICT for $n!\n";
		$n .= "(";
		my $first = 1;
		foreach my $param (@params) {
			if (!$first) {
				$n .= ", ".$param->type();
			} else {
				$n .= $param->type();
				$first = 0;
			}
		}
		$n .= ")";
	  }
    }

    return $n;
}

# /*! @function see
#     @abstract Add see/seealso (JavaDoc compatibility enhancement)
#  */
sub see {
    my $self = shift;
    my $liststring = shift;
    my $type = "See";
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();    

    $liststring =~ s/<br>/\n/sg;

    # Is it a see or seealso?

    if ($liststring =~ s/^seealso\s+//so) {
	$type = "See Also";
    } else {
	$liststring =~ s/^see\s+//so;
    }

# print "LS: $liststring\n";

    if ($liststring !~ /^\/\/$apiUIDPrefix\//) {
	my @list = split(/\s+/, $liststring);
	foreach my $see (@list) {
		my $apiref = $self->genRef("", $see, $see);
		my $apiuid = $apiref;
		$apiuid =~ s/^<!--\s*a\s+logicalPath\s*=\s*\"//so;
		$apiuid =~ s/"\s*-->\s*$see\s*<!--\s*\/a\s*-->//s;
		$self->attributelist($type, "$see $apiuid");
	}
    } else {
	$liststring =~ s/^\s*//s;
	$liststring =~ s/\s+/ /s;
	my @parts = split(/\s+/, $liststring, 2);

	# print "$type -> ".$parts[1]." -> ".$parts[0]."\n";
	
	$self->attributelist($type, $parts[1]."\n".$parts[0]);
    }

}

sub rawname {
    my $self = shift;
    my $localDebug = 0;

    if (@_) {
	my $name = shift;
	$self->{RAWNAME} = $name;
	print "RAWNAME: $name\n" if ($localDebug);
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
    my $localDebug = 0;

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

    %attlist->{$name}=$attribute;

    if ($long) {
        $self->{LONGATTRIBUTES} = \%attlist;
    } else {
        $self->{SINGLEATTRIBUTES} = \%attlist;
    }

    my $temp = $self->getAttributes(2);
    print "Attributes: $temp\n" if ($localDebug);;

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

    my $apiowner = $self->apiOwner();
    if ($apiowner->outputformat() eq "hdxml") { $xml = 1; }

    my $retval = "";
    if ($long != 1) {
        if ($self->{SINGLEATTRIBUTES}) {
	    %attlist = %{$self->{SINGLEATTRIBUTES}};
        }

        foreach my $key (sort strcasecmp keys %attlist) {
	    my $value = %attlist->{$key};
	    my $newatt = $value;
	    if (($key eq "Superclass" || $key eq "Extends&nbsp;Class") && !$xml) {
		my @valparts = split(/\cA/, $value);
		$newatt = "";
		foreach my $valpart (@valparts) {
			if (length($valpart)) {
				$valpart =~ s/^\s*//s;
				if ($valpart =~ /^(\w+)(\W.*)$/) {
					$newatt .= $self->genRef("class", $1, $1).$self->textToXML($2).", ";
				} else {
					$newatt .= $self->genRef("class", $valpart, $valpart).", ";
				}
			}
		}
		$newatt =~ s/, $//s;
	    } else {
		print "KEY: $key\n" if ($localDebug);
	    }
	    if ($xml) {
		$retval .= "<attribute><name>$key</name><value>$newatt</value></attribute>\n";
	    } else {
		$retval .= "<b>$key:</b> $newatt<br>\n";
	    }
        }
    }

    if ($long != 0) {
        if ($self->{LONGATTRIBUTES}) {
	    %attlist = %{$self->{LONGATTRIBUTES}};
        }

        foreach my $key (sort strcasecmp keys %attlist) {
	    my $value = %attlist->{$key};
	    if ($xml) {
		$retval .= "<longattribute><name>$key</name><value>$value</value></longattribute>\n";
	    } else {
		$retval .= "<b>$key:</b>\n\n<p>$value<p>\n";
	    }
        }
    }

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

    my $value = $longatts{$name};
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

    # print "list\n";
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

    my $apiowner = $self->apiOwner();
    if ($apiowner->outputformat() eq "hdxml") { $xml = 1; }

    my %attlists = ();
    if ($self->{ATTRIBUTELISTS}) {
	%attlists = %{$self->{ATTRIBUTELISTS}};
    }

    # print "list\n";
    my $retval = "";
    foreach my $key (sort strcasecmp keys %attlists) {
	if ($xml) {
	    $retval .= "<listattribute><name>$key</name><list>\n";
	} else {
	    $retval .= "<b>$key:</b><br><blockquote><dl>\n";
	}
	print "key $key\n" if ($localDebug);
	my @list = @{%attlists->{$key}};
	foreach my $item (@list) {
	    print "item: $item\n" if ($localDebug);
	    my ($name, $disc) = &getAPINameAndDisc($item);

	    if ($key eq "Included Defines") {
		# @@@ CHECK SIGNATURE
		my $apiref = $self->apiref($composite, "macro", $name);
		$name .= "$apiref";
	    }
	    if (($key eq "See Also" || $key eq "See") && !$xml) {
		$disc =~ s/^\s*//sgo;
		$disc =~ s/\s*$//sgo;
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
	    $retval .= "</dl></blockquote>\n";
	}
    }
    # print "done\n";
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
    if ($self->{ATTRIBUTELISTS}) {
        %attlists = %{$self->{ATTRIBUTELISTS}};
    }

    my @list = ();
    my $listref = %attlists->{$name};
    if ($listref) {
	@list = @{$listref};
    }
    push(@list, $attribute);

    %attlists->{$name}=\@list;
    $self->{ATTRIBUTELISTS} = \%attlists;
    # print "AL = $self->{ATTRIBUTELISTS}\n";

    # print $self->getAttributeLists()."\n";
}

sub apiOwner {
    my $self = shift;
    if (@_) {
	my $temp = shift;
	$self->{APIOWNER} = $temp;
    }
    return $self->{APIOWNER};
}

sub apiref {
    my $self = shift;
    my $filename = $self->filename();
    my $linenum = $self->linenum();
    my $composite = shift;
    my $args = 0;
    my $type = "";
    my $apiowner = $self->apiOwner();
    my $owningclass = ref($apiowner) || $self;
    my $paramSignature = "";

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

    # Don't provide API refs for inherited data or functions.
    my $forceuid = "";
    if ($self->origClass() ne "") {
	$forceuid = $self->generateLinkUID($composite);
    }

    # we sanitize things now.
    # if ($paramSignature =~ /[ <>\s\n\r]/o) {
	# warn("$filename:$linenum:apiref: bad signature \"$paramSignature\".  Dropping ref.\n");
	# return "";
    # }

    my $uid = "";
    if ($args && !$forceuid) {
      # Do this first to assign a UID, even if we're doing the composite page.
      $uid = $self->apiuid($type, $paramSignature);
    } else {
      $uid = $self->apiuid();
    }
    if ($composite && !$HeaderDoc::ClassAsComposite) {
	$uid = $self->compositePageUID();
    }
    if ($forceuid) { $uid = $forceuid; }

    my $ret = "";
    if (length($uid)) {
	my $name = $self->name();
	if ($self->can("rawname")) { $name = $self->rawname(); }
	my $extendedname = $name;
	if ($owningclass ne "HeaderDoc::Header" && $self->sublang() ne "C") {
		# Don't do this for COM interfaces and C pseudoclasses
		$extendedname = $apiowner->rawname() . "::" . $name;
	}
	$extendedname =~ s/\s//sgo;
	$extendedname =~ s/<.*?>//sgo;
        $extendedname =~ s/;//sgo;
	my $uidstring = "";
	my $indexgroup = $self->indexgroup();
	if (length($uid)) { $uidstring = " uid=$uid; "; }
	if (length($indexgroup)) { $uidstring .= " indexgroup=$indexgroup; "; }
	$ret .= "<!-- headerDoc=$type; $uidstring name=$extendedname -->\n";
	if (length($uid)) { $ret .= "<a name=\"$uid\"></a>\n"; }
    }
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
	# print "LINKUID WAS ".$self->{LINKUID}."\n";
	if ($composite) {
		return $self->compositePageUID();
	}
	return $self->{LINKUID};
    }

    my $classname = sanitize($self->apiOwner()->rawname());
    my $name = sanitize($self->rawname());
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

sub apiuid {
    my $self = shift;
    my $type = "AUTO";
    my $paramSignature_or_alt_define_name = "";
    my $filename = $self->filename();
    my $linenum = $self->linenum();

    if (@_) {
	$type = shift;
	if (@_) {
		$paramSignature_or_alt_define_name = shift;
	}
    } else {
# print "RETURNING APIUID ".$self->{APIUID}."\n";
	if ($self->{LINKUID}) { return $self->{LINKUID}; }
	return $self->{APIUID};
    }

    my $olduid = $self->{APIUID};
    if ($self->{LINKUID}) { $olduid = $self->{LINKUID}; }

    my $name = $self->name();
    my $localDebug = 0;
    my $className; 
    my $lang = $self->sublang();
    my $class = ref($self) || $self;

    if (!($self->can("conflict")) || ($self->can("conflict") && !($self->conflict()))) {
	$name = $self->rawname();
	if ($class eq "HeaderDoc::ObjCCategory") {
		# Category names are in the form "ClassName (DelegateName)"
		if ($name =~ /\s*\w+\s*\(.+\).*/o) {
			$name =~ s/.*\((.*)\)/$1/o;
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
		# warn("$filename:$linenum:apiref: bad name \"$name\".  Dropping ref.\n");
	    # }
	    # return "";
	# }
    } else {
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
		# warn("$filename:$linenum:apiref: bad name \"$name\".  Dropping ref.\n");
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
	    $lang = $self->lang();
	}
    }

    if ($lang eq "C") { $lang = "c"; }
    if ($lang eq "Csource") { $lang = "c"; }
    if ($lang eq "occCat") { $lang = "occ"; }
    if ($lang eq "intf") { $lang = "occ"; }

    $name =~ s/\n//smgo;

    # my $lang = "c";
    # my $class = ref($HeaderDoc::APIOwner) || $HeaderDoc::APIOwner;

    # if ($class =~ /^HeaderDoc::CPPClass$/o) {
        # $lang = "cpp";
    # } elsif ($class =~ /^HeaderDoc::ObjC/o) {
        # $lang = "occ";
    # }

    print "LANG: $lang\n" if ($localDebug);
    # my $classHeaderObject = HeaderDoc::APIOwner->headerObject();
    # if (!$classHeaderObject) { }
    if ($parentClassType eq "HeaderDoc::Header" || $lang eq "c") {
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
        $className = $parentClass->name();
	if (length($name)) { $className .= "/"; }
    }
    $className =~ s/\s//sgo;
    $className =~ s/<.*?>//sgo;

    # Macros are not part of a class in any way.
    my $class = ref($self) || $self;
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
    }

# warn("genRefSub: \"$lang\" \"$type\" \"$name\" \"$className\" \"$paramSignature_or_alt_define_name\"\n");

    my $uid = $self->genRefSub($lang, $type, $name, $className, $paramSignature_or_alt_define_name);

    $self->{APIUID} = $uid;
    unregisterUID($olduid, $name);
    registerUID($uid, $name);
# print "APIUID SET TO $uid\n";
    return $uid;

    # my $ret .= "<a name=\"$uid\"></a>\n";
    # return $ret;
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
    my $name = sanitize($orig_name);
    my $className = sanitize($orig_className);
    my $paramSignature = sanitize($orig_paramSignature);

    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();    
    my $localDebug = 0;

    if ($lang eq "C") { $lang = "c"; }

    my $uid = "//$apiUIDPrefix/$lang/$type/$className$name$paramSignature";
    return $uid;
}

sub throws {
    my $self = shift;

    if (@_) {
	my $new = shift;
	$new =~ s/\n//smgo;
        $self->{THROWS} .= "<li>$new</li>\n";
	$self->{XMLTHROWS} .= "<throw>$new</throw>\n";
	# print "Added $new to throw list.\n";
    }
    # print "dumping throw list.\n";
    if (length($self->{THROWS})) {
    	return ("<ul>\n" . $self->{THROWS} . "</ul>");
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

    if (@_) {
        $self->{ABSTRACT} = $self->linkfix(shift);
    }

    my $ret = $self->{ABSTRACT};

    # print "RET IS \"$ret\"\n";

    $ret =~ s/(\s*<br><br>\s*)*$//sg;

    # print "RET IS \"$ret\"\n";

    return $ret;
}

sub XMLabstract {
    my $self = shift;

    if (@_) {
        $self->{ABSTRACT} = shift;
    }
    return $self->htmlToXML($self->{ABSTRACT});
}


sub discussion {
    my $self = shift;

    if (@_) {
	my $olddisc = $self->{DISCUSSION};
	if ($olddisc && length($olddisc)) {
		$olddisc =~ s/<br>/\n/smgo;

		my $oldname = $self->name();

		if ($olddisc =~ /\n/o) {
		    my $nlcheck = $olddisc;
		    my $firstline = "";
		    if ($nlcheck =~ s/(.*?)\n//sog) {
			$firstline = $1;
		    } else {
			$firstline = $nlcheck;
			$nlcheck = "";
		    }
		    $nlcheck =~ s/\s//sog;
		    if (length($nlcheck)) {
			my $filename = $self->filename();
			my $linenum = $self->linenum();
			if (!$self->inDefineBlock()) {
				# We'll be quiet if we're in a define block, as
				# This is just the natural course of things.
				warn("$filename:$linenum:Multiple discussions found for $oldname.  Ignoring first.\n");
			}
			# It's bad, so don't include it at all.
			$firstline = "";
		    }
		    if (length($firstline)) {
			$self->name($oldname." ".$firstline);
		    }
		} else {
		    $self->name($oldname." ".$olddisc);
		}
	}

        my $discussion = "";
        $discussion = shift;

	# $discussion =~ s/<br>/\n/sgo;
	$discussion = $self->listfixup($discussion);

        $discussion =~ s/\n\n/<br>\n/go;
        $self->{DISCUSSION} = $self->linkfix($discussion);
    }
    return $self->{DISCUSSION};
}

sub listfixup
{
    my $self = shift;
    my $olddiscussion = shift;
    my $discussion = "";

    my $numListDebug = 0;

    if ($HeaderDoc::dumb_as_dirt) {
	print "BASIC MODE: LIST FIXUP DISABLED\n" if ($numListDebug);
	return $olddiscussion;
    }

    print "processing ".$self->name().".\n" if ($numListDebug);

    my @disclines = split(/([\n\r])/, $olddiscussion);
    my $curpos = 0;
    my $seekpos = 0;
    my $nlines = scalar(@disclines);

    my $oldinList = 0;
    while ($curpos < $nlines) {
	my $line = $disclines[$curpos];
	if ($line =~ /^\s*((?:-)?\d+)[\)\.\:\s]/o) {
		# this might be the first entry in a list.
	print "MAYBELIST\n" if ($numListDebug);
		my $inList = 1;
		my $foundblank = 0;
		my $basenum = $1;
		$seekpos = $curpos + 1;
		if (($seekpos >= $nlines) && !$oldinList) {
			$discussion .= "$line";
		} else {
		    while (($seekpos < $nlines) && ($inList == 1)) {
			my $scanline = @disclines[$seekpos];
			if ($scanline =~ /^<br><br>$/o) {
				# empty line
				$foundblank = 1;
				# print "BLANKLINE\n" if ($numListDebug);
			} elsif ($scanline =~ /^\s*((?:-)?\d+)[\)\.\:\s]/o) {
				# line starting with a number
				$foundblank = 0;
	# print "D1 is $1\n";
				if ($1 != ($basenum + 1)) {
					# They're noncontiguous.  Not a list.
					if (!$oldinList) {
						$discussion .= "$line";
					}
					$inList = 0;
				} else {
					# They're contiguous.  It's a list.
					$inList = 2;
				}
			} else {
				# text.
				if ($foundblank && ($scanline =~ /\S+/o)) {
					# skipped a line and more text.
					# end the list here.
					print "LIST MAY END ON $scanline\n" if ($numListDebug);
					print "BASENUM IS $basenum\n" if ($numListDebug);
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
			print "LISTCONTINUES: $line\n" if ($numListDebug);
		} elsif ($inList == 3) {
			# this is a singleton.  Don't touch it.
			$discussion .= $line;
			print "SINGLETON: $line\n" if ($numListDebug);
		} elsif ($inList == 2) {
			# this is the first entry in a list
			$line =~ s/^\s*((?:-)?\d+)[\)\.\:\s]//so;
			$basenum = $1;
			$discussion .= "<ol start=\"$basenum\"><li>$line";
			print "FIRSTENTRY: $line\n" if ($numListDebug);
		}
		$oldinList = $inList;
	} elsif ($line =~ /^<br><br>$/o) {
		if ($oldinList == 3 || $oldinList == 1) {
			# If 3, this was last entry in list before next
			# text.  If 1, this was last entry in list before
			# we ran out of lines.  In either case, it's a
			# blank line not followed by another numbered
			# line, so we're done.

			print "OUTERBLANKLINE\n" if ($numListDebug);
			$discussion .= "</li></ol>";
			$oldinList = 0;
		} else {
			 print "OIL: $oldinList\n" if ($numListDebug);
			$discussion .= "$line";
		}
	} else {
		print "TEXTLINE: \"$line\"\n" if ($numListDebug);
		$discussion .= $line;
	}
	$curpos++;
    }
    if ($oldinList) {
	$discussion .= "</li></ol>";
    }

    print "done processing ".$self->name().".\n" if ($numListDebug);
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
    my $filename = $self->filename();
    my $linenum = $self->linenum();
    my $tail = "";
    my $xml = 0;

    if ($self->outputformat() eq "hdxml") { $xml = 1; }

    # Generate requests with sublang always (so that, for
    # example, a c++ header can link to a class from within
    # a typedef declaration.  Generate anchors with lang
    # if the parent is a header, else sublang for stuff
    # within class braces so that you won't get name
    # resolution conflicts if something in a class has the
    # same name as a generic C entity, for example.

    my $lang = $self->sublang();
    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
	$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$typedefname, $varname, $constname, $structisbrace, $macronamesref,
	$classregexp, $classbraceregexp, $classclosebraceregexp,
	$accessregexp) = parseTokens($self->lang(), $self->sublang());

    if ($name =~ /^[\d\[\]]/o) {
	# Silently fail for [4] and similar.
	return $linktext;
    }

    if (($name =~ /^[=|+-\/&^~!*]/o) || ($name =~ /^\s*\.\.\.\s*$/o)) {
	# Silently fail for operators
	# and varargs macros.

	return $linktext;
    }
    # if (($name =~ /^\s*public:/o) || ($name =~ /^\s*private:/o) ||
	# ($name =~ /^\s*protected:/o)) {
    if (length($accessregexp) && ($name =~ /$accessregexp(:)?/)) {
	# Silently fail for these, too.

	return $linktext;
    }
    if ($name =~ s/\)\s*$//o) {
	if ($linktext =~ s/\)\s*$//o) {
		$tail = ")";
	} else {
		warn("$filename:$linenum:WARNING: Parenthesis in ref name, not in link text\n");
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
		warn("$filename:$linenum: bogus paren in #define\n");
	} elsif (($name eq "(") && $class eq "HeaderDoc::Function") {
		warn("$filename:$linenum: bogus paren in function\n");
	} elsif ($class eq "HeaderDoc::Function") {
		warn("$filename:$linenum: bogus paren in function\n");
	} else {
		warn("$filename:$linenum: $filename $classname $class $keystring generates bad crossreference ($name).  Dumping trace.\n");
		# my $declaration = $self->declaration();
		# warn("BEGINDEC\n$declaration\nENDDEC\n");
		$self->printObject();
	}
    }

    if ($name =~ /(.+)::(.+)/o) {
	my $classpart = $1;
	my $type = $2;
	if ($linktext !~ /::/o) {
		warn("$filename:$linenum:Bogus link text generated for item containing class separator.  Ignoring.\n");
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
	if (!$xml) {
        	$ret .= "<!-- a logicalPath=\"$ref1 $ref2 $ref3 $ref4 $ref5 $ref6 $ref7 $ref8 $ref9 $ref10 $ref11 $ref12 $ref13\" -->$type<!-- /a -->";
	} else {
        	$ret .= "<hd_link logicalPath=\"$ref1 $ref2 $ref3 $ref4 $ref5 $ref6 $ref7 $ref8 $ref9 $ref10 $ref11 $ref12 $ref13\">$type</hd_link>";
	}

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
		$class_or_enum_check =~ s/\s+$keywordquot\s+/ /sg;
	} else {
		$class_or_enum_check =~ s/\s+$keywordquot\s+/ /sgi;
	}
    }

    $class_or_enum_check =~ s/\s*//smgo;

    if (length($class_or_enum_check)) {
	SWITCH: {
	    ($keystring =~ /type/o && $lang eq "pascal") && do { $type = "tdef"; last SWITCH; };
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
	    ($keystring =~ /class/o) && do { $type = "cl"; $className=$name; $name=""; last SWITCH; };
	    ($keystring =~ /#(define|ifdef|ifndef|if|endif|pragma|include|import)/o) && do {
		    # Used to include || $keystring =~ /class/o
		    # defines and similar aren't followed by a type
		    return $linktext.$tail;
		};
	    {
		$type = "";
		my $name = $self->name();
		warn "$filename:$linenum keystring ($keystring) in $name type link markup\n";
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

	    if (!$xml) {
	        return "<!-- a logicalPath=\"$ref1 $ref2 $ref3 $ref4 $ref5 $ref6\" -->$linktext<!-- /a -->".$tail;
	    } else {
	        return "<hd_link logicalPath=\"$ref1 $ref2 $ref3 $ref4 $ref5 $ref6\">$linktext</hd_link>".$tail;
	    }
	} else {
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
        my $ref7a = $self->genRefSub($lang, "cl", $name, $className);
        my $ref8 = $self->genRefSub($lang, "tdef", $name, "");
        my $ref9 = $self->genRefSub($lang, "tag", $name, "");
        my $ref10 = $self->genRefSub($lang, "econst", $name, "");
        my $ref11 = $self->genRefSub($lang, "struct", $name, "");
        my $ref12 = $self->genRefSub($lang, "data", $name, $className);
        my $ref13 = $self->genRefSub($lang, "clconst", $name, $className);

        my $ref1 = $self->genRefSub($lang, "instm", $name, $className);
        my $ref2 = $self->genRefSub($lang, "clm", $name, $className);
        my $ref2a = $self->genRefSub($lang, "intfcm", $name, $className);
        my $ref3 = $self->genRefSub($lang, "func", $name, $className);
        my $ref4 = $self->genRefSub($lang, "ftmplt", $name, $className);
        my $ref5 = $self->genRefSub($lang, "defn", $name, "");
        my $ref6 = $self->genRefSub($lang, "macro", $name, "");

	my $masterref = "$ref7 $ref7a $ref8 $ref9 $ref10 $ref11 $ref12 $ref13 $ref1 $ref2 $ref2a $ref3 $ref4 $ref5 $ref6";

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
					$masterref = "$ref1 $ref2 $ref2a $ref3 $ref4 $ref5 $ref6";
					last SWITCH;
				};
			($optional_expected_type eq "var") && do {
					# Variable name.
					$masterref = "$ref12";
					last SWITCH;
				};
			($optional_expected_type eq "template") && do {
					# Could be any template parameter bit
					# (type or name).  Since we don't care
					# much if a parameter name happens to
					# something (ideally, it shouldn't),
					# we'll just assume we're getting a
					# type and be done with it.
					$masterref = "$ref7 $ref7a $ref8 $ref9 $ref10 $ref11 $ref13";
					last SWITCH;
				};
			($optional_expected_type eq "type") && do {
					$masterref = "$ref7 $ref7a $ref8 $ref9 $ref10 $ref11 $ref13";
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
				warn("Unknown reference class \"$optional_expected_type\" in genRef\n");
			}
		}
	}

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
    # my $filename = $HeaderDoc::headerObject->filename();
    my $filename = $self->filename();
    my $linenum = $self->linenum();
    my $case_sensitive = 1;

    if (!$self->isAPIOwner()) { my $apio = $self->apiOwner(); return $apio->keywords(); }
    if ($self->{KEYWORDHASH}) { return ($self->{CASESENSITIVE}, $self->{KEYWORDHASH}); }

    print "keywords\n" if ($localDebug);

    # print "Color\n" if ($localDebug);
    # print "lang = $HeaderDoc::lang\n";

    # Note: these are not all of the keywords of each language.
    # This should, however, be all of the keywords that can occur
    # in a function or data type declaration (e.g. the sort
    # of material you'd find in a header).  If there are missing
    # keywords that meet that criterion, please file a bug.

    my %CKeywords = ( 
	"auto" => 1, "const" => 1, "enum" => 1, "extern" => 1, "inline" => 1,
	"__inline__" => 1, "__inline" => 1, "__asm" => 1, "__asm__" => 1,
        "__attribute__" => 1,
	"register" => 1, "signed" => 1, "static" => 1, "struct" => 1, "typedef" => 1,
	"union" => 1, "unsigned" => 1, "volatile" => 1, "#define" => 1,
	"#ifdef" => 1, "#ifndef" => 1, "#if" => 1, "#endif" => 1,
 	"#pragma" => 1, "#include" => 1, "#import" => 1 , "NULL" => 1,
	"true" => 1, "false" => 1);
    my %CppKeywords = (%CKeywords,
	("class" => 1, 
	"friend" => 1,
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
	"nil" => 1,
	"YES" => 1,
	"NO" => 1 ));
    my %phpKeywords = (%CKeywords, ("function" => 1));
    my %javaKeywords = (%CKeywords, (
	"class" => 1, 
	"extends" => 1,
	"implements" => 1,
	"import" => 1,
	"instanceof" => 1,
	"interface" => 1,
	"native" => 1,
	"package" => 1,
	"private" => 1,
	"protected" => 1,
	"public" => 1,
	"strictfp" => 1,
	"super" => 1,
	"synchronized" => 1,
	"throws" => 1,
	"transient" => 1,
	"template" => 1,
	"volatile"  => 1));
    my %perlKeywords = ( "sub"  => 1);
    my %shellKeywords = ( "sub"  => 1);
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
    my %MIGKeywords = (
	"routine" => 1, "simpleroutine" => 1, "countinout" => 1, "inout" => 1, "in" => 1, "out" => 1,
	"subsystem" => 1, "skip" => 1, "#define" => 1,
	"#ifdef" => 1, "#ifndef" => 1, "#if" => 1, "#endif" => 1,
 	"#pragma" => 1, "#include" => 1, "#import" => 1 );

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
	    warn "$filename:$linenum:Unknown language ($lang:$sublang)\n";
	}
    }
    if ($lang eq "Csource") {
	SWITCH: {
	    ($sublang eq "Csource") && do { last SWITCH; };
	    warn "$filename:$linenum:Unknown language ($lang:$sublang)\n";
	}
    }
    if ($lang eq "php") {
	SWITCH: {
	    ($sublang eq "php") && do { %keywords = %phpKeywords; last SWITCH; };
	    warn "$filename:$linenum:Unknown language ($lang:$sublang)\n";
	}
    }
    if ($lang eq "java") {
	SWITCH: {
	    ($sublang eq "java") && do { %keywords = %javaKeywords; last SWITCH; };
	    ($sublang eq "javascript") && do { %keywords = %javaKeywords; last SWITCH; };
	    warn "$filename:$linenum:Unknown language ($lang:$sublang)\n";
	}
    }
    if ($lang eq "perl") {
	SWITCH: {
	    ($sublang eq "perl") && do { %keywords = %perlKeywords; last SWITCH; };
	    warn "$filename:$linenum:Unknown language ($lang:$sublang)\n";
	}
    }
    if ($lang eq "shell") {
	SWITCH: {
	    ($sublang eq "shell") && do { %keywords = %shellKeywords; last SWITCH; };
	    warn "$filename:$linenum:Unknown language ($lang:$sublang)\n";
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
	# print "keyword $keyword\n";
    # }

    $self->{KEYWORDHASH} = \%keywords;
    $self->{CASESENSITIVE} = $case_sensitive;

# print "KEYS\n";foreach my $key (keys %keywords) { print "KEY: $key\n"; }print "ENDKEYS\n";

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
	print "FASTPATH FOR $debugname\n" if ($localDebug);
	return $xmldec;
    }

    # print "RETURNING:\n$xmldec\nENDRETURN\n";

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

		if ($HeaderDoc::use_styles && !$disable_styles) {
			my $parseTree_ref = $self->parseTree();
			my $parseTree = ${$parseTree_ref};
			bless($parseTree, "HeaderDoc::ParseTree");
			if ($self->can("isBlock") && $self->isBlock()) {
				$xmldec = "";
				my @tree_refs = @{$self->parseTreeList()};

				foreach my $tree_ref (@tree_refs) {
					my $tree = ${$tree_ref};
					bless($tree,  "HeaderDoc::ParseTree");
					$xmldec .= $tree->xmlTree()."\n";
				}

			} else {
				$xmldec = $parseTree->xmlTree();
			}
			$self->{DECLARATIONINHTML} = $xmldec;
		} else {
        		$self->{DECLARATIONINHTML} = $self->textToXML($xmldec);
		}


		return $xmldec;
	}
		
	my $declaration = shift;

	if ($HeaderDoc::use_styles && !$disable_styles) {
	  # print "I AM ".$self->name()." ($self)\n";
	  if ($self->can("isBlock") && $self->isBlock()) {
		my $declaration = "";
		my @defines = $self->parsedParameters();

		foreach my $define (@defines) {
			$declaration .= $define->declarationInHTML();
			$declaration .= "\n";
		}
		$declaration = "";
	  } else {
		my $parseTree_ref = $self->parseTree();
		my $parseTree = ${$parseTree_ref};
		bless($parseTree, "HeaderDoc::ParseTree");
		# print "PT: ".$parseTree."\n";
		$declaration = $parseTree->htmlTree();
	  }
	}

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

sub availability {
    my $self = shift;

    if (@_) {
        $self->{AVAILABILITY} = shift;
    }
    return $self->{AVAILABILITY};
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

	print "updated is $updated\n" if ($localdebug);
	if (!($updated =~ /\d\d\d\d-\d\d-\d\d/o )) {
	    if (!($updated =~ /\d\d-\d\d-\d\d\d\d/o )) {
		if (!($updated =~ /\d\d-\d\d-\d\d/o )) {
		    # my $filename = $HeaderDoc::headerObject->filename();
		    my $filename = $self->filename();
		    my $linenum = $self->linenum();
		    warn "$filename:$linenum:Bogus date format: $updated.\n";
		    warn "$filename:$linenum:Valid formats are MM-DD-YYYY, MM-DD-YY, and YYYY-MM-DD\n";
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
                    print "YEAR: $year" if ($localdebug);
		}
	    } else {
		print "03-25-2003 case.\n" if ($localdebug);
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
		# my $filename = $HeaderDoc::headerObject->filename();
		my $filename = $self->filename();
		my $linenum = $self->linenum();
		warn "$filename:$linenum:Invalid date (year = $year, month = $month, day = $day).\n";
		warn "$filename:$linenum:Valid formats are MM-DD-YYYY, MM-DD-YY, and YYYY-MM-DD\n";
		return $self->{UPDATED};
	} else {
		$self->{UPDATED} = HeaderDoc::HeaderElement::strdate($month, $day, $year);
		print "date set to ".$self->{UPDATED}."\n" if ($localdebug);
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
 
    print "------------------------------------\n";
    print "HeaderElement\n";
    print "name: $self->{NAME}\n";
    print "abstract: $self->{ABSTRACT}\n";
    print "declaration: $dec\n";
    print "declaration in HTML: $self->{DECLARATIONINHTML}\n";
    print "discussion: $self->{DISCUSSION}\n";
    print "linkageState: $self->{LINKAGESTATE}\n";
    print "accessControl: $self->{ACCESSCONTROL}\n\n";
    print "Tagged Parameter Descriptions:\n";
    my $taggedParamArrayRef = $self->{TAGGEDPARAMETERS};
    if ($taggedParamArrayRef) {
	my $arrayLength = @{$taggedParamArrayRef};
	if ($arrayLength > 0) {
	    &printArray(@{$taggedParamArrayRef});
	}
	print "\n";
    }
    my $fieldArrayRef = $self->{CONSTANTS};
    if ($fieldArrayRef) {
        my $arrayLength = @{$fieldArrayRef};
        if ($arrayLength > 0) {
            &printArray(@{$fieldArrayRef});
        }
        print "\n";
    }
}

sub linkfix {
    my $self = shift;
    my $inpString = shift;
    my @parts = split(/\</, $inpString);
    my $first = 1;
    my $outString = "";
    my $localDebug = 0;

    print "Parts:\n" if ($localDebug);
    foreach my $part (@parts) {
	print "$part\n" if ($localDebug);
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

			print "Found link.\nlinkpart: $linkpart\nrest: $rest\n" if ($localDebug);

			if ($linkpart =~ /target\=\".*\"/sio) {
			    print "link ok\n" if ($localDebug);
			    $outString .= "<$part";
			} else {
			    print "needs fix.\n" if ($localDebug);
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

sub strdate
{
    my $month = shift;
    my $day = shift;
    my $year = shift;
    my $format = $HeaderDoc::datefmt;

    my $time = strftime($format, 0, 0, 0,
	$day, $month, $year-1900, 0, 0, 0);
    return $time;

    # print "format $format\n";

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
	  print "Unknown date format ($format) in config file[1]\n";
	  print "Assuming MDY\n";
	  return "$month/$day/$year";
	}
	SWITCH: {
	  ($format =~ /^..M/io) && do { $dateString .= "$month$secondsep" ; last SWITCH; };
	  ($format =~ /^..D/io) && do { $dateString .= "$day$secondsep" ; last SWITCH; };
	  ($format =~ /^..Y/io) && do { $dateString .= "$year$secondsep" ; last SWITCH; };
	  ($firstsep eq "") && do { last SWITCH; };
	  print "Unknown date format ($format) in config file[2]\n";
	  print "Assuming MDY\n";
	  return "$month/$day/$year";
	}
	SWITCH: {
	  ($format =~ /^....M/io) && do { $dateString .= "$month" ; last SWITCH; };
	  ($format =~ /^....D/io) && do { $dateString .= "$day" ; last SWITCH; };
	  ($format =~ /^....Y/io) && do { $dateString .= "$year" ; last SWITCH; };
	  ($secondsep eq "") && do { last SWITCH; };
	  print "Unknown date format ($format) in config file[3]\n";
	  print "Assuming MDY\n";
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
	%CSS_STYLES->{$name} = $style;
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
    my $filename = $self->filename();
    my $line = $self->linenum();
    my $exit = 0;

    # This function, bugs notwithstanding, is no longer useful.
    return 1;

    SWITCH: {
	($class eq "HeaderDoc::APIOwner") && do { return 1; };
	($class eq "HeaderDoc::CPPClass") && do { return 1; };
	($class eq "HeaderDoc::Constant") && do { return 1; };
	($class eq "HeaderDoc::Enum") && do { $keyword = "enum"; last SWITCH; };
	($class eq "HeaderDoc::Function") && do { return 1; };
	($class eq "HeaderDoc::Header") && do { return 1; };
	($class eq "HeaderDoc::Method") && do { return 1; };
	($class =~ /^HeaderDoc::ObjC/o) && do { return 1; };
	($class eq "HeaderDoc::PDefine") && do { $keyword = "#define"; last SWITCH; };
	($class eq "HeaderDoc::Struct") && do {
			if ($self->isUnion()) {
				$keyword = "union";
			} else {
				if ($lang eq "pascal") {
					$keyword = "record";
				} else {
					$keyword = "struct";
				}
			}
			last SWITCH;
		};
	($class eq "HeaderDoc::Typedef") && do {
				if ($lang eq "pascal") {
					$keyword = "type";
				} else {
					$keyword = "typedef";
				}
				last SWITCH;
			};
	($class eq "HeaderDoc::Var") && do { return 1; };
	{
	    return 1;
	}
    }

    my $declaration = $self->declaration();
    if ($declaration !~ /^\s*$keyword/m &&
        ($lang ne "pascal" || $declaration !~ /\W$keyword\W/m)) {
		if ($class eq "HeaderDoc::Typedef") {
			warn("$filename:$line:Keyword $keyword not found in $name declaration.\n");
			return 0;
		} else {
			if ($declaration !~ /^\s*typedef\s+$keyword/m) {
				warn("$filename:$line:Keyword $keyword not found in $name declaration.\n");
				print "DEC is $declaration\n";
				return 0;
			}
		}
    }

    return 1;
}

sub getStyle
{
    my $self = shift;
    my $name = shift;

   return %CSS_STYLES->{$name};
}

sub styleSheet
{
    my $self = shift;
    my $TOC = shift;
    my $css = "";
    my $stdstyles = 1;

# {
# print "style test\n";
# $self->setStyle("function", "background:#ffff80; color:#000080;");
# $self->setStyle("text", "background:#000000; color:#ffffff;");
# print "results:\n";
	# print "function: \"".$self->getStyle("function")."\"\n";
	# print "text: \"".$self->getStyle("text")."\"\n";
# }


    $css .= "<style type=\"text/css\">";
    $css .= "<!--";
    if ($HeaderDoc::styleImports) {
	$css .= "$HeaderDoc::styleImports ";
	if (!$TOC) { $stdstyles = 0; }
    }
    foreach my $stylename (sort strcasecmp keys %CSS_STYLES) {
	my $styletext = %CSS_STYLES->{$stylename};
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
    my $throws = "";
    my $abstract = $self->abstract();
    my $availability = $self->availability();
    my $updated = $self->updated();
    my $declaration = $self->declarationInHTML();
    my $result = "";
    my $localDebug = 0;
    # my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();
    my $filename = $self->filename();
    my $linenum = $self->linenum();
    my $list_attributes = $self->getAttributeLists($composite);
    my $short_attributes = $self->getAttributes(0);
    my $long_attributes = $self->getAttributes(1);
    my $class = ref($self) || $self;
    my $apio = $self->apiOwner();
    my $apioclass = ref($apio) || $apio;
    my $apiref = "";

# print "NAME: $name APIOCLASS: $apioclass APIUID: ".$self->apiuid()."\n";

    if ($self->can("result")) { $result = $self->result(); }
    if ($self->can("throws")) { $throws = $self->throws(); }


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

    $contentString .= $apiref;

    $contentString .= "<table border=\"0\"  cellpadding=\"2\" cellspacing=\"2\" width=\"300\">";
    $contentString .= "<tr>";
    $contentString .= "<td valign=\"top\" height=\"12\" colspan=\"5\">";
    my $urlname = sanitize($name);
    $contentString .= "<h3><a name=\"$urlname\">$name</a></h3>\n";
    $contentString .= "</td>";
    $contentString .= "</tr></table>";
    $contentString .= "<hr>";
    $contentString .= "<dl>";
    if (length($throws)) {
        $contentString .= "<dt><i>Throws:</i></dt>\n<dd>$throws</dd>\n";
    }
    # if (length($abstract)) {
        # $contentString .= "<dt><i>Abstract:</i></dt>\n<dd>$abstract</dd>\n";
    # }
    $contentString .= "</dl>";
    if (length($abstract)) {
        # $contentString .= "<dt><i>Abstract:</i></dt>\n<dd>$abstract</dd>\n";
        $contentString .= "<p>$abstract</p>\n";
    }

    if (length($short_attributes)) {
        $contentString .= $short_attributes;
    }
    if (length($list_attributes)) {
        $contentString .= $list_attributes;
    }
    $contentString .= "<blockquote><pre>$declaration</pre></blockquote>\n";

    my $arrayLength = @params;
    if (($arrayLength > 0) && (length($fieldHeading))) {
        my $paramContentString;
        foreach my $element (@params) {
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
                    $paramContentString .= "<dt>$apiref<code><i>$fName</i></code></dt><dd>$fDesc</dd>\n";
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
		# my $filename = $HeaderDoc::headerObject->name();
		my $classname = ref($self) || $self;
		$classname =~ s/^HeaderDoc:://o;
		if (!$HeaderDoc::ignore_apiuid_errors) {
			print "$filename:$linenum:warning: $classname ($name) field with name $fName has unknown type: $fType\n";
		}
	    }
        }
        if (length ($paramContentString)){
            $contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">$fieldHeading</font></h5>\n";       
            $contentString .= "<blockquote>\n";
            # $contentString .= "<table border=\"1\"  width=\"90%\">\n";
            # $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
            $contentString .= "<dl>\n";
            $contentString .= $paramContentString;
            # $contentString .= "</table>\n</blockquote>\n";
            $contentString .= "</dl>\n</blockquote>\n";
        }
    }
    if (@constants) {
        $contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">Constants</font></h5>\n";       
        $contentString .= "<blockquote>\n";
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
		# print "MARKING APIREF $uid used\n";
		$HeaderDoc::appleRefUsed{$uid} = 1;
                $contentString .= "<dt><a name=\"$uid\"><code>$cName</code></a></dt><dd>$cDesc</dd>\n";
	    } else {
                $contentString .= "<dt><code>$cName</code></dt><dd>$cDesc</dd>\n";
	    }
        }
        # $contentString .= "</table>\n</blockquote>\n";
        $contentString .= "</dl>\n</blockquote>\n";
    }

    if (scalar(@fields)) {
        $contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">$fieldHeading</font></h5>\n";
        $contentString .= "<blockquote>\n";
        # $contentString .= "<table border=\"1\"  width=\"90%\">\n";
        # $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
        $contentString .= "<dl>";

	# foreach my $element (@fields) {
		# print "ETYPE: $element->{TYPE}\n";
	# }

        foreach my $element (@fields) {
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
                my $filename = $HeaderDoc::headerObject->name();
		if (!$HeaderDoc::ignore_apiuid_errors) {
                	print "$filename:$linenum:warning: struct/typdef/union ($name) field with name $fName has unknown type: $fType\n";
			# $element->printObject();
		}
            }
        }

        # $contentString .= "</table>\n</blockquote>\n";
        $contentString .= "</dl>\n</blockquote>\n";
    }

    # if (length($desc)) {$contentString .= "<p>$desc</p>\n"; }
    $contentString .= "<dl>";
    if (length($result)) { 
        $contentString .= "<dt><i>$func_or_method result</i></dt><dd>$result</dd>\n";
    }
    if (length($desc)) {$contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">Discussion</font></h5><p>$desc</p>\n"; }

    # if (length($desc)) {$contentString .= "<p>$desc</p>\n"; }
    if (length($long_attributes)) {
        $contentString .= $long_attributes;
    }

    if (length($availability)) {
        $contentString .= "<dt><i>Availability</i></dt><dd>$availability</dd>\n";
    }
    if (length($updated)) {
        $contentString .= "<dt><i>Updated:</i></dt><dd>$updated</dd>\n";
    }
    $contentString .= "</dl>\n";
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
    }
    return @{ $self->{TAGGEDPARAMETERS} };
}

sub parsedParameters
{
    # Override this in subclasses where relevant.
    return ();
}

# Compare tagged parameters to parsed parameters (for validation)
sub taggedParsedCompare {
    my $self = shift;
    my @tagged = $self->taggedParameters();
    my @parsed = $self->parsedParameters();
    my $funcname = $self->name();
    my $filename = $self->filename();
    my $linenum = $self->linenum();
    my $tpcDebug = 0;
    my $struct = 0;
    my $strict = $HeaderDoc::force_parameter_tagging;
    my %taggednames = ();
    my %parsednames = ();

    if ($self->{TPCDONE}) { return; }
    $self->{TPCDONE} = 1;

    my @fields = ();
    if ($self->can("fields")) {
	$struct = 1;
	@fields = $self->fields();
    }

    my @constants = $self->constants();

    my $apiOwner = $self->isAPIOwner();

    foreach my $myfield (@fields) { 
	$taggednames{$myfield} = $myfield;
	my $nscomp = $myfield->name();
	$nscomp =~ s/\s*//sgo;
	$nscomp =~ s/^\**//sso;
	if (!length($nscomp)) {
		$nscomp = $myfield->type();
		$nscomp =~ s/\s*//sgo;
	}
	$taggednames{$nscomp}=$myfield;
    }
    if (!$apiOwner) {
	foreach my $myconstant (@constants) {
		my $nscomp = $myconstant->name();
		$nscomp =~ s/\s*//sgo;
		$nscomp =~ s/^\**//sso;
		if (!length($nscomp)) {
			$nscomp = $myconstant->type();
			$nscomp =~ s/\s*//sgo;
		}
		$taggednames{$nscomp}=$myconstant;
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
    }

    if ($HeaderDoc::ignore_apiuid_errors) {
	# This avoids warnings generated by the need to
	# run documentationBlock once prior to the actual parse
	# to generate API references.
	if ($tpcDebug) { print "ignore_apiuid_errors set.  Skipping tagged/parsed comparison.\n"; }
	# return;
    }

    if ($self->lang() ne "C") {
	if ($tpcDebug) { print "Language not C.  Skipping tagged/parsed comparison.\n"; }
	return;
    }


    if ($tpcDebug) {
	print "Tagged Parms:\n" if ($tpcDebug);
	foreach my $obj (@tagged) {
		bless($obj, "HeaderDoc::HeaderElement");
		bless($obj, $obj->class());
		print "TYPE: \"" .$obj->type . "\"\nNAME: \"" . $obj->name() ."\"\n";
	}
    }

	print "Parsed Parms:\n" if ($tpcDebug);
	foreach my $obj (@parsed) {
		bless($obj, "HeaderDoc::HeaderElement");
		bless($obj, $obj->class());
		print "TYPE:" .$obj->type . "\nNAME:" . $obj->name()."\n" if ($tpcDebug);
		my $nscomp = $obj->name();
		$nscomp =~ s/\s*//sgo;
		$nscomp =~ s/^\**//sso;
		if (!length($nscomp)) {
			$nscomp = $obj->type();
			$nscomp =~ s/\s*//sgo;
		}
		$parsednames{$nscomp}=$obj;
	}

    foreach my $taggedname (keys %taggednames) {
	    if (!$parsednames{$taggedname}) {
		my $tp = $taggednames{$taggedname};
		my $apio = $tp->apiOwner();
		print "APIO: $apio\n" if ($tpcDebug);
		my $tpname = $tp->type . " " . $tp->name();
		my $oldfud = $self->{PPFIXUPDONE};
		if (!$self->fixupParsedParameters($tp->name)) {
		    if (!$oldfud) {
			# Fixup may have changed things.
			my @newparsed = $self->parsedParameters();
			%parsednames = ();
			foreach my $obj (@newparsed) {
				bless($obj, "HeaderDoc::HeaderElement");
				bless($obj, $obj->class());
				print "TYPE:" .$obj->type . "\nNAME:" . $obj->name()."\n" if ($tpcDebug);
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
			warn("$filename:$linenum:Parameter $tpname does not appear in $funcname declaration.\n");
			print "---------------\n";
			print "Candidates are:\n";
			foreach my $ppiter (@parsed) {
				print "   \"".$ppiter->name()."\"\n";
			}
			print "---------------\n";
		    }
		}
	    }
    }
    if ($strict) { #  && !$struct
	foreach my $parsedname (keys %parsednames) {
		if (!$taggednames{$parsedname}) {
			my $pp = $parsednames{$parsedname};
			my $ppname = $pp->type . " " . $pp->name();
    			if (!$HeaderDoc::ignore_apiuid_errors) {
			    warn("$filename:$linenum:Parameter $ppname in $funcname declaration is not tagged.\n");
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
			print "Associating additional fields.\n" if ($addDebug);
			# print "ORIG: $origref\n";
			bless($origref, "HeaderDoc::HeaderElement");
			# print "ORIG: $origref\n";
			bless($origref, $origref->class());
			foreach my $origpp ($origref->parsedParameters()) {
				print "adding \"".$origpp->type()."\" \"".$origpp->name()."\" to $name\n" if ($addDebug);
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


# for subclass/superclass merging
sub parsedParamCompare {
    my $self = shift;
    my $compareObj = shift;
    my @comparelist = $compareObj->parsedParameters();
    my $name = $self->name();
    my $localDebug = 0;

    my @params = $self->parsedParameters();

    if (scalar(@params) != scalar(@comparelist)) { 
	print "parsedParamCompare: function $name arg count differs (".
		scalar(@params)." != ".  scalar(@comparelist) . ")\n" if ($localDebug);
	return 0;
    } # different number of args

    my $pos = 0;
    my $nparams = scalar(@params);
    while ($pos < $nparams) {
	my $compareparam = @comparelist[$pos];
	my $param = @params[$pos];
	if ($compareparam->type() ne $param->type()) {
	    print "parsedParamCompare: function $name no match for argument " .
		$param->name() . ".\n" if ($localDebug);
	    return 0;
	}
	$pos++;
    }

    print "parsedParamCompare: function $name matched.\n" if ($localDebug);
    return 1;
}

sub returntype {
    my $self = shift;
    my $localDebug = 0;

    if (@_) { 
        $self->{RETURNTYPE} = shift;
	print "$self: SET RETURN TYPE TO ".$self->{RETURNTYPE}."\n" if ($localDebug);
    }

    print "$self: RETURNING RETURN TYPE ".$self->{RETURNTYPE}."\n" if ($localDebug);
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

print "SA: ".scalar(@array)."\n" if ($localDebug);

$HeaderDoc::count++;

    foreach my $param (@array) {
	my $reducedname = $name;
	my $reducedpname = $param->name;
	$reducedname =~ s/\W//sgo;
	$reducedpname =~ s/\W//sgo;
	print "comparing \"$reducedname\" to \"$reducedpname\"\n" if ($localDebug);
	if ($reducedname eq $reducedpname) {
		print "PARAM WAS $param\n" if ($localDebug);
		return $param;
	}
    }

    print "NO SUCH PARAM\n" if ($localDebug);
    return 0;
}

sub XMLdocumentationBlock {
    my $self = shift;
    my $class = ref($self) || $self;
    my $compositePageString = "";
    my $filename = $self->filename();
    my $linenum = $self->linenum();

    my $name = $self->textToXML($self->name(), 1, "$filename:$linenum:Name");
    my $availability = $self->htmlToXML($self->availability(), 1, "$filename:$linenum:Availability");
    my $updated = $self->htmlToXML($self->updated(), 1, "$filename:$linenum:Updated");
    my $abstract = $self->htmlToXML($self->abstract(), 1, "$filename:$linenum:Abstract");
    my $discussion = $self->htmlToXML($self->discussion(), 0, "$filename:$linenum:Discussion");
    my $group = $self->htmlToXML($self->group(), 0, "$filename:$linenum:Group");
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

    if ($sublang eq "cpp") {
	$langstring = "cpp";
    } elsif ($sublang eq "C") {
	$langstring = "c";
    } elsif ($lang eq "C") {
	$langstring = "occ";
    } else {
	# java, javascript, et al
	$langstring = "$sublang";
    }

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
			$type = "clm";
		}
		if ($self->isTemplate()) {
			$type = "ftmplt";
		}
		if ($apioclass eq "HeaderDoc::CPPClass") {
			my $paramSignature = $self->getParamSignature();

			if (length($paramSignature)) {
				$paramSignature = "/$paramSignature"; # @@@SIGNATURE
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
		$uid = $self->apiuid("data");
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
        print "setting params\n" if ($localDebug);
        @params = $self->taggedParameters();
        if ($self->can("parsedParameters")) {
            $self->taggedParsedCompare();
        }
    } elsif ($self->can("fields")) {
        if ($self->can("parsedParameters")) {
            $self->taggedParsedCompare();
        }
    } else {
        print "type $class has no taggedParameters function\n" if ($localDebug);
    }

    my @parsedparams = ();
    if ($self->can("parsedParameters")) {
	@parsedparams = $self->parsedParameters();
    }

    my @origconstants = $self->constants();
    my @constants = ();
    my @fields = ();
    foreach my $copyfield (@origfields) {
        bless($copyfield, "HeaderDoc::HeaderElement");
	bless($copyfield, $copyfield->class()); # MinorAPIElement");
        # print "FIELD: ".$copyfield->name."\n";
        if ($copyfield->can("hidden")) {
            if (!$copyfield->hidden()) {
                push(@fields, $copyfield);
            }
        }
    }
    foreach my $copyconstant (@origconstants) {
        bless($copyconstant, "HeaderDoc::HeaderElement");
	bless($copyconstant, $copyconstant->class()); # MinorAPIElement");
        # print "CONST: ".$copyconstant->name."\n";
        if ($copyconstant->can("hidden")) {
            if (!$copyconstant->hidden()) {
                push(@constants, $copyconstant);
            }
        }
        # print "HIDDEN: ".$copyconstant->hidden()."\n";
    }

	# if (@fields) {
		# $contentString .= "<$fieldHeading>\n";
		# for my $field (@fields) {
			# my $name = $field->name();
			# my $desc = $field->discussion();
			# # print "field $name $desc\n";
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
		$declaration = $parseTree->xmlTree();
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
					# print "MARKING APIREF $uid used\n";
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

	if (@fields) {
		$compositePageString .= "<$fieldHeading>\n";
                foreach my $field (@fields) {
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

	if (@params) {
		$compositePageString .= "<$fieldHeading>\n";
                foreach my $field (@params) {
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

    if (scalar(@parsedparams) && ($class ne "HeaderDoc::PDefine" || !$self->isBlock())) {
	# PDefine blocks use parsed parameters to store all of the defines
	# in a define block, so this would be bad.

        my $paramContentString;
        foreach my $element (@parsedparams) {
            my $pName = $self->textToXML($element->name());
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
    if ($self->can('result')) { $self->result(); }
    my $attlists = "";
    if ($self->can('getAttributeLists')) { $self->getAttributeLists(0); }
    my $atts = "";
    if ($self->can('getAttributes')) { $self->getAttributes(); }

    if (length($atts)) {
        $compositePageString .= "<attributes>$atts</attributes>\n";
    }
    if ($class eq "HeaderDoc::Header") {
	my $includeref = $HeaderDoc::perHeaderIncludes{$filename};
	if ($includeref) {
		my @includes = @{$includeref};

		$compositePageString .= "<includes>\n";
		foreach my $include (@includes) {
			print "Included file: $include\n" if ($localDebug);

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

	# @@@ CLASSES!
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


sub apirefSetup
{
    my $self = shift;
    my $force = 0;

    if (@_) {
	$force = shift;
    }

    if (!$force && $self->{APIREFSETUPDONE}) {
	# print "SHORTCUT: $self\n";
	return ($self->{KEEPCONSTANTS}, $self->{KEEPFIELDS}, $self->{KEEPPARAMS},
		$self->{FIELDHEADING}, $self->{FUNCORMETHOD});
    }
	# print "REDO: $self\n";

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


    if ($self->can("fields")) { @origfields = $self->fields(); }
    if ($self->can("taggedParameters")){ 
	print "setting params\n" if ($localDebug);
	@params = $self->taggedParameters();
	if ($self->can("parsedParameters")) {
	    $self->taggedParsedCompare();
	}
    } elsif ($self->can("fields")) {
	if ($self->can("parsedParameters")) {
	    $self->taggedParsedCompare();
	}
    } else {
	print "type $class has no taggedParameters function\n" if ($localDebug);
    }

    # my @constants = @origconstants;
    # my @fields = @origfields;
    my @constants = ();
    my @fields = ();

    foreach my $copyfield (@origfields) {
        bless($copyfield, "HeaderDoc::HeaderElement");
	bless($copyfield, $copyfield->class()); # MinorAPIElement");
	print "FIELD: ".$copyfield->name."\n" if ($localDebug);
	if ($copyfield->can("hidden")) {
	    if (!$copyfield->hidden()) {
		push(@fields, $copyfield);
	    } else {
		print "HIDDEN\n" if ($localDebug);
	    }
	}
    }

    foreach my $copyconstant (@origconstants) {
        bless($copyconstant, "HeaderDoc::HeaderElement");
	bless($copyconstant, $copyconstant->class()); # MinorAPIElement");
	# print "CONST: ".$copyconstant->name."\n";
	if ($copyconstant->can("hidden")) {
	    if (!$copyconstant->hidden()) {
		push(@constants, $copyconstant);
	    }
	}
	# print "HIDDEN: ".$copyconstant->hidden()."\n";
    }
	# print "SELF WAS $self\n";

    SWITCH: {
	($class eq "HeaderDoc::Function") && do {
			if ($apioclass eq "HeaderDoc::Header") {
				$typename = "func";
			} else {
				$typename = "clm";
				if ($apio->can("getMethodType")) {
					$typename = $apio->getMethodType($self->declaration);
				}
			}
			print "Function type: $typename\n" if ($localDebug);
			if ($self->isTemplate()) {
				$typename = "ftmplt";
			}
			if ($apioclass eq "HeaderDoc::CPPClass") {
				my $paramSignature = $self->getParamSignature();

				print "paramSignature: $paramSignature\n" if ($localDebug);

				if (length($paramSignature)) {
					$paramSignature = "/$paramSignature"; # @@@SIGNATURE
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
			$fieldHeading = "Parameter Descriptions";
			$apiRefType = "";
			$func_or_method = "function";
		};
	($class eq "HeaderDoc::Constant") && do {
			if ($apioclass eq "HeaderDoc::Header") {
				$typename = "data";
			} else {
				$typename = "clconst";
			}
			$fieldHeading = "Field Descriptions";
			$apiRefType = "";
		};
	($class eq "HeaderDoc::Enum") && do {
			$typename = "tag";
			$fieldHeading = "Constants";
			# if ($self->masterEnum()) {
				$apiRefType = "econst";
			# } else {
				# $apiRefType = "";
			# }
		};
	($class eq "HeaderDoc::PDefine") && do {
			$typename = "macro";
			$fieldHeading = "Parameter Descriptions";
			$apiRefType = "";
		};
	($class eq "HeaderDoc::Method") && do {
			$typename = $self->getMethodType($declarationRaw);
			$fieldHeading = "Parameter Descriptions";
			$apiRefType = "";
			if ($apio->can("className")) {  # to get the class name from Category objects
				$className = $apio->className();
			} else {
				$className = $apio->name();
			}
			$func_or_method = "method";
		};
	($class eq "HeaderDoc::Struct") && do {
			$typename = "tag";
			$fieldHeading = "Field Descriptions";
			$apiRefType = "";
		};
	($class eq "HeaderDoc::Typedef") && do {
			$typename = "tdef";

        		if ($self->isFunctionPointer()) {
				$fieldHeading = "Parameter Descriptions";
				last SWITCH;
			}
        		if ($self->isEnumList()) {
				$fieldHeading = "Constants";
				last SWITCH;
			}
        		$fieldHeading = "Field Descriptions";

			$apiRefType = "";
			$func_or_method = "function";
		};
	($class eq "HeaderDoc::Var") && do {
			$typename = "data";
			$fieldHeading = "Field Descriptions";
			if ($self->can('isFunctionPointer')) {
			    if ($self->isFunctionPointer()) {
				$fieldHeading = "Parameter Descriptions";
			    }
			}
			$apiRefType = "";
		};
    }
    if (!length($apiref)) {
	$apiref = $self->apiref(0, $typename);
    }

    if (@constants) {
	foreach my $element (@constants) {
	    my $uid = $element->apiuid("econst");
	}
    }

    if (@params) {
      foreach my $element (@params) {
	if (length($apiRefType)) {
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

sub processComment
{
    # warn "processComment called on raw HeaderElement\n";
    return;
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

1;
