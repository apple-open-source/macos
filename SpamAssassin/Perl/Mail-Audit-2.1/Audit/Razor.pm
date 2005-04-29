package Mail::Audit::Razor;
use Mail::Audit;
use vars qw(@VERSION $config);
$VERSION = '1.8';
1;

package Mail::Audit;

use strict;
use Razor::Client;
use Razor::Agent;
use Razor::String qw(hash);

sub is_spam
{
    my ($self) = @_;

    my @message = (split("\n", $self->header), "", "", @{$self->body}); 

    my $razor = new Razor::Agent($Mail::Audit::Razor::config || "$ENV{HOME}/razor.conf");
    my $response = $razor->check(sigs => [hash(\@message)])
	|| $razor->raise_error();

    return $response->[0];
}

sub spam_handle
{
    my ($self, $data, $action) = @_;

    if ($self->is_spam)
    {
	$self->accept($data)
	    if ($action eq "accept");
	$self->ignore()
	    if ($action eq "ignore");
	$self->reject($data)
	    if ($action eq "reject");
	$self->pipe($data)
	    if ($action eq "pipe");

	return 1;
    }

    return 0;
}

sub spam_accept
{
    &spam_handle(@_[0,1], "accept");
}

sub spam_ignore
{
    &spam_handle(@_[0,1], "ignore");
}

sub spam_reject
{
    &spam_handle(@_[0,1], "reject");
}

sub spam_pipe
{
    &spam_handle(@_[0,1], "pipe");
}
	
1;
__END__

=pod

=head1 NAME

Mail::Audit::Razor - Mail::Audit plugin for the Vipul's Razor spam detection system

=head1 SYNOPSIS

    use Mail::Audit qw(Razor);
        my $mail = Mail::Audit->new;
    ...
        $mail->spam_accept($spambox);
        $mail->accept;

=head1 DESCRIPTION

This is a Mail::Audit plugin that uses the Vipul's Razor distributed spam detection system to detect and deal with spam.  It requires the C<Razor::Client> and C<Razor::Agent> modules from C<http://razor.sourceforge.net>.

While Razor never flags false positives, it has been having problems with false reporting.  Until that gets fixed I'd advise against ignoring or rejecting spam with this module.

=head2 METHODS

=over 4

=item C<spam_accept($where)>

Calls C<accept> and returns 1 if the message is spam, otherwise 0.

=item C<spam_ignore>

Calls C<ignore> and returns 1 if the message is spam, otherwise 0.

=item C<spam_reject($reason)>

Calls C<reject> and returns 1 if the message is spam, otherwise 0.

=item C<spam_pipe($program)>

Calls C<pipe> and returns 1 if the message is spam, otherwise 0.

=item C<is_spam>

Returns 1 if the message is spam, 0 if it is not.

=head2 VARIABLES

=item C<$Mail::Audit::Razor::config>

The path to your razor config file.  The default is C<$ENV{HOME}/razor.conf>.

=head1 AUTHOR

Nate Mueller <nate@cs.wisc.edu>

=head1 SEE ALSO

C<http://razor.sourceforge.net> and
L<Mail::Audit>
