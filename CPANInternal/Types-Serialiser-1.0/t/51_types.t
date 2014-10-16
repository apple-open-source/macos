BEGIN { $| = 1; print "1..20\n"; }

use Types::Serialiser;

print "ok 1\n";

$dec = Types::Serialiser::false;
print !Types::Serialiser::is_true  $dec ? "" : "not ", "ok 2\n";
print Types::Serialiser::is_false  $dec ? "" : "not ", "ok 3\n";
print Types::Serialiser::is_bool   $dec ? "" : "not ", "ok 4\n";
print $dec == 0                         ? "" : "not ", "ok 5\n";
print !$dec == 1                        ? "" : "not ", "ok 6\n";
print $dec eq 0                         ? "" : "not ", "ok 7\n";
print $dec-1 < 0                        ? "" : "not ", "ok 8\n";
print $dec+1 > 0                        ? "" : "not ", "ok 9\n";
print $dec*2 == 0                       ? "" : "not ", "ok 10\n";

$dec = Types::Serialiser::true;
print Types::Serialiser::is_true   $dec ? "" : "not ", "ok 11\n";
print !Types::Serialiser::is_false $dec ? "" : "not ", "ok 12\n";
print Types::Serialiser::is_bool   $dec ? "" : "not ", "ok 13\n";
print $dec == 1                         ? "" : "not ", "ok 14\n";
print !$dec == 0                        ? "" : "not ", "ok 15\n";
print $dec eq 1                         ? "" : "not ", "ok 16\n";
print $dec-1 <= 0                       ? "" : "not ", "ok 17\n";
print $dec-2 < 0                        ? "" : "not ", "ok 18\n";
print $dec*2 == 2                       ? "" : "not ", "ok 19\n";

$dec = Types::Serialiser::error;
print Types::Serialiser::is_error  $dec ? "" : "not ", "ok 20\n";

