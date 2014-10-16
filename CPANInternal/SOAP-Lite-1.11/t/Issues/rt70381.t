use Test::More;

if (not $ENV{PROFILE_PERFORMANCE}) {
	plan ('skip_all' => 'This is a performance test. Set PROFILE_PERFORMANCE env var to a true value to run');
	exit 1;
}
plan  qw(no_plan);

use SOAP::Lite;


my $content = SOAP::Data->name('test')->uri('http://example.org')
	->value([
		\SOAP::Data->name('level1')->value('value1')->uri('http:/example.org'),
		\SOAP::Data->name('level1')->value('value1')->uri('http:/example.org'),
		\SOAP::Data->name('level1')->value('value1')->uri('http:/example.org'),
		\SOAP::Data->name('level1')->value('value1')->uri('http:/example.org'),
		\SOAP::Data->name('level1')->value('value1')->uri('http:/example.org'),
		\SOAP::Data->name('level1')->value('value1')->uri('http:/example.org'),
		\SOAP::Data->name('level1')->value('value1')->uri('http:/example.org'),
		\SOAP::Data->name('level1')->value('value1')->uri('http:/example.org'),
		\SOAP::Data->name('level1')->value('value1')->uri('http:/example.org'),
		\SOAP::Data->name('level1')->value('value1')->uri('http:/example.org'),
	]);

my $soap = SOAP::Lite->proxy('loopback://');

for (1..100) {
	my $response = $soap->call('test', \$content);
}


