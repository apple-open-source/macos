package Mail::Audit::PGP;
use Mail::Audit;
use vars qw(@VERSION);
$VERSION = '1.7';
1;

package Mail::Audit;
use strict;

sub fix_pgp_headers {
    my $item = shift;
    my $item_body = $item->body;
    my $body;
    my $content_type = $item->get('Content-type');

    # todo: update this to be MIME::Entity-compatible.
    # only munge the headers as shown here if the message is non-mime, or if the message is singlepart plain/text

    unless ($content_type =~ /^message\/|^multipart\/|^application\/pgp/) {
       $body .= $_ foreach (@$item_body);

       if ($body =~ /^-----BEGIN PGP MESSAGE-----/m and
           $body =~ /^-----END PGP MESSAGE-----/m) {
           $item->put_header("Content-Type:", 
                      "application/pgp; format=text; x-action=encrypt");
       }
       if ($body =~ /^-----BEGIN PGP SIGNED MESSAGE-----/m and
           $body =~ /^-----BEGIN PGP SIGNATURE-----/m and
           $body =~ /^-----END PGP SIGNATURE-----/m) {
           $item->put_header("Content-Type:", 
                      "application/pgp; format=text; x-action=sign");
       }
    }

    return 0;
}


1;
__END__

=pod

=head1 NAME

Mail::Audit::PGP - Mail::Audit plugin for PGP header fixing

=head1 SYNOPSIS

    use Mail::Audit qw(PGP);
	my $mail = Mail::Audit->new;
    ...
    $mail->fix_pgp_headers;

=head1 DESCRIPTION

This is a Mail::Audit plugin which provides a method for checking
whether a given email contains a PGP-signed or -encrypted message, and
if so, adds the relevant headers to tell the mailer to check the
signature or decrypt it.

=head1 AUTHOR

Simon Cozens <simon@cpan.org>

=head1 SEE ALSO

L<Mail::Audit>
