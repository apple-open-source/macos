__END__

=head1 NAME

SOAP::Transport::IO - Server side IO support for SOAP::Lite

=head1 SYNOPSIS

  use SOAP::Transport::IO;

  SOAP::Transport::IO::Server

    # you may specify as parameters for new():
    # -> new( in => 'in_file_name' [, out => 'out_file_name'] )
    # -> new( in => IN_HANDLE      [, out => OUT_HANDLE] )
    # -> new( in => *IN_HANDLE     [, out => *OUT_HANDLE] )
    # -> new( in => \*IN_HANDLE    [, out => \*OUT_HANDLE] )
  
    # -- OR --
    # any combinations
    # -> new( in => *STDIN, out => 'out_file_name' )
    # -> new( in => 'in_file_name', => \*OUT_HANDLE )
  
    # -- OR --
    # use in() and/or out() methods
    # -> in( *STDIN ) -> out( *STDOUT )
  
    # -- OR --
    # use default (when nothing specified):
    #      in => *STDIN, out => *STDOUT
  
    # don't forget, if you want to accept parameters from command line
    # \*HANDLER will be understood literally, so this syntax won't work 
    # and server will complain
  
    -> new(@ARGV)
  
    # specify path to My/Examples.pm here
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method') 
    -> handle
  ;

=head1 DESCRIPTION

=head1 COPYRIGHT

Copyright (C) 2000-2001 Paul Kulchenko. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHOR

Paul Kulchenko (paulclinger@yahoo.com)

=cut
