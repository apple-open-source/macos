use inc::Module::Install;
name 'Class-Trigger';
all_from 'lib/Class/Trigger.pm';

tests 't/*.t t/*/*.t t/*/*/*.t t/*/*/*/*.t';
test_requires 'Test::More' => 0.32;
test_requires 'IO::Scalar';
test_requires 'IO::WrapTie';
use_test_base;
auto_include;
WriteAll;
