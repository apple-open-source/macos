#!./perl

# $Id: tied.pl,v 0.18 2006/10/08 03:37:29 ray Exp $
#
#  Copyright (c) 1995-1998, Raphael Manfredi
#  
#  You may redistribute only under the terms of the Artistic License,
#  as specified in the README file that comes with the distribution.
#
# $Log: tied.pl,v $
# Revision 0.18  2006/10/08 03:37:29  ray
# Commented out VERSION causes errors with DynaLoader in perl 5.6.1 (and
# probably all earlier versions. It was removed.
#
# Revision 0.14  2003/09/07 22:02:36  ray
# VERSION 0.15
#
# Revision 0.13.2.1  2003/09/07 21:51:13  ray
# added support for unicode hash keys. This is only really a bug in 5.8.0 and
# the test in t/03scalar supports this.
#
# Revision 0.13  2002/06/12 06:41:55  ray
# VERSION 0.13
#
# Revision 0.11  2001/07/29 19:31:05  ray
# VERSION 0.11
#
# Revision 0.10  2001/04/29 21:56:10  ray
# VERSION 0.10
#
# Revision 0.9  2001/03/05 00:11:49  ray
# version 0.9
#
# Revision 0.9  2000/08/21 23:06:34  ray
# added support for code refs
#
# Revision 0.8  2000/08/11 17:08:36  ray
# Release 0.08.
#
# Revision 0.7  2000/08/01 00:43:48  ray
# release 0.07.
#
# Revision 0.6.2.1  2000/08/01 00:42:53  ray
# modified to use as a require statement.
#
# Revision 0.6  2000/08/01 01:38:38  ray
# "borrowed" code from Storable
#
# Revision 0.6  1998/06/04 16:08:40  ram
# Baseline for first beta release.
#

require 't/dump.pl';

package TIED_HASH;

sub TIEHASH {
	my $self = bless {}, shift;
	return $self;
}

sub FETCH {
	my $self = shift;
	my ($key) = @_;
	$main::hash_fetch++;
	return $self->{$key};
}

sub STORE {
	my $self = shift;
	my ($key, $value) = @_;
	$self->{$key} = $value;
}

sub FIRSTKEY {
	my $self = shift;
	scalar keys %{$self};
	return each %{$self};
}

sub NEXTKEY {
	my $self = shift;
	return each %{$self};
}

package TIED_ARRAY;

sub TIEARRAY {
	my $self = bless [], shift;
	return $self;
}

sub FETCH {
	my $self = shift;
	my ($idx) = @_;
	$main::array_fetch++;
	return $self->[$idx];
}

sub STORE {
	my $self = shift;
	my ($idx, $value) = @_;
	$self->[$idx] = $value;
}

sub FETCHSIZE {
	my $self = shift;
	return @{$self};
}

package TIED_SCALAR;

sub TIESCALAR {
	my $scalar;
	my $self = bless \$scalar, shift;
	return $self;
}

sub FETCH {
	my $self = shift;
	$main::scalar_fetch++;
	return $$self;
}

sub STORE {
	my $self = shift;
	my ($value) = @_;
	$$self = $value;
}

1;
