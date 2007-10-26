package HTTP::Proxy::BodyFilter::complete;

use strict;
use HTTP::Proxy;
use HTTP::Proxy::BodyFilter;
use vars qw( @ISA );
@ISA = qw( HTTP::Proxy::BodyFilter );
use Carp;

sub filter {
    my ( $self, $dataref, $message, $protocol, $buffer ) = @_;
    return unless defined $buffer;

    $$buffer  = $$dataref;
    $$dataref = "";
}

1;

__END__

=head1 NAME

HTTP::Proxy::BodyFilter::complete - A filter that passes on a complete body or nothing

=head1 SYNOPSIS

    use HTTP::Proxy;
    use HTTP::Proxy::BodyFilter::simple;
    use HTTP::Proxy::BodyFilter::complete;

    my $proxy = HTTP::Proxy->new;

    # pass the complete response body to our filter (in one pass)
    $proxy->push_filter(
        mime => 'text/html',
        response => HTTP::Proxy::BodyFilter::complete->new,
        response => HTTP::Proxy::BodyFilter::simple->new(
            sub {
                # some complex processing that needs
                # the whole response body
            }
        );
    );

    $proxy->start;

=head1 DESCRIPTION

The HTTP::Proxy::BodyFilter::complete filter will ensure that the next
filter in the filter chain will only receive complete messages body
(either request or response).

It will store the chunks of data as they arrive, only to pass the B<entire>
message body after the whole message has been received by the proxy.

Subsequent filters is the chain will receive the whole body as a big
piece of data.

=head1 CAVEAT EMPTOR

This consumes memory and time.

Use with caution, otherwise your client will timeout, or your proxy will
run out of memory.

=head1 METHOD

This filter has only one method, called automatically:

=over 4

=item filter()

Stores the incoming data in memory until the last moment. The data
is then released to the subsequent filters in the chain.

=back

=head1 AUTHOR

Philippe "BooK" Bruhat, E<lt>book@cpan.orgE<gt>.

=head1 THANKS

Thanks to Simon Cozens and Merijn H. Brandt, who needed this almost at
the same time. C<;-)>

=head1 COPYRIGHT

Copyright 2004-2005, Philippe Bruhat.

=head1 LICENSE

This module is free software; you can redistribute it or modify it under
the same terms as Perl itself.

=cut

