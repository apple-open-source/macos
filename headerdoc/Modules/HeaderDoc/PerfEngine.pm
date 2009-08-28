#! /usr/bin/perl -w
#
# Class name: PerfEngine
# Synopsis: Performance Testing Engine
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

package HeaderDoc::PerfEngine;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash unregisterUID registerUID quote html2xhtml sanitize unregister_force_uid_clear);
use HeaderDoc::PerfPoint;
use File::Basename;
use strict;
use vars qw($VERSION @ISA);
use POSIX qw(strftime);

use Carp;

$HeaderDoc::PerfEngine::VERSION = '$Revision: 1.3 $';

my $perfDebug = 0;

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
    my @temp1 = ();
    my @temp2 = ();
    $self->{COMPLETE} = \@temp1;
    $self->{PENDING} = \@temp2;
}

sub checkpoint {
    my $self = shift;
    my $entering = shift;
    # my $bt = Devel::StackTrace->new;
    # my $btstring = $bt->as_string;
    my $bt = Carp::longmess("");
    $bt =~ s/^.*?\n//s;
    $bt =~ s/\n/ /sg;

    if ($perfDebug) { print STDERR "CP: $bt\n"; }

    if ($entering) {
	$self->addCheckpoint($bt);
    } else {
	$self->matchCheckpoint($bt);
    }
}

sub addCheckpoint
{
    my $self = shift;
    my $bt = shift;

    if ($perfDebug) {
	print STDERR "Adding checkpoint.  Backtrace: $bt\n";
    }
    my $checkpoint = HeaderDoc::PerfPoint->new( backtrace => $bt);
    push(@{$self->{PENDING}}, $checkpoint);
}

sub matchCheckpoint
{
    my $self = shift;
    my $bt = shift;
    my @keep = ();

    my $localDebug = 0;

    if ($perfDebug) {
	print STDERR "Routine returned.  Backtrace: $bt\n";
    }

    foreach my $point (@{$self->{PENDING}}) {
	if ($point->{BACKTRACE} eq $bt) {
		if ($localDebug) {
			print STDERR "MATCHED\n";
		}
		$point->finished();
		push(@{$self->{COMPLETE}}, $point);
	} else {
		push(@keep, $point);
	}
    }
    $self->{PENDING} = \@keep;
}

sub printstats
{
    my $self = shift;

    my %pointsByBacktrace = ();

    foreach my $point (@{$self->{COMPLETE}}) {
	# print STDERR "POINT: ".$point->{BACKTRACE}."\n";
	my $arrayref = $pointsByBacktrace{$point->{BACKTRACE}};
	if (!$arrayref) {
		# print STDERR "NEW\n";
		my @temparray = ();
		$arrayref = \@temparray;
	# } else {
		# print STDERR "OLD\n";
	}
	my @array = @{$arrayref};
	push(@array, $point);
	$pointsByBacktrace{$point->{BACKTRACE}} = \@array;
    }

    print STDERR "Completed routines:\n";
    my $first = 1;
    foreach my $bt (keys %pointsByBacktrace) {
	my $arrayref = $pointsByBacktrace{$bt};
	my @array = @{$arrayref};
	my $maxusec = 0;
	my $ttlsec = 0;
	my $ttlusec = 0;
	my $count = 0;

	if ($first) {
		$first = 0;
	} else { 
		printSeparator();
	}

	print STDERR "$bt\n";
	foreach my $point (@array) {
		my $usec = $point->{SECS} * 1000000;
		$usec += $point->{USECS};
		if ($usec > $maxusec) {
			$maxusec = $usec;
		}
		$ttlsec += $point->{SECS};
		$ttlusec += $point->{USECS};
		if ($ttlusec > 1000000) {
			$ttlusec -= 1000000;
			$ttlsec += 1;
		}
		$count++;
	}
	print STDERR "COUNT: $count\n";
	print STDERR "MAX: $maxusec usec\n";
	print STDERR "TTL: $ttlsec seconds, $ttlusec usec\n";
    }


    print STDERR "\n\nIncomplete routines:\n";

    $first = 1;
    foreach my $point (@{$self->{PENDING}}) {
	if ($first) {
		$first = 0;
	} else { 
		printSeparator();
	}
	print STDERR $point->{BACKTRACE}."\n";
    }

}

sub printSeparator
{
    print STDERR "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n";
}

1;
