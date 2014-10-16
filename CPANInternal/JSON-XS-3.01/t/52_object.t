BEGIN { $| = 1; print "1..20\n"; }
BEGIN { $^W = 0 } # hate

use JSON::XS;

$json = JSON::XS->new->convert_blessed->allow_tags->allow_nonref;

print "ok 1\n";

sub JSON::XS::tojson::TO_JSON {
   print @_ == 1 ? "" : "not ", "ok 3\n";
   print JSON::XS::tojson:: eq ref $_[0] ? "" : "not ", "ok 4\n";
   print $_[0]{k} == 1 ? "" : "not ", "ok 5\n";
   7
}

$obj = bless { k => 1 }, JSON::XS::tojson::;

print "ok 2\n";

$enc = $json->encode ($obj);
print $enc eq 7 ? "" : "not ", "ok 6 # $enc\n";

print "ok 7\n";

sub JSON::XS::freeze::FREEZE {
   print @_ == 2 ? "" : "not ", "ok 8\n";
   print $_[1] eq "JSON" ? "" : "not ", "ok 9\n";
   print JSON::XS::freeze:: eq ref $_[0] ? "" : "not ", "ok 10\n";
   print $_[0]{k} == 1 ? "" : "not ", "ok 11\n";
   (3, 1, 2)
}

sub JSON::XS::freeze::THAW {
   print @_ == 5 ? "" : "not ", "ok 13\n";
   print JSON::XS::freeze:: eq $_[0] ? "" : "not ", "ok 14\n";
   print $_[1] eq "JSON" ? "" : "not ", "ok 15\n";
   print $_[2] == 3 ? "" : "not ", "ok 16\n";
   print $_[3] == 1 ? "" : "not ", "ok 17\n";
   print $_[4] == 2 ? "" : "not ", "ok 18\n";
   777
}

$obj = bless { k => 1 }, JSON::XS::freeze::;
$enc = $json->encode ($obj);
print $enc eq '("JSON::XS::freeze")[3,1,2]' ? "" : "not ", "ok 12 # $enc\n";

$dec = $json->decode ($enc);
print $dec eq 777 ? "" : "not ", "ok 19\n";

print "ok 20\n";

