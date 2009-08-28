# Copyright (c) 2003-2004 Derrik Pates <dpates@dsdk12.net> and Graham Barr
# <gbarr@pobox.com>. All rights reserved.  This program is free
# software; you can redistribute it and/or modify it under the same
# terms as Perl itself.

package Net::LDAPI;
@Net::LDAPI::ISA = ( 'Net::LDAP' );
$Net::LDAPI::VERSION = "0.03";

use strict;
use Net::LDAP;

sub new {
  shift->SUPER::new(@_, scheme => 'ldapi');
}

1;

__END__

=head1 NAME

Net::LDAPI - use LDAP over a UNIX domain socket

=head1 SYNOPSIS

 use Net::LDAPI;

 $ldapi = Net::LDAPI->new('/var/run/ldapi');

 # alternate way
 use Net::LDAP;

 $ldapi = Net::LDAP->new('ldapi://');

=head1 DESCRIPTION

Communicate using the LDAP protocol to a directory server using a UNIX
domain socket. This mechanism is non-standard, UNIX-specific and not
widely supported.

All the normal C<Net::LDAP> methods can be used with a C<Net::LDAPI>
object; see L<Net::LDAP> for details.

=head1 CONSTRUCTOR

=over 4

=item new ( [SOCKPATH] )

Create a new connection. SOCKPATH can optionally be specified, to
specify the location of the UNIX domain socket to connect to. See
L<Net::LDAP/new> for details.

=back

=head1 SEE ALSO

L<Net::LDAP>,
L<IO::Socket::UNIX>

=head1 BUGS

None yet.

=head1 AUTHOR

Derrik Pates E<lt>dpates@dsdk12.netE<gt>

=head1 COPYRIGHT

Copyright (c) 2003-2004, Derrik Pates and Graham Barr. All
rights reserved. This library is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

=cut
