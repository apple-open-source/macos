# -*- perl -*-
#

require 5.004;
use strict;

require "t/lib.pl";


my $numTests = 11;
my $numTest = 0;


my $cfg = <<"EOF";

{
    clients => [ {
	'mask'   => '^127\.0\.0\.1\$',
	'accept' => 1,
	'methods' => {
	    'CalcServer' => {
		'NewHandle' => 1,
		'CallMethod' => 1
	    },
	    'Calculator' => {
		'new' => 1,
		'add' => 1,
		'multiply' => 1,
		'divide' => 1,
		'subtract' => 1
	    }
	} }
    ]
}
EOF
if (!open(FILE, ">t/methods.cfg")  ||  !(print FILE ($cfg))  || !close(FILE)) {
    die "Error while creating config file t/methods.cfg: $!";
}


my($handle, $port);
($handle, $port) = Net::Daemon::Test->Child($numTests,
					    $^X, '-Iblib/lib',
					    '-Iblib/arch',
					    't/server', '--mode=single',
					    '--debug', '--timeout', 60,
					    '--configfile', 't/methods.cfg');


my @opts = ('peeraddr' => '127.0.0.1', 'peerport' => $port, 'debug' => 1,
	    'application' => 'CalcServer', 'version' => 0.01,
	    'timeout' => 20);


my($client, $calculator) = RunTests(@opts);
Test(!eval { $calculator->not_permitted() });
Test($@ =~ /permitted/);

END { $handle->Terminate() if $handle };
