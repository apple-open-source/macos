#! /usr/bin/perl -w
#
# Class name: PerfPoint
# Synopsis: Test Point Object for Performance Testing Engine
#
# Last Updated: $Date: 2009/03/30 19:38:51 $
#
# Copyright (c) 2005 Apple Computer, Inc.  All rights reserved.
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

package HeaderDoc::PerfPoint;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash unregisterUID registerUID quote html2xhtml sanitize unregister_force_uid_clear);
use File::Basename;
use strict;
use vars qw($VERSION @ISA);
use POSIX qw(strftime);
use Time::HiRes qw( usleep ualarm gettimeofday tv_interval );

use Carp;

$HeaderDoc::PerfPoint::VERSION = '$Revision: 1.3 $';

my $perfDebug = 1;

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
    $self->{BACKTRACE} = undef;
    ($self->{STARTSEC}, $self->{STARTUSEC}) = gettimeofday();
    $self->{FINISHSEC} = undef;
    $self->{FINISHUSEC} = undef;
    $self->{SECS} = undef;
    $self->{USECS} = undef;
}

sub finished {
    my $self = shift;
    my $localDebug = 0;

    ($self->{FINISHSEC}, $self->{FINISHUSEC}) = gettimeofday();
    $self->{SECS} = $self->{FINISHSEC} - $self->{STARTSEC};
    $self->{USECS} = $self->{FINISHUSEC} - $self->{STARTUSEC};

    if ($self->{USECS} < 0) {
	$self->{USECS} += 1000000;
	$self->{SECS} -= 1;
    }

    if ($localDebug) {
	print STDERR "BT: ".$self->{BACKTRACE}."\n";
	print STDERR "STARTSEC: ".$self->{STARTSEC}."\n";
	print STDERR "STARTUSEC: ".$self->{STARTUSEC}."\n";
	print STDERR "FINISHSEC: ".$self->{FINISHSEC}."\n";
	print STDERR "FINISHUSEC: ".$self->{FINISHUSEC}."\n";
	print STDERR "SECONDS: ".$self->{SECS}."\n";
	print STDERR "USECS: ".$self->{USECS}."\n";
    }

}


1;
