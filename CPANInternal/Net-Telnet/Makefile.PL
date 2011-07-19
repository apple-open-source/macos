## -*- Perl -*-

use ExtUtils::MakeMaker qw(WriteMakefile);

WriteMakefile(NAME	   => "Net::Telnet",
	      DISTNAME	   => "Net-Telnet",
	      VERSION_FROM => "lib/Net/Telnet.pm",
	      dist	   => { COMPRESS => "gzip", SUFFIX => "gz" },
	      ($] >= 5.005 ?
	       (ABSTRACT   => "Interact with TELNET port or other TCP ports",
		AUTHOR	   => "Jay Rogers <jay\@rgrs.com>") : ()),
	      );
