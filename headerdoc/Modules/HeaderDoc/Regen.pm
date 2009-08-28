#! /usr/bin/perl
#
# Class name: Typedef
# Synopsis: Holds typedef info parsed by headerDoc
#
# Last Updated: $Date: 2009/03/30 19:38:52 $
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
package HeaderDoc::Regen;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash linesFromFile);
use HeaderDoc::HeaderElement;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::APIOwner;
use XML::Twig;

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::Regen::VERSION = '$Revision: 1.3 $';

sub stringFromFile
{
    my $name = shift;
    my @strings = &linesFromFile($name);
    my $ret = "";

    foreach my $string (@strings) { $ret .= $string; }
    return $ret;
}

sub regenerate
{
    my $self = shift;
    my $inpath = shift;
    my $outpath = shift;

    print STDERR "Would regenerate $inpath->$outpath if this did something.\n";

    my $xmlfile = $inpath;
    $xmlfile =~ s/^(.*)\..*?$/$1.xml/o;

    if (!-f $xmlfile) {
	print STDERR "No XML file found for $inpath.  Skipping.\n";
    # } else {
	# print STDERR "found $xmlfile\n";
    }
    my $xml_string = stringFromFile($xmlfile);
    # print STDERR "XS: $xml_string\n";
    my $twig = XML::Twig->new(keep_encoding => 1, keep_spaces => 1);
    $twig->parse($xml_string);

    # Twig::New();



}

1;

