# ======================================================================
#
# Copyright (C) 2000-2004 Paul Kulchenko (paulclinger@yahoo.com)
# SOAP::Lite is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.
#
# $Id: MQ.pm,v 1.1 2004/10/16 17:45:17 byrnereese Exp $
#
# ======================================================================

__END__

=head1 NAME

SOAP::Transport::MQ - Server/Client side MQ support for SOAP::Lite

=head1 SYNOPSIS

=over 4

=item Client

  use SOAP::Lite 
    uri => 'http://my.own.site.com/My/Examples',
    proxy => 'mq://server:port?Channel=CHAN1;QueueManager=QM_SOAP;RequestQueue=SOAPREQ1;ReplyQueue=SOAPRESP1',
  ;

  print getStateName(1);

=item Server

  use SOAP::Transport::MQ;

  my $server = SOAP::Transport::MQ::Server
    ->new('mq://server:port?Channel=CHAN1;QueueManager=QM_SOAP;RequestQueue=SOAPREQ1;ReplyQueue=SOAPRESP1')
    # specify list of objects-by-reference here 
    -> objects_by_reference(qw(My::PersistentIterator My::SessionIterator My::Chat))
    # specify path to My/Examples.pm here
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method')
  ;

  print "Contact to SOAP server\n";
  do { $server->handle } while sleep 1;

=back

=head1 DESCRIPTION

=head1 COPYRIGHT

Copyright (C) 2000-2001 Paul Kulchenko. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHOR

Paul Kulchenko (paulclinger@yahoo.com)

=cut
