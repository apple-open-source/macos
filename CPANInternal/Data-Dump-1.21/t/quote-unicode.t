#!perl -w

BEGIN {
    if ($] < 5.008) {
	print "1..0 # Skipped: perl-5.8 required\n";
	exit;
    }
}

use strict;
use Test qw(plan ok skip);

plan tests => 8;

use Data::Dump qw(dump);

ok(dump("\x{FF}"), qq("\\xFF"));
ok(dump("\xFF\x{FFF}"), qq("\\xFF\\x{FFF}"));
ok(dump(join("", map chr($_), 400 .. 500)), qq("\\x{190}\\x{191}\\x{192}\\x{193}\\x{194}\\x{195}\\x{196}\\x{197}\\x{198}\\x{199}\\x{19A}\\x{19B}\\x{19C}\\x{19D}\\x{19E}\\x{19F}\\x{1A0}\\x{1A1}\\x{1A2}\\x{1A3}\\x{1A4}\\x{1A5}\\x{1A6}\\x{1A7}\\x{1A8}\\x{1A9}\\x{1AA}\\x{1AB}\\x{1AC}\\x{1AD}\\x{1AE}\\x{1AF}\\x{1B0}\\x{1B1}\\x{1B2}\\x{1B3}\\x{1B4}\\x{1B5}\\x{1B6}\\x{1B7}\\x{1B8}\\x{1B9}\\x{1BA}\\x{1BB}\\x{1BC}\\x{1BD}\\x{1BE}\\x{1BF}\\x{1C0}\\x{1C1}\\x{1C2}\\x{1C3}\\x{1C4}\\x{1C5}\\x{1C6}\\x{1C7}\\x{1C8}\\x{1C9}\\x{1CA}\\x{1CB}\\x{1CC}\\x{1CD}\\x{1CE}\\x{1CF}\\x{1D0}\\x{1D1}\\x{1D2}\\x{1D3}\\x{1D4}\\x{1D5}\\x{1D6}\\x{1D7}\\x{1D8}\\x{1D9}\\x{1DA}\\x{1DB}\\x{1DC}\\x{1DD}\\x{1DE}\\x{1DF}\\x{1E0}\\x{1E1}\\x{1E2}\\x{1E3}\\x{1E4}\\x{1E5}\\x{1E6}\\x{1E7}\\x{1E8}\\x{1E9}\\x{1EA}\\x{1EB}\\x{1EC}\\x{1ED}\\x{1EE}\\x{1EF}\\x{1F0}\\x{1F1}\\x{1F2}\\x{1F3}\\x{1F4}"));
ok(dump("\x{1_00FF}"), qq("\\x{100FF}"));
ok(dump("\x{FFF}\x{1_00FF}" x 30), qq(("\\x{FFF}\\x{100FF}" x 30)));

# Ensure that displaying long upgraded string does not downgrade
$a = "";
$a .= chr($_) for 128 .. 255;
$a .= "\x{FFF}"; chop($a); # upgrade
ok(utf8::is_utf8($a));
skip($] < 5.010 ? "perl-5.10 required" : "",
    dump($a), 'pack("H*","' . join('', map sprintf("%02x", $_), 128..255). '")');
ok(utf8::is_utf8($a));
