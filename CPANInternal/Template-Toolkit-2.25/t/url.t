#============================================================= -*-perl-*-
#
# t/url.t
#
# Template script testing URL plugin.
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 2000 Andy Wardley.  All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use lib qw( ../lib );
use Template qw( :status );
use Template::Test;
use Template::Plugin::URL;
$^W = 1;

$Template::Test::DEBUG = 0;

my $urls = {
    product => {
	map {
	    $_->{ name }, Template::Plugin::URL->new(undef, # no context 
						     $_->{ url  },
						     $_->{ args });
	} 
	(
	 {
	     name => 'view',
	     url  => '/product',
	 },
	 {
	     name => 'add',
	     url  => '/product',
	     args => { action => 'add' },
	 },
	 {
	     name => 'edit',
	     url  => '/product',
	     args => { action => 'edit', style => 'editor' },
	 },
	 ),
    },
};

my $vars = {
    url => $urls,
    sorted => \&sort_params,
    no_escape => sub { $Template::Plugin::URL::JOINT = '&' },
};

test_expect(\*DATA, { INTERPOLATE => 1 }, $vars);

# url params are constructed in a non-deterministic order.  we obviously
# can't test against this so we use this devious hack to reorder a
# query so that its parameters are in alphabetical order.
# ------------------------------------------------------------------------
# later note: in adding support for parameters with multiple values, the
# sort_params() hacked below got broken so as a temporary solution, I
# changed teh URL plugin to sort all params by key when generating the 
# URL

sub sort_params {
    my $query  = shift;
    my ($base, $args) = split(/\?/, $query);
    my (@args, @keys, %argtab);

    print STDERR "sort_parms(\"$query\")\n" if $Template::Test::DEBUG;

    @args = split('&amp;', $args);
    @keys = map { (split('=', $_))[0] } @args;
    @argtab{ @keys } = @args;
    @keys = sort keys %argtab;
    @args = map { $argtab{ $_ } } @keys;
    $args = join('&amp;', @args);
    $query = join('?', length $base ? ($base, $args) : $args);

    print STDERR "returning [$query]\n" if $Template::Test::DEBUG;

    return $query;
}
 

#------------------------------------------------------------------------
# test input
#------------------------------------------------------------------------

__DATA__
-- test --
[% USE url -%]
loaded
[% url %]
[% url('foo') %]
[% url(foo='bar') %]
[% url('bar', wiz='woz') %]

-- expect --
loaded

foo
foo=bar
bar?wiz=woz

-- test --
[% USE url('here') -%]
[% url %]
[% url('there') %]
[% url(any='where') %]
[% url('every', which='way') %]
[% sorted( url('every', which='way', you='can') ) %]

-- expect --
here
there
here?any=where
every?which=way
every?which=way&amp;you=can

-- test --
[% USE url('there', name='fred') -%]
[% url %]
[% url(name='tom') %]
[% sorted( url(age=24) ) %]
[% sorted( url(age=42, name='frank') ) %]

-- expect --
there?name=fred
there?name=tom
there?age=24&amp;name=fred
there?age=42&amp;name=frank

-- test --
[% USE url('/cgi-bin/woz.pl') -%]
[% url(name="Elrich von Benjy d'Weiro") %]

-- expect --
/cgi-bin/woz.pl?name=Elrich%20von%20Benjy%20d%27Weiro

-- test --
[% USE url '/script' { one => 1, two => [ 2, 4 ], three => [ 3, 6, 9] } -%]
[% url  %]

-- expect --
/script?one=1&amp;three=3&amp;three=6&amp;three=9&amp;two=2&amp;two=4

-- test --
[% url.product.view %]
[% url.product.view(style='compact') %]
-- expect --
/product
/product?style=compact

-- test --
[% url.product.add %]
[% url.product.add(style='compact') %]
-- expect --
/product?action=add
/product?action=add&amp;style=compact

-- test --
[% url.product.edit %]
[% url.product.edit(style='compact') %]
-- expect --
/product?action=edit&amp;style=editor
/product?action=edit&amp;style=compact

-- test --
[% CALL no_escape -%]
[% url.product.edit %]
[% url.product.edit(style='compact') %]
-- expect --
/product?action=edit&style=editor
/product?action=edit&style=compact
