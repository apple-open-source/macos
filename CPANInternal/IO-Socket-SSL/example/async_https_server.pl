##########################################################
# example HTTPS server using nonblocking sockets
# requires Event::Lib
# at the moment the response consists only of the HTTP
# request, send back as text/plain
##########################################################

use strict;
use IO::Socket;
use IO::Socket::SSL;
use Event::Lib;
use Errno ':POSIX';

#$Net::SSLeay::trace=3;

eval 'use Debug';
*{DEBUG} = sub {} if !defined(&DEBUG);

# create server socket
my $server = IO::Socket::INET->new(
	LocalAddr => '0.0.0.0:9000',
	Listen => 10,
	Reuse => 1,
	Blocking => 0,
) || die $!;

event_new( $server, EV_READ|EV_PERSIST, \&_s_accept )->add();
event_mainloop;

##########################################################
### accept new client on server socket
##########################################################
sub _s_accept {
	my $fds = shift->fh;
	my $fdc = $fds->accept || return;
	DEBUG( "new client" );

	$fdc = IO::Socket::SSL->start_SSL( $fdc,
		SSL_startHandshake => 0,
		SSL_server => 1,
	) || die $!;

	$fdc->blocking(0);
	_ssl_accept( undef,$fdc );
}

##########################################################
### ssl handshake with client
### called again and again until the handshake is done
### this is called first from _s_accept w/o an event
### and later enters itself as new event until the 
### handshake is done
### if the handshake is done it inits the buffers for the 
### client socket and adds an event for reading the HTTP header
##########################################################
sub _ssl_accept {
	my ($event,$fdc) = @_;
	$fdc ||= $event->fh;
	if ( $fdc->accept_SSL ) {
		DEBUG( "new client ssl handshake done" );
		# setup the client
		${*$fdc}{rbuf} =  ${*$fdc}{wbuf} = '';
		event_new( $fdc, EV_READ, \&_client_read_header )->add;
	} elsif ( $! != EAGAIN ) {
		die "new client failed: $!|$SSL_ERROR";
	} else {
		DEBUG( "new client need to retry accept: $SSL_ERROR" );
		my $what = 
			$SSL_ERROR == SSL_WANT_READ  ? EV_READ  :
			$SSL_ERROR == SSL_WANT_WRITE ? EV_WRITE :
			die "unknown error";
		event_new( $fdc, $what,  \&_ssl_accept )->add;
	}
}

	
##########################################################
### read http header
### this will re-add itself as an event until the full
### http header was read
### after reading the header it will setup the response
### which will for now just send the header back as text/plain
##########################################################
sub _client_read_header {
	my $event = shift;
	my $fdc = $event->fh;
	DEBUG( "reading header" );
	my $rbuf_ref = \${*$fdc}{rbuf};
	my $n = sysread( $fdc,$$rbuf_ref,8192,length($$rbuf_ref));
	if ( !defined($n)) {
		die $! if $! != EAGAIN;
		DEBUG( $SSL_ERROR );
		if ( $SSL_ERROR == SSL_WANT_WRITE ) {
			# retry read once I can write
			event_new( $fdc, EV_WRITE, \&_client_read_header )->add;
		} else {
			$event->add; # retry
		}
	} elsif ( $n == 0 ) {
		DEBUG( "connection closed" );
		close($fdc);
	} else {
		# check if we have the whole http header
		my $i = index( $$rbuf_ref,"\r\n\r\n" );   # check \r\n\r\n
		$i = index( $$rbuf_ref,"\n\n" ) if $i<0;  # bad clients send \n\n only
		if ( $i<0 ) {
			$event->add; # read more from header
			return;
		}

		# got full header, send request back (we don't serve real pages yet)
		my $header = substr( $$rbuf_ref,0,$i,'' );
		DEBUG( "got header:\n$header" );
		my $wbuf_ref = \${*$fdc}{wbuf};
		$$wbuf_ref = "HTTP/1.0 200 Ok\r\nContent-type: text/plain\r\n\r\n".$header;
		DEBUG( "will send $$wbuf_ref" );
		event_new( $fdc, EV_WRITE, \&_client_write_response )->add;
	}
}

##########################################################
### this is called to write the response to the client
### this will re-add itself as an event as until the full
### response was send
### if it's done it will just close the socket
##########################################################
sub _client_write_response {
	my $event = shift;
	DEBUG( "writing response" );
	my $fdc = $event->fh;
	my $wbuf_ref = \${*$fdc}{wbuf};
	my $n = syswrite( $fdc,$$wbuf_ref );
	if ( !defined($n) && $! == EAGAIN) {
		# retry
		DEBUG( $SSL_ERROR );
		if ( $SSL_ERROR == SSL_WANT_READ ) {
			# retry write once we can read
			event_new( $fdc, EV_READ, \&_client_write_response )->add;
		} else {
			$event->add; # retry again
		}
	} elsif ( $n == 0 ) {
		DEBUG( "connection closed: $!" );
		close($fdc);
	} else {
		DEBUG( "wrote $n bytes" );
		substr($$wbuf_ref,0,$n,'' );
		if ($$wbuf_ref eq '') {
			DEBUG( "done" );
			close($fdc);
		} else {
			# send more
			$event->add
		}
	}
}

