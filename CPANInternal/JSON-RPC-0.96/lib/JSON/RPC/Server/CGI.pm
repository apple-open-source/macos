##############################################################################
package JSON::RPC::Server::CGI;

use strict;
use CGI;
use JSON::RPC::Server; # for old Perl 5.005

use base qw(JSON::RPC::Server);

$JSON::RPC::Server::CGI::VERSION = '0.92';

sub new {
    my $class = shift;
    my $self  = $class->SUPER::new();
    my $cgi   = $self->cgi;

    $self->request( HTTP::Request->new($cgi->request_method, $cgi->url) );
    $self->path_info($cgi->path_info);

    $self;
}


sub retrieve_json_from_post {
    my $json = $_[0]->cgi->param('POSTDATA');
    return $json;
}


sub retrieve_json_from_get {
    my $self   = shift;
    my $cgi    = $self->cgi;
    my $params = {};

    $self->version(1.1);

    for my $name ($cgi->param) {
        my @values = $cgi->param($name);
        $params->{$name} = @values > 1 ? [@values] : $values[0];
    }

    my $method = $cgi->path_info;

    $method =~ s{^.*/}{};
    $self->{path_info} =~ s{/?[^/]+$}{};

    $self->json->encode({
        version => '1.1',
        method  => $method,
        params  => $params,
    });
}


sub response {
    my ($self, $response) = @_;
    print "Status: " . $response->code . "\015\012" . $response->headers_as_string("\015\012")
           . "\015\012" . $response->content;
}


sub cgi {
    $_[0]->{cgi} ||= new CGI;
}



1;
__END__


=head1 NAME

JSON::RPC::Server::CGI - JSON-RPC sever for CGI

=head1 SYNOPSIS

 # CGI version
 #--------------------------
 # In your CGI script
 use JSON::RPC::Server::CGI;
 
 my $server = JSON::RPC::Server::CGI->new;

 $server->dispatch('MyApp')->handle();
 
 # or  an array ref setting
 
 $server->dispatch( [qw/MyApp MyApp::Subclass/] )->handle();
 
 # or a hash ref setting
 
 $server->dispatch( {'/jsonrpc/API' => 'MyApp'} )->handle();
 
 
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

=head1 DESCRIPTION

Gets a client request.

Parses its JSON data.

Passes the server object and the object decoded from the JSON data to your procedure (method).

Takes your returned value (scalar or arrayref or hashref).

Sends a response.

Well, you write your procedure code only.


=head1 METHODS

They are inherited from the L<JSON::RPC::Server> methods basically.
The below methods are implemented in JSON::RPC::Server::CGI.

=over

=item new

Creates new JSON::RPC::Server::CGI object.

=item retrieve_json_from_post

retrieves a JSON request from the body in POST method.

=item retrieve_json_from_get

In the protocol v1.1, 'GET' request method is also allowable.
it retrieves a JSON request from the query string in GET method.

=item response

returns a response JSON data to a client.

=item cgi

returns the L<CGI> object.

=back

=head1 SEE ALSO

L<JSON::RPC::Server>,

L<JSON::RPC::Procedure>,

L<JSON>,

L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html>,

L<http://json-rpc.org/wiki/specification>,

=head1 AUTHOR

Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>


=head1 COPYRIGHT AND LICENSE

Copyright 2007-2008 by Makamaka Hannyaharamitu

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut
