# ======================================================================
#
# Copyright (C) 2000-2004 Paul Kulchenko (paulclinger@yahoo.com)
# SOAP::Lite is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.
#
# $Id: POP3.pm 36 2004-10-16 17:45:17Z byrnereese $
#
# ======================================================================

__END__

=head1 NAME

SOAP::Transport::POP3 - Server side POP3 support for SOAP::Lite

=head1 SYNOPSIS

  use SOAP::Transport::POP3;

  my $server = SOAP::Transport::POP3::Server
    -> new('pop://pop.mail.server')
    # if you want to have all in one place
    # -> new('pop://user:password@pop.mail.server') 
    # or, if you have server that supports MD5 protected passwords
    # -> new('pop://user:password;AUTH=+APOP@pop.mail.server') 
    # specify list of objects-by-reference here 
    -> objects_by_reference(qw(My::PersistentIterator My::SessionIterator My::Chat))
    # specify path to My/Examples.pm here
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method') 
  ;
  # you don't need to use next line if you specified your password in new()
  $server->login('user' => 'password') or die "Can't authenticate to POP3 server\n";

  # handle will return number of processed mails
  # you can organize loop if you want
  do { $server->handle } while sleep 10;

  # you may also call $server->quit explicitly to purge deleted messages

=head1 DESCRIPTION

=head1 COPYRIGHT

Copyright (C) 2000-2001 Paul Kulchenko. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHOR

Paul Kulchenko (paulclinger@yahoo.com)

=cut
