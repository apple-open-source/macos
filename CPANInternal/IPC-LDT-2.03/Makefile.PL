
# = HISTORY SECTION =====================================================================

# ---------------------------------------------------------------------------------------
# version | date   | author     | changes
# ---------------------------------------------------------------------------------------
# 2.03    |29.02.00| J. Stenzel | added perl version demand;
# 2.02    |20.02.00| J. Stenzel | first version.
# ---------------------------------------------------------------------------------------

# = POD SECTION =========================================================================

# Makefile.PL -- Makefile template for IPC::LDT
#
# This file is part of the "IPC::LDT" module package.
# IPC::LDT is free software, see the module for license
# details.


# demand perl version
require 5.00503;

# load module
use ExtUtils::MakeMaker;

# write makefile
WriteMakefile(
              NAME         => 'IPC::LDT',
              VERSION_FROM => 'LDT.pm',
              PREREQ_PM    => {
                               Filter::Util::Call => 1,
                               Storable           => 0.6,
                              },
              dist         => {
                               COMPRESS => 'gzip -9',
                               SUFFIX   => 'tgz',
                              },
             );
