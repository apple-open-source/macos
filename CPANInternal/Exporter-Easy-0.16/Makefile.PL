# $Header: /home/fergal/my/cvs/Exporter-Easy/Makefile.PL,v 1.1 2003/02/11 22:37:36 fergal Exp $

use ExtUtils::MakeMaker;

WriteMakefile(
	AUTHOR => 'Fergal Daly <fergal@esatclear.ie>',
	NAME => 'Exporter::Easy',
	VERSION_FROM => './lib/Exporter/Easy.pm',
	INSTALLDIRS => 'perl',
);
