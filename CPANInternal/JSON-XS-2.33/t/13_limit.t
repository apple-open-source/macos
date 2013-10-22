BEGIN { $| = 1; print "1..11\n"; }

use JSON::XS;

our $test;
sub ok($;$) {
   print $_[0] ? "" : "not ", "ok ", ++$test, "\n";
}

my $def = 512;

my $js = JSON::XS->new;

ok (!eval { $js->decode (("[" x ($def + 1)) . ("]" x ($def + 1))) });
ok (ref $js->decode (("[" x $def) . ("]" x $def)));
ok (ref $js->decode (("{\"\":" x ($def - 1)) . "[]" . ("}" x ($def - 1))));
ok (!eval { $js->decode (("{\"\":" x $def) . "[]" . ("}" x $def)) });

ok (ref $js->max_depth (32)->decode (("[" x 32) . ("]" x 32)));

ok ($js->max_depth(1)->encode ([]));
ok (!eval { $js->encode ([[]]), 1 });

ok ($js->max_depth(2)->encode ([{}]));
ok (!eval { $js->encode ([[{}]]), 1 });

ok (eval { ref $js->max_size (8)->decode ("[      ]") });
eval { $js->max_size (8)->decode ("[       ]") }; ok ($@ =~ /max_size/);

