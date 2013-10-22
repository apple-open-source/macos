#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Transport::HTTP;

SOAP::Transport::HTTP::CGI
  # specify path to My/Examples.pm here
  -> dispatch_to('/Your/Path/To/Deployed/Modules',
		 'Module::Name',
		 'Module::method')
  # enable compression support
  -> options({compress_threshold => 10000})
  -> handle
;
