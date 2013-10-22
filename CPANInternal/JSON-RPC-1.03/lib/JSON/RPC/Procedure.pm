package JSON::RPC::Procedure;
use strict;
use Carp ();
use Class::Accessor::Lite
    new => 1,
    rw => [ qw(
        id
        method
        params
    ) ]
;

1;

__END__

=head1 NAME

JSON::RPC::Procedure - A JSON::RPC Procedure

=head1 SYNOPSIS

    use JSON::RPC::Procedure;

    my $procedure = JSON::RPC::Procedure->new(
        id => ...,
        method => ...
        params => ...
    );

=head1 DESCRIPTION

A container for JSON RPC procedure information

=cut
