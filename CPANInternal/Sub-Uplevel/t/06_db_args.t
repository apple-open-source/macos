use lib qw(t/lib);
use strict;
use Test::More tests => 3;

BEGIN { use_ok('Sub::Uplevel'); }

sub get_caller_args {
    package DB;
    my @x = caller(1);
    return @DB::args;
}

sub addition {
    my $x;
    $x += $_ for @_;
    return $x;
}

sub wrap_addition {
    my @args = get_caller_args();
    my $sum = uplevel 1, \&addition, @_;
    return ($sum, @args);
}

my ($sum, @args) = wrap_addition(1, 2, 3);

is($sum, 6, "wrapper returned value correct");
is_deeply( \@args, [1, 2, 3], "wrapper returned args correct" );



