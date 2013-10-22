#============================================================= -*-perl-*-
# t/object.t
#
# Template script testing code bindings to objects.
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
use Template::Exception;
use Template::Test;
$^W = 1;

$Template::Test::DEBUG = 0;


#------------------------------------------------------------------------
# definition of test object class
#------------------------------------------------------------------------

package T1;
sub new {
    my $class = shift;
    bless { @_ }, $class;
}

sub die {
    die "barfed up\n";
}

package TestObject;

use vars qw( $AUTOLOAD );

sub new {
    my ($class, $params) = @_;
    $params ||= {};

    bless {
        PARAMS  => $params,
        DAYS    => [ qw( Monday Tuesday Wednesday Thursday 
                         Friday Saturday Sunday ) ],
        DAY     => 0,
        'public'   => 314,
        '.private' => 425,
        '_hidden'  => 537,
    }, $class;
}

sub yesterday {
    my $self = shift;
    return "Love was such an easy game to play...";
}

sub today {
    my $self = shift;
    return "Live for today and die for tomorrow.";
}

sub tomorrow {
    my ($self, $dayno) = @_;
    $dayno = $self->{ DAY }++
        unless defined $dayno;
    $dayno %= 7;
    return $self->{ DAYS }->[$dayno];
}

sub belief {
    my $self = shift;
    my $b = join(' and ', @_);
    $b = '<nothing>' unless length $b;
    return "Oh I believe in $b.";
}

sub concat {
    my $self = shift;
    local $" = ', ';
    $self->{ PARAMS }->{ args } = "ARGS: @_";
}

sub _private {
    my $self = shift;
    die "illegal call to private method _private()\n";
}


sub AUTOLOAD {
    my ($self, @params) = @_;
    my $name = $AUTOLOAD;
    $name =~ s/.*:://;
    return if $name eq 'DESTROY';

    my $value = $self->{ PARAMS }->{ $name };
    if (ref($value) eq 'CODE') {
        return &$value(@params);
    }
    elsif (@params) {
        return $self->{ PARAMS }->{ $name } = shift @params;
    }
    else {
        return $value;
    }
}

#------------------------------------------------------------------------
# another object for testing auto-stringification
#------------------------------------------------------------------------

package Stringy;

use overload '""' => 'stringify', fallback => 1;

sub new {
    my ($class, $text) = @_;
    bless \$text, $class;
}

sub stringify {
    my $self = shift;
    return "stringified '$$self'";
}

#------------------------------------------------------------------------
# Another object for tracking down a bug with DBIx::Class where TT is 
# causing the numification operator to be called.  Matt S Trout suggests
# we've got a truth test somewhere that should be a defined but that 
# doesn't appear to be the case...
# http://rt.cpan.org/Ticket/Display.html?id=23763
#------------------------------------------------------------------------

package Numbersome;

use overload 
    '""' => 'stringify',
    '0+' => 'numify', 
    fallback => 1;

sub new {
    my ($class, $text) = @_;
    bless \$text, $class;
}

sub numify {
    my $self = shift;
    return "FAIL: numified $$self";
}

sub stringify {
    my $self = shift;
    return "PASS: stringified $$self";
}

sub things {
    return [qw( foo bar baz )];
}

package GetNumbersome;

sub new {
    my ($class, $text) = @_;
    bless { }, $class;
}

sub num {
    Numbersome->new("from GetNumbersome");
}

#------------------------------------------------------------------------
# main 
#------------------------------------------------------------------------

package main;

sub new {
    my ($class, $text) = @_;
    bless \$text, $class;
}

my $objconf = { 
    'a' => 'alpha',
    'b' => 'bravo',
    'w' => 'whisky',
};

my $replace = {
    thing  => TestObject->new($objconf),
    string => Stringy->new('Test String'),
    t1     => T1->new(a => 10),
    num    => Numbersome->new("Numbersome"),
    getnum => GetNumbersome->new,
    %{ callsign() },
};

test_expect(\*DATA, { INTERPOLATE => 1 }, $replace);



#------------------------------------------------------------------------
# test input
#------------------------------------------------------------------------

__DATA__
# test method calling via autoload to get parameters
[% thing.a %] [% thing.a %]
[% thing.b %]
$thing.w
-- expect --
alpha alpha
bravo
whisky

# ditto to set parameters
-- test --
[% thing.c = thing.b -%]
[% thing.c %]
-- expect --
bravo

-- test --
[% thing.concat = thing.b -%]
[% thing.args %]
-- expect --
ARGS: bravo

-- test --
[% thing.concat(d) = thing.b -%]
[% thing.args %]
-- expect --
ARGS: delta, bravo

-- test --
[% thing.yesterday %]
[% thing.today %]
[% thing.belief(thing.a thing.b thing.w) %]
-- expect --
Love was such an easy game to play...
Live for today and die for tomorrow.
Oh I believe in alpha and bravo and whisky.

-- test --
Yesterday, $thing.yesterday
$thing.today
${thing.belief('yesterday')}
-- expect --
Yesterday, Love was such an easy game to play...
Live for today and die for tomorrow.
Oh I believe in yesterday.

-- test --
[% thing.belief('fish' 'chips') %]
[% thing.belief %]
-- expect --
Oh I believe in fish and chips.
Oh I believe in <nothing>.

-- test --
${thing.belief('fish' 'chips')}
$thing.belief
-- expect --
Oh I believe in fish and chips.
Oh I believe in <nothing>.

-- test --
[% thing.tomorrow %]
$thing.tomorrow
-- expect --
Monday
Tuesday

-- test --
[% FOREACH [ 1 2 3 4 5 ] %]$thing.tomorrow [% END %].
-- expect --
Wednesday Thursday Friday Saturday Sunday .


#------------------------------------------------------------------------
# test private methods do not get exposed
#------------------------------------------------------------------------
-- test --
before[% thing._private %] mid [% thing._hidden %]after
-- expect --
before mid after

-- test --
[% key = '_private' -%]
[[% thing.$key %]]
-- expect --
[]

-- test --
[% key = '.private' -%]
[[% thing.$key = 'foo' %]]
[[% thing.$key %]]
-- expect --
[]
[]

#------------------------------------------------------------------------
# test auto-stringification
#------------------------------------------------------------------------

-- test --
[% string.stringify %]
-- expect --
stringified 'Test String'

-- test --
[% string %]
-- expect --
stringified 'Test String'

-- test --
[% "-> $string <-" %]
-- expect --
-> stringified 'Test String' <-

-- test --
[% "$string" %]
-- expect --
stringified 'Test String'

-- test --
foo $string bar
-- expect --
foo stringified 'Test String' bar

-- test --
.[% t1.dead %].
-- expect --
..

-- test --
.[% TRY; t1.die; CATCH; error; END %].
-- expect --
.undef error - barfed up
.


#-----------------------------------------------------------------------
# try and pin down the numification bug
#-----------------------------------------------------------------------

-- test --
[% FOREACH item IN num.things -%]
* [% item %]
[% END -%]
-- expect --
* foo
* bar
* baz

-- test --
[% num %]
-- expect --
PASS: stringified Numbersome

-- test --
[% getnum.num %]
-- expect --
PASS: stringified from GetNumbersome

