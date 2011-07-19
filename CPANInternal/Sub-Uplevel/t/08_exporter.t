use lib qw(t/lib);
use strict;
use Test::More;

plan tests => 1;

# Goal of these tests: confirm that Sub::Uplevel will work with Exporter's
# import() function

package main;
require Importer;
require Bar;
Importer::import_for_me('Bar','func3');
can_ok('main','func3');

