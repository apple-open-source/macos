#!perl -w

use strict;
use Test qw(plan ok skip);

plan tests => 17;

use Data::Dump qw(dump quote);
$Data::Dump::TRY_BASE64 = 0;

ok(dump(""), qq(""));
ok(dump("\n"), qq("\\n"));
ok(dump("\0\1\x1F\0" . 3), qq("\\0\\1\\37\\x003"));
ok(dump("xx" x 30), qq(("x" x 60)));
ok(dump("xy" x 30), qq(("xy" x 30)));
ok(dump("\0" x 1024), qq(("\\0" x 1024)));
ok(dump("\$" x 1024), qq(("\\\$" x 1024)));
ok(dump("\n" x (1024 * 1024)), qq(("\\n" x 1048576)));
ok(dump("\x7F\x80\xFF"), qq("\\x7F\\x80\\xFF"));
ok(dump(join("", map chr($_), 0..127)), qq("\\0\\1\\2\\3\\4\\5\\6\\a\\b\\t\\n\\13\\f\\r\\16\\17\\20\\21\\22\\23\\24\\25\\26\\27\\30\\31\\32\\e\\34\\35\\36\\37 !\\"#\\\$%&'()*+,-./0123456789:;<=>?\\\@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\\x7F"));
ok(dump(join("", map chr($_), 0..255)), qq(pack("H*","000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff")));

if (eval { require MIME::Base64 }) {
    local $Data::Dump::TRY_BASE64 = 1;
    ok(dump(join("", map chr($_), 0..255)), "do {\n  require MIME::Base64;\n  MIME::Base64::decode(\"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==\");\n}");
}
else {
    skip("MIME::Base64 missing", 1);
}

ok(quote(""), qq(""));
ok(quote(42), qq("42"));
ok(quote([]) =~ /^"ARRAY\(/);
ok(quote('"'), qq("\\""));
ok(quote("\0" x 1024), join("", '"', ("\\0") x 1024, '"'));

