#============================================================= -*-perl-*-
#
# t/config.t
#
# Test the Template::Config module.
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
use lib  qw( ./lib ../lib );
use vars qw( $DEBUG );
use Template::Test;
use Template::Config;

ntests(44);
$DEBUG = 0;
$Template::Config::DEBUG = 0;

my $factory = 'Template::Config';

#------------------------------------------------------------------------
# parser
#------------------------------------------------------------------------

print STDERR "Testing parser...\n" if $DEBUG;
my $parser;

$parser = $factory->parser(PRE_CHOMP => 1, INTERPOLATE => 1)
    || print STDERR $factory->error(), "\n";

ok( $parser );
ok( $parser->{ PRE_CHOMP }   == 1);
ok( $parser->{ INTERPOLATE } == 1);

$parser = $factory->parser({ POST_CHOMP => 1 })
    || print STDERR $factory->error(), "\n";

ok( $parser );
ok( $parser->{ POST_CHOMP }   == 1);


#------------------------------------------------------------------------
# provider
#------------------------------------------------------------------------

print STDERR "Testing provider...\n" if $DEBUG;
my $provider;

$provider = $factory->provider(INCLUDE_PATH => 'here:there', 
				  PARSER       => $parser)
    || print STDERR $factory->error(), "\n";

ok( $provider );
ok( join('...', @{ $provider->{ INCLUDE_PATH } }) eq 'here...there' );
ok( $provider->{ PARSER }->{ POST_CHOMP } == 1);

$provider = $factory->provider({ 
    INCLUDE_PATH => 'cat:mat', 
    ANYCASE      => 1,
    INTERPOLATE  => 1
}) || print STDERR $factory->error(), "\n";

ok( $provider );
ok( join('...', @{ $provider->{ INCLUDE_PATH } }) eq 'cat...mat' );

# force provider to instantiate a parser and check it uses the correct
# parameters.
my $text = 'The cat sat on the mat';
ok( $provider->fetch(\$text) );
ok( $provider->{ PARSER }->{ ANYCASE     } == 1);
ok( $provider->{ PARSER }->{ INTERPOLATE } == 1);


#------------------------------------------------------------------------
# plugins
#------------------------------------------------------------------------

print STDERR "Testing plugins...\n" if $DEBUG;
my $plugins;

$plugins = $factory->plugins(PLUGIN_BASE => 'MyPlugins')
    || print STDERR $factory->error(), "\n";

ok( $plugins );
ok( join('+', @{$plugins->{ PLUGIN_BASE }}) eq 'MyPlugins+Template::Plugin' );

$plugins = $factory->plugins({
    LOAD_PERL   => 1,
    PLUGIN_BASE => 'NewPlugins',
}) || print STDERR $factory->error(), "\n";

ok( $plugins );
ok( $plugins->{ LOAD_PERL } == 1 );
ok( join('+', @{$plugins->{ PLUGIN_BASE }}) eq 'NewPlugins+Template::Plugin' );


#------------------------------------------------------------------------
# filters
#------------------------------------------------------------------------

print STDERR "Testing filters...\n" if $DEBUG;
my $filters;

$filters = $factory->filters(TOLERANT => 1)
    || print STDERR $factory->error(), "\n";

ok( $filters );
ok( $filters->{ TOLERANT } == 1);

$filters = $factory->filters({ TOLERANT => 1 }) 
    || print STDERR $factory->error(), "\n";

ok( $filters );
ok( $filters->{ TOLERANT } == 1);



#------------------------------------------------------------------------
# stash
#------------------------------------------------------------------------

print STDERR "Testing stash...\n" if $DEBUG;
my $stash;

$stash = $factory->stash(foo => 10, bar => 20)
    || print STDERR $factory->error(), "\n";

ok( $stash );
ok( $stash->get('foo') == 10);
ok( $stash->get('bar') == 20);

$stash = $factory->stash({
    foo => 30,
    bar => sub { 'forty' },
}) || print STDERR $factory->error(), "\n";

ok( $stash );
ok( $stash->get('foo') == 30);
ok( $stash->get('bar') eq 'forty' );


#------------------------------------------------------------------------
# context
#------------------------------------------------------------------------

print STDERR "Testing context...\n" if $DEBUG;
my $context;

$context = $factory->context()
    || print STDERR $factory->error(), "\n";

ok( $context );

$context = $factory->context(INCLUDE_PATH => 'anywhere')
    || print STDERR $factory->error(), "\n";

ok( $context );
ok( $context->{ LOAD_TEMPLATES }->[0]->{ INCLUDE_PATH }->[0] eq 'anywhere' );

$context = $factory->context({
    LOAD_TEMPLATES => [ $provider ],
    LOAD_PLUGINS   => [ $plugins ],
    LOAD_FILTERS   => [ $filters ],
    STASH          => $stash,
}) || print STDERR $factory->error(), "\n";

ok( $context );
ok( $context->stash->get('foo') == 30 );
ok( $context->{ LOAD_TEMPLATES }->[0]->{ PARSER    }->{ INTERPOLATE } == 1);
ok( $context->{ LOAD_PLUGINS   }->[0]->{ LOAD_PERL } == 1 );
ok( $context->{ LOAD_FILTERS   }->[0]->{ TOLERANT  } == 1 );

#------------------------------------------------------------------------
# service
#------------------------------------------------------------------------

print STDERR "Testing service...\n" if $DEBUG;
my $service;

$service = $factory->service(INCLUDE_PATH => 'amsterdam')
    || print STDERR $factory->error(), "\n";

ok( $service );
ok( $service->{ CONTEXT }->{ LOAD_TEMPLATES }->[0]->{ INCLUDE_PATH }->[0]
    eq 'amsterdam' );


#------------------------------------------------------------------------
# iterator
#------------------------------------------------------------------------


print STDERR "Testing iterator...\n" if $DEBUG;

my ($iterator, $value, $error);

$iterator = $factory->iterator([qw(foo bar baz)])
    || print STDERR $factory->error(), "\n";

ok( $iterator );

($value, $error) = $iterator->get_first();
ok( $value eq 'foo' );

($value, $error) = $iterator->get_next();
ok( $value eq 'bar' );
 
($value, $error) = $iterator->get_next();
ok( $value eq 'baz' );


#------------------------------------------------------------------------
# instdir
#------------------------------------------------------------------------

my $idir = Template::Config->instdir();

if ($Template::Config::INSTDIR) {
    ok( $idir eq $Template::Config::INSTDIR );
}
else {
    ok(  ! defined($idir) 
	&& $Template::Config::ERROR eq 'no installation directory' );
}

my $tdir = Template::Config->instdir('templates');

if ($Template::Config::INSTDIR) {
    ok( $tdir eq "$Template::Config::INSTDIR/templates" );
}
else {
    ok(  ! defined($tdir) 
	&& $Template::Config::ERROR eq 'no installation directory' );
}
