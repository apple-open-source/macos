#============================================================= -*-perl-*-
#
# t/vmethods/hash.t
#
# Testing hash virtual variable methods.
#
# Written by Andy Wardley <abw@cpan.org>
#
# Copyright (C) 1996-2006 Andy Wardley.  All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use warnings;
use lib qw( ./lib ../lib ../../lib );
use Template::Test;

# make sure we're using the Perl stash
$Template::Config::STASH = 'Template::Stash';

my $params = {
    hash      => { a => 'b', c => 'd' },
    uhash     => { tobe => '2b', nottobe => undef },
};

my $tt = Template->new() || die Template->error();
my $tc = $tt->context();

$tc->define_vmethod(hash  => dump => sub {
    my $hash = shift;
    return '{ ' 
        . join(', ', map { "$_ => '$hash->{$_}'" } sort keys %$hash)
        . ' }';
});

test_expect(\*DATA, undef, $params);

__DATA__

#------------------------------------------------------------------------
# hash virtual methods
#------------------------------------------------------------------------

-- test --
-- name hash keys --
[% hash.keys.sort.join(', ') %]
-- expect --
a, c

-- test --
-- name hash values --
[% hash.values.sort.join(', ') %]
-- expect --
b, d

-- test --
-- name hash each --
[% hash.each.sort.join(', ') %]
-- expect --
a, b, c, d

-- test --
-- name hash items --
[% hash.items.sort.join(', ') %]
-- expect --
a, b, c, d

-- test --
-- name hash size --
[% hash.size %]
-- expect --
2

-- test --
[% hash.defined('a') ? 'good' : 'bad' %]
[% hash.a.defined ? 'good' : 'bad' %]
[% hash.defined('x') ? 'bad' : 'good' %]
[% hash.x.defined ? 'bad' : 'good' %]
[% hash.defined ? 'good def' : 'bad def' %]
[% no_such_hash.defined ? 'bad no def' : 'good no def' %]
-- expect --
good
good
good
good
good def
good no def

-- test --
[% uhash.defined('tobe') ? 'good' : 'bad' %]
[% uhash.tobe.defined ? 'good' : 'bad' %]
[% uhash.exists('tobe') ? 'good' : 'bad' %]
[% uhash.defined('nottobe') ? 'bad' : 'good' %]
[% hash.nottobe.defined ? 'bad' : 'good' %]
[% uhash.exists('nottobe') ? 'good' : 'bad' %]
-- expect --
good
good
good
good
good
good

-- test --
-- name hash.pairs --
[% FOREACH pair IN hash.pairs -%]
* [% pair.key %] => [% pair.value %]
[% END %]
-- expect --
* a => b
* c => d

-- test --
-- name hash.list (old style) --
[% FOREACH pair IN hash.list -%]
* [% pair.key %] => [% pair.value %]
[% END %]
-- expect --
* a => b
* c => d



#------------------------------------------------------------------------
# user defined hash virtual methods
#------------------------------------------------------------------------

-- test --
-- name dump hash --
[% product = {
     id = 'abc-123',
     name = 'ABC Widget #123'
     price = 7.99
   };
   product.dump
%]
-- expect --
{ id => 'abc-123', name => 'ABC Widget #123', price => '7.99' }





