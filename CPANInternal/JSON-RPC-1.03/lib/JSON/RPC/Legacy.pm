package JSON::RPC::Legacy;

use strict;

1;
__END__

=pod

=head1 NAME

JSON::RPC - Perl implementation of JSON-RPC 1.1 protocol

=head1 DESCRIPTION

 JSON-RPC is a stateless and light-weight remote procedure call (RPC)
 protocol for inter-networking applications over HTTP. It uses JSON
 as the data format for of all facets of a remote procedure call,
 including all application data carried in parameters.

quoted from L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html>.


This module was in JSON package on CPAN before.
Now its interfaces was completely changed.

The old modules - L<JSONRPC::Transport::HTTP> and L<Apache::JSONRPC> are deprecated.
Please try to use JSON::RPC::Server and JSON::RPC::Client which support both JSON-RPC
protocol version 1.1 and 1.0.


=head1 EXAMPLES

CGI version.

 #--------------------------
 # In your application class
 package MyApp;
 
 use base qw(JSON::RPC::Procedure); # Perl 5.6 or more than
 
 sub echo : Public {    # new version style. called by clients
     # first argument is JSON::RPC::Server object.
     return $_[1];
 }
 
 
 sub sum : Public(a:num, b:num) { # sets value into object member a, b.
     my ($s, $obj) = @_;
     # return a scalar value or a hashref or an arryaref.
     return $obj->{a} + $obj->{b};
 }
 
  
 sub a_private_method : Private {
     # ... can't be called by client
 }
 
 
 sub sum_old_style {  # old version style. taken as Public
     my ($s, @arg) = @_;
    return $arg[0] + $arg[1];
 }
 
 
 #--------------------------
 # In your triger script.
 use JSON::RPC::Server::CGI;
 use MyApp;
 
 # simple
  JSON::RPC::Server::CGI->dispatch('MyApp')->handle();
 
 # or 
 JSON::RPC::Server::CGI->dispatch([qw/MyApp FooBar/])->handle();
 
 # or INFO_PATH version
 JSON::RPC::Server::CGI->dispatch({'/Test' => 'MyApp'})->handle();
 
 #--------------------------
 # Client
 use JSON::RPC::Client;
 
 my $client = new JSON::RPC::Client;

 my $uri = 'http://www.example.com/jsonrpc/Test';
 my $obj = {
    method  => 'sum', # or 'MyApp.sum'
    params  => [10, 20],
 };
 
 my $res = $client->call( $uri, $obj )
 
 if($res){
    if ($res->is_error) {
        print "Error : ", $res->error_message;
    }
    else {
        print $res->result;
    }
 }
 else {
    print $client->status_line;
 }
 
 # or
 
 $client->prepare($uri, ['sum', 'echo']);
 print $client->sum(10, 23);
 

See to L<JSON::RPC::Server::CGI>, L<JSON::RPC::Server::Daemon>, L<JSON::RPC::Server::Apache>
L<JSON::RPC::Client> and L<JSON::RPC::Procedure>.


=head1 ABOUT NEW VERSION

=over

=item supports JSON-RPC protocol v1.1


=back

=head1 TODO

=over

=item Document

=item Examples

=item More Tests


=back


=head1 AUTHOR

Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>


=head1 COPYRIGHT AND LICENSE

Copyright 2007-2008 by Makamaka Hannyaharamitu

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut


