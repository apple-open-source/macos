<?php
$dbh->dropSequence("test");
$e = $dbh->nextId("test", false);
if (DB::isError($e)) {
    print $e->getMessage() . "\n";
}
$a = $dbh->nextId("test");
$b = $dbh->nextId("test");
if (DB::isError($a)) {
    print "a: ".$a->toString()."\n";
} else {
    print "a=$a\n";
}
if (DB::isError($b)) {
    print "b: ".$b->toString()."\n";
} else {
    print "b=$b\n";
}
$dbh->dropSequence("test");
?>
