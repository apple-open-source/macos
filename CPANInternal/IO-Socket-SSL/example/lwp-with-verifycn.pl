use strict;
use warnings;

## !!! make sure that Net::SSL never gets loaded, otherwise it will
## be used instead of IO::Socket::SSL from LWP

use IO::Socket::SSL 'debug0';
use LWP::Simple;

IO::Socket::SSL::set_ctx_defaults( 
	SSL_verifycn_scheme => 'www', 
	SSL_verify_mode => 1,
	SSL_ca_file => 'verisign.pem', # root CA of verisign
);
print get( 'https://signin.ebay.com' );



