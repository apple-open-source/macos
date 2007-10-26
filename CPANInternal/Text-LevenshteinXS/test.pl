use Test;
BEGIN { plan tests => 6 };

use Text::LevenshteinXS qw(distance);

ok(1); 
if (distance("foo","four") == 2) {ok(1)} else {ok(0)}
if (distance("foo","foo")  == 0) {ok(1)} else {ok(0)}
if (distance("foo","")  == 3) {ok(1)} else {ok(0)}
if (distance("four","foo") == 2) {ok(1)} else {ok(0)}
if (distance("foo","bar") == 3) {ok(1)} else {ok(0)}





