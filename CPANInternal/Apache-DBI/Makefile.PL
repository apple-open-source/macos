#-------------------------------------------------------
#
# $Id: Makefile.PL,v 1.1.1.1 2004/05/25 23:07:10 jpetri Exp $
#
#-------------------------------------------------------

use ExtUtils::MakeMaker;

WriteMakefile(
    'NAME'         => 'Apache::DBI',
    'VERSION_FROM' => 'DBI.pm',
    'PREREQ_PM'    => { 'Test::More' => 0,    # without tests we don't know fun.
			'DBI'        => 1.00, # no DBI can never be any fun.
		      },
);

# EOF
