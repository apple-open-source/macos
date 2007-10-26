# This Makefile.PL creates a pass-through Makefile that simply calls
# the equivalent Module::Build methods for each make target.  See the
# documentation for Module::Build::Compat for more information.

use lib qw(lib);
use Module::Build::Compat;

Module::Build::Compat->run_build_pl(args => \@ARGV);
Module::Build::Compat->write_makefile();
