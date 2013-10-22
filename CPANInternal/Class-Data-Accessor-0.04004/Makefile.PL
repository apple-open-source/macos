# $Id$
use strict;
use warnings;
use inc::Module::Install 0.65;

name 'Class-Data-Accessor';
license 'perl';
perl_version '5.006001';
all_from 'lib/Class/Data/Accessor.pm';

requires 'Carp';

tests "t/*.t t/*/*.t";
clean_files "Class-Data-Accessor-* t/var";

eval {
    system 'pod2text lib/Class/Data/Accessor.pm > README';
};

auto_install;
WriteAll;
