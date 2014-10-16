package SOAP::Apache;

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Transport::HTTP;

my $server = SOAP::Transport::HTTP::Apache

  # specify list of objects-by-reference here 
  -> objects_by_reference(qw(My::PersistentIterator My::SessionIterator My::Chat))

  # specify path to My/Examples.pm here
  -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method') 

  # if you want to use module from BOTH mod_perl and mod_soap 
  # you should use either static or mixed dispatching
  # -> dispatch_to(qw(My::Examples My::Parameters My::PersistentIterator My::SessionIterator My::PingPong))

  # enable compression support
  -> options({compress_threshold => 10000})
; 

sub handler { $server->handler(@_) }

1;
