use inc::Module::Install;

name     'Class-Unload';
all_from 'lib/Class/Unload.pm';
author   'Dagfinn Ilmari Mannsåker <ilmari@ilmari.org>';

requires 'Class::Inspector';
build_requires 'Test::More';

WriteAll;

