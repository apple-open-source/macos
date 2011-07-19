use strict;
use inc::Module::Install;

name('DateTime-Format-Pg');
all_from('lib/DateTime/Format/Pg.pm');

requires 'DateTime' => '0.10';
requires 'DateTime::Format::Builder' => '0.72';
requires 'DateTime::TimeZone' => '0.05';

test_requires 'Test::More';

WriteAll;

