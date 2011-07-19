##############################################################################
# JSONRPC version 1.1
# http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html
##############################################################################

use strict;
use JSON ();
use Carp ();

##############################################################################

package JSON::RPC::Client;

$JSON::RPC::Client::VERSION = '0.93';

use LWP::UserAgent;


BEGIN {
    for my $method (qw/uri ua json content_type version id allow_call status_line/) {
        eval qq|
            sub $method {
                \$_[0]->{$method} = \$_[1] if defined \$_[1];
                \$_[0]->{$method};
            }
        |;
    }
}



sub AUTOLOAD {
    my $self   = shift;
    my $method = $JSON::RPC::Client::AUTOLOAD;

    $method =~ s/.*:://;

    return if ($method eq 'DESTROY');

    $method =~ s/^__(\w+)__$/$1/;  # avoid to call built-in methods (ex. __VERSION__ => VERSION)

    unless ( exists $self->allow_call->{ $method } ) {
        Carp::croak("Can't call the method not allowed by prepare().");
    }

    my @params = @_;
    my $obj = {
        method => $method,
        params => (ref $_[0] ? $_[0] : [@_]),
    };

    my $ret = $self->call($self->uri, $obj);

    if ( $ret and $ret->is_success ) {
        return $ret->result;
    }
    else {
        Carp::croak ( $ret ? '(Procedure error) ' . $ret->error_message : $self->status_line );
    }

}


sub create_json_coder {
    JSON->new->allow_nonref->utf8;
}


sub new {
    my $proto = shift;
    my $self  = bless {}, (ref $proto ? ref $proto : $proto);

    my $ua  = LWP::UserAgent->new(
        agent   => 'JSON::RPC::Client/' . $JSON::RPC::Client::VERSION . ' beta ',
        timeout => 10,
    );

    $self->ua($ua);
    $self->json( $proto->create_json_coder );
    $self->version('1.1');
    $self->content_type('application/json');

    return $self;
}


sub prepare {
    my ($self, $uri, $procedures) = @_;
    $self->uri($uri);
    $self->allow_call({ map { ($_ => 1) } @$procedures  });
}


sub call {
    my ($self, $uri, $obj) = @_;
    my $result;

    if ($uri =~ /\?/) {
       $result = $self->_get($uri);
    }
    else {
        Carp::croak "not hashref." unless (ref $obj eq 'HASH');
        $result = $self->_post($uri, $obj);
    }

    my $service = $obj->{method} =~ /^system\./ if ( $obj );

    $self->status_line($result->status_line);

    if ($result->is_success) {

        return unless($result->content); # notification?

        if ($service) {
            return JSON::RPC::ServiceObject->new($result, $self->json);
        }

        return JSON::RPC::ReturnObject->new($result, $self->json);
    }
    else {
        return;
    }
}


sub _post {
    my ($self, $uri, $obj) = @_;
    my $json = $self->json;

    $obj->{version} ||= $self->{version} || '1.1';

    if ($obj->{version} eq '1.0') {
        delete $obj->{version};
        if (exists $obj->{id}) {
            $self->id($obj->{id}) if ($obj->{id}); # if undef, it is notification.
        }
        else {
            $obj->{id} = $self->id || ($self->id('JSON::RPC::Client'));
        }
    }
    else {
        $obj->{id} = $self->id if (defined $self->id);
    }

    my $content = $json->encode($obj);

    $self->ua->post(
        $uri,
        Content_Type   => $self->{content_type},
        Content        => $content,
        Accept         => 'application/json',
    );
}


sub _get {
    my ($self, $uri) = @_;
    $self->ua->get(
        $uri,
        Accept         => 'application/json',
    );
}



##############################################################################

package JSON::RPC::ReturnObject;

$JSON::RPC::ReturnObject::VERSION = $JSON::RPC::VERSION;

BEGIN {
    for my $method (qw/is_success content jsontext version/) {
        eval qq|
            sub $method {
                \$_[0]->{$method} = \$_[1] if defined \$_[1];
                \$_[0]->{$method};
            }
        |;
    }
}


sub new {
    my ($class, $obj, $json) = @_;
    my $content = ( $json || JSON->new->utf8 )->decode( $obj->content );

    my $self = bless {
        jsontext  => $obj->content,
        content   => $content,
    }, $class;

    $content->{error} ? $self->is_success(0) : $self->is_success(1);

    $content->{version} ? $self->version(1.1) : $self->version(0) ;

    $self;
}


sub is_error { !$_[0]->is_success; }

sub error_message {
    $_[0]->version ? $_[0]->{content}->{error}->{message} : $_[0]->{content}->{error};
}


sub result {
    $_[0]->{content}->{result};
}


##############################################################################

package JSON::RPC::ServiceObject;

use base qw(JSON::RPC::ReturnObject);


sub sdversion {
    $_[0]->{content}->{sdversion} || '';
}


sub name {
    $_[0]->{content}->{name} || '';
}


sub result {
    $_[0]->{content}->{summary} || '';
}



1;
__END__


=pod


=head1 NAME

JSON::RPC::Client - Perl implementation of JSON-RPC client

=head1 SYNOPSIS

   use JSON::RPC::Client;
   
   my $client = new JSON::RPC::Client;
   my $url    = 'http://www.example.com/jsonrpc/API';
   
   my $callobj = {
      method  => 'sum',
      params  => [ 17, 25 ], # ex.) params => { a => 20, b => 10 } for JSON-RPC v1.1
   };
   
   my $res = $client->call($uri, $callobj);
   
   if($res) {
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
   
   
   # Easy access
   
   $client->prepare($uri, ['sum', 'echo']);
   print $client->sum(10, 23);
 

=head1 DESCRIPTION

This is JSON-RPC Client.
See L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html>.

Gets a perl object and convert to a JSON request data.

Sends the request to a server.

Gets a response returned by the server.

Converts the JSON response data to the perl object.


=head1 JSON::RPC::Client

=head2 METHODS

=over

=item $client = JSON::RPC::Client->new

Creates new JSON::RPC::Client object.

=item $response = $client->call($uri, $procedure_object)

Calls to $uri with $procedure_object.
The request method is usually C<POST>.
If $uri has query string, method is C<GET>.

About 'GET' method,
see to L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html#GetProcedureCall>.

Return value is L</JSON::RPC::ReturnObject>.


=item $client->prepare($uri, $arrayref_of_procedure)

Allow to call methods in contents of $arrayref_of_procedure.
Then you can call the prepared methods with an array reference or a list.

The return value is a result part of JSON::RPC::ReturnObject.

   $client->prepare($uri, ['sum', 'echo']);
   
   $res = $client->echo('foobar');  # $res is 'foobar'.
   
   $res = $client->sum(10, 20);     # sum up
   $res = $client->sum( [10, 20] ); # same as above

If you call a method which is not prepared, it will C<croak>.


Currently, B<can't call any method names as same as built-in methods>.

=item version

Sets the JSON-RPC protocol version.
1.1 by default.


=item id

Sets a request identifier.
In JSON-RPC 1.1, it is optoinal.

If you set C<version> 1.0 and don't set id,
the module sets 'JSON::RPC::Client' to it.


=item ua

Setter/getter to L<LWP::UserAgent> object.


=item json

Setter/getter to the JSON coder object.
Default is L<JSON>, likes this:

   $self->json( JSON->new->allow_nonref->utf8 );
   
   $json = $self->json;

This object serializes/deserializes JSON data.
By default, returned JSON data assumes UTF-8 encoded.


=item status_line

Returns status code;
After C<call> a remote procedure, the status code is set.

=item create_json_coder

(Class method)
Returns a JSON de/encoder in C<new>.
You can override it to use your favorite JSON de/encoder.


=back


=head1 JSON::RPC::ReturnObject

C<call> method or the methods set by C<prepared> returns this object.
(The returned JSON data is decoded by the JSON coder object which was passed
by the client object.)

=head2 METHODS

=over

=item is_success

If the call is successful, returns a true, otherwise a false.

=item is_error

If the call is not successful, returns a true, otherwise a false.

=item error_message

If the response contains an error message, returns it.

=item result

Returns the result part of a data structure returned by the called server.

=item content

Returns the whole data structure returned by the called server.

=item jsontext

Returns the row JSON data.

=item version

Returns the version of this response data.

=back

=head1 JSON::RPC::ServiceObject


=head1 RESERVED PROCEDURE

When a client call a procedure (method) name 'system.foobar',
JSON::RPC::Server look up MyApp::system::foobar.

L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html#ProcedureCall>

L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html#ServiceDescription>

There is JSON::RPC::Server::system::describe for default response of 'system.describe'.


=head1 SEE ALSO

L<http://json-rpc.org/wd/JSON-RPC-1-1-WD-20060807.html>

L<http://json-rpc.org/wiki/specification>

=head1 AUTHOR

Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>


=head1 COPYRIGHT AND LICENSE

Copyright 2007-2008 by Makamaka Hannyaharamitu

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut


