#!./perl

# $Id: dclone.t,v 0.18 2006/10/08 03:37:29 ray Exp $
#
# Id: dclone.t,v 0.6.1.1 2000/03/02 22:21:05 ram Exp 
#
#  Copyright (c) 1995-1998, Raphael Manfredi
#  
#  You may redistribute only under the terms of the Artistic License,
#  as specified in the README file that comes with the distribution.
#
# $Log: dclone.t,v $
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
# Revision 0.10.2.1  2001/07/28 21:47:49  ray
# commented out print statements.
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
# Revision 0.7  2000/08/01 00:31:42  ray
# release 0.07
#
# Revision 0.6  2000/07/28 21:37:20  ray
# "borrowed" code from Storable
#
# Revision 0.6.1.1  2000/03/02 22:21:05  ram
# patch9: added test case for "undef" bug in hashes
#
# Revision 0.6  1998/06/04  16:08:25  ram
# Baseline for first beta release.
#

require 't/dump.pl';

# use Storable qw(dclone);
use Clone qw(clone);

print "1..9\n";

$a = 'toto';
$b = \$a;
$c = bless {}, CLASS;
$c->{attribute} = 'attrval';
%a = ('key', 'value', 1, 0, $a, $b, 'cvar', \$c);
@a = ('first', undef, 3, -4, -3.14159, 456, 4.5,
	$b, \$a, $a, $c, \$c, \%a);

print "not " unless defined ($aref = clone(\@a));
print "ok 1\n";

$dumped = &dump(\@a);
print "ok 2\n";

$got = &dump($aref);
print "ok 3\n";

# print $got;
# print $dumped;
# print $_, "\n" for (@a);
# print $_, "\n" foreach (@$aref);
print "not " unless $got eq $dumped; 
print "ok 4\n";

package FOO; @ISA = qw(Clone);

sub make {
	my $self = bless {};
	$self->{key} = \%main::a;
	return $self;
};

package main;

$foo = FOO->make;
print "not " unless defined($r = $foo->clone);
print "ok 5\n";

# print &dump($foo);
# print &dump($r);
print "not " unless &dump($foo) eq &dump($r);
print "ok 6\n";

# Ensure refs to "undef" values are properly shared during cloning
my $hash;
push @{$$hash{''}}, \$$hash{a};
print "not " unless $$hash{''}[0] == \$$hash{a};
print "ok 7\n";

my $cloned = clone(clone($hash));
print "not " unless $$cloned{''}[0] == \$$cloned{a};
print "ok 8\n";

$$cloned{a} = "blah";
print "not " unless $$cloned{''}[0] == \$$cloned{a};
print "ok 9\n";

