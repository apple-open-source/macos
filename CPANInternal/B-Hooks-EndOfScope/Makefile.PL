use strict;
use warnings;
use inc::Module::Install;

name 'B-Hooks-EndOfScope';
all_from 'lib/B/Hooks/EndOfScope.pm';

requires 'Variable::Magic' => '0.34';
requires 'Sub::Exporter';
test_requires 'Test::More';

extra_tests;

makemaker_args(dist => { PREOP => 'pod2text lib/B/Hooks/EndOfScope.pm > README' });

WriteAll;
