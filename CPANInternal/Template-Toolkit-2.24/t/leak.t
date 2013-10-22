#============================================================= -*-perl-*-
#
# t/leak.t
#
# Attempts to detect memory leaks... but fails.  That's a Good Thing
# if it means there are no memory leaks (in this particular aspect)
# or a Bad Thing if it there are, but we're not smart enough to detect
# them. :-)
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
use lib qw( ./lib ../lib ../blib/arch );
use Template::Test;
$^W = 1;

$Template::Test::PRESERVE = 1;
#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY = 1;

#------------------------------------------------------------------------
package Holler;
use vars qw( $TRACE $PREFIX );
$TRACE = '';
$PREFIX = 'Holler:';

sub new {
    my $class = shift;
    my $id = shift || '<anon>';
    my $self  = bless \$id, $class;
    $self->trace("created");
    return $self;
}

sub trace {
    my $self = shift;
    $TRACE  .= "$$self @_\n";
}

sub clear {
    $TRACE = '';
    return '';
}

sub DESTROY {
    my $self = shift;
    $self->trace("destroyed");
}

#------------------------------------------------------------------------
package Plugin::Holler;
use base qw( Template::Plugin );

sub new {
    my ($class, $context, @args) = @_;
    bless {
	context => $context,
	holler  => Holler->new(@args),
    }, $class;
}

sub trace {
    my $self = shift;
    $self->{ context }->process('trace');
}

#------------------------------------------------------------------------
package main;

my $ttcfg = {
    INCLUDE_PATH   => -d 't' ? 't/test/src' : 'test/src',
    PLUGIN_FACTORY => { holler => 'Plugin::Holler' },
    EVAL_PERL      => 1,
    BLOCKS         => {
        trace => "TRACE ==[% trace %]==",
    },
};

my $ttvars = {
    holler => sub { Holler->new(@_) },
    trace  => sub { $Holler::TRACE },
    clear  => \&Holler::clear,
    v56 => ( $^V && eval '$^V ge v5.6.0' && eval '$^V le v5.7.0' ),
};

test_expect(\*DATA, $ttcfg, $ttvars);

__DATA__

-- test --
[% a = holler('first'); trace %]
-- expect --
first created

-- test --
[% trace %]
-- expect --
first created
first destroyed

-- test --
[% clear; b = [ ]; b.0 = holler('list'); trace %]
-- expect --
list created

-- test --
[% trace %]
-- expect --
list created
list destroyed

-- stop --


-- test --
[% BLOCK shout; a = holler('second'); END -%]
[% clear; PROCESS shout; trace %]
-- expect --
second created

-- test --
[% BLOCK shout; a = holler('third'); END -%]
[% clear; INCLUDE shout; trace %]
-- expect --
third created
third destroyed

-- test --
[% MACRO shout BLOCK; a = holler('fourth'); END -%]
[% clear; shout; trace %]
-- expect --
fourth created
fourth destroyed

-- test --
[% clear; USE holler('holler plugin'); trace %]
-- expect --
holler plugin created

-- test --
[% BLOCK shout; USE holler('process plugin'); END -%]
[% clear; PROCESS shout; holler.trace %]
-- expect --
TRACE ==process plugin created
==

-- test --
[% BLOCK shout; USE holler('include plugin'); END -%]
[% clear; INCLUDE shout; trace %]
-- expect --
include plugin created
include plugin destroyed

-- test --
[% MACRO shout BLOCK; USE holler('macro plugin'); END -%]
[% clear; shout; trace %]
-- expect --
macro plugin created
macro plugin destroyed

-- test --
[%  MACRO shout BLOCK; 
	USE holler('macro plugin'); 
	holler.trace;
    END 
-%]
[% clear; shout; trace %]
-- expect --
TRACE ==macro plugin created
==macro plugin created
macro plugin destroyed

-- test --
[% clear; PROCESS leak1; trace %]
-- expect --
<leak1>
</leak1>
Hello created

-- test --
[% clear; INCLUDE leak1; trace %]
-- expect --
<leak1>
</leak1>
Hello created
Hello destroyed

-- test --
[% clear; PROCESS leak2; trace %]
-- expect --
<leak2>
</leak2>
Goodbye created

-- test --
[% clear; INCLUDE leak2; trace %]
-- expect --
<leak2>
</leak2>
Goodbye created
Goodbye destroyed

-- test --
[%  MACRO leak BLOCK; 
	PROCESS leak1 + leak2;
        USE holler('macro plugin'); 
    END 
-%]
[% IF v56;
	clear; leak; trace;
    ELSE;
       "Perl version < 5.6.0 or > 5.7.0, skipping this test";
    END
-%]
-- expect --
-- process --
[% IF v56 -%]
<leak1>
</leak1>
<leak2>
</leak2>
Hello created
Goodbye created
macro plugin created
Hello destroyed
Goodbye destroyed
macro plugin destroyed
[% ELSE -%]
Perl version < 5.6.0 or > 5.7.0, skipping this test
[% END -%]

-- test --
[% PERL %]
    Holler->clear();
    my $h = Holler->new('perl');
    $stash->set( h => $h );
[% END -%]
[% trace %]
-- expect --
perl created

-- test --
[% BLOCK x; PERL %]
    Holler->clear();
    my $h = Holler->new('perl');
    $stash->set( h => $h );
[% END; END -%]
[% x; trace %]
-- expect --
perl created
perl destroyed

-- test --
[% MACRO y PERL %]
    Holler->clear();
    my $h = Holler->new('perl macro');
    $stash->set( h => $h );
[% END -%]
[% y; trace %]
-- expect --
perl macro created
perl macro destroyed




