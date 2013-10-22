package
    MyApp::Subclass;


use strict;
use base qw(MyApp);


sub sum : String(a,b) {
    my $s = shift;
    return ($_[0]->{a} + $_[0]->{b}) * 2;
}


1;
__END__

=pod

=head1 NAME

MyApp::Subclass - sample JSON-RPC server class


=head1 DESCRIPTION

This module is a smple code (for Perl 5.6 or later).
Please check the source.

It is a MyApp subclass, so methods are inherited.


=head2 PROCEDURES

=over


=item sum

Takes two numbers and returns the added and multiplied number.


=cut

=head1 AUTHOR

Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>


=head1 COPYRIGHT AND LICENSE

Copyright 2008 by Makamaka Hannyaharamitu

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut


