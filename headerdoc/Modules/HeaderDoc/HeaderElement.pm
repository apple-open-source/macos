#! /usr/bin/perl -w
#
# Class name: HeaderElement
# Synopsis: Root class for Function, Typedef, Constant, etc. -- used by HeaderDoc.
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2003/08/28 17:59:27 $
#
# Copyright (c) 1999 Apple Computer, Inc.  All Rights Reserved.
# The contents of this file constitute Original Code as defined in and are
# subject to the Apple Public Source License Version 1.1 (the "License").
# You may not use this file except in compliance with the License.  Please
# obtain a copy of the License at http://www.apple.com/publicsource and
# read it before using this file.
#
# This Original Code and all software distributed under the License are
# distributed on an TAS ISU basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the License for
# the specific language governing rights and limitations under the
# License.
#
######################################################################

package HeaderDoc::HeaderElement;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';

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
    $self->{ABSTRACT} = undef;
    $self->{DISCUSSION} = undef;
    $self->{DECLARATION} = undef;
    $self->{DECLARATIONINHTML} = undef;
    $self->{OUTPUTFORMAT} = undef;
    $self->{FILENAME} = undef;
    $self->{NAME} = undef;
    $self->{GROUP} = undef;
    $self->{THROWS} = undef;
    $self->{XMLTHROWS} = undef;
    $self->{UPDATED} = undef;
    $self->{LINKAGESTATE} = undef;
    $self->{ACCESSCONTROL} = undef;
    $self->{SINGLEATTRIBUTES} = ();
    $self->{LONGATTRIBUTES} = ();
    $self->{ATTRIBUTELISTS} = undef;
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

    if (@_) {
        my $name = shift;

	$name =~ s/\n$//sg;
	$name =~ s/\s$//sg;

        $self->{NAME} = $name;
    } else {
    	my $n = $self->{NAME};
		return $n;
	}
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
    }

    %attlist->{$name}=$attribute;

    if ($long) {
        $self->{LONGATTRIBUTES} = \%attlist;
    } else {
        $self->{SINGLEATTRIBUTES} = \%attlist;
    }

    my $temp = $self->getAttributes();
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

    my $retval = "";
    if ($long != 1) {
        if ($self->{SINGLEATTRIBUTES}) {
	    %attlist = %{$self->{SINGLEATTRIBUTES}};
        }

        foreach my $key (sort keys %attlist) {
	    my $value = %attlist->{$key};
	    my $newatt = $value;
	    if ($key eq "Superclass") {
		my $ref = $self->make_classref($value);
		$newatt = "<!-- a logicalPath=\"$ref\" -->$value<!-- /a -->";
	    } else {
		print "KEY: $key\n" if ($localDebug);
	    }
	    $retval .= "<b>$key:</b> $newatt<br>\n";
        }
    }

    if ($long != 0) {
        if ($self->{LONGATTRIBUTES}) {
	    %attlist = %{$self->{LONGATTRIBUTES}};
        }

        foreach my $key (sort keys %attlist) {
	    my $value = %attlist->{$key};
	    $retval .= "<b>$key:</b>\n\n<p>$value<p>\n";
        }
    }

    return $retval;
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
    foreach my $key (sort keys %attlists) {
	if ($key eq $name) { return 1; }
    }
    return 0;
}

sub getAttributeLists
{
    my $self = shift;
    my $localDebug = 0;

    my %attlists = ();
    if ($self->{ATTRIBUTELISTS}) {
	%attlists = %{$self->{ATTRIBUTELISTS}};
    }

    # print "list\n";
    my $retval = "";
    foreach my $key (sort keys %attlists) {
	$retval .= "<b>$key:</b><dl>\n";
	print "key $key\n" if ($localDebug);
	my @list = @{%attlists->{$key}};
	foreach my $item (@list) {
	    print "item: $item\n" if ($localDebug);
	    my ($name, $disc) = &getAPINameAndDisc($item);
	    $retval .= "<dt>$name</dt><dd>$disc</dd>";
	}
	$retval .= "</dl>\n";
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
    if (%attlists->{$name}) {
	@list = @{%attlists->{$name}};
    }
    push(@list, $attribute);

    %attlists->{$name}=\@list;
    $self->{ATTRIBUTELISTS} = \%attlists;
    # print "AL = $self->{ATTRIBUTELISTS}\n";

    # print $self->getAttributeLists()."\n";
}

sub appleref {
    my $self = shift;
    my $type = shift;

    my $name = $self->name;
    my $localDebug = 0;

    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();    
    my $className; 

    # Not yet implemented
    # my $lang = HeaderDoc::APIOwner->lang();

    $name =~ s/\n//smg;

    my $lang = "c";
    my $class = ref($HeaderDoc::APIOwner) || $HeaderDoc::APIOwner;

    if ($class =~ /^HeaderDoc::CPPClass$/) {
        $lang = "cpp";
    } elsif ($class =~ /^HeaderDoc::ObjC/) {
        $lang = "occ";
    }

    print "LANG: $lang\n" if ($localDebug);
    my $classHeaderObject = HeaderDoc::APIOwner->headerObject();
    if (!$classHeaderObject) {
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
        $className = $HeaderDoc::currentClass->name() . "/";
    }
    my $uid = "//$apiUIDPrefix/$lang/$type/$className$name";
    HeaderDoc::APIOwner->register_uid($uid);
    my $ret .= "<a name=\"$uid\"></a>\n";
    return $ret;
}

sub throws {
    my $self = shift;

    if (@_) {
	my $new = shift;
	$new =~ s/\n//smg;
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
    my $string = $self->{XMLTHROWS};

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
    return $self->{ABSTRACT};
}

sub XMLabstract {
    my $self = shift;

    if (@_) {
        $self->{ABSTRACT} = shift;
    }
    return $self->{ABSTRACT};
}


sub discussion {
    my $self = shift;

    if (@_) {
	if ($self->{DISCUSSION}) {
		my $oldname = $self->name();
		$self->name($oldname." ".$self->{DISCUSSION});
	}

        my $discussion = "";
        $discussion = shift;
        $discussion =~ s/\n\n/<br>\n/g;
        $self->{DISCUSSION} = $self->linkfix($discussion);
    }
    return $self->{DISCUSSION};
}

sub XMLdiscussion {
    my $self = shift;

    if (@_) {
        my $discussion = "";
        $discussion = shift;
        # $discussion =~ s/\n\n/<br>\n/g;
        $self->{DISCUSSION} = $discussion;
    }
    return $self->{DISCUSSION};
}


sub declaration {
    my $self = shift;
    my $dec = $self->declarationInHTML();
    # remove simple markup that we add to declarationInHTML
    $dec =~s/<br>/\n/gi;
    $dec =~s/<(\/)?tt>//gi;
    $dec =~s/<(\/)?b>//gi;
    $dec =~s/<(\/)?pre>//gi;
    $dec =~s/\&nbsp;//gi;
    $dec =~s/\&lt;/</gi;
    $dec =~s/\&gt;/>/gi;
    $self->{DECLARATION} = $dec;  # don't really have to have this ivar
    return $dec;
}

sub declarationInHTML {
    my $self = shift;

    if (@_) {
        $self->{DECLARATIONINHTML} = shift;
    }
    return $self->{DECLARATIONINHTML};
}

sub availability {
    my $self = shift;

    if (@_) {
        $self->{AVAILABILITY} = shift;
    }
    return $self->{AVAILABILITY};
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
	if (!($updated =~ /\d\d\d\d-\d\d-\d\d/ )) {
	    if (!($updated =~ /\d\d-\d\d-\d\d\d\d/ )) {
		if (!($updated =~ /\d\d-\d\d-\d\d/ )) {
		    my $filename = $HeaderDoc::headerObject->filename();
		    print "$filename:0:Bogus date format: $updated.\n";
		    print "Valid formats are MM-DD-YYYY, MM-DD-YY, and YYYY-MM-DD\n";
		    return $self->{UPDATED};
		} else {
		    $month =~ s/(\d\d)-\d\d-\d\d/$1/smg;
		    $day =~ s/\d\d-(\d\d)-\d\d/$1/smg;
		    $year =~ s/\d\d-\d\d-(\d\d)/$1/smg;

                    my $century;
                    $century = `date +%C`;
                    $century *= 100; 
                    $year += $century;
                    # $year += 2000;
                    print "YEAR: $year" if ($localdebug);
		}
	    } else {
		print "03-25-2003 case.\n" if ($localdebug);
		    $month =~ s/(\d\d)-\d\d-\d\d\d\d/$1/smg;
		    $day =~ s/\d\d-(\d\d)-\d\d\d\d/$1/smg;
		    $year =~ s/\d\d-\d\d-(\d\d\d\d)/$1/smg;
	    }
	} else {
		    $year =~ s/(\d\d\d\d)-\d\d-\d\d/$1/smg;
		    $month =~ s/\d\d\d\d-(\d\d)-\d\d/$1/smg;
		    $day =~ s/\d\d\d\d-\d\d-(\d\d)/$1/smg;
	}
	$month =~ s/\n//smg;
	$day =~ s/\n//smg;
	$year =~ s/\n//smg;
	$month =~ s/\s*//smg;
	$day =~ s/\s*//smg;
	$year =~ s/\s*//smg;

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
		my $filename = $HeaderDoc::headerObject->filename();
		print "$filename:0:Invalid date (year = $year, month = $month, day = $day).\n";
		print "$filename:0:Valid formats are MM-DD-YYYY, MM-DD-YY, and YYYY-MM-DD\n";
		return $self->{UPDATED};
	} else {
		$self->{UPDATED} = "$year-$month-$day";
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
		if ($part =~ /^\s*A\s*/) {
			$part =~ /^(.*?>)/;
			my $linkpart = $1;
			my $rest = $part;
			$rest =~ s/^$1//;

			print "Found link.\nlinkpart: $linkpart\nrest: $rest\n" if ($localDebug);

			if ($linkpart =~ /target\=\".*\"/i) {
			    print "link ok\n" if ($localDebug);
			    $outString .= "<$part";
			} else {
			    print "needs fix.\n" if ($localDebug);
			    $linkpart =~ s/\>$//;
			    $outString .= "<$linkpart target=\"_top\">$rest";
			}
		} else {
			$outString .= "<$part";
		}
	}
    }

    return $outString;
}

1;
