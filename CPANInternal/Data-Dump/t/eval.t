print "1..1\n";

use Data::Dump qw(dump);

# Create some structure;
$h = {af=>15, bf=>bless [1,2], "Foo"};
$h->{cf} = \$h->{af};
#$h->{bf}[2] = \$h;

@s = eval($dump_h = dump($h, $h, \$h, \$h->{af}));

$dump_s = dump(@s);

print "not " unless $dump_h eq $dump_s;
print "ok 1\n";

print "\n\$h = $dump_h;\n";
print "\n\$s = $dump_s;\n";
