<?php // -*- PHP -*-

function error_handler(&$obj) {
    print "sequences.inc error_handler\n";
}

$dbh->dropSequence("test");

// test that sequences are not created if "ondemand" is false
$e = $dbh->nextId("test", false);
if (DB::isError($e)) {
    print $e->getMessage() . "\n";
}

// test that sequences are created if "ondemand" is true, and that
// two successive nextIds return adjacent values
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
print "b-a=".($b-$a)."\n";

// test that the user-specified error handler is really disabled
// during nextId, with per-object handler as well as global handler
$dbh->dropSequence("test");
$dbh->setErrorHandling(PEAR_ERROR_CALLBACK, "error_handler");
$c = $dbh->nextId("test");
if (DB::isError($c)) {
    print "c: ".$c->toString()."\n";
} else {
    print "c=$c\n";
}
$dbh->dropSequence("test");
$dbh->_default_error_mode = null;
PEAR::setErrorHandling(PEAR_ERROR_CALLBACK, "error_handler");
$d = $dbh->nextId("test");
if (DB::isError($d)) {
    print "d: ".$d->toString()."\n";
} else {
    print "d=$d\n";
}

// final clean-up
$dbh->dropSequence("test");

?>
