#!/usr/bin/perl

use strict;
use inc::Module::Install;

name        'Test-use-ok';
license     'MIT';
all_from    'lib/Test/use/ok.pm';
requires    'Test::More';
license     'perl';

sign; WriteAll;
