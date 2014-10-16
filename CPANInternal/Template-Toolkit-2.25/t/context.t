#!/usr/bin/perl -w                                         # -*- perl -*-
#============================================================= -*-perl-*-
#
# t/context.t
#
# Test the Template::Context.pm module.
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 1996-2000 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1998-2000 Canon Research Centre Europe Ltd.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use lib qw( ./lib ../lib );
use Template::Test;
use Template::Constants qw( :debug );

my $DEBUG = grep(/^--?d(debug)?$/, @ARGV);
#$Template::Test::DEBUG = 1;

ntests(54);

# script may be being run in distribution root or 't' directory
my $dir   = -d 't' ? 't/test' : 'test';
my $tt = Template->new({
    INCLUDE_PATH => "$dir/src:$dir/lib",	
    TRIM         => 1,
    POST_CHOMP   => 1,
    DEBUG        => $DEBUG ? DEBUG_CONTEXT : 0,
});

my $ttperl = Template->new({
    INCLUDE_PATH => "$dir/src:$dir/lib",
    TRIM         => 1,
    EVAL_PERL    => 1,
    POST_CHOMP   => 1,
    DEBUG        => $DEBUG ? DEBUG_CONTEXT : 0,
});

#------------------------------------------------------------------------
# misc
#------------------------------------------------------------------------

# test we created a context object and check internal values
my $context = $tt->service->context();
ok( $context );
ok( $context eq $tt->context() );
ok( $context->trim() );
ok( ! $context->eval_perl() );

ok( $context = $ttperl->service->context() );
ok( $context->trim() );
ok( $context->eval_perl() );

#------------------------------------------------------------------------
# template()
#------------------------------------------------------------------------

banner('testing template()');

# test we can fetch a template via template()
my $template = $context->template('header');
ok( $template );
ok( UNIVERSAL::isa($template, 'Template::Document') );

# test that non-existance of a template is reported
eval { $template = $context->template('no_such_template') };
ok( $@ );
ok( "$@" eq 'file error - no_such_template: not found' );

# check that template() returns CODE and Template::Document refs intact
my $code = sub { return "this is a hard-coded template" };
$template = $context->template($code);
ok( $template eq $code );

my $doc = "this is a document";
$doc = bless \$doc, 'Template::Document';
$template = $context->template($doc);
ok( $template eq $doc );
ok( $$doc = 'this is a document' );

# check the use of visit() and leave() to add temporary BLOCK lookup 
# tables to the context's search space
my $blocks1 = {
    some_block_1 => 'hello',
};
my $blocks2 = {
    some_block_2 => 'world',
};

eval { $context->template('some_block_1') };
ok( $@ );
$context->visit('no doc', $blocks1);
ok( $context->template('some_block_1') eq 'hello' );
eval { $context->template('some_block_2') };
ok( $@ );
$context->visit('no doc', $blocks2);
ok(   $context->template('some_block_1') eq 'hello' );
ok(   $context->template('some_block_2') eq 'world' );
$context->leave();
ok(   $context->template('some_block_1') eq 'hello' );
eval { $context->template('some_block_2') };
ok( $@ );
$context->leave();
eval { $context->template('some_block_1') };
ok( $@ );
eval { $context->template('some_block_2') };
ok( $@ );


# test that reset() clears all blocks
$context->visit('no doc', $blocks1);
ok(   $context->template('some_block_1') eq 'hello' );
eval { $context->template('some_block_2') };
ok( $@ );
$context->visit('no doc', $blocks2);
ok(   $context->template('some_block_1') eq 'hello' );
ok(   $context->template('some_block_2') eq 'world' );
$context->reset();
eval { $context->template('some_block_1') };
ok( $@ );
eval { $context->template('some_block_2') };
ok( $@ );

#------------------------------------------------------------------------
# plugin()
#------------------------------------------------------------------------

banner('testing plugin()');

my $plugin = $context->plugin('Table', [ [1,2,3,4], { rows => 2 } ]);
ok( $plugin );
ok( ref $plugin eq 'Template::Plugin::Table' );

my $row = $plugin->row(0);
ok( $row && ref $row eq 'ARRAY' );
ok( $row->[0] == 1 );
ok( $row->[1] == 3 );

eval {
  $plugin = $context->plugin('no_such_plugin');
};
ok( "$@" eq 'plugin error - no_such_plugin: plugin not found' );

#------------------------------------------------------------------------
# filter()
#------------------------------------------------------------------------

banner('testing filter()');

my $filter = $context->filter('html');
ok( $filter );
ok( ref $filter eq 'CODE' );
ok( &$filter('<input/>') eq '&lt;input/&gt;' );

$filter = $context->filter('replace', [ 'foo', 'bar' ], 'repsave');
ok( $filter );
ok( ref $filter eq 'CODE' );
ok( &$filter('this is foo, so it is') eq 'this is bar, so it is' );

# check filter got cached
$filter = $context->filter('repsave');
ok( $filter );
ok( ref $filter eq 'CODE' );
match( &$filter('this is foo, so it is'), 'this is bar, so it is' );


#------------------------------------------------------------------------
# include() and process()
#------------------------------------------------------------------------

banner('testing include()');

$context = $tt->context();
ok( $context );

my $stash = $context->stash();
ok( $stash );

$stash->set('a', 'alpha');
ok( $stash->get('a') eq 'alpha' );

my $text = $context->include('baz');
ok( $text eq 'This is the baz file, a: alpha' );

$text = $context->include('baz', { a => 'bravo' });
ok( $text eq 'This is the baz file, a: bravo' );

# check stash hasn't been altered
ok( $stash->get('a') eq 'alpha' );

$text = $context->process('baz');
ok( $text eq 'This is the baz file, a: alpha' );

# check stash *has* been altered
ok( $stash->get('a') eq 'charlie' );

$text = $context->process('baz', { a => 'bravo' });
ok( $text eq 'This is the baz file, a: bravo' );
ok( $stash->get('a') eq 'charlie' );

