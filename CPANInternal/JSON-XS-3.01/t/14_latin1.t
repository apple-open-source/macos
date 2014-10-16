BEGIN { $| = 1; print "1..4\n"; }

use JSON::XS;

my $xs = JSON::XS->new->latin1->allow_nonref;

print $xs->encode ("\x{12}\x{89}       ") eq "\"\\u0012\x{89}       \"" ? "" : "not ", "ok 1\n";
print $xs->encode ("\x{12}\x{89}\x{abc}") eq "\"\\u0012\x{89}\\u0abc\"" ? "" : "not ", "ok 2\n";

print $xs->decode ("\"\\u0012\x{89}\""       ) eq "\x{12}\x{89}"        ? "" : "not ", "ok 3\n";
print $xs->decode ("\"\\u0012\x{89}\\u0abc\"") eq "\x{12}\x{89}\x{abc}" ? "" : "not ", "ok 4\n";

