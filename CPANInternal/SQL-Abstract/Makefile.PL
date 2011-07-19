use inc::Module::Install 0.79;
use strict;
use warnings;

use 5.006001;

perl_version '5.006001';
name 'SQL-Abstract';
author 'Nathan Wiger <nate@wiger.org>';

all_from 'lib/SQL/Abstract.pm';

requires "List::Util"   => 0;
requires "Scalar::Util" => 0;

test_requires "Test::Builder"   => 0;
test_requires "Test::Deep"      => 0;
test_requires "Test::More"      => 0;
test_requires "Test::Exception" => 0;
test_requires "Test::Warn"      => 0;
test_requires "Clone"           => 0.31;

tests_recursive 't';

WriteAll();
