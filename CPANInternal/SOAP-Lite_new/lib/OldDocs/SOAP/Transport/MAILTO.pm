# ======================================================================
#
# Copyright (C) 2000-2004 Paul Kulchenko (paulclinger@yahoo.com)
# SOAP::Lite is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.
#
# $Id: MAILTO.pm 36 2004-10-16 17:45:17Z byrnereese $
#
# ======================================================================

__END__

=head1 NAME

SOAP::Transport::MAILTO - Client side SMTP/sendmail support for SOAP::Lite

=head1 SYNOPSIS

  use SOAP::Lite;

  SOAP::Lite
    -> uri('http://soaplite.com/My/Examples')                
    -> proxy('mailto:destination.email@address', smtp => 'smtp.server', From => 'your.email', Subject => 'SOAP message')

    # or 
    # -> proxy('mailto:destination.email@address?From=your.email&Subject=SOAP%20message', smtp => 'smtp.server')

    # or if you want to send with sendmail
    # -> proxy('mailto:destination.email@address?From=your.email&Subject=SOAP%20message')

    # or if your sendmail is in undiscoverable place
    # -> proxy('mailto:destination.email@address?From=your.email&Subject=SOAP%20message', sendmail => 'command to run your sendmail')

    -> getStateName(12)
  ;

=head1 DESCRIPTION

=head1 COPYRIGHT

Copyright (C) 2000-2001 Paul Kulchenko. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHOR

Paul Kulchenko (paulclinger@yahoo.com)

=cut
