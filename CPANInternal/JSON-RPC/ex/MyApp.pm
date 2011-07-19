package
    MyApp;

use 5.006;
use strict;
use base qw(JSON::RPC::Procedure); # requires Perl 5.6 or later

use Data::Dumper;


sub _allowable_procedure {
    return {
        echo => \&echo,
        sum  => \&sum,
    };
}


sub echo : Public {
    my ($s, $args) = @_;
    return $args->[0];
}


sub now : Public() {
    return scalar(localtime);
}


sub sum : Number(a:num, b:num) {
    my ($s, $obj) = @_;
    return $obj->{a} + $obj->{b};
}


sub sum2 : Public {
    my $s = shift;

    if ($s->version) { # JSONRPC 1.1
        my $arg = shift;
        return $arg->[0] + $arg->[1];
    }
    else { # JSONRPC 1.0
        return $_[0] + $_[1];
    }

}


sub sum3 : String(a, b) {
    my $s = shift;
    return $_[0]->{a} + $_[0]->{b};
}


sub sum4 : Private {
    my $s = shift;
    # This is private...
}






package
    MyApp::system;

sub describe {
    {
        sdversion => "1.0",
        name      => 'MyApp',
    };
}



1;
__END__

=pod

=head1 NAME

MyApp - sample JSON-RPC server class

=head1 DESCRIPTION

This module is a smple code (for Perl 5.6 or later).
Please check the source.


=head2 PROCEDURES

=over

=item echo

Takes a scalar and returns it as is.

=item now

Returns the current time.


=item sum

Takes two numbers and returns the total.

  sum : Number(a:num, b:num)

The two numbers are automatically set into 'a' and 'b'.

=item sum2

Takes two numbers and returns the total.

  sum2 : Public

This routine is a sample for both JSONRPC 1.1 and 1.0

=item sum3

Same as sum3 but its format is difference.

  sum3 : String(a, b)

=item sum4

This is a private procedure, so client can't call this.

  sum4 : Private


=back MyApp::system::describe

This is a reserved procedure returns a C<Service Description> object.

See to L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html#ServiceDescription>.

=item _allowable_procedure

If you change the name into C<allowable_procedure>,
clients are able to call C<echo> and C<sum> only.

C<allowable_procedure> is a special name and the method
returns a hash reference contains procedure names and its code reference.

  sub allowable_procedure {
      return {
          echo => \&echo,
          sum  => \&sum,
      };
  }


=cut

=head1 AUTHOR

Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>


=head1 COPYRIGHT AND LICENSE

Copyright 2008 by Makamaka Hannyaharamitu

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut

