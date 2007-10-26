#!perl -w

use URI::file;

@tests =  (
[ "file",          "unix",       "win32",         "mac" ],
#----------------  ------------  ---------------  --------------
[ "file://localhost/foo/bar",
	           "!/foo/bar",  "!\\foo\\bar",   "!foo:bar", ],
[ "file:///foo/bar",
	           "/foo/bar",   "\\foo\\bar",    "!foo:bar", ],
[ "file:/foo/bar", "!/foo/bar",  "!\\foo\\bar",   "foo:bar", ],
[ "foo/bar",       "foo/bar",    "foo\\bar",      ":foo:bar",],
[ "file://foo/bar","!//foo/bar", "!\\\\foo\\bar", "!foo:bar"],
[ "file://a:/",    "!//a:/",     "!A:\\",         undef],
[ "file:///A:/",   "/A:/",       "A:\\",          undef],
[ "file:///",      "/",          "\\",            undef],
[ ".",             ".",          ".",             ":"],
[ "..",            "..",         "..",            "::"],
[ "%2E",           "!.",         "!.",           ":."],
[ "../%2E%2E",     "!../..",     "!..\\..",      "::.."],
);

@os = @{shift @tests};
shift @os;  # file

my $num = @tests;
print "1..$num\n";

$testno = 1;

for $t (@tests) {
   my @t = @$t;
   my $file = shift @t;
   my $err;

   my $u = URI->new($file, "file");
   my $i = 0;
   for $os (@os) {
       my $f = $u->file($os);
       my $expect = $t[$i];
       $f = "<undef>" unless defined $f;
       $expect = "<undef>" unless defined $expect;
       my $loose;
       $loose++ if $expect =~ s/^!//;
       if ($expect ne $f) {
           print "URI->new('$file', 'file')->file('$os') ne $expect, but $f\n";
           $err++;
       }
       if (defined($t[$i]) && !$loose) {
	   $u2 = URI::file->new($t[$i], $os);
           unless ($u2->as_string eq $file) {
              print "URI::file->new('$t[$i]', '$os') ne $file, but $u2\n";
              $err++;
           }
       }
       $i++;
   }
   print "not " if $err;
   print "ok $testno\n";
   $testno++;
}
