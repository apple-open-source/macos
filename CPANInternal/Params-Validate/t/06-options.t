#!/usr/bin/perl -w

use strict;

BEGIN
{
    $ENV{PERL_NO_VALIDATION} = 0;
    require Params::Validate;
    Params::Validate->import(':all');
}

use Test;
plan test => $] == 5.006 ? 3 : 7;

Params::Validate::validation_options( stack_skip => 2 );

sub foo
{
    my %p = validate(@_, { bar => 1 });
}

sub bar { foo(@_) }

sub baz { bar(@_) }

eval { baz() };

ok( $@ );
ok( $@ =~ /mandatory.*missing.*call to main::bar/i );

Params::Validate::validation_options( stack_skip => 3 );

eval { baz() };

ok( $@ );

unless ( $] == 5.006 )
{
    ok( $@ =~ /mandatory.*missing.*call to main::baz/i );

    Params::Validate::validation_options
        ( on_fail => sub { die bless { hash => 'ref' }, 'Dead' } );

    eval { baz() };

    ok( $@ );
    ok( $@->{hash} eq 'ref' );
    ok( UNIVERSAL::isa( $@, 'Dead' ) );
}

