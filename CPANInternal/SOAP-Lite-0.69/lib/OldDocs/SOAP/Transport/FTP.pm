__END__

=head1 NAME

SOAP::Transport::FTP - Client side FTP support for SOAP::Lite

=head1 SYNOPSIS

  use SOAP::Lite 
    uri => 'http://my.own.site.com/My/Examples',
    proxy => 'ftp://login:password@ftp.somewhere.com/relative/path/to/file.xml', # ftp server
    # proxy => 'ftp://login:password@ftp.somewhere.com//absolute/path/to/file.xml', # ftp server
  ;

  print getStateName(1);

=head1 DESCRIPTION

=head1 COPYRIGHT

Copyright (C) 2000-2001 Paul Kulchenko. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHOR

Paul Kulchenko (paulclinger@yahoo.com)

=cut
