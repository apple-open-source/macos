# -*- perl -*-

use inc::Module::Install;

name('Convert-ASN1');
abstract('Convert between perl data structures and ASN.1 encoded packets');
author('Graham Barr <gbarr@cpan.org>');
version_from('lib/Convert/ASN1.pm');
license('perl');
requires( perl => 5.005 );

clean_files(qw(y.tab.pl));

include_deps('Test::More');

check_nmake();      # check and download nmake.exe for Win32

&Makefile->write;
&Meta->write;

sub MY::postamble {

  return <<POSTAMBLE;

mydist : manifest distsign dist

run_byacc:
	\$(PERL) mkparse parser.y lib/Convert/ASN1/parser.pm

POSTAMBLE

}

