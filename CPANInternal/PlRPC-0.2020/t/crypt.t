# -*- perl -*-
#

require 5.004;
use strict;

eval { require Crypt::DES };
if ($@ || $Crypt::DES::VERSION < 2.03) {
    print "1..0\n";
    exit 0;
}

require "t/lib.pl";


my $numTests = 18;
my $numTest = 0;

my $hostkey = 'b3a6d83ef3187ac4';
my $userkey = '9823adc3287efa98';


# Create a configfile with host encryption.
my $cfg = <<"EOF";

require Crypt::DES;
{
    clients => [ {
	'mask'   => '^127\.0\.0\.1\$',
	'accept' => 1,
	'cipher' => Crypt::DES->new(pack("H*", "$hostkey")),
	'users' => [ {
	    'name' => 'bob'
	    },
	    {
	    'name' => 'jim',
	    'cipher' => Crypt::DES->new(pack("H*", "$userkey"))
	    } ] }
    ]
}
EOF
if (!open(FILE, ">t/crypt.cfg")  ||  !(print FILE ($cfg))  || !close(FILE)) {
    die "Error while creating config file t/crypt.cfg: $!";
}


my($handle, $port);
($handle, $port) = Net::Daemon::Test->Child($numTests,
					    $^X, '-Iblib/lib',
					    '-Iblib/arch',
					    't/server', '--mode=single',
					    '--debug', '--timeout', 60,
					    '--configfile', 't/crypt.cfg');


require Crypt::DES;
my $hostcipher = Crypt::DES->new(pack("H*", $hostkey));
my $usercipher = Crypt::DES->new(pack("H*", $userkey));

my @opts = ('peeraddr' => '127.0.0.1', 'peerport' => $port, 'debug' => 1,
	    'application' => 'CalcServer', 'version' => 0.01,
	    'timeout' => 20, 'usercipher' => $hostcipher);


RunTests('user' => 'bob', @opts);

RunTests('usercipher' => $usercipher, 'user' => 'jim', @opts);
$handle->Terminate() if $handle;









