#
# a test server for testing IO::Socket::SSL-class's behavior
# (marko.asplund at kronodoc.fi).
#
# $Id: ssl_server.pl,v 1.7 2000/11/08 09:25:21 aspa Exp $.
#

use strict;
use IO::Socket::SSL;


my ($sock, $s, $v_mode);

if($ARGV[0] eq "DEBUG") { $IO::Socket::SSL::DEBUG = 1; }

# Check to make sure that we were not accidentally run in the wrong
# directory:
unless (-d "certs") {
    if (-d "../certs") {
	chdir "..";
    } else {
	die "Please run this example from the IO::Socket::SSL distribution directory!\n";
    }
}

if(!($sock = IO::Socket::SSL->new( Listen => 5,
				   LocalAddr => 'localhost',
				   LocalPort => 9000,
				   Proto     => 'tcp',
				   Reuse     => 1,
				   SSL_verify_mode => 0x01,
				   SSL_passwd_cb => sub {return "bluebell"},
				 )) ) {
    warn "unable to create socket: ", &IO::Socket::SSL::errstr, "\n";
    exit(0);
}
warn "socket created: $sock.\n";

while (1) {
  warn "waiting for next connection.\n";
  
  while(($s = $sock->accept())) {
      my ($peer_cert, $subject_name, $issuer_name, $date, $str);
      
      if( ! $s ) {
	  warn "error: ", $sock->errstr, "\n";
	  next;
      }
      
      warn "connection opened ($s).\n";
      
      if( ref($sock) eq "IO::Socket::SSL") {
	  $subject_name = $s->peer_certificate("subject");
	  $issuer_name = $s->peer_certificate("issuer");
      }
      
      warn "\t subject: '$subject_name'.\n";
      warn "\t issuer: '$issuer_name'.\n";
  
      my $date = localtime();
      print $s "my date command says it's: '$date'";
      close($s);
      warn "\t connection closed.\n";
  }
}


$sock->close();

warn "loop exited.\n";
