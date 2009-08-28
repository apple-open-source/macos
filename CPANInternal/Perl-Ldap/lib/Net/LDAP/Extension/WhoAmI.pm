
package Net::LDAP::Extension::WhoAmI;

require Net::LDAP::Extension;

$VERSION = "0.01";
@ISA = qw(Net::LDAP::Extension);

sub Net::LDAP::who_am_i {
  my $ldap = shift;
  my %opt = @_;

  my $res = $ldap->extension( name => '1.3.6.1.4.1.4203.1.11.3', %opt );
  return $res;
}

1;

__END__

=head1 NAME

Net::LDAP::Extension::WhoAmI - LDAP "Who am I?" Operation

=head1 SYNOPSIS

 use Net::LDAP;
 use Net::LDAP::Extension::WhoAmI;

 $ldap = Net::LDAP->new( "ldap.mydomain.eg" );

 $ldap->bind('cn=Joe User,cn=People,dc=example,dc=com",
             password => 'secret');

 $mesg = $ldap->who_am_i();

 die "error: ", $mesg->code(), ": ", $mesg->error()  if ($mesg->code());

 print "you are bound with authzId ", $mesg->response(), "\n";


=head1 DESCRIPTION

C<Net::LDAP::Extension::WhoAmI> implements the C<Who am I?>
extended LDAPv3 operation as described in draft-zeilenga-ldap-authzid-09.

It implements no object by itself but extends the L<Net::LDAP> object 
by another method:

=head1 METHODS

=over 4

=item who_am_i

Obtain the authorization identity which the server has associated 
with the user or application entity.

=back

=head1 SEE ALSO

L<Net::LDAP>,
L<Net::LDAP::Extension>

=head1 AUTHOR

Norbert Klasen E<lt>norbert.klasen@avinci.deE<gt>,

Please report any bugs, or post any suggestions, to the perl-ldap
mailing list E<lt>perl-ldap@perl.orgE<gt>

=head1 COPYRIGHT

Copyright (c) 2002-2004 Graham Barr. All rights reserved. This program is
free software; you can redistribute it and/or modify it under the same
terms as Perl itself.

=cut

