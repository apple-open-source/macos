#!/bin/ksh
##########
export MANPATH=/trane/mach/man:/trane/share/man:/usr/man:/usr/X11/man:/usr/openwin/man:/var/man
export PATH=/trane/mach/bin:/trane/share/bin:$PATH
export HMANPRG=rman
export HMANOPT='-b -f html'
exec /home/teto/dev/hman/hman.pl
