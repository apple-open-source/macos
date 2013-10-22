use Class::Std::Utils;
use Test::More 'no_plan';

my $anon_1 = anon_scalar();
my $anon_2 = anon_scalar();

is ref($anon_1), 'SCALAR'       => 'anon_scalar returns scalar ref';
ok !defined(${$anon_1})         => 'anon_scalar returns empty scalar';
ok $anon_1 != $anon_2           => 'anon_scalar returns distinct ref';
