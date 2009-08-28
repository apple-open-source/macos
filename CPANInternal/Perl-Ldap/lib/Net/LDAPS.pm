# Copyright (c) 2000-2004 Chris Ridd <chris.ridd@isode.com> and
# Graham Barr <gbarr@pobox.com>. All rights reserved.  This program is
# free software; you can redistribute it and/or modify it under the
# same terms as Perl itself.

package Net::LDAPS;
@Net::LDAPS::ISA = ( 'Net::LDAP' );
$Net::LDAPS::VERSION = "0.05";

use strict;
use Net::LDAP;

sub new {
  shift->SUPER::new(@_, scheme => 'ldaps');
}

1;

__END__

=head1 NAME

Net::LDAPS - use LDAP over an SSL connection

=head1 SYNOPSIS

 use Net::LDAPS;

 $ldaps = Net::LDAPS->new('myhost.example.com',
			  port => '10000',
			  verify => 'require',
			  capath => '/usr/local/cacerts/');

 # alternate way
 use Net::LDAP;

 $ldaps = Net::LDAP->new('ldaps://myhost.example.com:10000',
			 verify => 'require',
			 capath => '/usr/local/cacerts/');

=head1 DESCRIPTION

Communicate using the LDAP protocol to a directory server using an
encrypted (SSL) network connection. This mechanism is non-standard but
widely supported; consider using LDAPv3 with the standard TLS
extension if possible (many servers do not support it yet.) See
L<Net::LDAP/start_tls>.

All the normal C<Net::LDAP> methods can be used with a C<Net::LDAPS>
object; see L<Net::LDAP> for details.

=head1 CONSTRUCTOR

=over 4

=item new ( HOST [, OPTIONS ] )

Create a new connection. HOST is the hostname to contact. OPTIONS is a
number of key/value pairs. See L<Net::LDAP/new> for details.

=back

=head1 SEE ALSO

L<Net::LDAP>,
L<IO::Socket::SSL>

=head1 BUGS

You cannot have more than one LDAPS connection at any one time, due to
restrictions in the underlying Net::SSLeay code.

=head1 AUTHOR

Chris Ridd E<lt>chris.ridd@isode.comE<gt>

=head1 COPYRIGHT

Copyright (c) 2000-2004, Chris Ridd and Graham Barr. All rights reserved. This
library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

