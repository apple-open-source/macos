use inc::Module::Install 0.67;

name      'Class-C3-Componentised';
all_from  'lib/Class/C3/Componentised.pm';
author    'Ash Berlin <ash@cpan.org>';


requires  'Class::C3';
requires  'Class::Inspector';
requires  'Carp';
requires  'Test::Exception';

build_requires 'FindBin';

WriteAll;
