require "regex";
require "comparator-i;ascii-numeric";
require "envelope";

if address :regex :comparator "i;ascii-numeric" "from" "sirius(\\+.*)?@friep\\.example\\.com" {
	keep;
	stop;
}

if address :regex "from" "sirius(+\\+.*)?@friep\\.example\\.com" {
	keep;
	stop;
}

if header :regex "from" "sirius(\\+.*)?@friep\\.ex[]ample.com" {
    keep;
    stop;
}

if envelope :regex "from" "sirius(\\+.*)?@friep\\.ex[]ample.com" {
    keep;
    stop;
}

discard;
