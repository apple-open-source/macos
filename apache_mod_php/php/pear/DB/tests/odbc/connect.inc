<?php
require_once "DB.php";
require "dsn.inc";
if (!$dsn) {
    die("tests not configure for this environment");
}
$dbh = DB::connect($dsn);
if (DB::isError($dbh)) {
    die("connect.inc: ".$dbh->getMessage());
}
?>
