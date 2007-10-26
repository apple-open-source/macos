use inc::Module::Install;

name           'Class-Inspector';
all_from       'lib/Class/Inspector.pm';
requires       'File::Spec' => '0.80';
build_requires 'Test::More' => '0.47';

WriteAll;
