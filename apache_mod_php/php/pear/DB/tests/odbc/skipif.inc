<?php
// This script prints "skip" unless:
// * the odbc extension is built-in or loadable, AND
// * there is a database called "test" accessible
//   with no username/password, AND
// * we have create/drop privileges on the entire "test"
//   database
if (!extension_loaded("odbc")) {
  $dlext = (substr(PHP_OS, 0, 3) == "WIN") ? ".dll" : ".so";
  @dl("odbc$dlext");
}
if (!extension_loaded("odbc")) {
    die("skip\n");
}
require "dsn.inc";
if (!$nativedsn) {
    die("skip\n");
}
$conn = @odbc_connect($nativedsn, $user, $pass);
if (!is_resource($conn)) {
    die("skip\n");
}
?>
