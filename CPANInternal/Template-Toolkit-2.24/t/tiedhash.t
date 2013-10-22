#============================================================= -*-perl-*-
#
# t/tiedhash.t
#
# Template script testing variable via a tied hash.
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 1996-2001 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1998-2001 Canon Research Centre Europe Ltd.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use warnings;
use lib qw( blib/lib blib/arch lib ../blib/lib ../blib/arch ../lib );
use Template::Test;
use Template::Stash;
our $DEBUG = grep(/-d/, @ARGV);

eval {
    require Template::Stash::XS;
};
if ($@) {
    warn $@;
    skip_all('cannot load Template::Stash::XS');
}

#print "stash: $Template::Config::STASH\n";
#$Template::Config::STASH = 'Template::Stash::XS';

our $STORE_PREFIX = '';
our $FETCH_PREFIX = '';

#------------------------------------------------------------------------
package My::Tied::Hash;
use Tie::Hash;
use base 'Tie::StdHash';

sub FETCH {
    my ($hash, $key) = @_;
    print STDERR "FETCH($key)\n" if $main::DEBUG;
    my $val = $hash->{ $key };
    return $val ? (ref $val ? $val : "$main::FETCH_PREFIX$val") : undef;
}

sub STORE { 
    my ($hash, $key, $val) = @_; 
    print STDERR "STORE($key, $val)\n" if $main::DEBUG;
    $hash->{ $key } = ref $val ? $val : "$main::STORE_PREFIX$val";
}

#------------------------------------------------------------------------
package My::Tied::List;
use Tie::Array;
use base 'Tie::StdArray';

sub FETCH {
    my ($list, $n) = @_;
    print STDERR "FETCH from list [$n]\n" if $main::DEBUG;
    my $val = $list->[ $n ];
    return $val ? (ref $val ? $val : "$main::FETCH_PREFIX$val") : undef;
}

sub STORE {
    my ($list, $n, $val) = @_;
    print STDERR "STORE to list [$n => $val]\n" if $main::DEBUG;
    $list->[$n] = ref $val ? $val : "$main::STORE_PREFIX$val";
}

#------------------------------------------------------------------------
package main;


# setup a tied hash and a tied list
my @list;
tie @list, 'My::Tied::List';
push(@list, 10, 20, 30);

my %hash = (a => 'alpha');
tie %hash, 'My::Tied::Hash';
$hash{ a } = 'alpha';
$hash{ b } = 'bravo';
$hash{ zero } = 0;
$hash{ one } = 1;

# now turn on the prefixes so we can track items going in 
# and out of the tied hash/list
$FETCH_PREFIX = 'FETCH:';
$STORE_PREFIX = 'STORE:';

my $data = {
    hash => \%hash,
    list => \@list,
};
my $stash_perl = Template::Stash->new($data);
my $stash_xs   = Template::Stash::XS->new($data);
my $tt = [
    perl => Template->new( STASH => $stash_perl ),
    xs   => Template->new( STASH => $stash_xs ),
];
test_expect(\*DATA, $tt);

__DATA__

#------------------------------------------------------------------------
# first try with the Perl stash
#------------------------------------------------------------------------

# hash tests

-- test --
[% hash.a %]
-- expect --
FETCH:alpha

-- test --
[% hash.b %]
-- expect --
FETCH:bravo

-- test --
ready
set:[% hash.c = 'cosmos' %]
go:[% hash.c %]
-- expect --
ready
set:
go:FETCH:STORE:cosmos

-- test --
[% hash.foo.bar = 'one' -%]
[% hash.foo.bar %]
-- expect --
one

# list tests

-- test --
[% list.0 %]
-- expect --
FETCH:10

-- test --
[% list.first %]-[% list.last %]
-- expect --
FETCH:10-FETCH:30

-- test --
[% list.push(40); list.last %]
-- expect --
FETCH:40

-- test --
[% list.4 = 50; list.4 %]
-- expect --
FETCH:STORE:50


#------------------------------------------------------------------------
# now try using the XS stash
#------------------------------------------------------------------------

# hash tests

-- test --
-- use xs --
[% hash.a %]
-- expect --
FETCH:alpha

-- test --
[% hash.b %]
-- expect --
FETCH:bravo

-- test --
[% hash.c = 'crazy'; hash.c %]
-- expect --
FETCH:STORE:crazy

-- test --
[% DEFAULT hash.c = 'more crazy'; hash.c %]
-- expect --
FETCH:STORE:crazy

-- test --
[% hash.wiz = 'woz' -%]
[% hash.wiz %]
-- expect --
FETCH:STORE:woz

-- test --
[% DEFAULT hash.zero = 'nothing';
   hash.zero
%]
-- expect --
FETCH:STORE:nothing

-- test --
before: [% hash.one %]
after: [% DEFAULT hash.one = 'solitude';
   hash.one
%]
-- expect --
before: FETCH:1
after: FETCH:1

-- test --
[% hash.foo = 10; hash.foo = 20; hash.foo %]
-- expect --
FETCH:STORE:20

# this test should create an intermediate hash
-- test --
[% DEFAULT hash.person = { };
   hash.person.name  = 'Arthur Dent';
   hash.person.email = 'dent@tt2.org'; 
-%]
name:  [% hash.person.name %]
email: [% hash.person.email %]
-- expect --
name:  Arthur Dent
email: dent@tt2.org


# list tests

-- test --
[% list.0 %]
-- expect --
FETCH:10

-- test --
[% list.first %]-[% list.last %]
-- expect --
FETCH:10-FETCH:STORE:50

-- test --
[% list.push(60); list.last %]
-- expect --
FETCH:60

-- test --
[% list.5 = 70; list.5 %]
-- expect --
FETCH:STORE:70

-- test --
[% DEFAULT list.5 = 80; list.5 %]
-- expect --
FETCH:STORE:70

-- test --
[% list.10 = 100; list.10 %]
-- expect --
FETCH:STORE:100

-- test --
[% stuff = [ ];
   stuff.0 = 'some stuff';
   stuff.0
-%]
-- expect --
some stuff

