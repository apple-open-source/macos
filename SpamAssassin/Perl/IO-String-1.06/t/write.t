print "1..1\n";

#$IO::String::DEBUG++;

use IO::String;
$io = IO::String->new;

print $io "Heisan\n";
$io->print("a", "b", "c");

{
    local($\) = "\n";
    print $io "d", "e";
    local($,) = ",";
    print $io "f", "g", "h";
}

$foo = "1234567890";

syswrite($io, $foo, length($foo));
$io->syswrite($foo);
$io->syswrite($foo, length($foo));
$io->write($foo, length($foo), 5);
$io->write("xxx\n", 100, -1);

for (1..3) {
    printf $io "i(%d)", $_;
    $io->printf("[%d]\n", $_);
}
select $io;
print "\n";

$io->setpos(0);
print "h";


local *str = $io->string_ref;

select STDOUT;
print $str;

print "not " unless $str eq "heisan\nabcde\nf,g,h\n" .
                            ("1234567890" x 3) . "67890\n" .
                            "i(1)[1]\ni(2)[2]\ni(3)[3]\n\n";
print "ok 1\n";

