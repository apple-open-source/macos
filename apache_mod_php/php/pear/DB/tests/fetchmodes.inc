<?php

$dbh->setErrorHandling(PEAR_ERROR_DIE);

// testing default default
$sth = $dbh->query("SELECT * FROM phptest");
$row = $sth->fetchRow();
//var_dump($row);
print implode(" ", array_keys($row))."\n";

// testing ordered default
$dbh->setFetchMode(DB_FETCHMODE_ORDERED);
$sth = $dbh->query("SELECT * FROM phptest");
$row = $sth->fetchRow();
//var_dump($row);
print implode(" ", array_keys($row))."\n";

// testing assoc default
$dbh->setFetchMode(DB_FETCHMODE_ASSOC);
$sth = $dbh->query("SELECT * FROM phptest");
$row = $sth->fetchRow();
//var_dump($row);
print implode(" ", array_keys($row))."\n";

// testing ordered default with assoc specified
$dbh->setFetchMode(DB_FETCHMODE_ORDERED);
$sth = $dbh->query("SELECT * FROM phptest");
$row = $sth->fetchRow(DB_FETCHMODE_ASSOC);
//var_dump($row);
print implode(" ", array_keys($row))."\n";

// testing assoc default with ordered specified
$dbh->setFetchMode(DB_FETCHMODE_ASSOC);
$sth = $dbh->query("SELECT * FROM phptest");
$row = $sth->fetchRow(DB_FETCHMODE_ORDERED);
//var_dump($row);
print implode(" ", array_keys($row))."\n";

?>
