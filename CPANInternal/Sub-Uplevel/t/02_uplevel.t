#!/usr/bin/perl -Tw

use lib qw(t/lib);
use strict;
use Test::More tests => 23;

BEGIN { use_ok('Sub::Uplevel'); }
can_ok('Sub::Uplevel', 'uplevel');
can_ok(__PACKAGE__, 'uplevel');

#line 11
ok( !caller,                         "top-level caller() not screwed up" );

eval { die };
is( $@, "Died at $0 line 13.\n",           'die() not screwed up' );

sub foo {
    join " - ", caller;
}

sub bar {
    uplevel(1, \&foo);
}

#line 25
is( bar(), "main - $0 - 25",    'uplevel()' );


# Sure, but does it fool die?
sub try_die {
    die "You must die!  I alone am best!";
}

sub wrap_die {
    uplevel(1, \&try_die);
}

# line 38
eval { wrap_die() };
is( $@, "You must die!  I alone am best! at $0 line 30.\n", 'die() fooled' );


# how about warn?
sub try_warn {
    warn "HA!  You don't fool me!";
}

sub wrap_warn {
    uplevel(1, \&try_warn);
}


my $warning;
{ 
    local $SIG{__WARN__} = sub { $warning = join '', @_ };
#line 56
    wrap_warn();
}
is( $warning, "HA!  You don't fool me! at $0 line 44.\n", 'warn() fooled' );


# Carp?
use Carp;
sub try_croak {
# line 64
    croak("Now we can fool croak!");
}

sub wrap_croak {
# line 68
    uplevel(shift, \&try_croak);
}


# depending on perl version, we could get 'require 0' or 'eval {...}'
# in the stack. This test used to be 'require 0' for <= 5.006, but
# it broke on 5.005_05 test release, so we'll just take either
# line 72
eval { wrap_croak(1) };
my $croak_regex = quotemeta( <<"CARP" );
Now we can fool croak! at $0 line 64
	main::wrap_croak(1) called at $0 line 72
CARP
$croak_regex .= '\t(require 0|eval \{\.\.\.\})'
                . quotemeta( " called at $0 line 72" );
like( $@, "/$croak_regex/", 'croak() fooled');

# Try to wrap higher -- this may have been a problem that was exposed on
# Test Exception
# line 75
eval { wrap_croak(2) };
$croak_regex = quotemeta( <<"CARP" );
Now we can fool croak! at $0 line 64
CARP
like( $@, "/$croak_regex/", 'croak() fooled');

#line 79
ok( !caller,                                "caller() not screwed up" );

eval { die "Dying" };
is( $@, "Dying at $0 line 81.\n",           'die() not screwed up' );



# how about carp?
sub try_carp {
# line 88
    carp "HA!  Even carp is fooled!";
}

sub wrap_carp {
    uplevel(1, \&try_carp);
}


$warning = '';
{ 
    local $SIG{__WARN__} = sub { $warning = join '', @_ };
#line 98
    wrap_carp();
}
is( $warning, <<CARP, 'carp() fooled' );
HA!  Even carp is fooled! at $0 line 88
	main::wrap_carp() called at $0 line 98
CARP


use Foo;
can_ok( 'main', 'fooble' );

#line 114
sub core_caller_check {
    return CORE::caller(0);
}

sub caller_check {
    return caller(shift);
}

is_deeply(   [ ( caller_check(0), 0, 4 )[0 .. 3] ], 
             ['main', $0, 122, 'main::caller_check' ],
    'caller check' );

is( (() = caller_check(0)), (() = core_caller_check(0)) ,
    "caller() with args returns right number of values"
);

sub core_caller_no_args {
    return CORE::caller();
}

sub caller_no_args {
    return caller();
}

is( (() = caller_no_args()), (() = core_caller_no_args()),
    "caller() with no args returns right number of values"
);

sub deep_caller {
    return caller(1);
}

sub check_deep_caller {
    deep_caller();
}

#line 134
is_deeply([(check_deep_caller)[0..2]], ['main', $0, 134], 'shallow caller' );

sub deeper { deep_caller() }        # caller 0
sub still_deeper { deeper() }       # caller 1 -- should give this line, 137
sub ever_deeper  { still_deeper() } # caller 2

is_deeply([(ever_deeper)[0..2]], ['main', $0, 137], 'deep caller()' );

# This uplevel() should not effect deep_caller's caller(1).
sub yet_deeper { uplevel( 1, \&ever_deeper) }
is_deeply([(yet_deeper)[0..2]],  ['main', $0, 137],  'deep caller() + uplevel' );

sub target { caller }
sub yarrow { uplevel( 1, \&target ) }
sub hock   { uplevel( 1, \&yarrow ) }

is_deeply([(hock)], ['main', $0, 150],  'nested uplevel()s' );

# Deep caller inside uplevel
package Delegator; 
# line 159
sub delegate { main::caller_check(shift) }
    
package Wrapper;
use Sub::Uplevel;
sub wrap { uplevel( 1, \&Delegator::delegate, @_ ) }

package main;

is( (Wrapper::wrap(0))[0], 'Delegator', 
    'deep caller check of parent sees real calling package' 
);

is( (Wrapper::wrap(1))[0], 'main', 
    'deep caller check of grandparent sees package above uplevel' 
);

