#!/bin/ksh
##########
export MANPATH=/trane/mach/man:/trane/share/man:/usr/man:/usr/X11/man:/usr/openwin/man:/var/man
export PATH=/trane/mach/bin:/trane/share/bin:$PATH
export HMANPRG=rman
export HMANOPT='-b -f html'
export QUERY_STRING='DirectPath=/usr/man/cat4.Z/cdf.4'
export QUERY_STRING='ManTopic=test&ManSection=key'
perl -d /home/teto/dev/hman/hman.pl
