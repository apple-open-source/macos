#!perl -w

print "1..1\n";

use Data::Dump;

$a = {
   a => qr/Foo/,
   b => qr,abc/,is,
   c => qr/ foo /x,
   d => qr/foo/msix,
   e => qr//,
   f => qr/
     # hi there
     how do this look
   /x,
   g => qr,///////,,
   h => qr*/|,:*,
   i => qr*/|,:#*,
};

print "not " unless Data::Dump::dump($a) . "\n" eq <<'EOT'; print "ok 1\n";
{
  a => qr/Foo/,
  b => qr|abc/|si,
  c => qr/ foo /x,
  d => qr/foo/msix,
  e => qr//,
  f => qr/
            # hi there
            how do this look
          /x,
  g => qr|///////|,
  h => qr#/|,:#,
  i => qr/\/|,:#/,
}
EOT

#print Data::Dump::dump($a), "\n";
