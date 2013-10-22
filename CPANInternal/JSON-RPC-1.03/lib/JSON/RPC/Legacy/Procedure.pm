package JSON::RPC::Legacy::Procedure;

#
# http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html
#

$JSON::RPC::Legacy::Procedure::VERSION = '0.90';

use strict;
use attributes;
use Carp ();

my $Procedure = {};


sub check { $Procedure->{$_[0]} ? attributes::get($_[1]) : {}; }


sub FETCH_CODE_ATTRIBUTES {
    my ($pkg, $code) = @_;
    my $procedure = $Procedure->{$pkg}{$code} || { return_type => undef, argument_type => undef };

    return {
        return_type   => $procedure->{return_type},
        argument_type => $procedure->{argument_type},
    };
}


sub MODIFY_CODE_ATTRIBUTES {
    my ($pkg, $code, $attr) = @_;
    my ($ret_type, $args);

    if ($attr =~ /^([A-Z][a-z]+)(?:\(\s*([^)]*)\s*\))?$/) {
        $ret_type = $1 if (defined $1);
        $args     = $2 if (defined $2);
    }

    unless ($ret_type =~ /^Private|Public|Arr|Obj|Bit|Bool|Num|Str|Nil|None/) {
        Carp::croak("Invalid type '$attr'. Specify 'Parivate' or 'Public' or One of JSONRPC Return Types.");
    }

    if ($ret_type ne 'Private' and defined $args) {
        $Procedure->{$pkg}{$code}{argument_type} = _parse_argument_type($args);
    }

    $Procedure->{$pkg}{$code}{return_type} = $ret_type;

    return;
}



sub _parse_argument_type {
    my $text = shift;

    my $declaration;
    my $pos;
    my $name;

    $text =~ /^([,: a-zA-Z0-9]*)?$/;

    unless ( defined($declaration = $1) ) {
        Carp::croak("Invalid argument type.");
    }

    my @args = split/\s*,\s*/, $declaration;

    my $i = 0;

    $pos  = [];
    $name = {};

    for my $arg (@args) {
        if ($arg =~ /([_0-9a-zA-Z]+)(?::([a-z]+))?/) {
            push @$pos, $1;
            $name->{$1} = $2;
        }
    }

    return {
        position    => $pos,
        names       => $name,
    };
}



1;
__END__

=pod


=head1 NAME

JSON::RPC::Legacy::Procedure - JSON-RPC Service attributes

=head1 SYNOPSIS

 package MyApp;
 
 use base ('JSON::RPC::Legacy::Procedure');
 
 sub sum : Public {
     my ($s, @arg) = @_;
     return $arg[0] + $arg[1];
 }
 
 # or 
 
 sub sum : Public(a, b) {
     my ($s, $obj) = @_;
     return $obj->{a} + $obj->{b};
 }
 
 # or 
 
 sub sum : Number(a:num, b:num) {
     my ($s, $obj) = @_;
     return $obj->{a} + $obj->{b};
 }
 
 # private method can't be called by clients
 
 sub _foobar : Private {
     # ...
 }
 

=head1 DESCRIPTION

Using this module, you can write a subroutine with a special attribute.


Currently, in below attributes, only Public and Private are available.
Others are same as Public.

=over

=item Public

Means that a client can call this procedure.

=item Private

Means that a client can't call this procedure.

=item Arr

Means that its return values is an array object.

=item Obj

Means that its return values is a member object.

=item Bit

=item Bool

Means that a return values is a C<true> or C<false>.


=item Num

Means that its return values is a number.

=item Str

Means that its return values is a string.

=item Nil

=item None

Means that its return values is a C<null>.

=back


=head1 TODO

=over

=item Auto Service Description


=item Type check

=back

=head1 SEE ALSO

L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html>


=head1 AUTHOR

Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>


=head1 COPYRIGHT AND LICENSE

Copyright 2007 by Makamaka Hannyaharamitu

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 


=cut
