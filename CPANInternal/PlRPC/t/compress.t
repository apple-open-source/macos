# -*- perl -*-
#

require 5.004;
use strict;

eval { require Compress::Zlib };
if ($@) {
    print "1..0\n";
    exit 0;
}

require "t/lib.pl";


my $numTests = 18;
my $numTest = 0;

# Create a configfile with compression
my $cfg = <<"EOF";
require Compress::Zlib;

{
    clients => [ {
	'mask'   => '^127\.0\.0\.1\$',
	'accept' => 1,
	'users' => [ {
	    'name' => 'bob'
	    },
	    {
	    'name' => 'jim',
	    } ] }
    ]
}
EOF
if (!open(FILE, ">t/compress.cfg")
    ||  !(print FILE ($cfg))
    || !close(FILE)) {
    die "Error while creating config file t/compress.cfg: $!";
}


my($handle, $port);
if (@ARGV) {
    $port = $ARGV[0];
} else {
    ($handle, $port) = Net::Daemon::Test->Child
	($numTests, $^X, '-Iblib/lib', '-Iblib/arch', 't/server',
	 '--mode=single', '--debug', '--timeout', 60,
	 '--configfile', 't/compress.cfg', '--compression=gzip');
}

my @opts = ('peeraddr' => '127.0.0.1', 'peerport' => $port, 'debug' => 1,
	    'application' => 'CalcServer', 'version' => 0.01,
	    'timeout' => 20, 'compression' => 'gzip');


RunTests('user' => 'bob', @opts);

RunTests('user' => 'jim', @opts);
$handle->Terminate() if $handle;









