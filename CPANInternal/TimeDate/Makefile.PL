# This -*- perl -*- script makes the Makefile
# $Id: //depot/TimeDate/Makefile.PL#11 $

#--- Distribution section ---

$DISTNAME = 'TimeDate';
$VERSION  = "1.16";

#--- Write the Makefile

BEGIN { require 5.004 }

use ExtUtils::MakeMaker;

WriteMakefile(
	VERSION   => $VERSION,
	NAME      => $DISTNAME,
);


