<?php

require_once "connect.inc";

function mktable_error_handler($errobj) {
    print "mktable error: ".$errobj->getMessage().": ".$errobj->getUserInfo();
    print "\n";
    exit;
}

$dbh->setErrorHandling(PEAR_ERROR_RETURN);
$dbh->query("DROP TABLE phptest");

$dbh->setErrorHandling(PEAR_ERROR_CALLBACK, "mktable_error_handler");
$test_mktable_query = "CREATE TABLE phptest (a INTEGER, b VARCHAR2(40), c LONG VARCHAR, d VARCHAR2(20))";
$dbh->query($test_mktable_query);
$dbh->query("INSERT INTO phptest VALUES(42, 'bing', 'This is a test', '1999-11-21')");

$dbh->setErrorHandling(PEAR_ERROR_RETURN);

?>
