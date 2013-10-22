package JSON::RPC::Constants;
use strict;
use parent qw(Exporter);

our @EXPORT_OK = qw(
    JSONRPC_DEBUG
    RPC_PARSE_ERROR
    RPC_INVALID_REQUEST
    RPC_METHOD_NOT_FOUND
    RPC_INVALID_PARAMS
    RPC_INTERNAL_ERROR
);
our %EXPORT_TAGS = (all => \@EXPORT_OK);

my %constants;
BEGIN {
    %constants = (
        JSONRPC_DEBUG     => $ENV{PERL_JSONRPC_DEBUG} ? 1 : 0,
        RPC_PARSE_ERROR      => -32700,
        RPC_INVALID_REQUEST  => -32600,
        RPC_METHOD_NOT_FOUND => -32601,
        RPC_INVALID_PARAMS   => -32602,
        RPC_INTERNAL_ERROR   => -32603,
    );
    require constant;
    constant->import( \%constants );
}

1;

__END__

=head1 NAME

JSON::RPC::Constants - Constants

=head1 SYNOPSIS

    use JSON::RPC::Constants qw(:all);
    # or, import one by one

=head1 DEBUG

=over 4 

=item B<JSONRPC_DEBUG>

Set to true if PERL_JSONRPC_DEBUG environmental variable is set to a value that evaluates to true. False otherwise. 

This controls debug output of the module.

=back

=head1 JSON RPC VALUES

These values are defined as per JSON RPC RFC.

=head2 RPC_PARSE_ERROR

=head2 RPC_INVALID_REQUEST

=head2 RPC_METHOD_NOT_FOUND

=head2 RPC_INVALID_PARAMS

=head2 RPC_INTERNAL_ERROR

=cut
