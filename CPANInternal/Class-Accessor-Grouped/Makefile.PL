# $Id: Makefile.PL 7804 2009-10-20 23:19:14Z caelum $
use strict;
use warnings;
use inc::Module::Install 0.91;

name 'Class-Accessor-Grouped';
license 'perl';
perl_version '5.006001';
all_from 'lib/Class/Accessor/Grouped.pm';

requires 'Carp';
requires 'Scalar::Util';
requires 'MRO::Compat';
requires 'Class::Inspector';
requires 'Sub::Name' => '0.04';

# removed due to segfaults, see _hasXS()
#requires 'Class::XSAccessor';

test_requires 'Sub::Identify';

clean_files "Class-Accessor-Grouped-* t/var";

if (-e 'MANIFEST.SKIP') {
    system('pod2text lib/Class/Accessor/Grouped.pm > README');
}

auto_provides;
auto_install;

resources repository =>
'http://dev.catalyst.perl.org/repos/bast/trunk/Class-Accessor-Grouped/';

WriteAll;
