<?php

$dbh = DB::connect("mysql://localhost/test");

$dbh->setErrorHandling(PEAR_ERROR_RETURN);
$dbh->query("DROP TABLE phptest");

$dbh->setErrorHandling(PEAR_ERROR_TRIGGER);
$dbh->query("CREATE TABLE phptest (a INTEGER, b VARCHAR(40), c TEXT, d DATE)");
$dbh->query("INSERT INTO phptest VALUES(42, 'bing', 'This is a test', '1999-11-21')");

$dbh->setErrorHandling(PEAR_ERROR_RETURN);

?>
