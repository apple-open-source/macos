use Test::More tests => 4;
use XML::Parser::Lite;

my $comment = '';
my $parser = new XML::Parser::Lite(
	Handlers => {
		Comment => sub { $comment .= $_[1]; },
	}
);

my $xml = <<'EOT';
<?xml version="1.0" encoding="UTF-8"?>
<!-- seed-viewer -->
<test>
</test>
EOT

eval {
    $parser->parse($xml);
};
ok(! $@);
is($comment, ' seed-viewer ');

$comment = '';
$xml = <<'EOT';
<?xml version="1.0" encoding="UTF-8"?>
<!-- seed_viewer -->
<test>
</test>
EOT
eval {
    $parser->parse($xml);
};
ok(! $@);
is($comment, ' seed_viewer ');
