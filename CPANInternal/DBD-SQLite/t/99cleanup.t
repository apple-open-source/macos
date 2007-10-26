use Test;
BEGIN { plan tests => 2 }
ok(-e 'foo');
unlink('foo');
ok(!-e 'foo');

