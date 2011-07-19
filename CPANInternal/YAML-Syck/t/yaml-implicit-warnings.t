use strict;
use warnings;
use YAML::Syck;
use Test::More 'no_plan';

my $warnings;
BEGIN { $SIG{__WARN__} = sub { $warnings .= "@_" } }

$YAML::Syck::ImplicitUnicode = 1;
ok !$warnings, "no warnings";
