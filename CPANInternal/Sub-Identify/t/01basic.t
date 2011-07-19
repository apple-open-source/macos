#!perl

use Test::More tests => 30;

BEGIN { use_ok 'Sub::Identify', ':all' }

diag "running pure-Perl version of Sub::Identify" if $Sub::Identify::IsPurePerl;

sub buffy { }
sub vamp::spike { }
*slayer = *buffy;
*human::william = \&vamp::spike;

is( sub_name( \&sub_name ), 'sub_name' );
is( sub_name( \&buffy ), 'buffy' );
is( sub_name( \&vamp::spike ), 'spike' );
is( sub_name( \&slayer ), 'buffy' );
is( sub_name( \&human::william ), 'spike' );

is( stash_name( \&stash_name ), 'Sub::Identify' );
is( stash_name( \&buffy ), 'main' );
is( stash_name( \&vamp::spike ), 'vamp' );
is( stash_name( \&slayer ), 'main' );
is( stash_name( \&human::william ), 'vamp' );

is( sub_fullname( \&sub_fullname ), 'Sub::Identify::sub_fullname' );
is( sub_fullname( \&buffy ), 'main::buffy' );
is( sub_fullname( \&vamp::spike ), 'vamp::spike' );
is( sub_fullname( \&slayer ), 'main::buffy' );
is( sub_fullname( \&human::william ), 'vamp::spike' );

is( join('*', get_code_info( \&sub_fullname )), 'Sub::Identify*sub_fullname' );
is( join('*', get_code_info( \&buffy )), 'main*buffy' );
is( join('*', get_code_info( \&vamp::spike )), 'vamp*spike' );
is( join('*', get_code_info( \&slayer )), 'main*buffy' );
is( join('*', get_code_info( \&human::william )), 'vamp*spike' );

sub xander;
sub vamp::drusilla;
is( sub_name( \&xander ), 'xander', 'undefined subroutine' );
is( sub_fullname( \&xander ), 'main::xander', 'undefined subroutine' );
is( join('*', get_code_info( \&xander )), 'main*xander', 'undefined subroutine' );
is( sub_name( \&vamp::drusilla ), 'drusilla', 'undefined subroutine' );
is( sub_fullname( \&vamp::drusilla ), 'vamp::drusilla', 'undefined subroutine' );

is( sub_name( sub {} ), '__ANON__' );
my $anon = sub {};
is( stash_name( $anon ), 'main' );
is( sub_fullname( $anon ), 'main::__ANON__' );
is( join('*', get_code_info( sub { 'ah non' } )), 'main*__ANON__' );
