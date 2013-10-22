#============================================================= -*-perl-*-
#
# t/base.t
#
# Test the Template::Base.pm module.
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

ntests(24);


#------------------------------------------------------------------------
# a dummy module, derived from Template::Base and destined for failure
#------------------------------------------------------------------------
package Template::Fail;
use base qw( Template::Base );
use vars qw( $ERROR );
use Template::Base;

sub _init {
    my $self = shift;
    return $self->error('expected failure');
}


#------------------------------------------------------------------------
# another dummy module, expecting a 'name' parameter
#------------------------------------------------------------------------
package Template::Named;
use base qw( Template::Base );
use vars qw( $ERROR );
use Template::Base;

sub _init {
    my ($self, $params) = @_;
    $self->{ NAME } = $params->{ name } 
	|| return $self->error("No name!");
    return $self;
}

sub name {
    $_[0]->{ NAME };
}


#------------------------------------------------------------------------
# module to test version
#------------------------------------------------------------------------
package Template::Version;
use Template::Base;
use base qw( Template::Base );
use vars qw( $ERROR $VERSION );
$VERSION = 3.14;


#------------------------------------------------------------------------
# main package, run some tests
#------------------------------------------------------------------------
package main;

my ($mod, $pkg);

# instantiate a base class object and test error reporting/returning
$mod = Template::Base->new();
ok( $mod );
$mod->error('barf');
ok( $mod->error() eq 'barf' );

# Template::Fail should never work, but we check it reports errors OK
ok( ! Template::Fail->new() );
ok(   Template::Fail->error eq 'expected failure');
ok(  $Template::Fail::ERROR eq 'expected failure');

# Template::Named should only work with a 'name'parameters
$mod = Template::Named->new();
ok( ! $mod );
ok( $Template::Named::ERROR eq 'No name!'  );
ok( Template::Named->error() eq 'No name!' );

# give it what it wants...
$mod = Template::Named->new({ name => 'foo' });
ok( $mod );
ok( $mod->name() eq 'foo' );
ok( ! $mod->error() );

# ... in 2 different flavours
$mod = Template::Named->new(name => 'foo');
ok( $mod );
ok( $mod->name() eq 'foo' );
ok( ! $mod->error() );

# test the use of error() for setting and retrieving object errors
ok( ! defined $mod->error('more errors') );
ok( $mod->error() eq 'more errors' );

# check package error is still set, then clear.
ok( Template::Named->error() eq 'No name!' );
$Template::Named::ERROR = '';

# test via $pkg indirection
$pkg = 'Template::Named';
$mod = $pkg->new();
ok( ! $mod );
ok( $pkg->error eq 'No name!' );

$mod = $pkg->new({ name => 'bar' });
ok( $mod && $mod->name eq 'bar' );
ok( ! $mod->error );

#------------------------------------------------------------------------
# test module_version() method
#------------------------------------------------------------------------

$pkg = 'Template::Version';
is( $pkg->module_version(), 3.14, 'package version' );

my $obj = $pkg->new() || die $pkg->error();
ok( $obj, 'created a version object' );
is( $obj->module_version(), 3.14, 'object version' );


