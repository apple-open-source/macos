# ======================================================================
#
# Copyright (C) 2000-2004 Paul Kulchenko (paulclinger@yahoo.com)
# SOAP::Lite is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.
#
# $Id: JABBER.pm,v 1.1 2004/10/16 17:45:17 byrnereese Exp $
#
# ======================================================================

__END__

=head1 NAME

SOAP::Transport::JABBER - Server/Client side JABBER support for SOAP::Lite

=head1 SYNOPSIS

=over 4

=item Client

  use SOAP::Lite 
    uri => 'http://my.own.site.com/My/Examples',
    proxy => 'jabber://username:password@jabber.org:5222/soaplite_server@jabber.org/',
    #         proto    username passwd   server     port destination                resource (optional)
  ;

  print getStateName(1);

=item Server

  use SOAP::Transport::JABBER;

  my $server = SOAP::Transport::JABBER::Server
    -> new('jabber://username:password@jabber.org:5222')
    # specify list of objects-by-reference here 
    -> objects_by_reference(qw(My::PersistentIterator My::SessionIterator My::Chat))
    # specify path to My/Examples.pm here
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method')
  ;

  print "Contact to SOAP server\n";
  do { $server->handle } while sleep 10;

=back

=head1 DESCRIPTION

=head1 COPYRIGHT

Copyright (C) 2000-2001 Paul Kulchenko. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHOR

Paul Kulchenko (paulclinger@yahoo.com)

=cut
